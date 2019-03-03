// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_api.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/permissions/permissions_data.h"

using extensions::api::events::Rule;

namespace AddRules = extensions::api::events::Event::AddRules;
namespace GetRules = extensions::api::events::Event::GetRules;
namespace RemoveRules = extensions::api::events::Event::RemoveRules;

namespace extensions {

namespace {

constexpr char kDeclarativeEventPrefix[] = "declarative";
constexpr char kDeclarativeContentEventPrefix[] = "declarativeContent.";
constexpr char kDeclarativeWebRequestEventPrefix[] = "declarativeWebRequest.";
constexpr char kDeclarativeWebRequestWebViewEventPrefix[] =
    "webViewInternal.declarativeWebRequest.";

// The type of Declarative API. To collect more granular metrics, a distinction
// is made when the declarative web request API is used from a webview.
enum class DeclarativeAPIType {
  kContent,
  kWebRequest,
  kWebRequestWebview,
  kUnknown,
};

// Describes the possible types of declarative API function calls.
// These values are recorded as UMA. New enum values can be added, but existing
// enum values must never be renumbered or deleted and reused.
enum DeclarativeAPIFunctionType {
  kDeclarativeContentAddRules = 0,
  kDeclarativeContentRemoveRules = 1,
  kDeclarativeContentGetRules = 2,
  kDeclarativeWebRequestAddRules = 3,
  kDeclarativeWebRequestRemoveRules = 4,
  kDeclarativeWebRequestGetRules = 5,
  kDeclarativeWebRequestWebviewAddRules = 6,
  kDeclarativeWebRequestWebviewRemoveRules = 7,
  kDeclarativeWebRequestWebviewGetRules = 8,
  kDeclarativeApiFunctionCallTypeMax,
};

DeclarativeAPIType GetDeclarativeAPIType(const std::string& event_name) {
  if (base::StartsWith(event_name, kDeclarativeContentEventPrefix,
                       base::CompareCase::SENSITIVE))
    return DeclarativeAPIType::kContent;
  if (base::StartsWith(event_name, kDeclarativeWebRequestEventPrefix,
                       base::CompareCase::SENSITIVE))
    return DeclarativeAPIType::kWebRequest;
  if (base::StartsWith(event_name, kDeclarativeWebRequestWebViewEventPrefix,
                       base::CompareCase::SENSITIVE))
    return DeclarativeAPIType::kWebRequestWebview;
  return DeclarativeAPIType::kUnknown;
}

void RecordUMAHelper(DeclarativeAPIFunctionType type) {
  DCHECK_LT(type, kDeclarativeApiFunctionCallTypeMax);
  UMA_HISTOGRAM_ENUMERATION("Extensions.DeclarativeAPIFunctionCalls", type,
                            kDeclarativeApiFunctionCallTypeMax);
}

void ConvertBinaryDictionaryValuesToBase64(base::Value* dict);

// Encodes |binary| as base64 and returns a new StringValue populated with the
// encoded string.
base::Value ConvertBinaryToBase64(const base::Value& binary) {
  std::string binary_data(binary.GetBlob().data(), binary.GetBlob().size());
  std::string data64;
  base::Base64Encode(binary_data, &data64);
  return base::Value(std::move(data64));
}

// Parses through |args| replacing any BinaryValues with base64 encoded
// StringValues. Recurses over any nested ListValues, and calls
// ConvertBinaryDictionaryValuesToBase64 for any nested DictionaryValues.
void ConvertBinaryListElementsToBase64(base::Value* args) {
  for (auto& value : args->GetList()) {
    if (value.is_blob()) {
      value = ConvertBinaryToBase64(value);
    } else if (value.is_list()) {
      ConvertBinaryListElementsToBase64(&value);
    } else if (value.is_dict()) {
      ConvertBinaryDictionaryValuesToBase64(&value);
    }
  }
}

// Parses through |dict| replacing any BinaryValues with base64 encoded
// StringValues. Recurses over any nested DictionaryValues, and calls
// ConvertBinaryListElementsToBase64 for any nested ListValues.
void ConvertBinaryDictionaryValuesToBase64(base::Value* dict) {
  for (auto it : dict->DictItems()) {
    auto& value = it.second;
    if (value.is_blob()) {
      value = ConvertBinaryToBase64(value);
    } else if (value.is_list()) {
      ConvertBinaryListElementsToBase64(&value);
    } else if (value.is_dict()) {
      ConvertBinaryDictionaryValuesToBase64(&value);
    }
  }
}

}  // namespace

RulesFunction::RulesFunction() {}

RulesFunction::~RulesFunction() {}

bool RulesFunction::HasPermission() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));
  int web_view_instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &web_view_instance_id));

  // <webview> embedders use the declarativeWebRequest API via
  // <webview>.onRequest.
  if (web_view_instance_id && extension_->permissions_data()->HasAPIPermission(
                                  APIPermission::kWebView)) {
    return true;
  }
  return ExtensionFunction::HasPermission();
}

bool RulesFunction::RunAsync() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  int web_view_instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &web_view_instance_id));
  int embedder_process_id = render_frame_host()->GetProcess()->GetID();

  RecordUMA(event_name);

  bool from_web_view = web_view_instance_id != 0;
  // If we are not operating on a particular <webview>, then the key is 0.
  int rules_registry_id = RulesRegistryService::kDefaultRulesRegistryID;
  if (from_web_view) {
    // Sample event names:
    // webViewInternal.declarativeWebRequest.onRequest.
    // webViewInternal.declarativeWebRequest.onMessage.
    // The "webViewInternal." prefix is removed from the event name.
    std::size_t found = event_name.find(kDeclarativeEventPrefix);
    EXTENSION_FUNCTION_VALIDATE(found != std::string::npos);
    event_name = event_name.substr(found);

    rules_registry_id = WebViewGuest::GetOrGenerateRulesRegistryID(
        embedder_process_id, web_view_instance_id);
  }

  // The following call will return a NULL pointer for apps_shell, but should
  // never be called there anyways.
  rules_registry_ = RulesRegistryService::Get(browser_context())->
      GetRulesRegistry(rules_registry_id, event_name);
  DCHECK(rules_registry_.get());
  // Raw access to this function is not available to extensions, therefore
  // there should never be a request for a nonexisting rules registry.
  EXTENSION_FUNCTION_VALIDATE(rules_registry_.get());

  if (content::BrowserThread::CurrentlyOn(rules_registry_->owner_thread())) {
    bool success = RunAsyncOnCorrectThread();
    SendResponse(success);
  } else {
    scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner =
        content::BrowserThread::GetTaskRunnerForThread(
            rules_registry_->owner_thread());
    base::PostTaskAndReplyWithResult(
        thread_task_runner.get(), FROM_HERE,
        base::Bind(&RulesFunction::RunAsyncOnCorrectThread, this),
        base::Bind(&RulesFunction::SendResponse, this));
  }

  return true;
}

bool EventsEventAddRulesFunction::RunAsyncOnCorrectThread() {
  ConvertBinaryListElementsToBase64(args_.get());
  std::unique_ptr<AddRules::Params> params(AddRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(devlin): Remove the dependency on linked_ptr here.
  std::vector<linked_ptr<api::events::Rule>> linked_rules;
  for (api::events::Rule& rule : params->rules) {
    linked_rules.push_back(
        make_linked_ptr(new api::events::Rule(std::move(rule))));
  }
  error_ = rules_registry_->AddRules(extension_id(), linked_rules);

  if (error_.empty()) {
    std::unique_ptr<base::ListValue> rules_value(new base::ListValue());
    for (const auto& rule : linked_rules)
      rules_value->Append(rule->ToValue());
    SetResult(std::move(rules_value));
  }

  return error_.empty();
}

void EventsEventAddRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type = kDeclarativeApiFunctionCallTypeMax;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = kDeclarativeContentAddRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = kDeclarativeWebRequestAddRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = kDeclarativeWebRequestWebviewAddRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED();
      return;
  }
  RecordUMAHelper(type);
}

bool EventsEventRemoveRulesFunction::RunAsyncOnCorrectThread() {
  std::unique_ptr<RemoveRules::Params> params(
      RemoveRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->rule_identifiers.get()) {
    error_ = rules_registry_->RemoveRules(extension_id(),
                                          *params->rule_identifiers);
  } else {
    error_ = rules_registry_->RemoveAllRules(extension_id());
  }

  return error_.empty();
}

void EventsEventRemoveRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type = kDeclarativeApiFunctionCallTypeMax;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = kDeclarativeContentRemoveRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = kDeclarativeWebRequestRemoveRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = kDeclarativeWebRequestWebviewRemoveRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED();
      return;
  }
  RecordUMAHelper(type);
}

bool EventsEventGetRulesFunction::RunAsyncOnCorrectThread() {
  std::unique_ptr<GetRules::Params> params(GetRules::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<linked_ptr<Rule> > rules;
  if (params->rule_identifiers.get()) {
    rules_registry_->GetRules(
        extension_id(), *params->rule_identifiers, &rules);
  } else {
    rules_registry_->GetAllRules(extension_id(), &rules);
  }

  std::unique_ptr<base::ListValue> rules_value(new base::ListValue());
  for (const auto& rule : rules)
    rules_value->Append(rule->ToValue());
  SetResult(std::move(rules_value));

  return true;
}

void EventsEventGetRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type = kDeclarativeApiFunctionCallTypeMax;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = kDeclarativeContentGetRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = kDeclarativeWebRequestGetRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = kDeclarativeWebRequestWebviewGetRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED();
      return;
  }
  RecordUMAHelper(type);
}

}  // namespace extensions
