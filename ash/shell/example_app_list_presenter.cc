// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/example_app_list_presenter.h"

#include <memory>

#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ash/shell/example_factory.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"

namespace {

// An example app list view delegate factory.
class ExampleAppListViewDelegateFactory
    : public app_list::AppListViewDelegateFactory {
 public:
  ExampleAppListViewDelegateFactory()
      : app_list_view_delegate_(ash::shell::CreateAppListViewDelegate()) {}
  ~ExampleAppListViewDelegateFactory() override = default;

  // app_list::AppListViewDelegateFactory:
  app_list::AppListViewDelegate* GetDelegate() override {
    return app_list_view_delegate_.get();
  }

 private:
  std::unique_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListViewDelegateFactory);
};

}  // namespace

namespace ash {
namespace shell {

ExampleAppListPresenter::ExampleAppListPresenter()
    : binding_(this),
      app_list_presenter_impl_(
          std::make_unique<AppListPresenterDelegateFactory>(
              std::make_unique<ExampleAppListViewDelegateFactory>())) {
  // Note: This example |app_list_presenter_impl_| does not report visibility
  // changes to the app_list::mojom::AppList implementation owned by ShellPort.
}

ExampleAppListPresenter::~ExampleAppListPresenter() = default;

app_list::mojom::AppListPresenterPtr
ExampleAppListPresenter::CreateInterfacePtrAndBind() {
  app_list::mojom::AppListPresenterPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void ExampleAppListPresenter::Show(int64_t display_id) {
  app_list_presenter_impl_.Show(display_id);
}

void ExampleAppListPresenter::Dismiss() {
  app_list_presenter_impl_.Dismiss();
}

void ExampleAppListPresenter::ToggleAppList(int64_t display_id) {
  app_list_presenter_impl_.ToggleAppList(display_id);
}

void ExampleAppListPresenter::StartVoiceInteractionSession() {}

void ExampleAppListPresenter::ToggleVoiceInteractionSession() {}

void ExampleAppListPresenter::UpdateYPositionAndOpacity(
    int new_y_position,
    float background_opacity) {}

void ExampleAppListPresenter::EndDragFromShelf(
    app_list::mojom::AppListState app_list_state) {}

void ExampleAppListPresenter::ProcessMouseWheelOffset(int offset) {}

}  // namespace shell
}  // namespace ash
