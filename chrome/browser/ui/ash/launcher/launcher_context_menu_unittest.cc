// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"

#include <memory>

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/fake_tablet_mode_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/arc_launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/extension_launcher_context_menu.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsItemPresentInMenu(ui::MenuModel* menu, int command_id) {
  ui::MenuModel* model = menu;
  int index = 0;
  return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                     &index);
}

bool IsItemEnabledInMenu(ui::MenuModel* menu, int command_id) {
  ui::MenuModel* model = menu;
  int index = 0;
  return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                     &index) &&
         menu->IsEnabledAt(index);
}

std::string GetAppNameInShelfGroup(uint32_t task_id) {
  return base::StringPrintf("AppInShelfGroup%d", task_id);
}

class LauncherContextMenuTest : public ash::AshTestBase {
 protected:

  LauncherContextMenuTest() {}

  void SetUp() override {
    arc_test_.SetUp(&profile_);
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    ash::AshTestBase::SetUp();
    model_ = std::make_unique<ash::ShelfModel>();
    launcher_controller_ =
        std::make_unique<ChromeLauncherController>(&profile_, model_.get());

    tablet_mode_client_ = std::make_unique<TabletModeClient>();
    tablet_mode_client_->InitForTesting(
        fake_tablet_mode_controller_.CreateInterfacePtr());
  }

  std::unique_ptr<LauncherContextMenu> CreateLauncherContextMenu(
      ash::ShelfItemType shelf_item_type,
      int64_t display_id) {
    ash::ShelfItem item;
    item.id = ash::ShelfID("idmockidmockidmockidmockidmockid");
    item.type = shelf_item_type;
    return LauncherContextMenu::Create(controller(), &item, display_id);
  }

  // Creates app window and set optional ARC application id.
  views::Widget* CreateArcWindow(std::string& window_app_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* widget = new views::Widget();
    widget->Init(params);
    widget->Show();
    widget->Activate();
    exo::ShellSurface::SetApplicationId(widget->GetNativeWindow(),
                                        window_app_id);
    return widget;
  }

  void TearDown() override {
    launcher_controller_.reset();
    ash::AshTestBase::TearDown();
  }

  ArcAppTest& arc_test() { return arc_test_; }

  Profile* profile() { return &profile_; }

  ChromeLauncherController* controller() { return launcher_controller_.get(); }

  ash::ShelfModel* model() { return model_.get(); }

 private:
  TestingProfile profile_;
  ArcAppTest arc_test_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ash::ShelfModel> model_;
  std::unique_ptr<ChromeLauncherController> launcher_controller_;

  FakeTabletModeController fake_tablet_mode_controller_;
  std::unique_ptr<TabletModeClient> tablet_mode_client_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenuTest);
};

// Verifies that "New Incognito window" menu item in the launcher context
// menu is disabled when Incognito mode is switched off (by a policy).
TEST_F(LauncherContextMenuTest,
       NewIncognitoWindowMenuIsDisabledWhenIncognitoModeOff) {
  const int64_t display_id = GetPrimaryDisplay().id();
  // Initially, "New Incognito window" should be enabled.
  std::unique_ptr<LauncherContextMenu> menu =
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(
      menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  menu = CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  // The item should be disabled.
  ASSERT_TRUE(IsItemPresentInMenu(
      menu.get(), LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_FALSE(
      menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW));
}

// Verifies that "New window" menu item in the launcher context
// menu is disabled when Incognito mode is forced (by a policy).
TEST_F(LauncherContextMenuTest,
       NewWindowMenuIsDisabledWhenIncognitoModeForced) {
  const int64_t display_id = GetPrimaryDisplay().id();
  // Initially, "New window" should be enabled.
  std::unique_ptr<LauncherContextMenu> menu =
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  ASSERT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_NEW_WINDOW));
  EXPECT_TRUE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  menu = CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  ASSERT_TRUE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_NEW_WINDOW));
  EXPECT_FALSE(menu->IsCommandIdEnabled(LauncherContextMenu::MENU_NEW_WINDOW));
}

// Verifies that "Close" is not shown in context menu if no browser window is
// opened.
TEST_F(LauncherContextMenuTest,
       DesktopShellLauncherContextMenuVerifyCloseItem) {
  const int64_t display_id = GetPrimaryDisplay().id();
  std::unique_ptr<LauncherContextMenu> menu =
      CreateLauncherContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  ASSERT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
}

// Verifies context menu and app menu items for ARC app.
TEST_F(LauncherContextMenuTest, ArcLauncherMenusCheck) {
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>(arc_test().fake_apps().begin(),
                                       arc_test().fake_apps().begin() + 1));
  const std::string app_id = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);
  const std::string app_name = arc_test().fake_apps()[0].name;

  controller()->PinAppWithID(app_id);

  const ash::ShelfID shelf_id(app_id);
  const ash::ShelfItem* item = controller()->GetItem(shelf_id);
  ASSERT_TRUE(item);
  EXPECT_EQ(base::UTF8ToUTF16(app_name), item->title);
  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_TRUE(item_delegate->GetAppMenuItems(0 /* event_flags */).empty());

  const int64_t display_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      item_delegate->GetContextMenu(display_id);
  ASSERT_TRUE(menu);

  // ARC app is pinned but not running.
  EXPECT_TRUE(
      IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

  // ARC app is running.
  std::string window_app_id1("org.chromium.arc.1");
  CreateArcWindow(window_app_id1);
  arc_test().app_instance()->SendTaskCreated(1, arc_test().fake_apps()[0],
                                             std::string());

  item_delegate = model()->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  ash::MenuItemList menu_list =
      item_delegate->GetAppMenuItems(0 /* event_flags */);
  ASSERT_EQ(1U, menu_list.size());
  EXPECT_EQ(base::UTF8ToUTF16(app_name), menu_list[0]->label);

  menu = item_delegate->GetContextMenu(display_id);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

  // ARC non-launchable app is running.
  const std::string app_id2 = ArcAppTest::GetAppId(arc_test().fake_apps()[1]);
  const std::string app_name2 = arc_test().fake_apps()[1].name;
  std::string window_app_id2("org.chromium.arc.2");
  CreateArcWindow(window_app_id2);
  arc_test().app_instance()->SendTaskCreated(2, arc_test().fake_apps()[1],
                                             std::string());
  const ash::ShelfID shelf_id2(app_id2);
  const ash::ShelfItem* item2 = controller()->GetItem(shelf_id2);
  ASSERT_TRUE(item2);
  EXPECT_EQ(base::UTF8ToUTF16(app_name2), item2->title);
  ash::ShelfItemDelegate* item_delegate2 =
      model()->GetShelfItemDelegate(shelf_id2);
  ASSERT_TRUE(item_delegate2);

  menu_list = item_delegate2->GetAppMenuItems(0 /* event_flags */);
  ASSERT_EQ(1U, menu_list.size());
  EXPECT_EQ(base::UTF8ToUTF16(app_name2), menu_list[0]->label);

  menu = item_delegate2->GetContextMenu(display_id);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

  // Shelf group context menu.
  std::vector<arc::mojom::ShortcutInfo> shortcuts = arc_test().fake_shortcuts();
  shortcuts[0].intent_uri +=
      ";S.org.chromium.arc.shelf_group_id=arc_test_shelf_group;end";
  arc_test().app_instance()->SendInstallShortcuts(shortcuts);
  const std::string app_id3 =
      arc::ArcAppShelfId("arc_test_shelf_group",
                         ArcAppTest::GetAppId(arc_test().fake_apps()[2]))
          .ToString();

  constexpr int apps_to_test_in_shelf_group = 2;
  const std::string app_name3 = arc_test().fake_apps()[2].name;
  for (uint32_t i = 0; i < apps_to_test_in_shelf_group; ++i) {
    const uint32_t task_id = 3 + i;
    std::string window_app_id3 =
        base::StringPrintf("org.chromium.arc.%d", task_id);
    CreateArcWindow(window_app_id3);
    arc_test().app_instance()->SendTaskCreated(
        task_id, arc_test().fake_apps()[2], shortcuts[0].intent_uri);
    // Set custom name.
    arc_test().app_instance()->SendTaskDescription(
        task_id, GetAppNameInShelfGroup(task_id),
        std::string() /* icon_png_data_as_string */);
    const ash::ShelfID shelf_id3(app_id3);
    const ash::ShelfItem* item3 = controller()->GetItem(shelf_id3);
    ASSERT_TRUE(item3);

    // Validate item label is correct
    EXPECT_EQ(base::UTF8ToUTF16(app_name3), item3->title);

    ash::ShelfItemDelegate* item_delegate3 =
        model()->GetShelfItemDelegate(shelf_id3);
    ASSERT_TRUE(item_delegate3);

    menu = item_delegate3->GetContextMenu(display_id);
    ASSERT_TRUE(menu);

    EXPECT_FALSE(
        IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
    EXPECT_FALSE(
        IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
    EXPECT_TRUE(
        IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

    menu_list = item_delegate3->GetAppMenuItems(0 /* event_flags */);
    ASSERT_EQ(i + 1, menu_list.size());

    // Ensure custom names are set in the app menu items. Note, they are
    // in reverse order, based on activation order.
    for (uint32_t j = 0; j <= i; ++j) {
      EXPECT_EQ(base::UTF8ToUTF16(GetAppNameInShelfGroup(3 + j)),
                menu_list[i - j]->label);
    }
  }
}

TEST_F(LauncherContextMenuTest, ArcDeferredLauncherContextMenuItemCheck) {
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>(arc_test().fake_apps().begin(),
                                       arc_test().fake_apps().begin() + 2));
  const std::string app_id1 = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);
  const std::string app_id2 = ArcAppTest::GetAppId(arc_test().fake_apps()[1]);

  controller()->PinAppWithID(app_id1);

  arc_test().StopArcInstance();

  const ash::ShelfID shelf_id1(app_id1);
  const ash::ShelfID shelf_id2(app_id2);

  EXPECT_TRUE(controller()->GetItem(shelf_id1));
  EXPECT_FALSE(controller()->GetItem(shelf_id2));

  arc::LaunchApp(profile(), app_id1, ui::EF_LEFT_MOUSE_BUTTON);
  arc::LaunchApp(profile(), app_id2, ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(controller()->GetItem(shelf_id1));
  EXPECT_TRUE(controller()->GetItem(shelf_id2));

  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(shelf_id1);
  ASSERT_TRUE(item_delegate);
  std::unique_ptr<ui::MenuModel> menu =
      item_delegate->GetContextMenu(0 /* display_id */);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));

  item_delegate = model()->GetShelfItemDelegate(shelf_id2);
  ASSERT_TRUE(item_delegate);
  menu = item_delegate->GetContextMenu(0 /* display_id */);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(
      IsItemPresentInMenu(menu.get(), LauncherContextMenu::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), LauncherContextMenu::MENU_CLOSE));
}

TEST_F(LauncherContextMenuTest, CommandIdsMatchEnumsForHistograms) {
  // Tests that CommandId enums are not changed as the values are used in
  // histograms.
  EXPECT_EQ(0, LauncherContextMenu::MENU_OPEN_NEW);
  EXPECT_EQ(1, LauncherContextMenu::MENU_CLOSE);
  EXPECT_EQ(2, LauncherContextMenu::MENU_PIN);
  EXPECT_EQ(3, LauncherContextMenu::LAUNCH_TYPE_PINNED_TAB);
  EXPECT_EQ(4, LauncherContextMenu::LAUNCH_TYPE_REGULAR_TAB);
  EXPECT_EQ(5, LauncherContextMenu::LAUNCH_TYPE_FULLSCREEN);
  EXPECT_EQ(6, LauncherContextMenu::LAUNCH_TYPE_WINDOW);
  EXPECT_EQ(7, LauncherContextMenu::MENU_NEW_WINDOW);
  EXPECT_EQ(8, LauncherContextMenu::MENU_NEW_INCOGNITO_WINDOW);
}

TEST_F(LauncherContextMenuTest, ArcContextMenuOptions) {
  // Tests that there are 4 ARC app context menu options. If you're
  // adding a context menu option ensure that you have added the enum to
  // tools/metrics/enums.xml and that you haven't modified the order of the
  // existing enums.
  arc_test().app_instance()->RefreshAppList();
  arc_test().app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>(arc_test().fake_apps().begin(),
                                       arc_test().fake_apps().begin() + 1));
  const std::string app_id = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);

  controller()->PinAppWithID(app_id);
  const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
  ASSERT_TRUE(item);
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<LauncherContextMenu> menu =
      std::make_unique<ArcLauncherContextMenu>(controller(), item, primary_id);

  // Test that there are 4 items in an ARC app context menu.
  EXPECT_EQ(4, menu->GetItemCount());
}

}  // namespace
