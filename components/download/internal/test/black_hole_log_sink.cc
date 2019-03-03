// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/test/black_hole_log_sink.h"

namespace download {
namespace test {

void BlackHoleLogSink::OnServiceStatusChanged() {}

void BlackHoleLogSink::OnServiceDownloadsAvailable() {}

void BlackHoleLogSink::OnServiceDownloadChanged(const std::string& guid) {}

void BlackHoleLogSink::OnServiceDownloadFailed(CompletionType completion_type,
                                               const Entry& entry) {}

void BlackHoleLogSink::OnServiceRequestMade(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult start_result) {}

}  // namespace test
}  // namespace download
