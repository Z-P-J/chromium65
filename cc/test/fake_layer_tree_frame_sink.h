// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_
#define CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"

namespace viz {
class BeginFrameSource;
}

namespace cc {

class FakeLayerTreeFrameSink : public LayerTreeFrameSink {
 public:
  class Builder {
   public:
    Builder();
    ~Builder();

    // Build should only be called once. After calling Build, the Builder is
    // no longer valid and should not be used (other than to destroy it).
    std::unique_ptr<FakeLayerTreeFrameSink> Build();

    // Calls a function on both the compositor and worker context.
    template <typename... Args>
    Builder& AllContexts(void (TestWebGraphicsContext3D::*fn)(Args...),
                         Args... args) {
      DCHECK(compositor_context_provider_);
      DCHECK(worker_context_provider_);
      (compositor_context_provider_->UnboundTestContext3d()->*fn)(
          std::forward<Args>(args)...);
      (worker_context_provider_->UnboundTestContext3d()->*fn)(
          std::forward<Args>(args)...);

      return *this;
    }

   private:
    scoped_refptr<TestContextProvider> compositor_context_provider_;
    scoped_refptr<TestContextProvider> worker_context_provider_;
  };

  ~FakeLayerTreeFrameSink() override;

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d() {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        TestContextProvider::Create(), TestContextProvider::CreateWorker()));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d(
      scoped_refptr<TestContextProvider> context_provider) {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        std::move(context_provider), TestContextProvider::CreateWorker()));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d(
      scoped_refptr<TestContextProvider> context_provider,
      scoped_refptr<TestContextProvider> worker_context_provider) {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        std::move(context_provider), std::move(worker_context_provider)));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d(
      std::unique_ptr<TestWebGraphicsContext3D> context) {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        TestContextProvider::Create(std::move(context)),
        TestContextProvider::CreateWorker()));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3dForGpuRasterization(
      int max_msaa_samples = 0) {
    return Builder()
        .AllContexts(&TestWebGraphicsContext3D::set_gpu_rasterization, true)
        .AllContexts(&TestWebGraphicsContext3D::SetMaxSamples, max_msaa_samples)
        .Build();
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> CreateSoftware() {
    return base::WrapUnique(new FakeLayerTreeFrameSink(nullptr, nullptr));
  }

  // LayerTreeFrameSink implementation.
  void SubmitCompositorFrame(viz::CompositorFrame frame) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  bool BindToClient(LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;

  viz::CompositorFrame* last_sent_frame() { return last_sent_frame_.get(); }
  size_t num_sent_frames() { return num_sent_frames_; }

  LayerTreeFrameSinkClient* client() { return client_; }

  const std::vector<viz::TransferableResource>& resources_held_by_parent() {
    return resources_held_by_parent_;
  }

  gfx::Rect last_swap_rect() const { return last_swap_rect_; }

  void ReturnResourcesHeldByParent();

 protected:
  FakeLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider);

  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
  TestSharedBitmapManager test_shared_bitmap_manager_;

  std::unique_ptr<viz::CompositorFrame> last_sent_frame_;
  size_t num_sent_frames_ = 0;
  std::vector<viz::TransferableResource> resources_held_by_parent_;
  gfx::Rect last_swap_rect_;

 private:
  void DidReceiveCompositorFrameAck();

  std::unique_ptr<viz::BeginFrameSource> begin_frame_source_;
  base::WeakPtrFactory<FakeLayerTreeFrameSink> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_
