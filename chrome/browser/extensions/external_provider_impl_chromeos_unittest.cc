// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

namespace extensions {

namespace {

const char kExternalAppId[] = "kekdneafjmhmndejhmbcadfiiofngffo";
const char kStandaloneAppId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kStandaloneChildAppId[] = "hcglmfcclpfgljeaiahehebeoaiicbko";

class ExternalProviderImplChromeOSTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplChromeOSTest()
      : fake_user_manager_(new chromeos::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)) {}

  ~ExternalProviderImplChromeOSTest() override {}

  void InitServiceWithExternalProviders(bool standalone) {
    InitServiceWithExternalProvidersAndUserType(standalone,
                                                false /* is_child */);
  }

  void InitServiceWithExternalProvidersAndUserType(bool standalone,
                                                   bool is_child) {
    InitializeEmptyExtensionService();

    if (is_child)
      profile_.get()->SetSupervisedUserId(supervised_users::kChildAccountSUID);

    service_->Init();

    if (standalone) {
      external_externsions_overrides_.reset(new base::ScopedPathOverride(
          chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
          data_dir().Append("external_standalone")));
    } else {
      external_externsions_overrides_.reset(new base::ScopedPathOverride(
          chrome::DIR_EXTERNAL_EXTENSIONS, data_dir().Append("external")));
    }

    ProviderCollection providers;
    extensions::ExternalProviderImpl::CreateExternalProviders(
        service_, profile_.get(), &providers);

    for (std::unique_ptr<ExternalProviderInterface>& provider : providers)
      service_->AddProviderForTesting(std::move(provider));
  }

  // ExtensionServiceTestBase overrides:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
  }

  void TearDown() override {
    chromeos::KioskAppManager::Shutdown();
  }

  // Waits until all possible standalone extensions are installed.
  void WaitForPendingStandaloneExtensionsInstalled() {
    service_->CheckForExternalUpdates();
    base::RunLoop().RunUntilIdle();
    extensions::PendingExtensionManager* const pending_extension_manager =
        service_->pending_extension_manager();
    while (pending_extension_manager->IsIdPending(kStandaloneAppId) ||
           pending_extension_manager->IsIdPending(kStandaloneChildAppId)) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  std::unique_ptr<base::ScopedPathOverride> external_externsions_overrides_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProviderImplChromeOSTest);
};

}  // namespace

// Normal mode, external app should be installed.
TEST_F(ExternalProviderImplChromeOSTest, Normal) {
  InitServiceWithExternalProviders(false);

  service_->CheckForExternalUpdates();
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources()).Wait();

  EXPECT_TRUE(service_->GetInstalledExtension(kExternalAppId));
}

// App mode, no external app should be installed.
TEST_F(ExternalProviderImplChromeOSTest, AppMode) {
  base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  command->AppendSwitchASCII(switches::kForceAppMode, std::string());
  command->AppendSwitchASCII(switches::kAppId, std::string("app_id"));

  InitServiceWithExternalProviders(false);

  service_->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(service_->GetInstalledExtension(kExternalAppId));
}

// Normal mode, standalone app should be installed, because sync is enabled but
// not running.
TEST_F(ExternalProviderImplChromeOSTest, Standalone) {
  InitServiceWithExternalProviders(true);

  WaitForPendingStandaloneExtensionsInstalled();

  EXPECT_TRUE(service_->GetInstalledExtension(kStandaloneAppId));
  // Also include apps available for child.
  EXPECT_TRUE(service_->GetInstalledExtension(kStandaloneChildAppId));
}

// Should include only subset of default apps
TEST_F(ExternalProviderImplChromeOSTest, StandaloneChild) {
  InitServiceWithExternalProvidersAndUserType(true /* standalone */,
                                              true /* is_child */);

  WaitForPendingStandaloneExtensionsInstalled();

  // kStandaloneAppId is not available for child.
  EXPECT_FALSE(service_->GetInstalledExtension(kStandaloneAppId));
  EXPECT_TRUE(service_->GetInstalledExtension(kStandaloneChildAppId));
}

// Normal mode, standalone app should be installed, because sync is disabled.
TEST_F(ExternalProviderImplChromeOSTest, SyncDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);

  InitServiceWithExternalProviders(true);

  service_->CheckForExternalUpdates();
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources()).Wait();

  EXPECT_TRUE(service_->GetInstalledExtension(kStandaloneAppId));
}

// User signed in, sync service started, install app when sync is disabled by
// policy.
// flaky: crbug.com/706506
TEST_F(ExternalProviderImplChromeOSTest, DISABLED_PolicyDisabled) {
  InitServiceWithExternalProviders(true);

  // Log user in, start sync.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(
      new ProfileManagerWithoutInit(temp_dir().GetPath()));
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile_.get());
  signin->SetAuthenticatedAccountInfo("gaia-id-test_user@gmail.com",
                                      "test_user@gmail.com");
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get())
      ->UpdateCredentials("test_user@gmail.com", "oauth2_login_token");

  // App sync will wait for priority sync to complete.
  service_->CheckForExternalUpdates();

  // Sync is dsabled by policy.
  profile_->GetPrefs()->SetBoolean(syncer::prefs::kSyncManaged, true);

  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources()).Wait();

  EXPECT_TRUE(service_->GetInstalledExtension(kStandaloneAppId));

  TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
}

// User signed in, sync service started, install app when priority sync is
// completed.
TEST_F(ExternalProviderImplChromeOSTest, PriorityCompleted) {
  InitServiceWithExternalProviders(true);

  // User is logged in.
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile_.get());
  signin->SetAuthenticatedAccountInfo("gaia-id-test_user@gmail.com",
                                      "test_user@gmail.com");

  // App sync will wait for priority sync to complete.
  service_->CheckForExternalUpdates();

  // Priority sync completed.
  PrefServiceSyncableFromProfile(profile_.get())
      ->GetSyncableService(syncer::PRIORITY_PREFERENCES)
      ->MergeDataAndStartSyncing(syncer::PRIORITY_PREFERENCES,
                                 syncer::SyncDataList(),
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock()));

  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources()).Wait();

  EXPECT_TRUE(service_->GetInstalledExtension(kStandaloneAppId));
}

}  // namespace extensions
