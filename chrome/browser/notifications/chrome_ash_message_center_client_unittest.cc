// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notifier_id.h"

namespace {

class ChromeAshMessageCenterClientTest : public testing::Test {
 protected:
  ChromeAshMessageCenterClientTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ChromeAshMessageCenterClientTest() override {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
  }

  void TearDown() override { client_.reset(); }

  TestingProfile* CreateProfile(const std::string& name) {
    TestingProfile* profile =
        testing_profile_manager_.CreateTestingProfile(name);

    GetFakeUserManager()->AddUser(AccountId::FromUserEmail(name));
    GetFakeUserManager()->LoginUser(AccountId::FromUserEmail(name));
    return profile;
  }

  base::FilePath GetProfilePath(const std::string& base_name) {
    return testing_profile_manager_.profile_manager()
        ->user_data_dir()
        .AppendASCII(base_name);
  }

  void SwitchActiveUser(const std::string& name) {
    GetFakeUserManager()->SwitchActiveUser(AccountId::FromUserEmail(name));
  }

  void CreateClient() {
    client_.reset(new ChromeAshMessageCenterClient(nullptr));
  }

  ChromeAshMessageCenterClient* message_center_client() {
    return client_.get();
  }

 protected:
  void RefreshNotifierList() {
    message_center_client()->GetNotifierList(
        base::BindOnce(&ChromeAshMessageCenterClientTest::SetNotifierUiData,
                       base::Unretained(this)));
  }

  std::vector<ash::mojom::NotifierUiDataPtr> notifiers_;

 private:
  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void SetNotifierUiData(std::vector<ash::mojom::NotifierUiDataPtr> notifiers) {
    notifiers_ = std::move(notifiers);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<ChromeAshMessageCenterClient> client_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAshMessageCenterClientTest);
};

// TODO(mukai): write a test case to reproduce the actual guest session scenario
// in ChromeOS.

TEST_F(ChromeAshMessageCenterClientTest, NotifierSortOrder) {
  TestingProfile* profile = CreateProfile("Profile-1");
  extensions::TestExtensionSystem* test_extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ExtensionService* extension_service =
      test_extension_system->CreateExtensionService(
          &command_line, base::FilePath() /* install_directory */,
          false /* autoupdate_enabled*/);

  extensions::ExtensionBuilder foo_app;
  // Foo is an app with name Foo and should appear second.
  const std::string kFooId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  // Bar is an app with name Bar and should appear first.
  const std::string kBarId = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

  // Baz is an app with name Baz and should not appear in the notifier list
  // since it doesn't have notifications permission.
  const std::string kBazId = "cccccccccccccccccccccccccccccccc";

  // Baf is a hosted app which should not appear in the notifier list.
  const std::string kBafId = "dddddddddddddddddddddddddddddddd";

  foo_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Set("permissions",
               extensions::ListBuilder().Append("notifications").Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service->AddExtension(foo_app.Build().get());

  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Bar")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Set("permissions",
               extensions::ListBuilder().Append("notifications").Build())
          .Build());
  bar_app.SetID(kBarId);
  extension_service->AddExtension(bar_app.Build().get());

  extensions::ExtensionBuilder baz_app;
  baz_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "baz")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Build());
  baz_app.SetID(kBazId);
  extension_service->AddExtension(baz_app.Build().get());

  extensions::ExtensionBuilder baf_app;
  baf_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "baf")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app",
               extensions::DictionaryBuilder()
                   .Set("urls", extensions::ListBuilder()
                                    .Append("http://localhost/extensions/"
                                            "hosted_app/main.html")
                                    .Build())
                   .Build())
          .Set("launch",
               extensions::DictionaryBuilder()
                   .Set("urls", extensions::ListBuilder()
                                    .Append("http://localhost/extensions/"
                                            "hosted_app/main.html")
                                    .Build())
                   .Build())
          .Build());

  baf_app.SetID(kBafId);
  extension_service->AddExtension(baf_app.Build().get());
  CreateClient();

  RefreshNotifierList();
  ASSERT_EQ(2u, notifiers_.size());
  EXPECT_EQ(kBarId, notifiers_[0]->notifier_id.id);
  EXPECT_EQ(kFooId, notifiers_[1]->notifier_id.id);
}

TEST_F(ChromeAshMessageCenterClientTest, SetWebPageNotifierEnabled) {
  Profile* profile = CreateProfile("MyProfile");
  CreateClient();

  GURL origin("https://example.com/");

  message_center::NotifierId notifier_id(origin);

  ContentSetting default_setting =
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL);
  ASSERT_EQ(CONTENT_SETTING_ASK, default_setting);

  PermissionManager* permission_manager = PermissionManager::Get(profile);

  // (1) Enable the permission when the default is to ask (expected to set).
  message_center_client()->SetNotifierEnabled(notifier_id, true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);

  // (2) Disable the permission when the default is to ask (expected to clear).
  message_center_client()->SetNotifierEnabled(notifier_id, false);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);

  // Change the default content setting vaule for notifications to ALLOW.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);

  // (3) Disable the permission when the default is allowed (expected to set).
  message_center_client()->SetNotifierEnabled(notifier_id, false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);

  // (4) Enable the permission when the default is allowed (expected to clear).
  message_center_client()->SetNotifierEnabled(notifier_id, true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);

  // Now change the default content setting value to BLOCK.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_BLOCK);

  // (5) Enable the permission when the default is blocked (expected to set).
  message_center_client()->SetNotifierEnabled(notifier_id, true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);

  // (6) Disable the permission when the default is blocked (expected to clear).
  message_center_client()->SetNotifierEnabled(notifier_id, false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_manager
                ->GetPermissionStatus(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      origin, origin)
                .content_setting);
}

}  // namespace
