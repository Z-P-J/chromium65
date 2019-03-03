// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"
#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"
#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/input/input_event_stream_validator.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/widget.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/input_event_ack_source.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ui {
class LatencyInfo;
struct DidOverscrollParams;
}  // namespace ui

namespace content {

class InputDispositionHandler;

class CONTENT_EXPORT InputRouterImplClient : public InputRouterClient {
 public:
  virtual mojom::WidgetInputHandler* GetWidgetInputHandler() = 0;
  virtual void OnImeCancelComposition() = 0;
  virtual void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds) = 0;
};

// A default implementation for browser input event routing.
class CONTENT_EXPORT InputRouterImpl
    : public InputRouter,
      public GestureEventQueueClient,
      public FlingControllerClient,
      public MouseWheelEventQueueClient,
      public PassthroughTouchEventQueueClient,
      public TouchpadTapSuppressionControllerClient,
      public mojom::WidgetInputHandlerHost {
 public:
  InputRouterImpl(InputRouterImplClient* client,
                  InputDispositionHandler* disposition_handler,
                  const Config& config);
  ~InputRouterImpl() override;

  // InputRouter
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event) override;
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override;
  void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event) override;
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override;
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override;
  bool HasPendingEvents() const override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  void SetFrameTreeNodeId(int frame_tree_node_id) override;
  void SetForceEnableZoom(bool enabled) override;
  cc::TouchAction AllowedTouchAction() override;
  void BindHost(mojom::WidgetInputHandlerHostRequest request,
                bool frame_handler) override;
  void ProgressFling(base::TimeTicks current_time) override;
  void StopFling() override;

  // InputHandlerHost impl
  void CancelTouchTimeout() override;
  void SetWhiteListedTouchAction(cc::TouchAction touch_action,
                                 uint32_t unique_touch_event_id,
                                 InputEventAckState state) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void DidStopFlinging() override;
  void ImeCancelComposition() override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  friend class InputRouterImplTest;

  // Keeps track of last position of touch points and sets MovementXY for them.
  void SetMovementXYForTouchPoints(blink::WebTouchEvent* event);

  // TouchpadTapSuppressionControllerClient
  void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& mouse_event) override;

  // PassthroughTouchEventQueueClient
  void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& touch_event) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override;
  void OnFilteringTouchEvent(const blink::WebTouchEvent& touch_event) override;

  // GestureEventFilterClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& gesture_event) override;
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result) override;

  // FlingControllerClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override;
  void SetNeedsBeginFrameForFlingProgress() override;

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& touch_event) override;
  void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                            InputEventAckSource ack_source,
                            InputEventAckState ack_result) override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;

  void FilterAndSendWebInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info,
      mojom::WidgetInputHandler::DispatchEventCallback callback);

  void KeyboardEventHandled(
      const NativeWebKeyboardEventWithLatencyInfo& event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll,
      const base::Optional<cc::TouchAction>& touch_action);
  void MouseEventHandled(
      const MouseEventWithLatencyInfo& event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll,
      const base::Optional<cc::TouchAction>& touch_action);
  void TouchEventHandled(
      const TouchEventWithLatencyInfo& touch_event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll,
      const base::Optional<cc::TouchAction>& touch_action);
  void GestureEventHandled(
      const GestureEventWithLatencyInfo& gesture_event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll,
      const base::Optional<cc::TouchAction>& touch_action);
  void MouseWheelEventHandled(
      const MouseWheelEventWithLatencyInfo& event,
      InputEventAckSource source,
      const ui::LatencyInfo& latency,
      InputEventAckState state,
      const base::Optional<ui::DidOverscrollParams>& overscroll,
      const base::Optional<cc::TouchAction>& touch_action);

  // IPC message handlers
  void OnHasTouchEventHandlers(bool has_handlers);
  void OnSetTouchAction(cc::TouchAction touch_action);

  // Called when a touch timeout-affecting bit has changed, in turn toggling the
  // touch ack timeout feature of the |touch_event_queue_| as appropriate. Input
  // to that determination includes current view properties and the allowed
  // touch action. Note that this will only affect platforms that have a
  // non-zero touch timeout configuration.
  void UpdateTouchAckTimeoutEnabled();

  InputRouterImplClient* client_;
  InputDispositionHandler* disposition_handler_;
  int frame_tree_node_id_;

  // Whether there are any active flings in the renderer. As the fling
  // end notification is asynchronous, we use a count rather than a boolean
  // to avoid races in bookkeeping when starting a new fling.
  int active_renderer_fling_count_;

  // Whether the TouchScrollStarted event has been sent for the current
  // gesture scroll yet.
  bool touch_scroll_started_sent_;

  bool wheel_scroll_latching_enabled_;
  MouseWheelEventQueue wheel_event_queue_;
  PassthroughTouchEventQueue touch_event_queue_;
  GestureEventQueue gesture_event_queue_;
  TouchActionFilter touch_action_filter_;
  InputEventStreamValidator input_stream_validator_;
  InputEventStreamValidator output_stream_validator_;

  float device_scale_factor_;

  // Last touch position relative to screen. Used to compute movementX/Y.
  base::flat_map<int, gfx::Point> global_touch_position_;

  // The host binding associated with the widget input handler from
  // the widget.
  mojo::Binding<mojom::WidgetInputHandlerHost> host_binding_;

  // The host binding associated with the widget input handler from
  // the frame.
  mojo::Binding<mojom::WidgetInputHandlerHost> frame_host_binding_;

  base::WeakPtr<InputRouterImpl> weak_this_;
  base::WeakPtrFactory<InputRouterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputRouterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_IMPL_H_
