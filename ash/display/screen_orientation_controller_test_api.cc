// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller_test_api.h"

#include "ash/display/screen_orientation_controller_chromeos.h"

namespace ash {

ScreenOrientationControllerTestApi::ScreenOrientationControllerTestApi(
    ScreenOrientationController* controller)
    : controller_(controller) {}

void ScreenOrientationControllerTestApi::SetDisplayRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source,
    DisplayConfigurationController::RotationAnimation mode) {
  controller_->SetDisplayRotation(rotation, source, mode);
}

void ScreenOrientationControllerTestApi::SetRotationLocked(bool locked) {
  controller_->SetRotationLockedInternal(locked);
}

blink::WebScreenOrientationLockType
ScreenOrientationControllerTestApi::UserLockedOrientation() const {
  return controller_->user_locked_orientation_;
}

blink::WebScreenOrientationLockType
ScreenOrientationControllerTestApi::GetCurrentOrientation() const {
  return controller_->GetCurrentOrientation();
}

void ScreenOrientationControllerTestApi::UpdateNaturalOrientation() {
  controller_->UpdateNaturalOrientationForTest();
}

}  // namespace ash
