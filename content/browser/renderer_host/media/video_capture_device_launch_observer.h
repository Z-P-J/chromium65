// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_LAUNCH_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_LAUNCH_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

class VideoCaptureController;

class CONTENT_EXPORT VideoCaptureDeviceLaunchObserver {
 public:
  virtual ~VideoCaptureDeviceLaunchObserver() {}
  virtual void OnDeviceLaunched(VideoCaptureController* controller) = 0;
  virtual void OnDeviceLaunchFailed(VideoCaptureController* controller) = 0;
  virtual void OnDeviceLaunchAborted() = 0;
  virtual void OnDeviceConnectionLost(VideoCaptureController* controller) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEVICE_LAUNCH_OBSERVER_H_
