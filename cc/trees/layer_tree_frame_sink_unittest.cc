// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_frame_sink.h"

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestLayerTreeFrameSink : public LayerTreeFrameSink {
 public:
  explicit TestLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : LayerTreeFrameSink(std::move(context_provider),
                           std::move(worker_context_provider),
                           std::move(compositor_task_runner),
                           nullptr,
                           nullptr) {}

  void SubmitCompositorFrame(viz::CompositorFrame frame) override {
    client_->DidReceiveCompositorFrameAck();
  }
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override {}
};

TEST(LayerTreeFrameSinkTest, ContextLossInformsClient) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::CreateWorker();
  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  TestLayerTreeFrameSink layer_tree_frame_sink(provider, worker_provider,
                                               task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_TRUE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_TRUE(layer_tree_frame_sink.HasClient());

  // Verify DidLoseLayerTreeFrameSink callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_layer_tree_frame_sink_called());
  layer_tree_frame_sink.context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  layer_tree_frame_sink.context_provider()->ContextGL()->Flush();
  EXPECT_TRUE(client.did_lose_layer_tree_frame_sink_called());
}

TEST(LayerTreeFrameSinkTest, ContextLossFailsBind) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::CreateWorker();

  // Lose the context so BindToClient fails.
  context_provider->UnboundTestContext3d()->set_context_lost(true);

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  TestLayerTreeFrameSink layer_tree_frame_sink(context_provider,
                                               worker_provider, task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_FALSE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());
}

TEST(LayerTreeFrameSinkTest, WorkerContextLossInformsClient) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::CreateWorker();
  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  TestLayerTreeFrameSink layer_tree_frame_sink(provider, worker_provider,
                                               task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_TRUE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_TRUE(layer_tree_frame_sink.HasClient());

  // Verify DidLoseLayerTreeFrameSink callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_layer_tree_frame_sink_called());
  {
    viz::RasterContextProvider::ScopedRasterContextLock context_lock(
        layer_tree_frame_sink.worker_context_provider());
    context_lock.RasterInterface()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    context_lock.RasterInterface()->Flush();
  }
  task_runner->RunPendingTasks();
  EXPECT_TRUE(client.did_lose_layer_tree_frame_sink_called());
}

TEST(LayerTreeFrameSinkTest, WorkerContextLossFailsBind) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_provider =
      TestContextProvider::CreateWorker();

  // Lose the context so BindToClient fails.
  worker_provider->UnboundTestContext3d()->set_context_lost(true);

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  TestLayerTreeFrameSink layer_tree_frame_sink(context_provider,
                                               worker_provider, task_runner);
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());

  FakeLayerTreeFrameSinkClient client;
  EXPECT_FALSE(layer_tree_frame_sink.BindToClient(&client));
  EXPECT_FALSE(layer_tree_frame_sink.HasClient());
}

}  // namespace
}  // namespace cc
