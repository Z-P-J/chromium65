// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_view.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <memory>

#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

gfx::Rect BufferRectFromScreenRect(
    const gfx::Transform& screen_to_buffer_transform,
    const gfx::Size& buffer_size,
    const gfx::Rect& screen_rect) {
  gfx::Rect buffer_rect = cc::MathUtil::MapEnclosingClippedRect(
      screen_to_buffer_transform, screen_rect);
  buffer_rect.Intersect(gfx::Rect(buffer_size));
  return buffer_rect;
}

}  // namespace

FastInkView::ScopedPaint::ScopedPaint(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    const gfx::Transform& screen_to_buffer_transform,
    const gfx::Rect& screen_rect)
    : gpu_memory_buffer_(gpu_memory_buffer),
      buffer_rect_(BufferRectFromScreenRect(screen_to_buffer_transform,
                                            gpu_memory_buffer->GetSize(),
                                            screen_rect)),
      canvas_(buffer_rect_.size(), 1.0f, false) {
  canvas_.Translate(-buffer_rect_.OffsetFromOrigin());
  canvas_.Transform(screen_to_buffer_transform);
}

FastInkView::ScopedPaint::~ScopedPaint() {
  if (buffer_rect_.IsEmpty())
    return;

  {
    TRACE_EVENT0("ui", "FastInkView::ScopedPaint::Map");

    if (!gpu_memory_buffer_->Map()) {
      LOG(ERROR) << "Failed to map GPU memory buffer";
      return;
    }
  }

  // Copy result to GPU memory buffer. This is effectively a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT1("ui", "FastInkView::ScopedPaint::Copy", "buffer_rect",
                 buffer_rect_.ToString());

    uint8_t* data = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    int stride = gpu_memory_buffer_->stride(0);
    canvas_.GetBitmap().readPixels(
        SkImageInfo::MakeN32Premul(buffer_rect_.width(), buffer_rect_.height()),
        data + buffer_rect_.y() * stride + buffer_rect_.x() * 4, stride, 0, 0);
  }

  {
    TRACE_EVENT0("ui", "FastInkView::UpdateBuffer::Unmap");

    // Unmap to flush writes to buffer.
    gpu_memory_buffer_->Unmap();
  }
}

struct FastInkView::Resource {
  Resource() = default;
  ~Resource() {
    // context_provider might be null in unit tests when ran with --mash
    // TODO(kaznacheev) Have MASH provide a context provider for tests
    // when crbug/772562 is fixed
    if (!context_provider)
      return;
    gpu::gles2::GLES2Interface* gles2 = context_provider->ContextGL();
    if (sync_token.HasData())
      gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    if (texture)
      gles2->DeleteTextures(1, &texture);
    if (image)
      gles2->DestroyImageCHROMIUM(image);
  }
  scoped_refptr<viz::ContextProvider> context_provider;
  uint32_t texture = 0;
  uint32_t image = 0;
  gpu::Mailbox mailbox;
  gpu::SyncToken sync_token;
  bool damaged = true;
};

class FastInkView::LayerTreeFrameSinkHolder
    : public cc::LayerTreeFrameSinkClient,
      public ash::ShellObserver {
 public:
  LayerTreeFrameSinkHolder(FastInkView* view,
                           std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
      : view_(view), frame_sink_(std::move(frame_sink)) {
    frame_sink_->BindToClient(this);
  }
  ~LayerTreeFrameSinkHolder() override {
    if (frame_sink_)
      frame_sink_->DetachFromClient();
    if (shell_)
      shell_->RemoveShellObserver(this);
  }

  // Delete frame sink after having reclaimed all exported resources.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // crbug.com/765763
  static void DeleteWhenLastResourceHasBeenReclaimed(
      std::unique_ptr<LayerTreeFrameSinkHolder> holder) {
    if (holder->last_frame_size_in_pixels_.IsEmpty()) {
      // Delete sink holder immediately if no frame has been submitted.
      DCHECK(holder->exported_resources_.empty());
      return;
    }

    // Submit an empty frame to ensure that pending release callbacks will be
    // processed in a finite amount of time.
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.source_id =
        viz::BeginFrameArgs::kManualSourceId;
    frame.metadata.begin_frame_ack.sequence_number =
        viz::BeginFrameArgs::kStartingFrameNumber;
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.device_scale_factor =
        holder->last_frame_device_scale_factor_;
    std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
    pass->SetNew(1, gfx::Rect(holder->last_frame_size_in_pixels_),
                 gfx::Rect(holder->last_frame_size_in_pixels_),
                 gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    holder->frame_sink_->SubmitCompositorFrame(std::move(frame));

    // Delete sink holder immediately if not waiting for exported resources to
    // be reclaimed.
    if (holder->exported_resources_.empty())
      return;

    ash::Shell* shell = ash::Shell::Get();
    holder->shell_ = shell;
    holder->view_ = nullptr;

    // If we have exported resources to reclaim then extend the lifetime of
    // holder by adding it as a shell observer. The holder will delete itself
    // when shell shuts down or when all exported resources have been reclaimed.
    shell->AddShellObserver(holder.release());
  }

  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             viz::ResourceId resource_id,
                             std::unique_ptr<Resource> resource) {
    exported_resources_[resource_id] = std::move(resource);
    last_frame_size_in_pixels_ = frame.size_in_pixels();
    last_frame_device_scale_factor_ = frame.metadata.device_scale_factor;
    frame_sink_->SubmitCompositorFrame(std::move(frame));
  }

  void DamageExportedResources() {
    for (auto& entry : exported_resources_)
      entry.second->damaged = true;
  }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {
    for (auto& entry : resources) {
      auto it = exported_resources_.find(entry.id);
      DCHECK(it != exported_resources_.end());
      std::unique_ptr<Resource> resource = std::move(it->second);
      exported_resources_.erase(it);
      resource->sync_token = entry.sync_token;
      if (view_ && !entry.lost)
        view_->ReclaimResource(std::move(resource));
    }

    if (shell_ && exported_resources_.empty())
      ScheduleDelete();
  }
  void SetTreeActivationCallback(const base::Closure& callback) override {}
  void DidReceiveCompositorFrameAck() override {
    if (view_)
      view_->DidReceiveCompositorFrameAck();
  }
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 base::TimeTicks time,
                                 base::TimeDelta refresh,
                                 uint32_t flags) override {}
  void DidDiscardCompositorFrame(uint32_t presentation_token) override {}
  void DidLoseLayerTreeFrameSink() override {
    exported_resources_.clear();
    if (shell_)
      ScheduleDelete();
  }
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

  // Overridden from ash::ShellObserver:
  void OnShellDestroyed() override {
    shell_->RemoveShellObserver(this);
    shell_ = nullptr;
    // Make sure frame sink never outlives the shell.
    frame_sink_->DetachFromClient();
    frame_sink_.reset();
    ScheduleDelete();
  }

 private:
  void ScheduleDelete() {
    if (delete_pending_)
      return;
    delete_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  FastInkView* view_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;
  base::flat_map<viz::ResourceId, std::unique_ptr<Resource>>
      exported_resources_;
  gfx::Size last_frame_size_in_pixels_;
  float last_frame_device_scale_factor_ = 1.0f;
  ash::Shell* shell_ = nullptr;
  bool delete_pending_ = false;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeFrameSinkHolder);
};

FastInkView::FastInkView(aura::Window* container) : weak_ptr_factory_(this) {
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "FastInkOverlay";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = container;
  params.layer_type = ui::LAYER_SOLID_COLOR;

  gfx::Rect screen_bounds = container->GetRootWindow()->GetBoundsInScreen();
  widget_->Init(params);
  widget_->Show();
  widget_->SetContentsView(this);
  widget_->SetBounds(screen_bounds);
  set_owned_by_client();

  // Take the root transform and apply this during buffer update instead of
  // leaving this up to the compositor. The benefit is that HW requirements
  // for being able to take advantage of overlays and direct scanout are
  // reduced significantly. Frames are submitted to the compositor with the
  // inverse transform to cancel out the transformation that would otherwise
  // be done by the compositor.
  screen_to_buffer_transform_ =
      widget_->GetNativeWindow()->GetHost()->GetRootTransform();

  buffer_size_ = gfx::ToEnclosedRect(cc::MathUtil::MapClippedRect(
                                         screen_to_buffer_transform_,
                                         gfx::RectF(screen_bounds.width(),
                                                    screen_bounds.height())))
                     .size();

  // Create a single GPU memory buffer. Content will be written into this
  // buffer without any buffering. The result is that we might be modifying
  // the buffer while it's being displayed. This provides minimal latency
  // but with potential tearing. Note that we have to draw into a temporary
  // surface and copy it into GPU memory buffer to avoid flicker.
  gpu_memory_buffer_ =
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBuffer(buffer_size_,
                                  SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                                               : gfx::BufferFormat::BGRA_8888,
                                  gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                                  gpu::kNullSurfaceHandle);
  LOG_IF(ERROR, !gpu_memory_buffer_) << "Failed to create GPU memory buffer";

  frame_sink_holder_ = std::make_unique<LayerTreeFrameSinkHolder>(
      this, widget_->GetNativeView()->CreateLayerTreeFrameSink());
}

FastInkView::~FastInkView() {
  LayerTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_));
}

void FastInkView::UpdateSurface(const gfx::Rect& content_rect,
                                const gfx::Rect& damage_rect,
                                bool auto_refresh) {
  content_rect_ = content_rect;
  damage_rect_.Union(damage_rect);
  auto_refresh_ = auto_refresh;
  pending_compositor_frame_ = true;

  if (!damage_rect.IsEmpty()) {
    frame_sink_holder_->DamageExportedResources();
    for (auto& resource : returned_resources_)
      resource->damaged = true;
  }

  if (!pending_compositor_frame_ack_)
    SubmitCompositorFrame();
}

void FastInkView::SubmitCompositorFrame() {
  TRACE_EVENT1("ui", "FastInkView::SubmitCompositorFrame", "damage",
               damage_rect_.ToString());

  float device_scale_factor = widget_->GetLayer()->device_scale_factor();
  gfx::Rect output_rect(gfx::ConvertSizeToPixel(
      device_scale_factor,
      widget_->GetNativeView()->GetBoundsInScreen().size()));

  gfx::Rect quad_rect;
  gfx::Rect damage_rect;
  // Continuously redraw the full output rectangle when in auto-refresh mode.
  // This is necessary in order to allow single buffered updates without having
  // buffer changes outside the contents area cause artifacts.
  if (auto_refresh_) {
    quad_rect = gfx::Rect(buffer_size_);
    damage_rect = gfx::Rect(output_rect);
  } else {
    // Use minimal quad and damage rectangles when auto-refresh mode is off.
    quad_rect = BufferRectFromScreenRect(screen_to_buffer_transform_,
                                         buffer_size_, content_rect_);
    damage_rect = gfx::ConvertRectToPixel(device_scale_factor, damage_rect_);
    damage_rect.Intersect(output_rect);
    pending_compositor_frame_ = false;
  }
  damage_rect_ = gfx::Rect();

  std::unique_ptr<Resource> resource;
  // Reuse returned resource if available.
  if (!returned_resources_.empty()) {
    resource = std::move(returned_resources_.back());
    returned_resources_.pop_back();
  }

  // Create new resource if needed.
  if (!resource)
    resource = std::make_unique<Resource>();

  if (resource->damaged) {
    // Acquire context provider for resource if needed.
    // Note: We make no attempts to recover if the context provider is later
    // lost. It is expected that this class is short-lived and requiring a
    // new instance to be created in lost context situations is acceptable and
    // keeps the code simple.
    if (!resource->context_provider) {
      resource->context_provider = aura::Env::GetInstance()
                                       ->context_factory()
                                       ->SharedMainThreadContextProvider();
      if (!resource->context_provider) {
        LOG(ERROR) << "Failed to acquire a context provider";
        return;
      }
    }

    gpu::gles2::GLES2Interface* gles2 = resource->context_provider->ContextGL();

    if (resource->sync_token.HasData()) {
      gles2->WaitSyncTokenCHROMIUM(resource->sync_token.GetConstData());
      resource->sync_token = gpu::SyncToken();
    }

    if (resource->texture) {
      gles2->ActiveTexture(GL_TEXTURE0);
      gles2->BindTexture(GL_TEXTURE_2D, resource->texture);
    } else {
      gles2->GenTextures(1, &resource->texture);
      gles2->ActiveTexture(GL_TEXTURE0);
      gles2->BindTexture(GL_TEXTURE_2D, resource->texture);
      gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gles2->GenMailboxCHROMIUM(resource->mailbox.name);
      gles2->ProduceTextureDirectCHROMIUM(resource->texture,
                                          resource->mailbox.name);
    }

    if (resource->image) {
      gles2->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, resource->image);
    } else {
      resource->image = gles2->CreateImageCHROMIUM(
          gpu_memory_buffer_->AsClientBuffer(), buffer_size_.width(),
          buffer_size_.height(), SK_B32_SHIFT ? GL_RGBA : GL_BGRA_EXT);
      if (!resource->image) {
        LOG(ERROR) << "Failed to create image";
        return;
      }
    }
    gles2->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, resource->image);

    // For mus and mash, the compositor isn't sharing the GPU channel with
    // FastInkView, so it cannot consume unverified sync token generated here.
    // We need generate verified sync token for mus and mash.
    if (ash::Shell::GetAshConfig() == ash::Config::CLASSIC)
      gles2->GenUnverifiedSyncTokenCHROMIUM(resource->sync_token.GetData());
    else
      gles2->GenSyncTokenCHROMIUM(resource->sync_token.GetData());

    resource->damaged = false;
  }

  viz::TransferableResource transferable_resource;
  transferable_resource.id = next_resource_id_++;
  transferable_resource.format = viz::RGBA_8888;
  transferable_resource.filter = GL_LINEAR;
  transferable_resource.size = buffer_size_;
  transferable_resource.mailbox_holder = gpu::MailboxHolder(
      resource->mailbox, resource->sync_token, GL_TEXTURE_2D);
  // Use HW overlay if continuous updates are expected.
  transferable_resource.is_overlay_candidate = auto_refresh_;

  gfx::Transform target_to_buffer_transform(screen_to_buffer_transform_);
  target_to_buffer_transform.Scale(1.f / device_scale_factor,
                                   1.f / device_scale_factor);

  gfx::Transform buffer_to_target_transform;
  bool rv = target_to_buffer_transform.GetInverse(&buffer_to_target_transform);
  DCHECK(rv);

  const int kRenderPassId = 1;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  render_pass->SetNew(kRenderPassId, output_rect, damage_rect,
                      buffer_to_target_transform);

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      buffer_to_target_transform,
      /*quad_layer_rect=*/output_rect,
      /*visible_quad_layer_rect=*/output_rect,
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, /*are_contents_opaque=*/false, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  viz::CompositorFrame frame;
  // TODO(eseckler): FastInkView should use BeginFrames and set the ack
  // accordingly.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = device_scale_factor;

  viz::TextureDrawQuad* texture_quad =
      render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::RectF uv_crop(quad_rect);
  uv_crop.Scale(1.f / buffer_size_.width(), 1.f / buffer_size_.height());
  texture_quad->SetNew(quad_state, quad_rect, quad_rect,
                       /*needs_blending=*/true, transferable_resource.id,
                       /*premultiplied_alpha=*/true, uv_crop.origin(),
                       uv_crop.bottom_right(),
                       /*background_color=*/SK_ColorTRANSPARENT, vertex_opacity,
                       /*y_flipped=*/false,
                       /*nearest_neighbor=*/false,
                       /*secure_output_only=*/false);
  texture_quad->set_resource_size_in_pixels(transferable_resource.size);
  frame.resource_list.push_back(transferable_resource);

  DCHECK(!pending_compositor_frame_ack_);
  pending_compositor_frame_ack_ = true;

  frame.render_pass_list.push_back(std::move(render_pass));
  frame_sink_holder_->SubmitCompositorFrame(
      std::move(frame), transferable_resource.id, std::move(resource));
}

void FastInkView::SubmitPendingCompositorFrame() {
  if (pending_compositor_frame_ && !pending_compositor_frame_ack_)
    SubmitCompositorFrame();
}

void FastInkView::DidReceiveCompositorFrameAck() {
  pending_compositor_frame_ack_ = false;
  if (pending_compositor_frame_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FastInkView::SubmitPendingCompositorFrame,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void FastInkView::ReclaimResource(std::unique_ptr<Resource> resource) {
  returned_resources_.push_back(std::move(resource));
}

}  // namespace ash
