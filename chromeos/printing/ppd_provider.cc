// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_parser.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_line_reader.h"
#include "chromeos/printing/printing_constants.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

// Holds a metadata_v2 reverse-index response
struct ReverseIndexJSON {
  // Canonical name of printer
  std::string effective_make_and_model;

  // Name of printer manufacturer
  std::string manufacturer;

  // Name of printer model
  std::string model;

  // Restrictions for this manufacturer
  PpdProvider::Restrictions restrictions;
};

// Holds a metadata_v2 manufacturers response
struct ManufacturersJSON {
  // Name of printer manufacturer
  std::string name;

  // Key for lookup of this manutacturer's printers (JSON file)
  std::string reference;

  // Restrictions for this manufacturer
  PpdProvider::Restrictions restrictions;
};

// Holds a metadata_v2 printers response
struct PrintersJSON {
  // Name of printer
  std::string name;

  // Canonical name of printer
  std::string effective_make_and_model;

  // Restrictions for this printer
  PpdProvider::Restrictions restrictions;
};

// Holds a metadata_v2 ppd-index response
struct PpdIndexJSON {
  // Canonical name of printer
  std::string effective_make_and_model;

  // Ppd filename
  std::string ppd_filename;
};

// A queued request to download printer information for a manufacturer.
struct PrinterResolutionQueueEntry {
  // Localized manufacturer name
  std::string manufacturer;

  // URL we are going to pull from.
  GURL url;

  // User callback on completion.
  PpdProvider::ResolvePrintersCallback cb;
};

// A queued request to download reverse index information for a make and model
struct ReverseIndexQueueEntry {
  // Canonical Printer Name
  std::string effective_make_and_model;

  // URL we are going to pull from.
  GURL url;

  // User callback on completion.
  PpdProvider::ReverseLookupCallback cb;
};

// Holds manufacturer to printers relation
struct ManufacturerMetadata {
  // Key used to look up the printer list on the server. This is initially
  // populated.
  std::string reference;

  // Map from localized printer name to canonical-make-and-model string for
  // the given printer. Populated on demand.
  std::unique_ptr<std::unordered_map<std::string, PrintersJSON>> printers;
};

// Extract cupsFilter/cupsFilter2 filter names from a line from a ppd.

// cupsFilter2 lines look like this:
//
// *cupsFilter2: "application/vnd.cups-raster application/vnd.foo 100
// rastertofoo"
//
// cupsFilter lines look like this:
//
// *cupsFilter: "application/vnd.cups-raster 100 rastertofoo"
//
// |field_name| is the starting token we look for (*cupsFilter: or
// *cupsFilter2:).
//
// |num_value_tokens| is the number of tokens we expect to find in the
// value string.  The filter is always the last of these.
//
// Return the name of the filter, if one is found.
//
// This would be simpler with re2, but re2 is not an allowed dependency in
// this part of the tree.
base::Optional<std::string> ExtractCupsFilter(const std::string& line,
                                              const std::string& field_name,
                                              int num_value_tokens) {
  std::string delims(" \n\t\r\"");
  base::StringTokenizer line_tok(line, delims);

  if (!line_tok.GetNext()) {
    return {};
  }
  if (line_tok.token_piece() != field_name) {
    return {};
  }

  // Skip to the last of the value tokens.
  for (int i = 0; i < num_value_tokens; ++i) {
    if (!line_tok.GetNext()) {
      return {};
    }
  }
  if (line_tok.token_piece() != "") {
    return line_tok.token_piece().as_string();
  }
  return {};
}

// Extract the used cups filters from a ppd.
//
// Note that CUPS (post 1.5) discards all cupsFilter lines if *any*
// cupsFilter2 lines exist.
//
std::vector<std::string> ExtractFiltersFromPpd(
    const std::string& ppd_contents) {
  std::string line;
  base::Optional<std::string> tmp;
  auto ppd_reader = PpdLineReader::Create(ppd_contents, 255);
  std::vector<std::string> cups_filters;
  std::vector<std::string> cups_filter2s;
  while (ppd_reader->NextLine(&line)) {
    tmp = ExtractCupsFilter(line, "*cupsFilter:", 3);
    if (tmp.has_value()) {
      cups_filters.push_back(tmp.value());
    }
    tmp = ExtractCupsFilter(line, "*cupsFilter2:", 4);
    if (tmp.has_value()) {
      cups_filter2s.push_back(tmp.value());
    }
  }
  if (!cups_filter2s.empty()) {
    return cups_filter2s;
  }
  return cups_filters;
}

// Returns false if there are obvious errors in the reference that will prevent
// resolution.
bool PpdReferenceIsWellFormed(const Printer::PpdReference& reference) {
  int filled_fields = 0;
  if (!reference.user_supplied_ppd_url.empty()) {
    ++filled_fields;
    GURL tmp_url(reference.user_supplied_ppd_url);
    if (!tmp_url.is_valid() || !tmp_url.SchemeIs("file")) {
      LOG(ERROR) << "Invalid url for a user-supplied ppd: "
                 << reference.user_supplied_ppd_url
                 << " (must be a file:// URL)";
      return false;
    }
  }
  if (!reference.effective_make_and_model.empty()) {
    ++filled_fields;
  }

  // All effective-make-and-model strings should be lowercased, since v2.
  // Since make-and-model strings could include non-Latin chars, only checking
  // that it excludes all upper-case chars A-Z.
  if (!std::all_of(reference.effective_make_and_model.begin(),
                   reference.effective_make_and_model.end(),
                   [](char c) -> bool { return !base::IsAsciiUpper(c); })) {
    return false;
  }
  // Should have exactly one non-empty field.
  return filled_fields == 1;
}

std::string PpdReferenceToCacheKey(const Printer::PpdReference& reference) {
  DCHECK(PpdReferenceIsWellFormed(reference));
  // The key prefixes here are arbitrary, but ensure we can't have an (unhashed)
  // collision between keys generated from different PpdReference fields.
  if (!reference.effective_make_and_model.empty()) {
    return std::string("em:") + reference.effective_make_and_model;
  } else {
    return std::string("up:") + reference.user_supplied_ppd_url;
  }
}

// Handles the result after PPD storage.
void OnPpdStored() {}

// Fetch the file pointed at by |url| and store it in |file_contents|.
// Returns true if the fetch was successful.
bool FetchFile(const GURL& url, std::string* file_contents) {
  CHECK(url.is_valid());
  CHECK(url.SchemeIs("file"));
  base::AssertBlockingAllowed();

  // Here we are un-escaping the file path represented by the url. If we don't
  // transform the url into a valid file path then the file may fail to be
  // opened by the system later.
  base::FilePath path;
  if (!net::FileURLToFilePath(url, &path)) {
    LOG(ERROR) << "Not a valid file URL.";
    return false;
  }
  return base::ReadFileToString(path, file_contents);
}

// Constructs and returns a printers' restrictions parsed from |dict|.
PpdProvider::Restrictions ComputeRestrictions(const base::Value& dict) {
  DCHECK(dict.is_dict());
  PpdProvider::Restrictions restrictions;

  const base::Value* min_milestone =
      dict.FindKeyOfType({"min_milestone"}, base::Value::Type::DOUBLE);
  const base::Value* max_milestone =
      dict.FindKeyOfType({"max_milestone"}, base::Value::Type::DOUBLE);

  if (min_milestone) {
    restrictions.min_milestone =
        base::Version(base::NumberToString(min_milestone->GetDouble()));
  }
  if (max_milestone) {
    restrictions.max_milestone =
        base::Version(base::NumberToString(max_milestone->GetDouble()));
  }

  return restrictions;
}

// Returns true if this |printer| is restricted from the
// |current_version|.
bool IsPrinterRestricted(const PrintersJSON& printer,
                         const base::Version& current_version) {
  const PpdProvider::Restrictions& restrictions = printer.restrictions;

  if (restrictions.min_milestone != base::Version("0.0") &&
      restrictions.min_milestone > current_version) {
    return true;
  }

  if (restrictions.max_milestone != base::Version("0.0") &&
      restrictions.max_milestone < current_version) {
    return true;
  }

  return false;
}

// Modifies |printers| by removing any restricted printers excluded from the
// current |version|, as judged by IsPrinterPermitted.
void FilterRestrictedPpdReferences(const base::Version& version,
                                   std::vector<PrintersJSON>* printers) {
  base::EraseIf(*printers, [&version](const PrintersJSON& printer) {
    return IsPrinterRestricted(printer, version);
  });
}

class PpdProviderImpl : public PpdProvider, public net::URLFetcherDelegate {
 public:
  // What kind of thing is the fetcher currently fetching?  We use this to
  // determine what to do when the fetcher returns a result.
  enum FetcherTarget {
    FT_LOCALES,        // Locales metadata.
    FT_MANUFACTURERS,  // List of manufacturers metadata.
    FT_PRINTERS,       // List of printers from a manufacturer.
    FT_PPD_INDEX,      // Master ppd index.
    FT_PPD,            // A Ppd file.
    FT_REVERSE_INDEX,  // List of sharded printers from a manufacturer
    FT_USB_DEVICES     // USB device id to canonical name map.
  };

  PpdProviderImpl(
      const std::string& browser_locale,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      scoped_refptr<PpdCache> ppd_cache,
      const base::Version& current_version,
      const PpdProvider::Options& options)
      : browser_locale_(browser_locale),
        url_context_getter_(url_context_getter),
        ppd_cache_(ppd_cache),
        disk_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        version_(current_version),
        options_(options),
        weak_factory_(this) {}

  // Resolving manufacturers requires a couple of steps, because of
  // localization.  First we have to figure out what locale to use, which
  // involves grabbing a list of available locales from the server.  Once we
  // have decided on a locale, we go out and fetch the manufacturers map in that
  // localization.
  //
  // This means when a request comes in, we either queue it and start background
  // fetches if necessary, or we satisfy it immediately from memory.
  void ResolveManufacturers(const ResolveManufacturersCallback& cb) override {
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "ResolveManufacturers must be called from a SequencedTaskRunner"
           "context";
    if (cached_metadata_.get() != nullptr) {
      // We already have this in memory.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(cb, PpdProvider::SUCCESS, GetManufacturerList()));
      return;
    }
    manufacturers_resolution_queue_.push_back(cb);
    MaybeStartFetch();
  }

  // If there are any queued ppd reference resolutions, attempt to make progress
  // on them.  Returns true if a fetch was started, false if no fetch was
  // started.
  bool MaybeStartNextPpdReferenceResolution() {
    while (!ppd_reference_resolution_queue_.empty()) {
      const PrinterSearchData& next =
          ppd_reference_resolution_queue_.front().first;
      // Have we successfully resolved next yet?
      bool resolved_next = false;
      if (!next.make_and_model.empty()) {
        // Check the index for each make-and-model guess.
        for (const std::string& make_and_model : next.make_and_model) {
          // Check if we need to load its ppd_index
          int ppd_index_shard = IndexShard(make_and_model);
          if (!base::ContainsKey(cached_ppd_idxs_, ppd_index_shard)) {
            StartFetch(GetPpdIndexURL(ppd_index_shard), FT_PPD_INDEX);
            return true;
          }
          if (base::ContainsKey(cached_ppd_idxs_[ppd_index_shard],
                                make_and_model)) {
            // Found a hit, satisfy this resolution.
            Printer::PpdReference ret;
            ret.effective_make_and_model = make_and_model;
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::Bind(ppd_reference_resolution_queue_.front().second,
                           PpdProvider::SUCCESS, ret));
            ppd_reference_resolution_queue_.pop_front();
            resolved_next = true;
            break;
          }
        }
      }
      if (!resolved_next) {
        // If we get to this point, either we don't have any make and model
        // guesses for the front entry, or they all missed.  Try USB ids
        // instead.  This entry will be completed when the usb fetch
        // returns.
        if (next.usb_vendor_id && next.usb_product_id) {
          StartFetch(GetUsbURL(next.usb_vendor_id), FT_USB_DEVICES);
          return true;
        }
        // We don't have anything else left to try.  NOT_FOUND it is.
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(ppd_reference_resolution_queue_.front().second,
                       PpdProvider::NOT_FOUND, Printer::PpdReference()));
        ppd_reference_resolution_queue_.pop_front();
      }
    }
    // Didn't start any fetches.
    return false;
  }

  // If there is work outstanding that requires a URL fetch to complete, start
  // going on it.
  void MaybeStartFetch() {
    if (fetch_inflight_) {
      // We'll call this again when the outstanding fetch completes.
      return;
    }

    if (MaybeStartNextPpdReferenceResolution()) {
      return;
    }

    if (!manufacturers_resolution_queue_.empty() ||
        !reverse_index_resolution_queue_.empty()) {
      if (locale_.empty()) {
        // Don't have a locale yet, figure that out first.
        StartFetch(GetLocalesURL(), FT_LOCALES);
      } else {
        // Get manufacturers based on the locale we have.
        if (!manufacturers_resolution_queue_.empty()) {
          StartFetch(GetManufacturersURL(locale_), FT_MANUFACTURERS);
        } else if (!reverse_index_resolution_queue_.empty()) {
          // Update the url with the locale before fetching
          ReverseIndexQueueEntry& entry =
              reverse_index_resolution_queue_.front();
          entry.url = GetReverseIndexURL(entry.effective_make_and_model);
          StartFetch(entry.url, FT_REVERSE_INDEX);
        }
      }
      return;
    }
    if (!printers_resolution_queue_.empty()) {
      StartFetch(printers_resolution_queue_.front().url, FT_PRINTERS);
      return;
    }
    while (!ppd_resolution_queue_.empty()) {
      auto& next = ppd_resolution_queue_.front();
      if (!next.first.user_supplied_ppd_url.empty()) {
        DCHECK(next.first.effective_make_and_model.empty());
        GURL url(next.first.user_supplied_ppd_url);
        DCHECK(url.is_valid());
        StartFetch(url, FT_PPD);
        return;
      }
      DCHECK(!next.first.effective_make_and_model.empty());
      int ppd_index_shard = IndexShard(next.first.effective_make_and_model);
      if (!base::ContainsKey(cached_ppd_idxs_, ppd_index_shard)) {
        // Have to have the ppd index before we can resolve by ppd server
        // key.
        StartFetch(GetPpdIndexURL(ppd_index_shard), FT_PPD_INDEX);
        return;
      }
      // Get the URL from the ppd index and start the fetch.
      auto& cached_ppd_index = cached_ppd_idxs_[ppd_index_shard];
      auto it = cached_ppd_index.find(next.first.effective_make_and_model);
      if (it != cached_ppd_index.end()) {
        StartFetch(GetPpdURL(it->second), FT_PPD);
        return;
      }
      // This ppd reference isn't in the index.  That's not good. Fail
      // out the current resolution and go try to start the next
      // thing if there is one.
      LOG(ERROR) << "PPD " << next.first.effective_make_and_model
                 << " not found in server index";

      FinishPpdResolution(std::move(next.second), PpdProvider::INTERNAL_ERROR,
                          std::string());
      ppd_resolution_queue_.pop_front();
    }
  }

  void ResolvePrinters(const std::string& manufacturer,
                       const ResolvePrintersCallback& cb) override {
    std::unordered_map<std::string, ManufacturerMetadata>::iterator it;
    if (cached_metadata_.get() == nullptr ||
        (it = cached_metadata_->find(manufacturer)) ==
            cached_metadata_->end()) {
      // User error.
      LOG(ERROR) << "Can't resolve printers for unknown manufacturer "
                 << manufacturer;
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(cb, PpdProvider::INTERNAL_ERROR, ResolvedPrintersList()));
      return;
    }
    if (it->second.printers.get() != nullptr) {
      // Satisfy from the cache.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS,
                                GetManufacturerPrinterList(it->second)));
    } else {
      // We haven't resolved this manufacturer yet.
      PrinterResolutionQueueEntry entry;
      entry.manufacturer = manufacturer;
      entry.url = GetPrintersURL(it->second.reference);
      entry.cb = cb;
      printers_resolution_queue_.push_back(entry);
      MaybeStartFetch();
    }
  }

  void ResolvePpdReference(const PrinterSearchData& search_data,
                           const ResolvePpdReferenceCallback& cb) override {
    // In v2 metadata, we work with lowercased effective_make_and_models.
    PrinterSearchData lowercase_search_data(search_data);
    for (auto& make_and_model : search_data.make_and_model) {
      lowercase_search_data.make_and_model.push_back(
          base::ToLowerASCII(make_and_model));
    }

    ppd_reference_resolution_queue_.push_back({lowercase_search_data, cb});
    MaybeStartFetch();
  }

  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    // In v2 metadata, we work with lowercased effective_make_and_models.
    Printer::PpdReference lowercase_reference(reference);
    lowercase_reference.effective_make_and_model =
        base::ToLowerASCII(lowercase_reference.effective_make_and_model);

    // Do a sanity check here, so we can assume |reference| is well-formed in
    // the rest of this class.
    if (!PpdReferenceIsWellFormed(lowercase_reference)) {
      FinishPpdResolution(std::move(cb), PpdProvider::INTERNAL_ERROR,
                          std::string());
      return;
    }
    // First step, check the cache.  If the cache lookup fails, we'll (try to)
    // consult the server.
    ppd_cache_->Find(PpdReferenceToCacheKey(lowercase_reference),
                     base::BindOnce(&PpdProviderImpl::ResolvePpdCacheLookupDone,
                                    weak_factory_.GetWeakPtr(),
                                    lowercase_reference, std::move(cb)));
  }

  void ReverseLookup(const std::string& effective_make_and_model,
                     const ReverseLookupCallback& cb) override {
    if (effective_make_and_model.empty()) {
      LOG(WARNING) << "Cannot resolve an empty make and model";
      PostReverseLookupFailure(PpdProvider::NOT_FOUND, cb);
      return;
    }

    // In v2 metadata, we work with lowercased effective_make_and_models.
    std::string lowercase_effective_make_and_model =
        base::ToLowerASCII(effective_make_and_model);

    ReverseIndexQueueEntry entry;
    entry.effective_make_and_model = lowercase_effective_make_and_model;
    entry.url = GetReverseIndexURL(lowercase_effective_make_and_model);
    entry.cb = cb;
    reverse_index_resolution_queue_.push_back(entry);
    MaybeStartFetch();
  }

  // Common handler that gets called whenever a fetch completes.  Note this
  // is used both for |fetcher_| fetches (i.e. http[s]) and file-based fetches;
  // |source| may be null in the latter case.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    switch (fetcher_target_) {
      case FT_LOCALES:
        OnLocalesFetchComplete();
        break;
      case FT_MANUFACTURERS:
        OnManufacturersFetchComplete();
        break;
      case FT_PRINTERS:
        OnPrintersFetchComplete();
        break;
      case FT_PPD_INDEX:
        OnPpdIndexFetchComplete(source->GetOriginalURL());
        break;
      case FT_PPD:
        OnPpdFetchComplete();
        break;
      case FT_REVERSE_INDEX:
        OnReverseIndexComplete();
        break;
      case FT_USB_DEVICES:
        OnUsbFetchComplete();
        break;
      default:
        LOG(DFATAL) << "Unknown fetch source";
    }
    fetch_inflight_ = false;
    MaybeStartFetch();
  }

 private:
  // Return the URL used to look up the supported locales list.
  GURL GetLocalesURL() {
    return GURL(options_.ppd_server_root + "/metadata_v2/locales.json");
  }

  GURL GetUsbURL(int vendor_id) {
    DCHECK_GT(vendor_id, 0);
    DCHECK_LE(vendor_id, 0xffff);

    return GURL(base::StringPrintf("%s/metadata_v2/usb-%04x.json",
                                   options_.ppd_server_root.c_str(),
                                   vendor_id));
  }

  // Return the URL used to get the |ppd_index_shard| index.
  GURL GetPpdIndexURL(int ppd_index_shard) {
    return GURL(base::StringPrintf("%s/metadata_v2/index-%02d.json",
                                   options_.ppd_server_root.c_str(),
                                   ppd_index_shard));
  }

  // Return the ppd index shard number from its |url|.
  int GetShardFromUrl(const GURL& url) {
    auto url_str = url.spec();
    if (url_str.empty()) {
      return -1;
    }

    // Strip shard number from 2 digits following 'index'
    int idx_pos = url_str.find_first_of("0123456789", url_str.find("index-"));
    return std::stoi(url_str.substr(idx_pos, 2));
  }

  // Return the URL to get a localized manufacturers map.
  GURL GetManufacturersURL(const std::string& locale) {
    return GURL(base::StringPrintf("%s/metadata_v2/manufacturers-%s.json",
                                   options_.ppd_server_root.c_str(),
                                   locale.c_str()));
  }

  // Return the URL used to get a list of printers from the manufacturer |ref|.
  GURL GetPrintersURL(const std::string& ref) {
    return GURL(base::StringPrintf(
        "%s/metadata_v2/%s", options_.ppd_server_root.c_str(), ref.c_str()));
  }

  // Return the URL used to get a ppd with the given filename.
  GURL GetPpdURL(const std::string& filename) {
    return GURL(base::StringPrintf(
        "%s/ppds/%s", options_.ppd_server_root.c_str(), filename.c_str()));
  }

  // Return the URL to get a localized, shared manufacturers map.
  GURL GetReverseIndexURL(const std::string& effective_make_and_model) {
    return GURL(base::StringPrintf("%s/metadata_v2/reverse_index-%s-%02d.json",
                                   options_.ppd_server_root.c_str(),
                                   locale_.c_str(),
                                   IndexShard(effective_make_and_model)));
  }

  // Create and return a fetcher that has the usual (for this class) flags set
  // and calls back to OnURLFetchComplete in this class when it finishes.
  void StartFetch(const GURL& url, FetcherTarget target) {
    DCHECK(!fetch_inflight_);
    DCHECK_EQ(fetcher_.get(), nullptr);
    fetcher_target_ = target;
    fetch_inflight_ = true;

    if (url.SchemeIs("http") || url.SchemeIs("https")) {
      fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
      fetcher_->SetRequestContext(url_context_getter_.get());
      fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
      fetcher_->Start();
    } else if (url.SchemeIs("file")) {
      auto file_contents = std::make_unique<std::string>();
      std::string* content_ptr = file_contents.get();
      base::PostTaskAndReplyWithResult(
          disk_task_runner_.get(), FROM_HERE,
          base::Bind(&FetchFile, url, content_ptr),
          base::Bind(&PpdProviderImpl::OnFileFetchComplete, this,
                     base::Passed(&file_contents)));
    }
  }

  // Handle the result of a file fetch.
  void OnFileFetchComplete(std::unique_ptr<std::string> file_contents,
                           bool success) {
    file_fetch_success_ = success;
    file_fetch_contents_ = success ? *file_contents : "";
    OnURLFetchComplete(nullptr);
  }

  void FinishPpdResolution(ResolvePpdCallback cb,
                           PpdProvider::CallbackResultCode result_code,
                           const std::string& ppd_contents) {
    if (result_code == PpdProvider::SUCCESS) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), PpdProvider::SUCCESS, ppd_contents,
                         ExtractFiltersFromPpd(ppd_contents)));
    } else {
      // Just post the failure.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), result_code, std::string(),
                                    std::vector<std::string>()));
    }
  }

  // Callback when the cache lookup for a ppd request finishes.  If we hit in
  // the cache, satisfy the resolution, otherwise kick it over to the fetcher
  // queue to be grabbed from a server.
  void ResolvePpdCacheLookupDone(const Printer::PpdReference& reference,
                                 ResolvePpdCallback cb,
                                 const PpdCache::FindResult& result) {
    if (result.success) {
      // Cache hit.
      FinishPpdResolution(std::move(cb), PpdProvider::SUCCESS, result.contents);
    } else {
      // Cache miss.  Queue it to be satisfied by the fetcher queue.
      ppd_resolution_queue_.push_back({reference, std::move(cb)});
      MaybeStartFetch();
    }
  }

  // Handler for the completion of the locales fetch.  This response should be a
  // list of strings, each of which is a locale in which we can answer queries
  // on the server.  The server (should) guarantee that we get 'en' as an
  // absolute minimum.
  //
  // Combine this information with the browser locale to figure out the best
  // locale to use, and then start a fetch of the manufacturers map in that
  // locale.
  void OnLocalesFetchComplete() {
    std::string contents;
    if (ValidateAndGetResponseAsString(&contents) != PpdProvider::SUCCESS) {
      FailQueuedMetadataResolutions(PpdProvider::SERVER_ERROR);
      return;
    }
    auto top_list = base::ListValue::From(base::JSONReader::Read(contents));

    if (top_list.get() == nullptr) {
      // We got something malformed back.
      FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
      return;
    }

    // This should just be a simple list of locale strings.
    std::vector<std::string> available_locales;
    bool found_en = false;
    for (const base::Value& entry : *top_list) {
      std::string tmp;
      // Locales should have at *least* a two-character country code.  100 is an
      // arbitrary upper bound for length to protect against extreme bogosity.
      if (!entry.GetAsString(&tmp) || tmp.size() < 2 || tmp.size() > 100) {
        FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
        return;
      }
      if (tmp == "en") {
        found_en = true;
      }
      available_locales.push_back(tmp);
    }
    if (available_locales.empty() || !found_en) {
      // We have no locales, or we didn't get an english locale (which is our
      // ultimate fallback)
      FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
      return;
    }
    // Everything checks out, set the locale, head back to fetch dispatch
    // to start the manufacturer fetch.
    locale_ = GetBestLocale(available_locales);
  }

  // Called when the |fetcher_| is expected have the results of a
  // manufacturer map (which maps localized manufacturer names to keys for
  // looking up printers from that manufacturer).  Use this information to
  // populate manufacturer_map_, and resolve all queued ResolveManufacturers()
  // calls.
  void OnManufacturersFetchComplete() {
    DCHECK_EQ(nullptr, cached_metadata_.get());
    std::vector<ManufacturersJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseManufacturersJSON(&contents);
    if (code != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed manufacturer parsing";
      FailQueuedMetadataResolutions(code);
      return;
    }
    cached_metadata_ = std::make_unique<
        std::unordered_map<std::string, ManufacturerMetadata>>();

    for (const auto& entry : contents) {
      (*cached_metadata_)[entry.name].reference = entry.reference;
    }

    std::vector<std::string> manufacturer_list = GetManufacturerList();
    // Complete any queued manufacturer resolutions.
    for (const auto& cb : manufacturers_resolution_queue_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS, manufacturer_list));
    }
    manufacturers_resolution_queue_.clear();
  }

  // The outstanding fetch associated with the front of
  // |printers_resolution_queue_| finished, use the response to satisfy that
  // ResolvePrinters() call.
  void OnPrintersFetchComplete() {
    CHECK(cached_metadata_.get() != nullptr);
    DCHECK(!printers_resolution_queue_.empty());
    std::vector<PrintersJSON> contents;

    PpdProvider::CallbackResultCode code =
        ValidateAndParsePrintersJSON(&contents);

    if (code != PpdProvider::SUCCESS) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(printers_resolution_queue_.front().cb, code,
                                ResolvedPrintersList()));
    } else {
      // This should be a list of lists of 2-element strings, where the first
      // element is the localized name of the printer and the second element
      // is the canonical name of the printer.
      const std::string& manufacturer =
          printers_resolution_queue_.front().manufacturer;
      auto it = cached_metadata_->find(manufacturer);

      // If we kicked off a resolution, the entry better have already been
      // in the map.
      CHECK(it != cached_metadata_->end());

      // Create the printer map in the cache, and populate it.
      auto& manufacturer_metadata = it->second;
      CHECK(manufacturer_metadata.printers.get() == nullptr);
      manufacturer_metadata.printers =
          std::make_unique<std::unordered_map<std::string, PrintersJSON>>();

      for (const auto& entry : contents) {
        manufacturer_metadata.printers->insert({entry.name, entry});
      }

      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(printers_resolution_queue_.front().cb,
                     PpdProvider::SUCCESS,
                     GetManufacturerPrinterList(manufacturer_metadata)));
    }
    printers_resolution_queue_.pop_front();
  }

  // Called when |fetcher_| should have just received an index mapping
  // ppd server keys to ppd filenames.  Use this to populate
  // |cached_ppd_idxs_|.
  void OnPpdIndexFetchComplete(const GURL& url) {
    std::vector<PpdIndexJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParsePpdIndexJSON(&contents);
    if (code != PpdProvider::SUCCESS) {
      FailQueuedServerPpdResolutions(code);
    } else {
      int ppd_index_shard = GetShardFromUrl(url);
      if (ppd_index_shard < 0) {
        FailQueuedServerPpdResolutions(PpdProvider::INTERNAL_ERROR);
        return;
      }

      auto& cached_ppd_index = cached_ppd_idxs_[ppd_index_shard];
      for (const auto& entry : contents) {
        cached_ppd_index.insert(
            {entry.effective_make_and_model, entry.ppd_filename});
      }
    }
  }

  // This is called when |fetcher_| should have just downloaded a ppd.  If we
  // downloaded something successfully, use it to satisfy the front of the ppd
  // resolution queue, otherwise fail out that resolution.
  void OnPpdFetchComplete() {
    DCHECK(!ppd_resolution_queue_.empty());
    std::string contents;

    if ((ValidateAndGetResponseAsString(&contents) != PpdProvider::SUCCESS)) {
      FinishPpdResolution(std::move(ppd_resolution_queue_.front().second),
                          PpdProvider::SERVER_ERROR, std::string());
    } else if (contents.size() > kMaxPpdSizeBytes) {
      FinishPpdResolution(std::move(ppd_resolution_queue_.front().second),
                          PpdProvider::PPD_TOO_LARGE, std::string());
    } else {
      ppd_cache_->Store(
          PpdReferenceToCacheKey(ppd_resolution_queue_.front().first), contents,
          base::Bind(&OnPpdStored));
      FinishPpdResolution(std::move(ppd_resolution_queue_.front().second),
                          PpdProvider::SUCCESS, contents);
    }
    ppd_resolution_queue_.pop_front();
  }

  // This is called when |fetch_| should have just downloaded a reverse index
  // file. If we downloaded something successfully, used the downloaded results
  // to satisfy the callback in the first item of the reverse index resolution
  // queue.
  void OnReverseIndexComplete() {
    DCHECK(!reverse_index_resolution_queue_.empty());
    std::vector<ReverseIndexJSON> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseReverseIndexJSON(&contents);
    const ReverseIndexQueueEntry& entry =
        reverse_index_resolution_queue_.front();

    if (code != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Request Failed or failed reverse index parsing";
      PostReverseLookupFailure(code, entry.cb);
    } else {
      auto found = std::find_if(contents.begin(), contents.end(),
                                [&entry](const ReverseIndexJSON& rij) -> bool {
                                  return rij.effective_make_and_model ==
                                         entry.effective_make_and_model;
                                });
      if (found != contents.end()) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::Bind(entry.cb, PpdProvider::SUCCESS,
                                  found->manufacturer, found->model));
      } else {
        LOG(ERROR) << "Failed to lookup printer in retrieved data response";
        PostReverseLookupFailure(PpdProvider::NOT_FOUND, entry.cb);
      }
    }
    reverse_index_resolution_queue_.pop_front();
  }

  // Called when |fetcher_| should have just downloaded a usb device map
  // for the vendor at the head of the |ppd_reference_resolution_queue_|.
  void OnUsbFetchComplete() {
    DCHECK(!ppd_reference_resolution_queue_.empty());
    std::string contents;
    std::string buffer;
    PpdProvider::CallbackResultCode result =
        ValidateAndGetResponseAsString(&buffer);
    int desired_device_id =
        ppd_reference_resolution_queue_.front().first.usb_product_id;
    if (result == PpdProvider::SUCCESS) {
      // Parse the JSON response.  This should be a list of the form
      // [
      //  [0x3141, "some canonical name"],
      //  [0x5926, "some othercanonical name"]
      // ]
      // So we scan through the response looking for our desired device id.
      auto top_list = base::ListValue::From(base::JSONReader::Read(buffer));

      if (top_list.get() == nullptr) {
        // We got something malformed back.
        LOG(ERROR) << "Malformed top list";
        result = PpdProvider::INTERNAL_ERROR;
      } else {
        // We'll set result to SUCCESS if we do find the device.
        result = PpdProvider::NOT_FOUND;
        for (const auto& entry : *top_list) {
          int device_id;
          const base::ListValue* sub_list;

          // Each entry should be a size-2 list with an integer and a string.
          if (!entry.GetAsList(&sub_list) || sub_list->GetSize() != 2 ||
              !sub_list->GetInteger(0, &device_id) ||
              !sub_list->GetString(1, &contents) || device_id < 0 ||
              device_id > 0xffff) {
            // Malformed data.
            LOG(ERROR) << "Malformed line in usb device list";
            result = PpdProvider::INTERNAL_ERROR;
            break;
          }
          if (device_id == desired_device_id) {
            // Found it.
            result = PpdProvider::SUCCESS;
            break;
          }
        }
      }
    }
    Printer::PpdReference ret;
    if (result == PpdProvider::SUCCESS) {
      ret.effective_make_and_model = contents;
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(ppd_reference_resolution_queue_.front().second,
                              result, ret));
    ppd_reference_resolution_queue_.pop_front();
  }

  // Something went wrong during metadata fetches.  Fail all queued metadata
  // resolutions with the given error code.
  void FailQueuedMetadataResolutions(PpdProvider::CallbackResultCode code) {
    for (const auto& cb : manufacturers_resolution_queue_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, code, std::vector<std::string>()));
    }
    manufacturers_resolution_queue_.clear();
  }

  // Fail all server-based ppd and ppd reference resolutions inflight, because
  // we failed to grab the necessary index data from the server.  Note we leave
  // any user-based ppd resolutions intact, as they don't depend on the data
  // we're missing.
  void FailQueuedServerPpdResolutions(PpdProvider::CallbackResultCode code) {
    base::circular_deque<std::pair<Printer::PpdReference, ResolvePpdCallback>>
        filtered_queue;
    for (auto& entry : ppd_resolution_queue_) {
      if (!entry.first.user_supplied_ppd_url.empty()) {
        filtered_queue.emplace_back(std::move(entry));
      } else {
        FinishPpdResolution(std::move(entry.second), code, std::string());
      }
    }
    ppd_resolution_queue_ = std::move(filtered_queue);

    // Everything in the ppdreference queue also depends on server information,
    // so should also be failed.
    auto task_runner = base::SequencedTaskRunnerHandle::Get();
    for (const auto& entry : ppd_reference_resolution_queue_) {
      task_runner->PostTask(
          FROM_HERE, base::Bind(entry.second, code, Printer::PpdReference()));
    }
    ppd_reference_resolution_queue_.clear();
  }

  // Given a list of possible locale strings (e.g. 'en-GB'), determine which of
  // them we should use to best serve results for the browser locale (which was
  // given to us at construction time).
  std::string GetBestLocale(const std::vector<std::string>& available_locales) {
    // First look for an exact match.  If we find one, just use that.
    for (const std::string& available : available_locales) {
      if (available == browser_locale_) {
        return available;
      }
    }

    // Next, look for an available locale that is a parent of browser_locale_.
    // Return the most specific one.  For example, if we want 'en-GB-foo' and we
    // don't have an exact match, but we do have 'en-GB' and 'en', we will
    // return 'en-GB' -- the most specific match which is a parent of the
    // requested locale.
    size_t best_len = 0;
    size_t best_idx = -1;
    for (size_t i = 0; i < available_locales.size(); ++i) {
      const std::string& available = available_locales[i];
      if (base::StringPiece(browser_locale_).starts_with(available + "-") &&
          available.size() > best_len) {
        best_len = available.size();
        best_idx = i;
      }
    }
    if (best_idx != static_cast<size_t>(-1)) {
      return available_locales[best_idx];
    }

    // Last chance for a match, look for the locale that matches the *most*
    // pieces of locale_, with ties broken by being least specific.  So for
    // example, if we have 'es-GB', 'es-GB-foo' but no 'es' available, and we're
    // requesting something for 'es', we'll get back 'es-GB' -- the least
    // specific thing that matches some of the locale.
    std::vector<base::StringPiece> browser_locale_pieces =
        base::SplitStringPiece(browser_locale_, "-", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_ALL);
    size_t best_match_size = 0;
    size_t best_match_specificity;
    best_idx = -1;
    for (size_t i = 0; i < available_locales.size(); ++i) {
      const std::string& available = available_locales[i];
      std::vector<base::StringPiece> available_pieces = base::SplitStringPiece(
          available, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      size_t match_size = 0;
      for (; match_size < available_pieces.size() &&
             match_size < browser_locale_pieces.size();
           ++match_size) {
        if (available_pieces[match_size] != browser_locale_pieces[match_size]) {
          break;
        }
      }
      if (match_size > 0 &&
          (best_idx == static_cast<size_t>(-1) ||
           match_size > best_match_size ||
           (match_size == best_match_size &&
            available_pieces.size() < best_match_specificity))) {
        best_idx = i;
        best_match_size = match_size;
        best_match_specificity = available_pieces.size();
      }
    }
    if (best_idx != static_cast<size_t>(-1)) {
      return available_locales[best_idx];
    }

    // Everything else failed.  Throw up our hands and default to english.
    return "en";
  }

  // Get the results of a fetch.  This is a little tricky, because a fetch
  // may have been done by |fetcher_|, or it may have been a file access, in
  // which case we want to look at |file_fetch_contents_|.  We distinguish
  // between the cases based on whether or not |fetcher_| is null.
  //
  // We return NOT_FOUND for 404 or file not found, SERVER_ERROR for other
  // errors, SUCCESS if everything was good.
  CallbackResultCode ValidateAndGetResponseAsString(std::string* contents) {
    CallbackResultCode ret;
    if (fetcher_.get() != nullptr) {
      if (fetcher_->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
        ret = PpdProvider::SERVER_ERROR;
      } else if (fetcher_->GetResponseCode() != net::HTTP_OK) {
        if (fetcher_->GetResponseCode() == net::HTTP_NOT_FOUND) {
          // A 404 means not found, everything else is a server error.
          ret = PpdProvider::NOT_FOUND;
        } else {
          ret = PpdProvider::SERVER_ERROR;
        }
      } else {
        fetcher_->GetResponseAsString(contents);
        ret = PpdProvider::SUCCESS;
      }
      fetcher_.reset();
    } else {
      // It's a file load.
      if (file_fetch_success_) {
        *contents = file_fetch_contents_;
      } else {
        contents->clear();
      }
      // A failure to load a file is always considered a NOT FOUND error (even
      // if the underlying causes is lack of access or similar, this seems to be
      // the best match for intent.
      ret = file_fetch_success_ ? PpdProvider::SUCCESS : PpdProvider::NOT_FOUND;
      file_fetch_contents_.clear();
    }
    return ret;
  }

  // Ensures that the fetched JSON is in the expected format, that is leading
  // with exactly |num_strings| and followed by an optional dictionary. Returns
  // PpdProvider::SUCCESS and saves JSON list in |top_list| if format is valid,
  // returns PpdProvider::INTERNAL_ERROR otherwise.
  PpdProvider::CallbackResultCode ParseAndValidateJSONFormat(
      base::Value::ListStorage* top_list,
      size_t num_strings) {
    std::string buffer;

    auto fetch_result = ValidateAndGetResponseAsString(&buffer);
    if (fetch_result != PpdProvider::SUCCESS) {
      return fetch_result;
    }

    auto ret_list = base::ListValue::From(base::JSONReader::Read(buffer));
    if (ret_list == nullptr) {
      return PpdProvider::INTERNAL_ERROR;
    }
    *top_list = std::move(ret_list->GetList());

    for (const auto& entry : *top_list) {
      if (!entry.is_list()) {
        return PpdProvider::INTERNAL_ERROR;
      }

      // entry must start with |num_strings| strings
      const base::Value::ListStorage& list = entry.GetList();
      if (list.size() < num_strings) {
        return PpdProvider::INTERNAL_ERROR;
      }
      for (size_t i = 0; i < num_strings; ++i) {
        if (!list[i].is_string()) {
          return PpdProvider::INTERNAL_ERROR;
        }
      }

      // entry may optionally have a last arg that must be a dict
      if (list.size() > num_strings && !list[num_strings].is_dict()) {
        return PpdProvider::INTERNAL_ERROR;
      }

      // entry may not have more than |num_strings| strings and one dict
      if (list.size() > num_strings + 1) {
        return PpdProvider::INTERNAL_ERROR;
      }
    }

    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a ReverseIndexJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParseReverseIndexJSON(
      std::vector<ReverseIndexJSON>* contents) {
    DCHECK(contents != nullptr);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 3);
    if (ret != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed to parse ReverseIndex metadata";
      return ret;
    }

    // Fetched data should be in the form {[effective_make_and_model],
    // [manufacturer], [model], [dictionary of metadata]}
    for (const auto& entry : top_list) {
      const base::Value::ListStorage& list = entry.GetList();

      ReverseIndexJSON rij_entry;
      rij_entry.effective_make_and_model = list[0].GetString();
      rij_entry.manufacturer = list[1].GetString();
      rij_entry.model = list[2].GetString();

      // Populate restrictions, if available
      if (list.size() > 3) {
        rij_entry.restrictions = ComputeRestrictions(list[3]);
      }

      contents->push_back(rij_entry);
    }
    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a ManufacturersJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParseManufacturersJSON(
      std::vector<ManufacturersJSON>* contents) {
    DCHECK(contents != NULL);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed to process Manufacturers metadata";
      return ret;
    }

    // Fetched data should be in form [[name], [canonical name],
    // {restrictions}]
    for (const auto& entry : top_list) {
      const base::Value::ListStorage& list = entry.GetList();
      ManufacturersJSON mj_entry;
      mj_entry.name = list[0].GetString();
      mj_entry.reference = list[1].GetString();

      // Populate restrictions, if available
      if (list.size() > 2) {
        mj_entry.restrictions = ComputeRestrictions(list[2]);
      }

      contents->push_back(mj_entry);
    }

    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a PrintersJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParsePrintersJSON(
      std::vector<PrintersJSON>* contents) {
    DCHECK(contents != NULL);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed to parse Printers metadata";
      return ret;
    }

    // Fetched data should be in form [[name], [canonical name],
    // {restrictions}]
    for (const auto& entry : top_list) {
      const base::Value::ListStorage& list = entry.GetList();
      PrintersJSON pj_entry;
      pj_entry.name = list[0].GetString();
      pj_entry.effective_make_and_model = list[1].GetString();

      // Populate restrictions, if available
      if (list.size() > 2) {
        pj_entry.restrictions = ComputeRestrictions(list[2]);
      }

      contents->push_back(pj_entry);
    }

    return PpdProvider::SUCCESS;
  }

  // Attempts to parse a PpdIndexJSON reply to |fetcher| into the passed
  // contents. Returns PpdProvider::SUCCESS on valid JSON formatting and filled
  // |contents|, clears |contents| otherwise.
  PpdProvider::CallbackResultCode ValidateAndParsePpdIndexJSON(
      std::vector<PpdIndexJSON>* contents) {
    DCHECK(contents != nullptr);
    contents->clear();

    base::Value::ListStorage top_list;
    auto ret = ParseAndValidateJSONFormat(&top_list, 2);
    if (ret != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed to parse PpdIndex metadata";
      return ret;
    }

    // Fetched data should be in the form {[effective_make_and_model],
    // [manufacturer], [model], [dictionary of metadata]}
    for (const auto& entry : top_list) {
      const base::Value::ListStorage& list = entry.GetList();

      PpdIndexJSON pij_entry;
      pij_entry.effective_make_and_model = list[0].GetString();
      pij_entry.ppd_filename = list[1].GetString();

      contents->push_back(pij_entry);
    }
    return PpdProvider::SUCCESS;
  }

  // Create the list of manufacturers from |cached_metadata_|.  Requires that
  // the manufacturer list has already been resolved.
  std::vector<std::string> GetManufacturerList() const {
    CHECK(cached_metadata_.get() != nullptr);
    std::vector<std::string> ret;
    ret.reserve(cached_metadata_->size());
    for (const auto& entry : *cached_metadata_) {
      ret.push_back(entry.first);
    }
    // TODO(justincarlson) -- this should be a localization-aware sort.
    sort(ret.begin(), ret.end());
    return ret;
  }

  // Get the list of printers from a given manufacturer from |cached_metadata_|.
  // Requires that we have already resolved this from the server.
  ResolvedPrintersList GetManufacturerPrinterList(
      const ManufacturerMetadata& meta) const {
    CHECK(meta.printers.get() != nullptr);
    std::vector<PrintersJSON> printers;
    printers.reserve(meta.printers->size());
    for (const auto& entry : *meta.printers) {
      printers.push_back(entry.second);
    }
    // TODO(justincarlson) -- this should be a localization-aware sort.
    sort(printers.begin(), printers.end(),
         [](const PrintersJSON& a, const PrintersJSON& b) -> bool {
           return a.name < b.name;
         });
    FilterRestrictedPpdReferences(version_, &printers);

    ResolvedPrintersList ret;
    ret.reserve(printers.size());
    for (const auto& printer : printers) {
      Printer::PpdReference ppd_ref;
      ppd_ref.effective_make_and_model = printer.effective_make_and_model;
      ret.push_back({printer.name, ppd_ref});
    }
    return ret;
  }

  void PostReverseLookupFailure(CallbackResultCode result,
                                const ReverseLookupCallback& cb) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cb, result, std::string(), std::string()));
  }

  // The hash function to calculate the hash of canonical identifiers to the
  // name of the ppd file for that printer.
  int IndexShard(std::string effective_make_and_model) {
    unsigned int hash = 5381;
    int kNumIndexShards = 20;

    for (char c : effective_make_and_model) {
      hash = hash * 33 + c;
    }
    return hash % kNumIndexShards;
  }

  // Map from (localized) manufacturer name to metadata for that manufacturer.
  // This is populated lazily.  If we don't yet have a manufacturer list, the
  // top pointer will be null.  When we create the top level map, then each
  // value will only contain a reference which can be used to resolve the
  // printer list from that manufacturer.  On demand, we use these references to
  // resolve the actual printer lists.
  std::unique_ptr<std::unordered_map<std::string, ManufacturerMetadata>>
      cached_metadata_;

  // Cached contents of the server indexs, which maps first a shard number to
  // the corresponding index map of PpdReference::effective_make_and_models to a
  // urls for the corresponding ppds. Starts as an empty map and filled lazily
  // as we need to fill in more indexs.
  std::unordered_map<int, std::unordered_map<std::string, std::string>>
      cached_ppd_idxs_;

  // Queued ResolveManufacturers() calls.  We will simultaneously resolve
  // all queued requests, so no need for a deque here.
  std::vector<ResolveManufacturersCallback> manufacturers_resolution_queue_;

  // Queued ResolvePrinters() calls.
  base::circular_deque<PrinterResolutionQueueEntry> printers_resolution_queue_;

  // Queued ResolvePpd() requests.

  base::circular_deque<std::pair<Printer::PpdReference, ResolvePpdCallback>>
      ppd_resolution_queue_;

  // Queued ResolvePpdReference() requests.
  base::circular_deque<
      std::pair<PrinterSearchData, ResolvePpdReferenceCallback>>
      ppd_reference_resolution_queue_;

  // Queued ReverseIndex() calls.
  base::circular_deque<ReverseIndexQueueEntry> reverse_index_resolution_queue_;

  // Locale we're using for grabbing stuff from the server.  Empty if we haven't
  // determined it yet.
  std::string locale_;

  // If the fetcher is active, what's it fetching?
  FetcherTarget fetcher_target_;

  // Fetcher used for all network fetches.  This is explicitly reset() when
  // a fetch has been processed.
  std::unique_ptr<net::URLFetcher> fetcher_;
  bool fetch_inflight_ = false;

  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;

  // For file:// fetches, a staging buffer and result flag for loading the file.
  std::string file_fetch_contents_;
  bool file_fetch_success_;

  // Cache of ppd files.
  scoped_refptr<PpdCache> ppd_cache_;

  // Where to run disk operations.
  scoped_refptr<base::SequencedTaskRunner> disk_task_runner_;

  // Current version used to filter restricted ppds
  base::Version version_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_;

 protected:
  ~PpdProviderImpl() override = default;
};

}  // namespace

PpdProvider::PrinterSearchData::PrinterSearchData() = default;
PpdProvider::PrinterSearchData::PrinterSearchData(
    const PrinterSearchData& other) = default;
PpdProvider::PrinterSearchData::~PrinterSearchData() = default;

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const std::string& browser_locale,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter,
    scoped_refptr<PpdCache> ppd_cache,
    const base::Version& current_version,
    const PpdProvider::Options& options) {
  return scoped_refptr<PpdProvider>(new PpdProviderImpl(
      browser_locale, url_context_getter, ppd_cache, current_version, options));
}
}  // namespace chromeos
