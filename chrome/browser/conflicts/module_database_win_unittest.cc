// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_database_win.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr content::ProcessType kProcessType1 = content::PROCESS_TYPE_BROWSER;
constexpr content::ProcessType kProcessType2 = content::PROCESS_TYPE_RENDERER;

constexpr wchar_t kDll1[] = L"dummy.dll";
constexpr wchar_t kDll2[] = L"foo.dll";

constexpr size_t kSize1 = 100 * 4096;
constexpr size_t kSize2 = 20 * 4096;

constexpr uint32_t kTime1 = 0xDEADBEEF;
constexpr uint32_t kTime2 = 0xBAADF00D;

constexpr uintptr_t kGoodAddress1 = 0x04000000u;
constexpr uintptr_t kGoodAddress2 = 0x05000000u;

}  // namespace

class ModuleDatabaseTest : public testing::Test {
 protected:
  ModuleDatabaseTest()
      : dll1_(kDll1),
        dll2_(kDll2),
        module_database_(base::SequencedTaskRunnerHandle::Get()) {}

  const ModuleDatabase::ModuleMap& modules() {
    return module_database_.modules_;
  }

  ModuleDatabase* module_database() { return &module_database_; }

  static uint32_t ProcessTypeToBit(content::ProcessType process_type) {
    return ModuleDatabase::ProcessTypeToBit(process_type);
  }

  void RunSchedulerUntilIdle() {
    // Call ScopedTaskEnvironment::RunUntilIdle() when it supports mocking time.
    base::TaskScheduler::GetInstance()->FlushForTesting();
    mock_time_task_runner_->RunUntilIdle();
  }

  void FastForwardToIdleTimer() {
    RunSchedulerUntilIdle();
    mock_time_task_runner_->FastForwardBy(ModuleDatabase::kIdleTimeout);
  }

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  // Must be before |module_database_|.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner_;

  ModuleDatabase module_database_;

  DISALLOW_COPY_AND_ASSIGN(ModuleDatabaseTest);
};

TEST_F(ModuleDatabaseTest, TasksAreBounced) {
  // Run a task on the current thread. This should not be bounced, so their
  // results should be immediately available.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  EXPECT_EQ(1u, modules().size());

  // Run similar tasks on another thread with another module. These should be
  // bounced.
  base::PostTask(FROM_HERE,
                 base::Bind(&ModuleDatabase::OnModuleLoad,
                            base::Unretained(module_database()), kProcessType2,
                            dll2_, kSize1, kTime1, kGoodAddress1));
  EXPECT_EQ(1u, modules().size());
  RunSchedulerUntilIdle();
  EXPECT_EQ(2u, modules().size());
}

TEST_F(ModuleDatabaseTest, DatabaseIsConsistent) {
  EXPECT_EQ(0u, modules().size());

  // Load a module.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  EXPECT_EQ(1u, modules().size());

  // Ensure that the process and module sets are up to date.
  auto m1 = modules().begin();
  EXPECT_EQ(dll1_, m1->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m1->second.process_types);

  // Provide a redundant load message for that module.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  EXPECT_EQ(1u, modules().size());

  // Ensure that the process and module sets haven't changed.
  EXPECT_EQ(dll1_, m1->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m1->second.process_types);

  // Load a second module into the process.
  module_database()->OnModuleLoad(kProcessType1, dll2_, kSize2, kTime2,
                                  kGoodAddress2);
  EXPECT_EQ(2u, modules().size());

  // Ensure that the process and module sets are up to date.
  auto m2 = modules().rbegin();
  EXPECT_EQ(dll2_, m2->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m2->second.process_types);

  // Load the dummy.dll in the second process as well.
  module_database()->OnModuleLoad(kProcessType2, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER) |
                ProcessTypeToBit(content::PROCESS_TYPE_RENDERER),
            m1->second.process_types);
}

// A dummy observer that only counts how many notifications it receives.
class DummyObserver : public ModuleDatabaseObserver {
 public:
  DummyObserver() = default;
  ~DummyObserver() override = default;

  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override {
    new_module_count_++;
  }

  void OnModuleDatabaseIdle() override {
    on_module_database_idle_called_ = true;
  }

  int new_module_count() { return new_module_count_; }
  bool on_module_database_idle_called() {
    return on_module_database_idle_called_;
  }

 private:
  int new_module_count_ = 0;
  bool on_module_database_idle_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(DummyObserver);
};

TEST_F(ModuleDatabaseTest, Observers) {
  // Assume there is no shell extensions or IMEs.
  module_database()->OnShellExtensionEnumerationFinished();
  module_database()->OnImeEnumerationFinished();

  DummyObserver before_load_observer;
  EXPECT_EQ(0, before_load_observer.new_module_count());

  module_database()->AddObserver(&before_load_observer);
  EXPECT_EQ(0, before_load_observer.new_module_count());

  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  RunSchedulerUntilIdle();

  EXPECT_EQ(1, before_load_observer.new_module_count());
  module_database()->RemoveObserver(&before_load_observer);

  // New observers get notified for past loaded modules.
  DummyObserver after_load_observer;
  EXPECT_EQ(0, after_load_observer.new_module_count());

  module_database()->AddObserver(&after_load_observer);
  EXPECT_EQ(1, after_load_observer.new_module_count());

  module_database()->RemoveObserver(&after_load_observer);
}

// Tests the idle cycle of the ModuleDatabase.
TEST_F(ModuleDatabaseTest, IsIdle) {
  // Assume there is no shell extensions or IMEs.
  module_database()->OnShellExtensionEnumerationFinished();
  module_database()->OnImeEnumerationFinished();

  // ModuleDatabase starts busy.
  EXPECT_FALSE(module_database()->IsIdle());

  // Can't fast forward to idle because a module load event is needed.
  FastForwardToIdleTimer();
  EXPECT_FALSE(module_database()->IsIdle());

  // A load module event starts the timer.
  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  EXPECT_FALSE(module_database()->IsIdle());

  FastForwardToIdleTimer();
  EXPECT_TRUE(module_database()->IsIdle());

  // A new shell extension resets the timer.
  module_database()->OnShellExtensionEnumerated(dll1_, kSize1, kTime1);
  EXPECT_FALSE(module_database()->IsIdle());

  FastForwardToIdleTimer();
  EXPECT_TRUE(module_database()->IsIdle());

  // Adding an observer while idle immediately calls OnModuleDatabaseIdle().
  DummyObserver is_idle_observer;
  module_database()->AddObserver(&is_idle_observer);
  EXPECT_TRUE(is_idle_observer.on_module_database_idle_called());

  module_database()->RemoveObserver(&is_idle_observer);

  // Make the ModuleDabatase busy.
  module_database()->OnModuleLoad(kProcessType2, dll2_, kSize2, kTime2,
                                  kGoodAddress2);
  EXPECT_FALSE(module_database()->IsIdle());

  // Adding an observer while busy doesn't.
  DummyObserver is_busy_observer;
  module_database()->AddObserver(&is_busy_observer);
  EXPECT_FALSE(is_busy_observer.on_module_database_idle_called());

  // Fast forward will call OnModuleDatabaseIdle().
  FastForwardToIdleTimer();
  EXPECT_TRUE(module_database()->IsIdle());
  EXPECT_TRUE(is_busy_observer.on_module_database_idle_called());

  module_database()->RemoveObserver(&is_busy_observer);
}

// The ModuleDatabase waits until shell extensions and IMEs are enumerated
// before notifying observers or going idle.
TEST_F(ModuleDatabaseTest, WaitUntilRegisteredModulesEnumerated) {
  // This observer is added before the first loaded module.
  DummyObserver before_load_observer;
  module_database()->AddObserver(&before_load_observer);
  EXPECT_EQ(0, before_load_observer.new_module_count());

  module_database()->OnModuleLoad(kProcessType1, dll1_, kSize1, kTime1,
                                  kGoodAddress1);
  FastForwardToIdleTimer();

  // Idle state is prevented.
  EXPECT_FALSE(module_database()->IsIdle());
  EXPECT_EQ(0, before_load_observer.new_module_count());
  EXPECT_FALSE(before_load_observer.on_module_database_idle_called());

  // This observer is added after the first loaded module.
  DummyObserver after_load_observer;
  module_database()->AddObserver(&after_load_observer);
  EXPECT_EQ(0, after_load_observer.new_module_count());
  EXPECT_FALSE(after_load_observer.on_module_database_idle_called());

  // Simulate the enumerations ending.
  module_database()->OnImeEnumerationFinished();
  module_database()->OnShellExtensionEnumerationFinished();

  EXPECT_EQ(1, before_load_observer.new_module_count());
  EXPECT_TRUE(before_load_observer.on_module_database_idle_called());
  EXPECT_EQ(1, after_load_observer.new_module_count());
  EXPECT_TRUE(after_load_observer.on_module_database_idle_called());

  module_database()->RemoveObserver(&after_load_observer);
  module_database()->RemoveObserver(&before_load_observer);
}
