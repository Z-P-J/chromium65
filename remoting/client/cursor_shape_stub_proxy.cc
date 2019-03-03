// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <remoting/client/cursor_shape_stub_proxy.h>
#include "base/bind.h"
#include "base/location.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

CursorShapeStubProxy::CursorShapeStubProxy(
    base::WeakPtr<protocol::CursorShapeStub> stub,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : stub_(stub), task_runner_(task_runner) {}

CursorShapeStubProxy::~CursorShapeStubProxy() = default;

void CursorShapeStubProxy::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&protocol::CursorShapeStub::SetCursorShape, stub_,
                            cursor_shape));
}

}  // namespace remoting
