// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_test_template.h"

namespace gpu {
namespace {

INSTANTIATE_TYPED_TEST_CASE_P(GpuMemoryBufferImplSharedMemory,
                              GpuMemoryBufferImplTest,
                              GpuMemoryBufferImplSharedMemory);

INSTANTIATE_TYPED_TEST_CASE_P(GpuMemoryBufferImplSharedMemory,
                              GpuMemoryBufferImplCreateTest,
                              GpuMemoryBufferImplSharedMemory);

}  // namespace
}  // namespace gpu
