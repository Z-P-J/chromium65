// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_QUERY_TRACKER_H_
#define GPU_COMMAND_BUFFER_CLIENT_QUERY_TRACKER_H_

#include <GLES2/gl2.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <bitset>
#include <list>
#include <memory>

#include "base/atomicops.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "gles2_impl_export.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"

namespace gpu {

class CommandBufferHelper;
class MappedMemoryManager;

namespace gles2 {

class GLES2Implementation;

// Manages buckets of QuerySync instances in mapped memory.
class GLES2_IMPL_EXPORT QuerySyncManager {
 public:
  static const size_t kSyncsPerBucket = 256;

  struct GLES2_IMPL_EXPORT Bucket {
    Bucket(QuerySync* sync_mem, int32_t shm_id, uint32_t shm_offset);
    ~Bucket();

    void FreePendingSyncs();

    QuerySync* syncs;
    int32_t shm_id;
    uint32_t base_shm_offset;
    std::bitset<kSyncsPerBucket> in_use_query_syncs;

    struct PendingSync {
      uint32_t index;
      int32_t submit_count;
    };
    std::vector<PendingSync> pending_syncs;
  };

  struct QueryInfo {
    QueryInfo(Bucket* bucket, uint32_t index)
        : bucket(bucket), sync(bucket->syncs + index) {}
    QueryInfo() = default;

    uint32_t index() const { return sync - bucket->syncs; }

    Bucket* bucket = nullptr;
    QuerySync* sync = nullptr;
    int32_t submit_count = 0;
  };

  explicit QuerySyncManager(MappedMemoryManager* manager);
  ~QuerySyncManager();

  bool Alloc(QueryInfo* info);
  void Free(const QueryInfo& sync);
  void Shrink(CommandBufferHelper* helper);

 private:
  FRIEND_TEST_ALL_PREFIXES(QuerySyncManagerTest, Shrink);

  MappedMemoryManager* mapped_memory_;
  base::circular_deque<std::unique_ptr<Bucket>> buckets_;

  DISALLOW_COPY_AND_ASSIGN(QuerySyncManager);
};

// Tracks queries for client side of command buffer.
class GLES2_IMPL_EXPORT QueryTracker {
 public:
  class GLES2_IMPL_EXPORT Query {
   public:
    enum State {
      kUninitialized,  // never used
      kActive,         // between begin - end
      kPending,        // not yet complete
      kComplete        // completed
    };

    Query(GLuint id, GLenum target, const QuerySyncManager::QueryInfo& info);

    GLenum target() const {
      return target_;
    }

    GLuint id() const {
      return id_;
    }

    int32_t shm_id() const { return info_.bucket->shm_id; }

    uint32_t shm_offset() const {
      return info_.bucket->base_shm_offset + sizeof(QuerySync) * info_.index();
    }

    void MarkAsActive() {
      state_ = kActive;
    }

    int32_t NextSubmitCount() const {
      int32_t submit_count = info_.submit_count + 1;
      if (submit_count == INT_MAX)
        submit_count = 1;
      return submit_count;
    }

    void MarkAsPending(int32_t token, int32_t submit_count) {
      info_.submit_count = submit_count;
      token_ = token;
      state_ = kPending;
    }

    base::subtle::Atomic32 submit_count() const { return info_.submit_count; }

    int32_t token() const { return token_; }

    bool NeverUsed() const {
      return state_ == kUninitialized;
    }

    bool Active() const {
      return state_ == kActive;
    }

    bool Pending() const {
      return state_ == kPending;
    }

    bool CheckResultsAvailable(CommandBufferHelper* helper);

    uint64_t GetResult() const;

   private:
    friend class QueryTracker;
    friend class QueryTrackerTest;

    void Begin(GLES2Implementation* gl);
    void End(GLES2Implementation* gl);
    void QueryCounter(GLES2Implementation* gl);

    GLuint id_;
    GLenum target_;
    QuerySyncManager::QueryInfo info_;
    State state_;
    int32_t token_;
    uint32_t flush_count_;
    uint64_t client_begin_time_us_;  // Only used for latency query target.
    uint64_t result_;
  };

  explicit QueryTracker(MappedMemoryManager* manager);
  ~QueryTracker();

  Query* CreateQuery(GLuint id, GLenum target);
  Query* GetQuery(GLuint id);
  Query* GetCurrentQuery(GLenum target);
  void RemoveQuery(GLuint id);
  void Shrink(CommandBufferHelper* helper);

  bool BeginQuery(GLuint id, GLenum target, GLES2Implementation* gl);
  bool EndQuery(GLenum target, GLES2Implementation* gl);
  bool QueryCounter(GLuint id, GLenum target, GLES2Implementation* gl);
  bool SetDisjointSync(GLES2Implementation* gl);
  bool CheckAndResetDisjoint();

  int32_t DisjointCountSyncShmID() const {
    return disjoint_count_sync_shm_id_;
  }

  uint32_t DisjointCountSyncShmOffset() const {
    return disjoint_count_sync_shm_offset_;
  }

 private:
  typedef base::hash_map<GLuint, std::unique_ptr<Query>> QueryIdMap;
  typedef base::flat_map<GLenum, Query*> QueryTargetMap;

  QueryIdMap queries_;
  QueryTargetMap current_queries_;
  QuerySyncManager query_sync_manager_;

  // The shared memory used for synchronizing timer disjoint values.
  MappedMemoryManager* mapped_memory_;
  int32_t disjoint_count_sync_shm_id_;
  uint32_t disjoint_count_sync_shm_offset_;
  DisjointValueSync* disjoint_count_sync_;
  uint32_t local_disjoint_count_;

  DISALLOW_COPY_AND_ASSIGN(QueryTracker);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_QUERY_TRACKER_H_
