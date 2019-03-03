// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_buffer_provider.h"

namespace cc {

FakeRasterBufferProviderImpl::FakeRasterBufferProviderImpl() = default;

FakeRasterBufferProviderImpl::~FakeRasterBufferProviderImpl() = default;

std::unique_ptr<RasterBuffer>
FakeRasterBufferProviderImpl::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  return nullptr;
}

void FakeRasterBufferProviderImpl::OrderingBarrier() {}

void FakeRasterBufferProviderImpl::Flush() {}

viz::ResourceFormat FakeRasterBufferProviderImpl::GetResourceFormat(
    bool must_support_alpha) const {
  return viz::ResourceFormat::RGBA_8888;
}

bool FakeRasterBufferProviderImpl::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

bool FakeRasterBufferProviderImpl::CanPartialRasterIntoProvidedResource()
    const {
  return true;
}

bool FakeRasterBufferProviderImpl::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  return true;
}

uint64_t FakeRasterBufferProviderImpl::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Callback<void()>& callback,
    uint64_t pending_callback_id) const {
  return 0;
}

void FakeRasterBufferProviderImpl::Shutdown() {}

}  // namespace cc
