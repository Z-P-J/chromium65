// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

using extensions::Extension;

// Test ensures that Wasm can run in Chrome Apps.
namespace {

class WasmAppTest : public ExtensionBrowserTest {};

IN_PROC_BROWSER_TEST_F(WasmAppTest, InstantiateWasmFromFetch) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("wasm_app"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("success", ExecuteScriptInBackgroundPage(extension->id(),
                                                     "instantiateFetch()"));
}

IN_PROC_BROWSER_TEST_F(WasmAppTest, InstantiateWasmFromArrayBuffer) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("wasm_app"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("success", ExecuteScriptInBackgroundPage(
                           extension->id(), "instantiateArrayBuffer()"));
}

}  // namespace