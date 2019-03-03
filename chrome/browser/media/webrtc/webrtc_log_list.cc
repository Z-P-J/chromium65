// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_list.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task_scheduler/post_task.h"
#include "components/upload_list/text_log_upload_list.h"
#include "content/public/browser/browser_context.h"

namespace {

const char kWebRtcLogDirectory[] = "WebRTC Logs";
const char kWebRtcLogListFilename[] = "Log List";

}  // namespace

// static
UploadList* WebRtcLogList::CreateWebRtcLogList(
    content::BrowserContext* browser_context) {
  base::FilePath log_list_path = GetWebRtcLogListFileForDirectory(
      GetWebRtcLogDirectoryForBrowserContextPath(browser_context->GetPath()));
  return new TextLogUploadList(log_list_path);
}

// static
base::FilePath WebRtcLogList::GetWebRtcLogDirectoryForBrowserContextPath(
    const base::FilePath& browser_context_path) {
  DCHECK(!browser_context_path.empty());
  return browser_context_path.AppendASCII(kWebRtcLogDirectory);
}

// static
base::FilePath WebRtcLogList::GetWebRtcLogListFileForDirectory(
    const base::FilePath& dir) {
  DCHECK(!dir.empty());
  return dir.AppendASCII(kWebRtcLogListFilename);
}
