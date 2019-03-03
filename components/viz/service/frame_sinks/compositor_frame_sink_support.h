// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/referenced_surface_tracker.h"
#include "components/viz/service/frame_sinks/surface_resource_holder.h"
#include "components/viz/service/frame_sinks/surface_resource_holder_client.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"

namespace viz {

class FrameSinkManagerImpl;
class Surface;
class SurfaceManager;

class VIZ_SERVICE_EXPORT CompositorFrameSinkSupport
    : public BeginFrameObserver,
      public SurfaceResourceHolderClient,
      public SurfaceClient,
      public CapturableFrameSink,
      public mojom::CompositorFrameSink {
 public:
  using AggregatedDamageCallback =
      base::RepeatingCallback<void(const LocalSurfaceId& local_surface_id,
                                   const gfx::Rect& damage_rect)>;

  static const uint64_t kFrameIndexStart = 2;

  CompositorFrameSinkSupport(mojom::CompositorFrameSinkClient* client,
                             FrameSinkManagerImpl* frame_sink_manager,
                             const FrameSinkId& frame_sink_id,
                             bool is_root,
                             bool needs_sync_tokens);
  ~CompositorFrameSinkSupport() override;

  const FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const SurfaceId& current_surface_id() const { return current_surface_id_; }

  const LocalSurfaceId& local_surface_id() const {
    return current_surface_id_.local_surface_id();
  }

  FrameSinkManagerImpl* frame_sink_manager() { return frame_sink_manager_; }

  // The provided callback will be run every time a surface owned by this object
  // or one of its descendents is determined to be damaged at aggregation time.
  void SetAggregatedDamageCallback(AggregatedDamageCallback callback);

  // Sets callback called on destruction.
  void SetDestructionCallback(base::OnceClosure callback);

  // This allows the FrameSinkManagerImpl to pass a BeginFrameSource to use.
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source);

  // SurfaceClient implementation.
  void OnSurfaceActivated(Surface* surface) override;
  void RefResources(
      const std::vector<TransferableResource>& resources) override;
  void UnrefResources(const std::vector<ReturnedResource>& resources) override;
  void ReturnResources(const std::vector<ReturnedResource>& resources) override;
  void ReceiveFromChild(
      const std::vector<TransferableResource>& resources) override;

  // mojom::CompositorFrameSink implementation.
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SetWantsAnimateOnlyBeginFrames() override;
  void DidNotProduceFrame(const BeginFrameAck& ack) override;
  void SubmitCompositorFrame(const LocalSurfaceId& local_surface_id,
                             CompositorFrame frame,
                             mojom::HitTestRegionListPtr hit_test_region_list,
                             uint64_t submit_time) override;

  void EvictCurrentSurface();

  // Submits a new CompositorFrame to |local_surface_id|. If |local_surface_id|
  // hasn't been submitted to before then a new Surface will be created for it.
  // Returns false if |frame| was rejected due to invalid data.
  // TODO(kylechar): Merge the two SubmitCompositorFrame() methods.
  bool SubmitCompositorFrame(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      mojom::HitTestRegionListPtr hit_test_region_list = nullptr);

  // CapturableFrameSink implementation.
  void AttachCaptureClient(CapturableFrameSink::Client* client) override;
  void DetachCaptureClient(CapturableFrameSink::Client* client) override;
  gfx::Size GetSurfaceSize() override;
  void RequestCopyOfSurface(
      std::unique_ptr<CopyOutputRequest> request) override;

  Surface* GetCurrentSurfaceForTesting();

 private:
  friend class FrameSinkManagerTest;

  // Updates surface references using |active_referenced_surfaces| from the most
  // recent CompositorFrame. This will add and remove top-level root references
  // if |is_root_| is true and |local_surface_id| has changed. Modifies surface
  // references stored in SurfaceManager.
  void UpdateSurfaceReferences(
      const LocalSurfaceId& local_surface_id,
      const std::vector<SurfaceId>& active_referenced_surfaces);

  // Creates a surface reference from the top-level root to |surface_id|.
  SurfaceReference MakeTopLevelRootReference(const SurfaceId& surface_id);

  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 base::TimeTicks time,
                                 base::TimeDelta refresh,
                                 uint32_t flags);

  // BeginFrameObserver implementation.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

  void UpdateNeedsBeginFramesInternal();
  Surface* CreateSurface(const SurfaceInfo& surface_info);

  void OnAggregatedDamage(const LocalSurfaceId& local_surface_id,
                          const gfx::Rect& damage_rect,
                          const CompositorFrame& frame) const;

  mojom::CompositorFrameSinkClient* const client_;

  FrameSinkManagerImpl* const frame_sink_manager_;
  SurfaceManager* const surface_manager_;

  const FrameSinkId frame_sink_id_;
  SurfaceId current_surface_id_;

  // If this contains a value then a surface reference from the top-level root
  // to SurfaceId(frame_sink_id_, referenced_local_surface_id_.value()) was
  // added. This will not contain a value if |is_root_| is false.
  base::Optional<LocalSurfaceId> referenced_local_surface_id_;

  SurfaceResourceHolder surface_resource_holder_;

  // Counts the number of CompositorFrames that have been submitted and have not
  // yet received an ACK.
  int ack_pending_count_ = 0;
  std::vector<ReturnedResource> surface_returned_resources_;

  // The begin frame source being observered. Null if none.
  BeginFrameSource* begin_frame_source_ = nullptr;

  // The last begin frame args generated by the begin frame source.
  BeginFrameArgs last_begin_frame_args_;

  // Whether a request for begin frames has been issued.
  bool needs_begin_frame_ = false;

  // Whether or not a frame observer has been added.
  bool added_frame_observer_ = false;

  bool wants_animate_only_begin_frames_ = false;

  const bool is_root_;
  const bool needs_sync_tokens_;

  // A callback that will be run at the start of the destructor if set.
  base::OnceClosure destruction_callback_;

  // TODO(crbug.com/754872): Remove once tab capture has moved into VIZ.
  AggregatedDamageCallback aggregated_damage_callback_;

  uint64_t last_frame_index_ = kFrameIndexStart;

  // The video capture clients hooking into this instance to observe frame
  // begins and damage, and then make CopyOutputRequests on the appropriate
  // frames.
  std::vector<CapturableFrameSink::Client*> capture_clients_;

  base::WeakPtrFactory<CompositorFrameSinkSupport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkSupport);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_COMPOSITOR_FRAME_SINK_SUPPORT_H_
