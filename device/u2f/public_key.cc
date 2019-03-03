// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/public_key.h"

#include <utility>

#include "base/macros.h"

namespace device {

PublicKey::~PublicKey() = default;

PublicKey::PublicKey(std::string algorithm)
    : algorithm_(std::move(algorithm)) {}

}  // namespace device
