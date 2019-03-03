// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/mark_page_accessed_task.h"

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const int64_t kTestOfflineId = 1234LL;
const char kTestClientNamespace[] = "default";
const ClientId kTestClientId(kTestClientNamespace, "1234");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("/test/path/file"));
const int64_t kTestFileSize = 876543LL;
}  // namespace

class MarkPageAccessedTaskTest : public testing::Test {
 public:
  MarkPageAccessedTaskTest();
  ~MarkPageAccessedTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  TestTaskRunner* runner() { return &runner_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  TestTaskRunner runner_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

MarkPageAccessedTaskTest::MarkPageAccessedTaskTest()
    : task_runner_(new base::TestMockTimeTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_) {}

MarkPageAccessedTaskTest::~MarkPageAccessedTaskTest() {}

void MarkPageAccessedTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void MarkPageAccessedTaskTest::TearDown() {
  store_test_util_.DeleteStore();
}

TEST_F(MarkPageAccessedTaskTest, MarkPageAccessed) {
  OfflinePageItem page(kTestUrl, kTestOfflineId, kTestClientId, kTestFilePath,
                       kTestFileSize);
  store_test_util()->InsertItem(page);

  base::Time current_time = base::Time::Now();
  auto task = std::make_unique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                     current_time);
  runner()->RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestUrl, offline_page->url);
  EXPECT_EQ(kTestClientId, offline_page->client_id);
  EXPECT_EQ(kTestFileSize, offline_page->file_size);
  EXPECT_EQ(1, offline_page->access_count);
  EXPECT_EQ(current_time, offline_page->last_access_time);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(kTestClientId.name_space)),
      1);
  histogram_tester()->ExpectUniqueSample(
      model_utils::AddHistogramSuffix(kTestClientId.name_space,
                                      "OfflinePages.PageAccessInterval"),
      (current_time - page.last_access_time).InMinutes(), 1);
}

TEST_F(MarkPageAccessedTaskTest, MarkPageAccessedTwice) {
  OfflinePageItem page(kTestUrl, kTestOfflineId, kTestClientId, kTestFilePath,
                       kTestFileSize);
  store_test_util()->InsertItem(page);

  base::Time current_time = base::Time::Now();
  auto task = std::make_unique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                     current_time);
  runner()->RunTask(std::move(task));

  auto offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestOfflineId, offline_page->offline_id);
  EXPECT_EQ(kTestUrl, offline_page->url);
  EXPECT_EQ(kTestClientId, offline_page->client_id);
  EXPECT_EQ(kTestFileSize, offline_page->file_size);
  EXPECT_EQ(1, offline_page->access_count);
  EXPECT_EQ(current_time, offline_page->last_access_time);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(kTestClientId.name_space)),
      1);
  histogram_tester()->ExpectUniqueSample(
      model_utils::AddHistogramSuffix(kTestClientId.name_space,
                                      "OfflinePages.PageAccessInterval"),
      (current_time - page.last_access_time).InMinutes(), 1);

  base::Time second_time = base::Time::Now();
  task = std::make_unique<MarkPageAccessedTask>(store(), kTestOfflineId,
                                                second_time);
  runner()->RunTask(std::move(task));

  offline_page = store_test_util()->GetPageByOfflineId(kTestOfflineId);
  EXPECT_EQ(kTestOfflineId, offline_page->offline_id);
  EXPECT_EQ(2, offline_page->access_count);
  EXPECT_EQ(second_time, offline_page->last_access_time);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.AccessPageCount",
      static_cast<int>(model_utils::ToNamespaceEnum(kTestClientId.name_space)),
      2);
  histogram_tester()->ExpectBucketCount(
      model_utils::AddHistogramSuffix(kTestClientId.name_space,
                                      "OfflinePages.PageAccessInterval"),
      (second_time - current_time).InMinutes(), 1);
}

}  // namespace offline_pages
