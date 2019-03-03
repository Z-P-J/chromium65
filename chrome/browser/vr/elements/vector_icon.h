// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_H_
#define CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class PointF;
struct VectorIcon;
}  // namespace gfx

namespace vr {

class VectorIconTexture;

class VectorIcon : public TexturedElement {
 public:
  explicit VectorIcon(int maximum_width_pixels);
  ~VectorIcon() override;

  // TODO(vollick): should just use TexturedElement::SetForegroundColor.
  void SetColor(SkColor color);
  SkColor GetColor() const;
  void SetIcon(const gfx::VectorIcon& icon);

  static void DrawVectorIcon(gfx::Canvas* canvas,
                             const gfx::VectorIcon& icon,
                             float size_px,
                             const gfx::PointF& corner,
                             SkColor color);

 protected:
  UiTexture* GetTexture() const override;

 private:
  std::unique_ptr<VectorIconTexture> texture_;
  DISALLOW_COPY_AND_ASSIGN(VectorIcon);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_H_
