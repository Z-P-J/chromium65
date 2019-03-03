// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

class TestClient : public content::DevToolsAgentHostClient {
 public:
  TestClient() {}
  ~TestClient() override {}
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
};

class ScopedDevtoolsOpener {
 public:
  explicit ScopedDevtoolsOpener(content::WebContents* web_contents)
      : agent_host_(content::DevToolsAgentHost::GetOrCreateFor(web_contents)) {
    EXPECT_TRUE(agent_host_);
    agent_host_->AttachClient(&test_client_);
    // Send Page.enable, which is required before any Page methods.
    agent_host_->DispatchProtocolMessage(
        &test_client_, "{\"id\": 0, \"method\": \"Page.enable\"}");
  }

  ~ScopedDevtoolsOpener() { agent_host_->DetachAllClients(); }

  void EnableAdBlocking(bool enabled) {
    // Send Page.setAdBlockingEnabled, should force activation.
    base::DictionaryValue ad_blocking_command;
    ad_blocking_command.SetInteger("id", 1);
    ad_blocking_command.SetString("method", "Page.setAdBlockingEnabled");
    auto params = base::MakeUnique<base::DictionaryValue>();
    params->SetBoolean("enabled", enabled);
    ad_blocking_command.SetDictionary("params", std::move(params));
    std::string json_string;
    JSONStringValueSerializer serializer(&json_string);
    ASSERT_TRUE(serializer.Serialize(ad_blocking_command));
    agent_host_->DispatchProtocolMessage(&test_client_, json_string);
  }

 private:
  TestClient test_client_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDevtoolsOpener);
};

}  // namespace

class SubresourceFilterDevtoolsBrowserTest
    : public SubresourceFilterBrowserTest {};

IN_PROC_BROWSER_TEST_F(SubresourceFilterDevtoolsBrowserTest,
                       ForceActivation_RequiresDevtools) {
  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation, the URL is not on the blacklist.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // Open up devtools and trigger forced activation.
  {
    ScopedDevtoolsOpener devtools(web_contents());
    devtools.EnableAdBlocking(true);

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

    // Close devtools, should stop forced activation.
  }
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterListInsertingBrowserTest,
                       WarningSiteWithForceActivation_LogsWarning) {
  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ConfigureURLWithWarning(url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});

  Configuration config(subresource_filter::ActivationLevel::ENABLED,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::BETTER_ADS);
  ResetConfiguration(std::move(config));

  // Should not trigger activation, the URL is not on the blacklist.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::ConsoleObserverDelegate console_observer(
      web_contents(), kActivationWarningConsoleMessage);
  web_contents()->SetDelegate(&console_observer);

  // Open up devtools and trigger forced activation.
  {
    ScopedDevtoolsOpener devtools(web_contents());
    devtools.EnableAdBlocking(true);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
    console_observer.Wait();
    EXPECT_EQ(console_observer.message(), kActivationWarningConsoleMessage);
    // Close devtools, should stop forced activation.
  }
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterDevtoolsBrowserTest,
                       ForceActivation_NoSubresourceLogging) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    "*show ads*");
  web_contents()->SetDelegate(&console_observer);
  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ScopedDevtoolsOpener devtools(web_contents());
  devtools.EnableAdBlocking(true);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_TRUE(console_observer.message().empty()) << console_observer.message();
}

}  // namespace subresource_filter
