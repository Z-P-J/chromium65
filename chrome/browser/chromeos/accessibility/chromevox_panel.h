// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include <stdint.h>

#include "ash/shell_observer.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

// Displays spoken feedback UI controls for the ChromeVox component extension
// in a small panel at the top of the display. Insets the display work area
// when visible.
// TODO(jamescook): Convert the ash::ShellObserver to a mojo interface for mash.
class ChromeVoxPanel : public views::WidgetDelegate,
                       public display::DisplayObserver,
                       public ash::ShellObserver {
 public:
  ChromeVoxPanel(content::BrowserContext* browser_context,
                 bool for_blocked_user_session);
  ~ChromeVoxPanel() override;

  aura::Window* GetRootWindow();

  void Close();
  void UpdatePanelHeight();
  void ResetPanelHeight();

  // WidgetDelegate overrides.
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  void DeleteDelegate() override;
  views::View* GetContentsView() override;

  // DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& new_display) override {}
  void OnDisplayRemoved(const display::Display& old_display) override {}
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ash::ShellObserver overrides:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* root_window) override;

  bool for_blocked_user_session() const { return for_blocked_user_session_; }

 private:
  class ChromeVoxPanelWebContentsObserver;

  // Methods indirectly invoked by the component extension.
  void DidFirstVisuallyNonEmptyPaint();
  void EnterFullscreen();
  void ExitFullscreen();
  void DisableSpokenFeedback();
  void Focus();

  void UpdateWidgetBounds();

  // Sends the height of the ChromeVox panel, which takes away space from the
  // available window manager work area at the top of the screen.
  void SendPanelHeightToAsh(int panel_height);

  views::Widget* widget_;
  std::unique_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;
  views::View* web_view_;
  bool panel_fullscreen_;
  const bool for_blocked_user_session_;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_CHROMEVOX_PANEL_H_
