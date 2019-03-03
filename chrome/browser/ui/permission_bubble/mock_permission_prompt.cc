// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/permission_bubble/mock_permission_prompt.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "ui/gfx/vector_icon_types.h"
#endif

MockPermissionPrompt::~MockPermissionPrompt() {
  if (factory_)
    factory_->HideView(this);
}

bool MockPermissionPrompt::CanAcceptRequestUpdate() {
  return can_update_ui_;
}

void MockPermissionPrompt::UpdateAnchorPosition() {}

gfx::NativeWindow MockPermissionPrompt::GetNativeWindow() {
  // This class should only be used when the UI is not necessary.
  NOTREACHED();
  return nullptr;
}

MockPermissionPrompt::MockPermissionPrompt(MockPermissionPromptFactory* factory,
                                           Delegate* delegate,
                                           bool can_update_ui)
    : factory_(factory), delegate_(delegate), can_update_ui_(can_update_ui) {
  for (const PermissionRequest* request : delegate_->Requests()) {
    // The actual prompt will call these, so test they're sane.
    EXPECT_FALSE(request->GetMessageTextFragment().empty());
#if defined(OS_ANDROID)
    EXPECT_FALSE(request->GetMessageText().empty());
    EXPECT_NE(0, request->GetIconId());
#else
    EXPECT_FALSE(request->GetIconId().is_empty());
#endif
  }
}
