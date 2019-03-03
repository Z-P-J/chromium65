// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_blocklist.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

class UsbBlocklistTest : public testing::Test {
 public:
  UsbBlocklistTest() : blocklist_(UsbBlocklist::Get()) {}

  const UsbBlocklist& list() { return blocklist_; }

  void SetDynamicBlocklist(base::StringPiece list) {
    params_manager_.ClearAllVariationParams();

    std::map<std::string, std::string> params;
    params["blocklist_additions"] = list.as_string();
    params_manager_.SetVariationParams("WebUSBBlocklist", params);

    blocklist_.ResetToDefaultValuesForTest();
  }

 private:
  void TearDown() override {
    // Because UsbBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    params_manager_.ClearAllVariationParams();
    blocklist_.ResetToDefaultValuesForTest();
  }

  variations::testing::VariationParamsManager params_manager_;
  UsbBlocklist& blocklist_;
};

TEST_F(UsbBlocklistTest, BasicExclusions) {
  SetDynamicBlocklist("18D1:58F0:0100");
  EXPECT_TRUE(list().IsExcluded({0x18D1, 0x58F0, 0x0100}));
  // An older device version is also blocked.
  EXPECT_TRUE(list().IsExcluded({0x18D1, 0x58F0, 0x0090}));
  // A newer device version is not blocked.
  EXPECT_FALSE(list().IsExcluded({0x18D1, 0x58F0, 0x0200}));
  // Other devices with nearby vendor and product IDs are not blocked.
  EXPECT_FALSE(list().IsExcluded({0x18D1, 0x58F1, 0x0100}));
  EXPECT_FALSE(list().IsExcluded({0x18D1, 0x58EF, 0x0100}));
  EXPECT_FALSE(list().IsExcluded({0x18D0, 0x58F0, 0x0100}));
  EXPECT_FALSE(list().IsExcluded({0x18D2, 0x58F0, 0x0100}));
}

TEST_F(UsbBlocklistTest, StringsWithNoValidEntries) {
  SetDynamicBlocklist("");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("~!@#$%^&*()-_=+[]{}/*-");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(":");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("::");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(",");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(",,");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(",::,");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("1:2:3");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("18D1:2:3000");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("0000:0x00:0000");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("0000:   0:0000");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("000g:0000:0000");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("☯");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());
}

TEST_F(UsbBlocklistTest, StringsWithOneValidEntry) {
  SetDynamicBlocklist("18D1:58F0:0101");
  EXPECT_EQ(1u, list().GetDynamicEntryCountForTest());
  EXPECT_TRUE(list().IsExcluded({0x18D1, 0x58F0, 0x0101}));

  SetDynamicBlocklist(" 18D1:58F0:0200  ");
  EXPECT_EQ(1u, list().GetDynamicEntryCountForTest());
  EXPECT_TRUE(list().IsExcluded({0x18D1, 0x58F0, 0x0200}));

  SetDynamicBlocklist(", 18D1:58F0:0201,  ");
  EXPECT_EQ(1u, list().GetDynamicEntryCountForTest());
  EXPECT_TRUE(list().IsExcluded({0x18D1, 0x58F0, 0x0201}));

  SetDynamicBlocklist("18D1:58F0:0202, 0000:1:0000");
  EXPECT_EQ(1u, list().GetDynamicEntryCountForTest());
  EXPECT_TRUE(list().IsExcluded({0x18D1, 0x58F0, 0x0202}));
}

TEST_F(UsbBlocklistTest, StaticEntries) {
  // The specific versions of these devices that we want to block are unknown.
  // The device versions listed here are abitrary chosen to test that any device
  // will be matched.

  // Yubikey NEO - OTP and CCID
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0111, 0x0100}));
  // Yubikey NEO - CCID only
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0112, 0x0100}));
  // Yubikey NEO - U2F and CCID
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0115, 0x0100}));
  // Yubikey NEO - OTP, U2F and CCID
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0116, 0x0100}));
  // Google Gnubby (WinUSB firmware)
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0211, 0x0100}));
  // Yubikey 4 - CCID only
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0404, 0x0100}));
  // Yubikey 4 - OTP and CCID
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0405, 0x0100}));
  // Yubikey 4 - U2F and CCID
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0406, 0x0100}));
  // Yubikey 4 - OTP, U2F and CCID
  EXPECT_TRUE(list().IsExcluded({0x1050, 0x0407, 0x0100}));

  // The non-WinUSB version of the Google Gnubby firmware is not in the static
  // list. Check that it is not matched despite a similar product ID.
  EXPECT_FALSE(list().IsExcluded({0x1050, 0x0200, 0x0100}));
}
