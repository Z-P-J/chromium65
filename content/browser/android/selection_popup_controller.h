// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SELECTION_POPUP_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_SELECTION_POPUP_CONTROLLER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/touch_selection/selection_event_type.h"

namespace gfx {
class PointF;
}

namespace content {

class RenderWidgetHostViewAndroid;
struct ContextMenuParams;

class SelectionPopupController : public RenderWidgetHostConnector {
 public:
  SelectionPopupController(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           WebContents* web_contents);

  // RendetWidgetHostConnector implementation.
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

  // Called from native -> java
  void OnSelectionEvent(ui::SelectionEventType event,
                        const gfx::RectF& selection_rect,
                        const gfx::PointF& bound_middle_point);
  void OnSelectionChanged(const std::string& text);
  bool ShowSelectionMenu(const ContextMenuParams& params, int handle_height);
  // |x| and |y| are in physical pixel scale.
  void OnShowUnhandledTapUIIfNeeded(int x_px, int y_px);
  void OnSelectWordAroundCaretAck(bool did_select,
                                  int start_adjust,
                                  int end_adjust);
  std::unique_ptr<ui::TouchHandleDrawable> CreateTouchHandleDrawable();
  void MoveRangeSelectionExtent(const gfx::PointF& extent);

  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent);

 private:
  ~SelectionPopupController() override {}
  base::android::ScopedJavaLocalRef<jobject> GetContext() const;

  JavaObjectWeakGlobalRef java_obj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SELECTION_POPUP_CONTROLLER_H_
