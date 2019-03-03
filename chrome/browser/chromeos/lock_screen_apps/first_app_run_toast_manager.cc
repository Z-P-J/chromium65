// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/first_app_run_toast_manager.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/lock_screen_apps/toast_dialog_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace lock_screen_apps {

namespace {

// Toast dialog's vertical offset from the app window bottom.
constexpr int kToastDialogVerticalOffset = 20;

}  // namespace

FirstAppRunToastManager::FirstAppRunToastManager(Profile* profile)
    : profile_(profile),
      toast_widget_observer_(this),
      app_window_observer_(this),
      native_app_window_observer_(this),
      weak_ptr_factory_(this) {}

FirstAppRunToastManager::~FirstAppRunToastManager() {
  Reset();
}

void FirstAppRunToastManager::RunForAppWindow(
    extensions::AppWindow* app_window) {
  if (app_window_)
    return;

  DCHECK(app_window->GetNativeWindow());

  const extensions::Extension* app = app_window->GetExtension();
  const base::DictionaryValue* toast_shown =
      profile_->GetPrefs()->GetDictionary(
          prefs::kNoteTakingAppsLockScreenToastShown);
  bool already_shown_for_app = false;
  if (toast_shown->GetBoolean(app->id(), &already_shown_for_app) &&
      already_shown_for_app) {
    return;
  }

  app_window_ = app_window;
  native_app_window_observer_.Add(app_window_->GetNativeWindow());

  if (app_window_->GetNativeWindow()->HasFocus()) {
    CreateAndShowToastDialog();
  } else {
    app_window_observer_.Add(
        extensions::AppWindowRegistry::Get(app_window_->browser_context()));
  }
}

void FirstAppRunToastManager::Reset() {
  native_app_window_observer_.RemoveAll();
  app_window_observer_.RemoveAll();
  toast_widget_observer_.RemoveAll();

  app_window_ = nullptr;

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (toast_widget_ && !toast_widget_->IsClosed())
    toast_widget_->Close();
  toast_widget_ = nullptr;
}

void FirstAppRunToastManager::OnWidgetDestroyed(views::Widget* widget) {
  Reset();
}

void FirstAppRunToastManager::OnAppWindowActivated(
    extensions::AppWindow* app_window) {
  if (app_window == app_window_) {
    app_window_observer_.RemoveAll();

    // Start toast dialog creation asynchronously so it happens after app window
    // activation completes.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&FirstAppRunToastManager::CreateAndShowToastDialog,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void FirstAppRunToastManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (toast_widget_ && window == app_window_->GetNativeWindow())
    AdjustToastWidgetBounds();
}

void FirstAppRunToastManager::CreateAndShowToastDialog() {
  auto* toast_dialog = new ToastDialogView(
      base::UTF8ToUTF16(app_window_->GetExtension()->short_name()),
      base::Bind(&FirstAppRunToastManager::ToastDialogDismissed,
                 weak_ptr_factory_.GetWeakPtr()));
  toast_dialog->Show();
  toast_widget_ = toast_dialog->GetWidget();
  AdjustToastWidgetBounds();
  toast_widget_observer_.Add(toast_widget_);
}

void FirstAppRunToastManager::ToastDialogDismissed() {
  {
    const extensions::Extension* app = app_window_->GetExtension();
    DictionaryPrefUpdate dict_update(
        profile_->GetPrefs(), prefs::kNoteTakingAppsLockScreenToastShown);
    dict_update->SetBoolean(app->id(), true);
  }
  Reset();
}

void FirstAppRunToastManager::AdjustToastWidgetBounds() {
  DCHECK(toast_widget_);
  DCHECK(app_window_);

  const gfx::Rect app_window_bounds =
      app_window_->GetNativeWindow()->GetBoundsInScreen();
  const gfx::Rect original_bounds = toast_widget_->GetWindowBoundsInScreen();

  gfx::Point intended_origin = gfx::Point(
      // Center toast widget horizontally relative to app_window bounds.
      app_window_bounds.x() +
          (app_window_bounds.width() - original_bounds.width()) / 2,
      // Position toast widget dialog at the bottom of app window, with an
      // additional offset (so poirtion of the dialog is painted outside the
      // app window bounds).
      app_window_bounds.bottom() - original_bounds.height() +
          kToastDialogVerticalOffset);

  toast_widget_->SetBounds(gfx::Rect(intended_origin, original_bounds.size()));
}

}  // namespace lock_screen_apps
