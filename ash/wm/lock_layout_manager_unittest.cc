// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_test_util.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

const int kVirtualKeyboardHeight = 100;

// A login implementation of WidgetDelegate.
class LoginTestWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit LoginTestWidgetDelegate(views::Widget* widget) : widget_(widget) {}
  ~LoginTestWidgetDelegate() override = default;

  // Overridden from WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  bool CanActivate() const override { return true; }
  bool ShouldAdvanceFocusToTopLevelWidget() const override { return true; }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(LoginTestWidgetDelegate);
};

}  // namespace

class LockLayoutManagerTest : public AshTestBase {
 public:
  void SetUp() override {
    // Allow a virtual keyboard (and initialize it per default).
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    Shell::GetPrimaryRootWindowController()->ActivateKeyboard(
        keyboard::KeyboardController::GetInstance());
  }

  void TearDown() override {
    Shell::GetPrimaryRootWindowController()->DeactivateKeyboard(
        keyboard::KeyboardController::GetInstance());
    AshTestBase::TearDown();
  }

  aura::Window* CreateTestLoginWindow(views::Widget::InitParams params,
                                      bool use_delegate) {
    aura::Window* parent =
        Shell::GetPrimaryRootWindowController()->GetContainer(
            ash::kShellWindowId_LockScreenContainer);
    params.parent = parent;
    views::Widget* widget = new views::Widget;
    if (use_delegate)
      params.delegate = new LoginTestWidgetDelegate(widget);
    widget->Init(params);
    widget->Show();
    aura::Window* window = widget->GetNativeView();
    return window;
  }

  // Show or hide the keyboard.
  void ShowKeyboard(bool show) {
    keyboard::KeyboardController* keyboard =
        keyboard::KeyboardController::GetInstance();
    ASSERT_TRUE(keyboard);
    if (show == keyboard->keyboard_visible())
      return;

    if (show) {
      keyboard->ShowKeyboard(false);
      if (keyboard->ui()->GetContentsWindow()->bounds().height() == 0) {
        keyboard->ui()->GetContentsWindow()->SetBounds(
            keyboard::KeyboardBoundsFromRootBounds(
                Shell::GetPrimaryRootWindow()->bounds(),
                kVirtualKeyboardHeight));
      }
    } else {
      keyboard->HideKeyboard(keyboard::KeyboardController::HIDE_REASON_MANUAL);
    }

    DCHECK_EQ(show, keyboard->keyboard_visible());
  }
};

TEST_F(LockLayoutManagerTest, NorwmalWindowBoundsArePreserved) {
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  const gfx::Rect bounds = gfx::Rect(10, 10, 300, 300);
  widget_params.bounds = bounds;
  std::unique_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));
  EXPECT_EQ(bounds.ToString(), window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get());
  window->SetBounds(work_area);

  EXPECT_EQ(work_area.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_NE(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());

  const gfx::Rect bounds2 = gfx::Rect(100, 100, 200, 200);
  window->SetBounds(bounds2);
  EXPECT_EQ(bounds2.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockLayoutManagerTest, MaximizedFullscreenWindowBoundsAreEqualToScreen) {
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  const gfx::Rect bounds = gfx::Rect(10, 10, 300, 300);
  widget_params.bounds = bounds;
  // Maximized TYPE_WINDOW_FRAMELESS windows needs a delegate defined otherwise
  // it won't get initial SetBounds event.
  std::unique_ptr<aura::Window> maximized_window(
      CreateTestLoginWindow(widget_params, true /* use_delegate */));

  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  widget_params.delegate = NULL;
  std::unique_ptr<aura::Window> fullscreen_window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));

  EXPECT_EQ(screen_bounds.ToString(),
            maximized_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(maximized_window.get());
  maximized_window->SetBounds(work_area);

  EXPECT_NE(work_area.ToString(),
            maximized_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            maximized_window->GetBoundsInScreen().ToString());

  work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(fullscreen_window.get());
  fullscreen_window->SetBounds(work_area);
  EXPECT_NE(work_area.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());

  const gfx::Rect bounds2 = gfx::Rect(100, 100, 200, 200);
  maximized_window->SetBounds(bounds2);
  fullscreen_window->SetBounds(bounds2);
  EXPECT_EQ(screen_bounds.ToString(),
            maximized_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());
}

TEST_F(LockLayoutManagerTest, AccessibilityPanel) {
  ash::ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);

  // Set the accessibility panel height.
  int chromevox_panel_height = 45;
  shelf_layout_manager->SetChromeVoxPanelHeight(chromevox_panel_height);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Rect target_bounds = primary_display.bounds();
  target_bounds.Inset(0 /* left */, chromevox_panel_height /* top */,
                      0 /* right */, 0 /* bottom */);

  EXPECT_EQ(target_bounds, window->GetBoundsInScreen());

  // Update the accessibility panel height, and verify the window bounds are
  // updated accordingly.
  chromevox_panel_height = 25;
  shelf_layout_manager->SetChromeVoxPanelHeight(chromevox_panel_height);

  target_bounds = primary_display.bounds();
  target_bounds.Inset(0 /* left */, chromevox_panel_height /* top */,
                      0 /* right */, 0 /* bottom */);

  EXPECT_EQ(target_bounds, window->GetBoundsInScreen());
}

TEST_F(LockLayoutManagerTest, KeyboardBounds) {
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect screen_bounds = primary_display.bounds();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));

  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // When virtual keyboard overscroll is enabled keyboard bounds should not
  // affect window bounds.
  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_ENABLED);
  ShowKeyboard(true);
  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());
  gfx::Rect keyboard_bounds =
      keyboard::KeyboardController::GetInstance()->current_keyboard_bounds();
  EXPECT_NE(keyboard_bounds, gfx::Rect());
  ShowKeyboard(false);

  // When keyboard is hidden make sure that rotating the screen gives 100% of
  // screen size to window.
  // Repro steps for http://crbug.com/401667:
  // 1. Set up login screen defaults: VK override disabled
  // 2. Show/hide keyboard, make sure that no stale keyboard bounds are cached.
  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_DISABLED);
  ShowKeyboard(true);
  ShowKeyboard(false);
  display_manager()->SetDisplayRotation(
      primary_display.id(), display::Display::ROTATE_90,
      display::Display::ROTATION_SOURCE_ACTIVE);
  primary_display = display::Screen::GetScreen()->GetPrimaryDisplay();
  screen_bounds = primary_display.bounds();
  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());
  display_manager()->SetDisplayRotation(
      primary_display.id(), display::Display::ROTATE_0,
      display::Display::ROTATION_SOURCE_ACTIVE);

  // When virtual keyboard overscroll is disabled keyboard bounds do
  // affect window bounds.
  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_DISABLED);
  ShowKeyboard(true);
  keyboard::KeyboardController* keyboard =
      keyboard::KeyboardController::GetInstance();
  primary_display = display::Screen::GetScreen()->GetPrimaryDisplay();
  screen_bounds = primary_display.bounds();
  gfx::Rect target_bounds(screen_bounds);
  target_bounds.set_height(
      target_bounds.height() -
      keyboard->ui()->GetContentsWindow()->bounds().height());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
  ShowKeyboard(false);

  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_NONE);

  keyboard->SetContainerType(keyboard::ContainerType::FLOATING);
  ShowKeyboard(true);
  primary_display = display::Screen::GetScreen()->GetPrimaryDisplay();
  screen_bounds = primary_display.bounds();
  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());
  ShowKeyboard(false);
}

TEST_F(LockLayoutManagerTest, MultipleMonitors) {
  UpdateDisplay("300x400,400x500");
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      ui::mojom::kResizeBehaviorCanMaximize);

  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));

  // Maximize the window with as the restore bounds is inside 2nd display but
  // lock container windows are always on primary display.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get());
  window->SetBounds(work_area);
  // Usually work_area takes Shelf into account but that doesn't affect
  // LockScreen container windows.
  EXPECT_NE(work_area.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockLayoutManagerTest, AccessibilityPanelWithMultipleMonitors) {
  UpdateDisplay("300x400,400x500");

  ash::ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);

  int chromevox_panel_height = 45;
  shelf_layout_manager->SetChromeVoxPanelHeight(chromevox_panel_height);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      ui::mojom::kResizeBehaviorCanMaximize);

  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0, chromevox_panel_height, 0, 0);
  EXPECT_EQ(target_bounds, window->GetBoundsInScreen());

  // Restore window with bounds in the second display, the window should be
  // shown in the primary display.
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(target_bounds, window->GetBoundsInScreen());

  // Force the window to secondary display - accessibility panel is only set
  // for the primary shelf, so it should not influence the screen bounds.
  window->SetBoundsInScreen(gfx::Rect(0, 0, 30, 40), GetSecondaryDisplay());

  target_bounds = gfx::Rect(300, 0, 400, 500);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(target_bounds, window->GetBoundsInScreen());
}

}  // namespace ash
