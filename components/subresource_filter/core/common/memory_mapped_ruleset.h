// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_MEMORY_MAPPED_RULESET_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_MEMORY_MAPPED_RULESET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"

namespace subresource_filter {

// A reference-counted wrapper around base::MemoryMappedFile. The |ruleset_file|
// supplied in the constructor is kept memory-mapped and is safe to access until
// the last reference to this instance is dropped.
class MemoryMappedRuleset : public base::RefCounted<MemoryMappedRuleset>,
                            public base::SupportsWeakPtr<MemoryMappedRuleset> {
 public:
  explicit MemoryMappedRuleset(base::File ruleset_file);

  const uint8_t* data() const { return ruleset_.data(); }
  size_t length() const { return base::strict_cast<size_t>(ruleset_.length()); }

 private:
  friend class base::RefCounted<MemoryMappedRuleset>;
  ~MemoryMappedRuleset();

  base::MemoryMappedFile ruleset_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMappedRuleset);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_MEMORY_MAPPED_RULESET_H_
