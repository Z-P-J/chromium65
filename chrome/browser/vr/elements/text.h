// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_H_

#include <memory>

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/font.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"

namespace gfx {
class RenderText;
}

namespace vr {

class RenderTextWrapper;
class TextTexture;

enum TextLayoutMode {
  kSingleLineFixedWidth,
  kSingleLineFixedHeight,
  kMultiLineFixedWidth,
};

// This class describes a formatting attribute, applicable to a Text element.
// Attributes are applied in order, and may override previous attributes.
// Formatting may be applied only to non-wrapping text.
class TextFormattingAttribute {
 public:
  enum Type {
    COLOR,
    WEIGHT,
    DIRECTIONALITY,
  };

  TextFormattingAttribute(SkColor color, const gfx::Range& range);
  TextFormattingAttribute(gfx::Font::Weight weight, const gfx::Range& range);
  explicit TextFormattingAttribute(gfx::DirectionalityMode directionality);

  void Apply(RenderTextWrapper* render_text) const;

  bool operator==(const TextFormattingAttribute& other) const;
  bool operator!=(const TextFormattingAttribute& other) const;

  Type type() { return type_; }
  gfx::Range range() { return range_; }
  SkColor color() { return color_; }
  gfx::Font::Weight weight() { return weight_; }
  gfx::DirectionalityMode directionality() { return directionality_; }

 private:
  Type type_;
  gfx::Range range_;
  union {
    SkColor color_;
    gfx::Font::Weight weight_;
    gfx::DirectionalityMode directionality_;
  };
};

typedef std::vector<TextFormattingAttribute> TextFormatting;

class Text : public TexturedElement {
 public:
  explicit Text(float font_height_dmms);
  ~Text() override;

  void SetFontHeightInDmm(float font_height_dmms);
  void SetText(const base::string16& text);

  // TODO(vollick): should use TexturedElement::SetForegroundColor
  void SetColor(SkColor color);

  // Formatting must be applied only to non-wrapping text elements.
  void SetFormatting(const TextFormatting& formatting);

  void SetAlignment(UiTexture::TextAlignment alignment);
  void SetLayoutMode(TextLayoutMode mode);

  // This text element does not typically feature a cursor, but since the cursor
  // position is determined while laying out text, a parent may wish to supply
  // cursor parameters and determine where the cursor was last drawn.
  void SetCursorEnabled(bool enabled);
  void SetCursorPosition(int position);

  // Returns the most recently computed cursor position, in pixels.  This is
  // used for scene dirtiness and testing.
  gfx::Rect GetRawCursorBounds() const;

  // Returns the most recently computed cursor position, in fractions of the
  // texture size, relative to the upper-left corner of the element.
  gfx::RectF GetCursorBounds() const;

  void OnSetSize(const gfx::SizeF& size) override;
  void UpdateElementSize() override;

  const std::vector<std::unique_ptr<gfx::RenderText>>& LayOutTextForTest();
  gfx::SizeF GetTextureSizeForTest() const;

 private:
  UiTexture* GetTexture() const override;

  TextLayoutMode text_layout_mode_ = kMultiLineFixedWidth;
  std::unique_ptr<TextTexture> texture_;
  DISALLOW_COPY_AND_ASSIGN(Text);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
