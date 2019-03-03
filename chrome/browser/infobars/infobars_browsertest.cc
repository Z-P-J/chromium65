// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_infobar_delegate_desktop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_infobar_delegate.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#include "chrome/browser/plugins/hung_plugin_infobar_delegate.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/google_api_keys_infobar_delegate.h"
#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/nacl/common/features.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#include "chrome/browser/ui/startup/session_crashed_infobar_delegate.h"
#endif

#if !defined(USE_AURA)
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"
#endif

class InfoBarsTest : public InProcessBrowserTest {
 public:
  InfoBarsTest() {}

  void InstallExtension(const char* filename) {
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII(filename));
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();

    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(browser()->profile()));

    std::unique_ptr<ExtensionInstallPrompt> client(new ExtensionInstallPrompt(
        browser()->tab_strip_model()->GetActiveWebContents()));
    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, std::move(client)));
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
    installer->InstallCrx(path);

    observer.WaitForExtensionLoaded();
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarsTest, TestInfoBarsCloseOnNewTheme) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html"));
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Adding a theme should create an infobar.
  {
    content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
    InstallExtension("theme.crx");
    infobar_added.Wait();
    EXPECT_EQ(1u, infobar_service->infobar_count());
  }

  // Adding a theme in a new tab should close the old tab's infobar.
  {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), embedded_test_server()->GetURL("/simple.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
    content::WindowedNotificationObserver infobar_removed(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());
    InstallExtension("theme2.crx");
    infobar_removed.Wait();
    infobar_added.Wait();
    EXPECT_EQ(0u, infobar_service->infobar_count());
    infobar_service = InfoBarService::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(1u, infobar_service->infobar_count());
  }

  // Switching back to the default theme should close the infobar.
  {
    content::WindowedNotificationObserver infobar_removed(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());
    ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
    infobar_removed.Wait();
    EXPECT_EQ(0u, infobar_service->infobar_count());
  }
}

namespace {

// Helper to return when an InfoBar has been removed or replaced.
class InfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  InfoBarObserver(infobars::InfoBarManager* manager, infobars::InfoBar* infobar)
      : manager_(manager), infobar_(infobar) {
    manager_->AddObserver(this);
  }

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    if (infobar != infobar_)
      return;
    manager_->RemoveObserver(this);
    run_loop_.Quit();
  }
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override {
    OnInfoBarRemoved(old_infobar, false);
  }

  void WaitForRemoval() { run_loop_.Run(); }

 private:
  infobars::InfoBarManager* manager_;
  infobars::InfoBar* infobar_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarObserver);
};

}  // namespace

class InfoBarUiTest : public UiBrowserTest {
 public:
  InfoBarUiTest() = default;

  // TestBrowserUi:
  void PreShow() override;
  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 private:
  // Returns the active tab.
  content::WebContents* GetWebContents();

  // Returns the InfoBarService associated with the active tab.
  InfoBarService* GetInfoBarService();

  // Sets |infobars_| to a sorted (by pointer value) list of all infobars from
  // the active tab.
  void UpdateInfoBars();

  infobars::InfoBarManager::InfoBars infobars_;
  infobars::InfoBarDelegate::InfoBarIdentifier desired_infobar_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarUiTest);
};

void InfoBarUiTest::PreShow() {
  UpdateInfoBars();
}

void InfoBarUiTest::ShowUi(const std::string& name) {
  using IBD = infobars::InfoBarDelegate;
  const base::flat_map<std::string, IBD::InfoBarIdentifier> kIdentifiers = {
      {"app_banner", IBD::APP_BANNER_INFOBAR_DELEGATE},
      {"hung_plugin", IBD::HUNG_PLUGIN_INFOBAR_DELEGATE},
      {"dev_tools", IBD::DEV_TOOLS_INFOBAR_DELEGATE},
      {"extension_dev_tools", IBD::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE},
      {"incognito_connectability",
       IBD::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE},
      {"theme_installed", IBD::THEME_INSTALLED_INFOBAR_DELEGATE},
      {"nacl", IBD::NACL_INFOBAR_DELEGATE},
      {"pepper_broker", IBD::PEPPER_BROKER_INFOBAR_DELEGATE},
      {"outdated_plugin", IBD::OUTDATED_PLUGIN_INFOBAR_DELEGATE},
      {"reload_plugin", IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE},
      {"plugin_observer", IBD::PLUGIN_OBSERVER_INFOBAR_DELEGATE},
      {"file_access_disabled", IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE},
      {"keystone_promotion", IBD::KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC},
      {"collected_cookies", IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE},
      {"installation_error", IBD::INSTALLATION_ERROR_INFOBAR_DELEGATE},
      {"alternate_nav", IBD::ALTERNATE_NAV_INFOBAR_DELEGATE},
      {"bad_flags", IBD::BAD_FLAGS_INFOBAR_DELEGATE},
      {"default_browser", IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE},
      {"google_api_keys", IBD::GOOGLE_API_KEYS_INFOBAR_DELEGATE},
      {"obsolete_system", IBD::OBSOLETE_SYSTEM_INFOBAR_DELEGATE},
      {"session_crashed", IBD::SESSION_CRASHED_INFOBAR_DELEGATE_MAC_IOS},
      {"page_info", IBD::PAGE_INFO_INFOBAR_DELEGATE},
      {"translate", IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA},
      {"data_reduction_proxy_preview",
       IBD::DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE},
      {"automation", IBD::AUTOMATION_INFOBAR_DELEGATE},
  };
  auto id = kIdentifiers.find(name);
  desired_infobar_ = (id == kIdentifiers.end()) ? IBD::INVALID : id->second;

  switch (desired_infobar_) {
    case IBD::APP_BANNER_INFOBAR_DELEGATE:
      banners::AppBannerInfoBarDelegateDesktop::Create(
          GetWebContents(), nullptr, nullptr, content::Manifest());
      break;

    case IBD::HUNG_PLUGIN_INFOBAR_DELEGATE:
      HungPluginInfoBarDelegate::Create(GetInfoBarService(), nullptr, 0,
                                        base::ASCIIToUTF16("Test Plugin"));
      break;

    case IBD::DEV_TOOLS_INFOBAR_DELEGATE:
      DevToolsInfoBarDelegate::Create(
          l10n_util::GetStringFUTF16(
              IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE,
              base::ASCIIToUTF16("file_path")),
          DevToolsInfoBarDelegate::Callback());
      break;

    case IBD::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE:
      extensions::ExtensionDevToolsInfoBar::Create("id", "Extension", nullptr,
                                                   base::Closure());
      break;

    case IBD::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE: {
      using Tracker = extensions::IncognitoConnectability::ScopedAlertTracker;
      extensions::IncognitoConnectabilityInfoBarDelegate::Create(
          GetInfoBarService(),
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO,
              base::ASCIIToUTF16("http://example.com"),
              base::ASCIIToUTF16("Test Extension")),
          base::Bind([](Tracker::Mode m) {}));
      break;
    }

    case IBD::THEME_INSTALLED_INFOBAR_DELEGATE:
      ThemeInstalledInfoBarDelegate::Create(
          GetInfoBarService(), nullptr,
          ThemeServiceFactory::GetForProfile(browser()->profile()), "New Theme",
          "id", ThemeService::kDefaultThemeID, true);
      break;

    case IBD::NACL_INFOBAR_DELEGATE:
#if BUILDFLAG(ENABLE_NACL)
      NaClInfoBarDelegate::Create(GetInfoBarService());
#else
      ADD_FAILURE() << "This infobar is not supported when NaCl is disabled.";
#endif
      break;

    case IBD::PEPPER_BROKER_INFOBAR_DELEGATE:
      PepperBrokerInfoBarDelegate::Create(
          GetInfoBarService(), GURL("http://example.com/"),
          base::ASCIIToUTF16("Test Plugin"), nullptr, nullptr,
          base::Callback<void(bool)>());
      break;

    case IBD::OUTDATED_PLUGIN_INFOBAR_DELEGATE:
      OutdatedPluginInfoBarDelegate::Create(
          GetInfoBarService(), nullptr,
          std::make_unique<PluginMetadata>(
              "test-plugin", base::ASCIIToUTF16("Test Plugin"), true, GURL(),
              GURL(), base::ASCIIToUTF16("Test"), std::string()));
      break;

    case IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE:
      ReloadPluginInfoBarDelegate::Create(
          GetInfoBarService(), nullptr,
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                     base::ASCIIToUTF16("Test Plugin")));
      break;

    case IBD::PLUGIN_OBSERVER_INFOBAR_DELEGATE:
      PluginObserver::CreatePluginObserverInfoBar(
          GetInfoBarService(), base::ASCIIToUTF16("Test Plugin"));
      break;

    case IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE:
      ChromeSelectFilePolicy(GetWebContents()).SelectFileDenied();
      break;

    case IBD::KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC:
#if defined(OS_MACOSX)
      KeystonePromotionInfoBarDelegate::Create(GetWebContents());
#else
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#endif
      break;

    case IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE:
      CollectedCookiesInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::INSTALLATION_ERROR_INFOBAR_DELEGATE: {
      const base::string16 msg =
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE);
      InstallationErrorInfoBarDelegate::Create(
          GetInfoBarService(),
          extensions::CrxInstallError(
              extensions::CrxInstallError::ERROR_OFF_STORE, msg));
      break;
    }

    case IBD::ALTERNATE_NAV_INFOBAR_DELEGATE: {
      AutocompleteMatch match;
      match.destination_url = GURL("http://intranetsite/");
      AlternateNavInfoBarDelegate::Create(GetWebContents(), base::string16(),
                                          match, GURL("http://example.com/"));
      break;
    }

    case IBD::BAD_FLAGS_INFOBAR_DELEGATE:
      chrome::ShowBadFlagsInfoBar(GetWebContents(),
                                  IDS_BAD_FLAGS_WARNING_MESSAGE,
                                  switches::kNoSandbox);
      break;

    case IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE:
#if defined(OS_CHROMEOS)
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#else
      chrome::DefaultBrowserInfoBarDelegate::Create(GetInfoBarService(),
                                                    browser()->profile());
#endif
      break;

    case IBD::GOOGLE_API_KEYS_INFOBAR_DELEGATE:
      GoogleApiKeysInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::OBSOLETE_SYSTEM_INFOBAR_DELEGATE:
      ObsoleteSystemInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::SESSION_CRASHED_INFOBAR_DELEGATE_MAC_IOS:
#if defined(OS_MACOSX)
      SessionCrashedInfoBarDelegate::Create(browser());
#else
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#endif
      break;

    case IBD::PAGE_INFO_INFOBAR_DELEGATE:
      PageInfoInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA: {
#if defined(USE_AURA)
      ADD_FAILURE() << "This infobar is not supported on this toolkit.";
#else
      ChromeTranslateClient::CreateForWebContents(GetWebContents());
      ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(GetWebContents());
      translate::TranslateInfoBarDelegate::Create(
          false, translate_client->GetTranslateManager()->GetWeakPtr(),
          GetInfoBarService(), false,
          translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "ja", "en",
          translate::TranslateErrors::NONE, false);
#endif
      break;
    }

    case IBD::DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE:
      PreviewsInfoBarDelegate::Create(
          GetWebContents(), previews::PreviewsType::LOFI, base::Time::Now(),
          true, true,
          PreviewsInfoBarDelegate::OnDismissPreviewsInfobarCallback(), nullptr);
      break;

    case IBD::AUTOMATION_INFOBAR_DELEGATE:
      AutomationInfoBarDelegate::Create();
      break;

    default:
      break;
  }
}

bool InfoBarUiTest::VerifyUi() {
  infobars::InfoBarManager::InfoBars old_infobars = infobars_;
  UpdateInfoBars();
  auto added = base::STLSetDifference<infobars::InfoBarManager::InfoBars>(
      infobars_, old_infobars);
  return (added.size() == 1) &&
         (added[0]->delegate()->GetIdentifier() == desired_infobar_);
}

void InfoBarUiTest::WaitForUserDismissal() {
  InfoBarObserver observer(GetInfoBarService(), infobars_.front());
  observer.WaitForRemoval();
}

content::WebContents* InfoBarUiTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

InfoBarService* InfoBarUiTest::GetInfoBarService() {
  return InfoBarService::FromWebContents(GetWebContents());
}

void InfoBarUiTest::UpdateInfoBars() {
  infobars_ = GetInfoBarService()->infobars_;
  std::sort(infobars_.begin(), infobars_.end());
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_app_banner) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_hung_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_dev_tools) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_extension_dev_tools) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_incognito_connectability) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_theme_installed) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(ENABLE_NACL)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_nacl) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_pepper_broker) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_outdated_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_reload_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_plugin_observer) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_file_access_disabled) {
  ShowAndVerifyUi();
}

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_keystone_promotion) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_collected_cookies) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_installation_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_alternate_nav) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_bad_flags) {
  ShowAndVerifyUi();
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_default_browser) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_google_api_keys) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_obsolete_system) {
  ShowAndVerifyUi();
}

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_session_crashed) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_page_info) {
  ShowAndVerifyUi();
}

#if !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_translate) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_data_reduction_proxy_preview) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_automation) {
  ShowAndVerifyUi();
}
