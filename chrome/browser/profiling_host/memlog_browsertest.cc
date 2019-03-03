// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/features.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/profiling_host/profiling_test_driver.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/zlib.h"

// Some builds don't support the allocator shim in which case the memory long
// won't function.
#if BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace profiling {

struct TestParam {
  ProfilingProcessHost::Mode mode;
  mojom::StackMode stack_mode;
  bool start_profiling_with_command_line_flag;
};

class MemlogBrowserTest : public InProcessBrowserTest,
                          public testing::WithParamInterface<TestParam> {
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    if (GetParam().start_profiling_with_command_line_flag) {
      if (GetParam().mode == ProfilingProcessHost::Mode::kAllRenderers) {
        command_line->AppendSwitchASCII(switches::kMemlog,
                                        switches::kMemlogModeAllRenderers);
      } else if (GetParam().mode == ProfilingProcessHost::Mode::kAll) {
        command_line->AppendSwitchASCII(switches::kMemlog,
                                        switches::kMemlogModeAll);
      } else {
        NOTREACHED();
      }

      if (GetParam().stack_mode == mojom::StackMode::PSEUDO) {
        command_line->AppendSwitchASCII(switches::kMemlogStackMode,
                                        switches::kMemlogStackModePseudo);
      } else if (GetParam().stack_mode == mojom::StackMode::NATIVE) {
        command_line->AppendSwitchASCII(switches::kMemlogStackMode,
                                        switches::kMemlogStackModeNative);
      } else if (GetParam().stack_mode == mojom::StackMode::MIXED) {
        command_line->AppendSwitchASCII(switches::kMemlogStackMode,
                                        switches::kMemlogStackModeMixed);
      } else {
        NOTREACHED();
      }
    }
  }
};

// Ensure invocations via TracingController can generate a valid JSON file with
// expected data.
IN_PROC_BROWSER_TEST_P(MemlogBrowserTest, EndToEnd) {
  LOG(INFO) << "Memlog mode: " << static_cast<int>(GetParam().mode);
  LOG(INFO) << "Memlog stack mode: " << static_cast<int>(GetParam().stack_mode);
  LOG(INFO) << "Started via command line flag: "
            << static_cast<int>(
                   GetParam().start_profiling_with_command_line_flag);
  ProfilingTestDriver driver;
  ProfilingTestDriver::Options options;
  options.mode = GetParam().mode;
  options.stack_mode = GetParam().stack_mode;
  options.profiling_already_started =
      GetParam().start_profiling_with_command_line_flag;

  EXPECT_TRUE(driver.RunTest(options));
}
// TODO(ajwong): Test what happens if profiling process crashes.
// http://crbug.com/780955

std::vector<TestParam> GetParams() {
  std::vector<TestParam> params;
  std::vector<ProfilingProcessHost::Mode> dynamic_start_modes;
  dynamic_start_modes.push_back(ProfilingProcessHost::Mode::kNone);
  dynamic_start_modes.push_back(ProfilingProcessHost::Mode::kMinimal);
  dynamic_start_modes.push_back(ProfilingProcessHost::Mode::kBrowser);
  dynamic_start_modes.push_back(ProfilingProcessHost::Mode::kGpu);

  std::vector<mojom::StackMode> stack_modes;
  stack_modes.push_back(mojom::StackMode::MIXED);
  stack_modes.push_back(mojom::StackMode::NATIVE);
  stack_modes.push_back(mojom::StackMode::PSEUDO);

  for (const auto& mode : dynamic_start_modes) {
    for (const auto& stack_mode : stack_modes) {
      params.push_back({mode, stack_mode, false});
    }
  }

  // There's no way to tell when profiling has started for a renderer, so it
  // must be started as soon as chrome has started.
  std::vector<ProfilingProcessHost::Mode> command_line_start_modes;
  command_line_start_modes.push_back(ProfilingProcessHost::Mode::kAll);
  command_line_start_modes.push_back(ProfilingProcessHost::Mode::kAllRenderers);
  for (const auto& mode : command_line_start_modes) {
    for (const auto& stack_mode : stack_modes) {
      params.push_back({mode, stack_mode, true});
    }
  }
  return params;
}

INSTANTIATE_TEST_CASE_P(Memlog,
                        MemlogBrowserTest,
                        ::testing::ValuesIn(GetParams()));

}  // namespace profiling

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
