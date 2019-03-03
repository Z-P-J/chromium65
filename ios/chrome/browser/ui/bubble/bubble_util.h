// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UTIL_H_

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

namespace bubble_util {

// Calculate the coordinates of the point of the bubble's arrow based on the
// |targetFrame| of the target UI element and the bubble's |arrowDirection|. The
// returned point is in the same coordinate system as |targetFrame|.
CGPoint AnchorPoint(CGRect targetFrame, BubbleArrowDirection arrowDirection);

// Calculate the maximum size of the bubble such that it stays within its
// superview's bounding coordinate space and does not overlap the other side of
// the anchor point. |anchorPoint| is the point on the target UI element the
// bubble is anchored at in the bubble's superview's coordinate system.
// |direction| is the bubble's direction. |alignment| is the bubble's alignment.
// |boundingSize| is the size of the superview. Uses the ICU default locale of
// the device to determine whether the language is RTL.
CGSize BubbleMaxSize(CGPoint anchorPoint,
                     BubbleArrowDirection direction,
                     BubbleAlignment alignment,
                     CGSize boundingSize);

// Calculate the bubble's frame. |anchorPoint| is the point on the UI element
// the bubble is pointing to. |size| is the size of the bubble. |direction| is
// the direction the bubble's arrow is pointing. |alignment| is the alignment of
// the anchor (either leading, centered, or trailing). |boundingWidth| is the
// width of the bubble's superview. Uses the ICU default locale of the device to
// determine whether the language is RTL.
CGRect BubbleFrame(CGPoint anchorPoint,
                   CGSize size,
                   BubbleArrowDirection direction,
                   BubbleAlignment alignment,
                   CGFloat boundingWidth);

}  // namespace bubble_util

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UTIL_H_
