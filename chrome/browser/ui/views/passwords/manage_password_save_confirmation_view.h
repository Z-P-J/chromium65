// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SAVE_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SAVE_CONFIRMATION_VIEW_H_

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_delegate_view_base.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

// A view confirming to the user that a password was saved and offering a link
// to the Google account manager.
class ManagePasswordSaveConfirmationView
    : public ManagePasswordsBubbleDelegateViewBase,
      public views::StyledLabelListener {
 public:
  explicit ManagePasswordSaveConfirmationView(
      content::WebContents* web_contents,
      views::View* anchor_view,
      const gfx::Point& anchor_point,
      DisplayReason reason);
  ~ManagePasswordSaveConfirmationView() override;

 private:
  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // LocationBarBubbleDelegateView:
  int GetDialogButtons() const override;
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordSaveConfirmationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SAVE_CONFIRMATION_VIEW_H_