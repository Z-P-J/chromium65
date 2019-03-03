// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DECODED_DRAW_IMAGE_H_
#define CC_PAINT_DECODED_DRAW_IMAGE_H_

#include <cfloat>
#include <cmath>

#include "base/optional.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

class CC_PAINT_EXPORT DecodedDrawImage {
 public:
  DecodedDrawImage(sk_sp<const SkImage> image,
                   const SkSize& src_rect_offset,
                   const SkSize& scale_adjustment,
                   SkFilterQuality filter_quality);
  DecodedDrawImage(base::Optional<uint32_t> transfer_cache_entry_id,
                   const SkSize& src_rect_offset,
                   const SkSize& scale_adjustment,
                   SkFilterQuality filter_quality);
  DecodedDrawImage(sk_sp<const SkImage> image, SkFilterQuality filter_quality);
  DecodedDrawImage(const DecodedDrawImage& other);
  DecodedDrawImage();
  ~DecodedDrawImage();

  const sk_sp<const SkImage>& image() const { return image_; }
  base::Optional<uint32_t> transfer_cache_entry_id() const {
    return transfer_cache_entry_id_;
  }
  const SkSize& src_rect_offset() const { return src_rect_offset_; }
  const SkSize& scale_adjustment() const { return scale_adjustment_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  bool is_scale_adjustment_identity() const {
    return std::abs(scale_adjustment_.width() - 1.f) < FLT_EPSILON &&
           std::abs(scale_adjustment_.height() - 1.f) < FLT_EPSILON;
  }

 private:
  sk_sp<const SkImage> image_;
  base::Optional<uint32_t> transfer_cache_entry_id_;
  SkSize src_rect_offset_;
  SkSize scale_adjustment_;
  SkFilterQuality filter_quality_;
};

}  // namespace cc

#endif  // CC_PAINT_DECODED_DRAW_IMAGE_H_
