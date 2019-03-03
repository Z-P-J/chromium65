// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/partner_browser_customizations.h"

#include "base/android/jni_android.h"
#include "jni/PartnerBrowserCustomizations_jni.h"

namespace chrome {
namespace android {

bool PartnerBrowserCustomizations::IsIncognitoDisabled() {
  return Java_PartnerBrowserCustomizations_isIncognitoDisabled(
      base::android::AttachCurrentThread());
}

}  // namespace android
}  // namespace chrome
