// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/bookmark_promo_bubble_view.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"

// static
BookmarkPromoBubbleView* BookmarkPromoBubbleView::CreateOwned(
    views::View* anchor_view) {
  return new BookmarkPromoBubbleView(anchor_view);
}

BookmarkPromoBubbleView::BookmarkPromoBubbleView(views::View* anchor_view)
    : FeaturePromoBubbleView(anchor_view,
                             views::BubbleBorder::TOP_RIGHT,
                             GetStringSpecifier(),
                             ActivationAction::ACTIVATE) {}

BookmarkPromoBubbleView::~BookmarkPromoBubbleView() = default;

// static
int BookmarkPromoBubbleView::GetStringSpecifier() {
  static constexpr int kTextIds[] = {IDS_BOOKMARK_PROMO_0, IDS_BOOKMARK_PROMO_1,
                                     IDS_BOOKMARK_PROMO_2};
  const std::string& str = variations::GetVariationParamValue(
      "BookmarkInProductHelp", "x_promo_string");
  size_t text_specifier;
  if (!base::StringToSizeT(str, &text_specifier) ||
      text_specifier >= arraysize(kTextIds)) {
    text_specifier = 0;
  }

  return kTextIds[text_specifier];
}
