// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/security_state/core/security_state.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif

class CommandUpdater;
class LocationBarView;
class OmniboxClient;
class OmniboxPopupContentsView;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RenderText;
}

namespace ui {
class OSExchangeData;
}  // namespace ui

// Views-implementation of OmniboxView.
class OmniboxViewViews : public OmniboxView,
                         public views::Textfield,
#if defined(OS_CHROMEOS)
                         public chromeos::input_method::InputMethodManager::
                             CandidateWindowObserver,
#endif
                         public views::TextfieldController,
                         public ui::CompositorObserver {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  OmniboxViewViews(OmniboxEditController* controller,
                   std::unique_ptr<OmniboxClient> client,
                   CommandUpdater* command_updater,
                   bool popup_window_mode,
                   LocationBarView* location_bar,
                   const gfx::FontList& font_list);
  ~OmniboxViewViews() override;

  // Initialize, create the underlying views, etc.
  void Init();

  // Exposes the RenderText for tests.
#if defined(UNIT_TEST)
  gfx::RenderText* GetRenderText() {
    return views::Textfield::GetRenderText();
  }
#endif

  // For use when switching tabs, this saves the current state onto the tab so
  // that it can be restored during a later call to Update().
  void SaveStateToTab(content::WebContents* tab);

  // Called when the window's active tab changes.
  void OnTabChanged(const content::WebContents* web_contents);

  // Called to clear the saved state for |web_contents|.
  void ResetTabState(content::WebContents* web_contents);

  // OmniboxView:
  void Update() override;
  base::string16 GetText() const override;
  using OmniboxView::SetUserText;
  void SetUserText(const base::string16& text,
                   bool update_popup) override;
  void EnterKeywordModeForDefaultSearchProvider() override;
  void GetSelectionBounds(base::string16::size_type* start,
                          base::string16::size_type* end) const override;
  void SelectAll(bool reversed) override;
  void RevertAll() override;
  void SetFocus() override;
  int GetTextWidth() const override;
  bool IsImeComposing() const override;

  // views::Textfield:
  gfx::Size GetMinimumSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  ui::TextInputType GetTextInputType() const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, FriendlyAccessibleLabel);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, MaintainCursorAfterFocusCycle);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, OnBlur);

  // Update the field with |text| and set the selection.
  void SetTextAndSelectedRange(const base::string16& text,
                               const gfx::Range& range);

  // Returns the selected text.
  base::string16 GetSelectedText() const;

  // Paste text from the clipboard into the omnibox.
  // Textfields implementation of Paste() pastes the contents of the clipboard
  // as is. We want to strip whitespace and other things (see GetClipboardText()
  // for details). The function invokes OnBefore/AfterPossibleChange() as
  // necessary.
  void OnPaste();

  // Handle keyword hint tab-to-search and tabbing through dropdown results.
  bool HandleEarlyTabActions(const ui::KeyEvent& event);

  // Updates |security_level_| based on the toolbar model's current value.
  void UpdateSecurityLevel();

  void ClearAccessibilityLabel();

  // OmniboxView:
  void SetWindowTextAndCaretPos(const base::string16& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  bool IsSelectAll() const override;
  bool DeleteAtEndPressed() override;
  void UpdatePopup() override;
  void ApplyCaretVisibility() override;
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  bool OnInlineAutocompleteTextMaybeChanged(const base::string16& display_text,
                                            size_t user_text_length) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText() override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  int GetWidth() const override;
  bool IsImeShowingPopup() const override;
  void ShowImeIfNeeded() override;
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override;
  void SetEmphasis(bool emphasize, const gfx::Range& range) override;
  void UpdateSchemeStyle(const gfx::Range& range) override;

  // views::Textfield:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnFocus() override;
  void OnBlur() override;
  bool IsCommandIdEnabled(int command_id) const override;
  base::string16 GetSelectionClipboardText() const override;
  void DoInsertChar(base::char16 ch) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void ExecuteTextEditCommand(ui::TextEditCommand command) override;

  // chromeos::input_method::InputMethodManager::CandidateWindowObserver:
#if defined(OS_CHROMEOS)
  void CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) override;
  void CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) override;
#endif

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnBeforeUserAction(views::Textfield* sender) override;
  void OnAfterUserAction(views::Textfield* sender) override;
  void OnAfterCutOrCopy(ui::ClipboardType clipboard_type) override;
  void OnWriteDragData(ui::OSExchangeData* data) override;
  void OnGetDragOperationsForTextfield(int* drag_operations) override;
  void AppendDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  int OnDrop(const ui::OSExchangeData& data) override;
  void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) override;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override;
  void OnCompositingChildResizing(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (smaller font size). This is used for popups.
  bool popup_window_mode_;

  std::unique_ptr<OmniboxPopupContentsView> popup_view_;

  security_state::SecurityLevel security_level_;

  // Selection persisted across temporary text changes, like popup suggestions.
  gfx::Range saved_temporary_selection_;

  // Holds the user's selection across focus changes.  There is only a saved
  // selection if this range IsValid().
  gfx::Range saved_selection_for_focus_change_;

  // Tracking state before and after a possible change.
  State state_before_change_;
  bool ime_composing_before_change_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;

  // |location_bar_view_| can be NULL in tests.
  LocationBarView* location_bar_view_;

  // True if the IME candidate window is open. When this is true, we want to
  // avoid showing the popup. So far, the candidate window is detected only
  // on Chrome OS.
  bool ime_candidate_window_open_;

  // Should we select all the text when we see the mouse button get released?
  // We select in response to a click that focuses the omnibox, but we defer
  // until release, setting this variable back to false if we saw a drag, to
  // allow the user to select just a portion of the text.
  bool select_all_on_mouse_release_;

  // Indicates if we want to select all text in the omnibox when we get a
  // GESTURE_TAP. We want to select all only when the textfield is not in focus
  // and gets a tap. So we use this variable to remember focus state before tap.
  bool select_all_on_gesture_tap_;

  // The time of the first character insert operation that has not yet been
  // painted. Used to measure omnibox responsiveness with a histogram.
  base::TimeTicks insert_char_time_;

  // The state machine for logging the Omnibox.CharTypedToRepaintLatency
  // histogram.
  enum {
    NOT_ACTIVE,           // Not currently tracking a char typed event.
    CHAR_TYPED,           // Character was typed.
    ON_PAINT_CALLED,      // Character was typed and OnPaint() called.
    COMPOSITING_COMMIT,   // Compositing was committed after OnPaint().
    COMPOSITING_STARTED,  // Compositing was started.
  } latency_histogram_state_;

  // The currently selected match, if any, with additional labelling text
  // such as the document title and the type of search, for example:
  // "Google https://google.com location from bookmark", or
  // "cats are liquid search suggestion".
  base::string16 friendly_suggestion_text_;

  // The number of added labelling characters before editable text begins.
  // For example,  "Google https://google.com location from history",
  // this is set to 7 (the length of "Google ").
  int friendly_suggestion_text_prefix_length_;

  ScopedObserver<ui::Compositor, ui::CompositorObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
