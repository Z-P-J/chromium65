// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_sync_ui_state_watcher.h"

#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/ash/app_sync_ui_state.h"

AppSyncUIStateWatcher::AppSyncUIStateWatcher(Profile* profile,
                                             AppListModelUpdater* model_updater)
    : app_sync_ui_state_(AppSyncUIState::Get(profile)),
      model_updater_(model_updater) {
  if (app_sync_ui_state_) {
    app_sync_ui_state_->AddObserver(this);
    OnAppSyncUIStatusChanged();
  }
}

AppSyncUIStateWatcher::~AppSyncUIStateWatcher() {
  if (app_sync_ui_state_)
    app_sync_ui_state_->RemoveObserver(this);
}

void AppSyncUIStateWatcher::OnAppSyncUIStatusChanged() {
  if (app_sync_ui_state_->status() == AppSyncUIState::STATUS_SYNCING)
    model_updater_->SetStatus(app_list::AppListModel::STATUS_SYNCING);
  else
    model_updater_->SetStatus(app_list::AppListModel::STATUS_NORMAL);
}
