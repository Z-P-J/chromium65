// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/auth/auth_prewarmer.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "ui/gfx/native_widget_types.h"

class AccountId;

namespace chromeos {

class ArcKioskController;
class AppLaunchController;
class DemoAppLauncher;
class LoginScreenContext;
class OobeUI;
class WebUILoginView;
class WizardController;

// An interface that defines OOBE/login screen host.
// Host encapsulates WebUI window OOBE/login controllers,
// UI implementation (such as LoginDisplay).
class LoginDisplayHost {
 public:
  // Returns the default LoginDisplayHost instance if it has been created.
  static LoginDisplayHost* default_host() { return default_host_; }

  LoginDisplayHost();
  virtual ~LoginDisplayHost();

  // Creates UI implementation specific login display instance (views/WebUI).
  // The caller takes ownership of the returned value.
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) = 0;

  // Returns corresponding native window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns instance of the OOBE WebUI.
  virtual OobeUI* GetOobeUI() const = 0;

  // Returns the current login view.
  virtual WebUILoginView* GetWebUILoginView() const = 0;

  // Called when browsing session starts before creating initial browser.
  virtual void BeforeSessionStart() = 0;

  // Called when user enters or returns to browsing session so LoginDisplayHost
  // instance may delete itself. |completion_callback| will be invoked when the
  // instance is gone.
  virtual void Finalize(base::OnceClosure completion_callback) = 0;

  // Toggles status area visibility.
  virtual void SetStatusAreaVisible(bool visible) = 0;

  // Starts out-of-box-experience flow or shows other screen handled by
  // Wizard controller i.e. camera, recovery.
  // One could specify start screen with |first_screen|.
  virtual void StartWizard(OobeScreen first_screen) = 0;

  // Returns current WizardController, if it exists.
  // Result should not be stored.
  virtual WizardController* GetWizardController() = 0;

  // Returns current AppLaunchController, if it exists.
  // Result should not be stored.
  AppLaunchController* GetAppLaunchController();

  // Starts screen for adding user into session.
  // |completion_callback| is invoked after login display host shutdown.
  // |completion_callback| can be null.
  virtual void StartUserAdding(base::OnceClosure completion_callback) = 0;

  // Cancel addint user into session.
  virtual void CancelUserAdding() = 0;

  // Starts sign in screen.
  void StartSignInScreen(const LoginScreenContext& context);

  // Invoked when system preferences that affect the signin screen have changed.
  virtual void OnPreferencesChanged() = 0;

  // Initiates authentication network prewarming.
  void PrewarmAuthentication();

  // Starts app launch splash screen. If |is_auto_launch| is true, the app is
  // being auto-launched with no delay.
  void StartAppLaunch(const std::string& app_id,
                      bool diagnostic_mode,
                      bool is_auto_launch);

  // Starts the demo app launch.
  void StartDemoAppLaunch();

  // Starts ARC kiosk splash screen.
  void StartArcKiosk(const AccountId& account_id);

  // Start voice interaction OOBE.
  virtual void StartVoiceInteractionOobe() = 0;

  // Returns whether current host is for voice interaction OOBE.
  virtual bool IsVoiceInteractionOobe() = 0;

 protected:
  // Default LoginDisplayHost. Child class sets the reference.
  static LoginDisplayHost* default_host_;

  virtual void OnStartSignInScreen(const LoginScreenContext& context) = 0;
  virtual void OnStartAppLaunch() = 0;
  virtual void OnStartArcKiosk() = 0;

  // Deletes |auth_prewarmer_|.
  void OnAuthPrewarmDone();

  // Active instance of authentication prewarmer.
  std::unique_ptr<AuthPrewarmer> auth_prewarmer_;

  // App launch controller.
  std::unique_ptr<AppLaunchController> app_launch_controller_;

  // Demo app launcher.
  std::unique_ptr<DemoAppLauncher> demo_app_launcher_;

  // ARC kiosk controller.
  std::unique_ptr<ArcKioskController> arc_kiosk_controller_;

 private:
  base::WeakPtrFactory<LoginDisplayHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
