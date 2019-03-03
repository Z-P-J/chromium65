// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_TARGET_H_
#define HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_TARGET_H_

#include "base/macros.h"
#include "headless/public/headless_export.h"

namespace headless {
class HeadlessDevToolsClient;

// A target which can be controlled and inspected using DevTools.
class HEADLESS_EXPORT HeadlessDevToolsTarget {
 public:
  HeadlessDevToolsTarget() {}
  virtual ~HeadlessDevToolsTarget() {}

  // Attach or detach a client to this target. A client must be attached in
  // order to send commands or receive notifications from the target.
  //
  // A single client may be attached to at most one target at a time. If
  // the target already has a client attached, AttachClient will return false.
  // ForceAttachClient will detach any existing connection before attaching
  // |client|.
  // Note that currently also only one client may be attached to a single target
  // at a time.
  //
  // |client| must outlive this target.
  virtual bool AttachClient(HeadlessDevToolsClient* client) = 0;
  virtual void ForceAttachClient(HeadlessDevToolsClient* client) = 0;
  virtual void DetachClient(HeadlessDevToolsClient* client) = 0;

  // Returns true if a devtools client is attached.
  virtual bool IsAttached() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessDevToolsTarget);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_TARGET_H_
