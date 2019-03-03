// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/file_errors.h"

class Profile;

namespace storage {

class FileSystemURL;

}  // namespace storage

namespace chromeos {

class RecentFile;

// RecentSource implementation for Drive files.
//
// All member functions must be called on the UI thread.
//
// TODO(nya): Write unit tests.
class RecentDriveSource : public RecentSource {
 public:
  explicit RecentDriveSource(Profile* profile);
  ~RecentDriveSource() override;

  // RecentSource overrides:
  void GetRecentFiles(Params params) override;

 private:
  static const char kLoadHistogramName[];

  void OnSearchMetadata(
      drive::FileError error,
      std::unique_ptr<drive::MetadataSearchResultVector> results);
  void OnGetMetadata(const storage::FileSystemURL& url,
                     base::File::Error result,
                     const base::File::Info& info);
  void OnComplete();

  Profile* const profile_;

  // Set at the beginning of GetRecentFiles().
  base::Optional<Params> params_;

  base::TimeTicks build_start_time_;

  int num_inflight_stats_ = 0;
  std::vector<RecentFile> files_;

  base::WeakPtrFactory<RecentDriveSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentDriveSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_
