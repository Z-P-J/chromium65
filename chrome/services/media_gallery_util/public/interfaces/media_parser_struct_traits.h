// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_INTERFACES_MEDIA_PARSER_STRUCT_TRAITS_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_INTERFACES_MEDIA_PARSER_STRUCT_TRAITS_H_

#include <string>

#include "base/containers/span.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/interfaces/media_parser.mojom.h"
#include "mojo/public/cpp/bindings/array_traits_span.h"

namespace mojo {

template <>
struct StructTraits<chrome::mojom::AttachedImageDataView,
                    ::metadata::AttachedImage> {
  static const std::string& type(const ::metadata::AttachedImage& image) {
    return image.type;
  }

  static base::span<const uint8_t> data(
      const ::metadata::AttachedImage& image) {
    // TODO(dcheng): perhaps metadata::AttachedImage should consider passing the
    // image data around in a std::vector<uint8_t>.
    return base::make_span(reinterpret_cast<const uint8_t*>(image.data.data()),
                           image.data.size());
  }

  static bool Read(chrome::mojom::AttachedImageDataView view,
                   ::metadata::AttachedImage* out);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_INTERFACES_MEDIA_PARSER_STRUCT_TRAITS_H_
