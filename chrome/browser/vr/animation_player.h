// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ANIMATION_PLAYER_H_
#define CHROME_BROWSER_VR_ANIMATION_PLAYER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "cc/animation/animation.h"
#include "chrome/browser/vr/transition.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class AnimationTarget;
class TransformOperations;
}  // namespace cc

namespace gfx {
class SizeF;
}  // namespace gfx

namespace vr {

// This is a simplified version of the cc::AnimationPlayer. Its sole purpose is
// the management of its collection of animations. Ticking them, updating their
// state, and deleting them as required.
//
// TODO(vollick): if cc::Animation and friends move into gfx/, then this class
// should follow suit. As such, it should not absorb any vr-specific
// functionality.
class AnimationPlayer final {
 public:
  static int GetNextAnimationId();
  static int GetNextGroupId();

  AnimationPlayer();
  ~AnimationPlayer();

  cc::AnimationTarget* target() const { return target_; }
  void set_target(cc::AnimationTarget* target) { target_ = target; }

  void AddAnimation(std::unique_ptr<cc::Animation> animation);
  void RemoveAnimation(int animation_id);
  void RemoveAnimations(int target_property);

  void Tick(base::TimeTicks monotonic_time);

  using Animations = std::vector<std::unique_ptr<cc::Animation>>;
  const Animations& animations() { return animations_; }

  // The transition is analogous to CSS transitions. When configured, the
  // transition object will cause subsequent calls the corresponding
  // TransitionXXXTo functions to induce transition animations.
  const Transition& transition() const { return transition_; }
  void set_transition(const Transition& transition) {
    transition_ = transition;
  }

  void SetTransitionedProperties(const std::set<int>& properties);
  void SetTransitionDuration(base::TimeDelta delta);

  // TODO(754820): Remove duplicate code from the transition functions
  // by using templates.
  void TransitionFloatTo(base::TimeTicks monotonic_time,
                         int target_property,
                         float current,
                         float target);
  void TransitionTransformOperationsTo(base::TimeTicks monotonic_time,
                                       int target_property,
                                       const cc::TransformOperations& current,
                                       const cc::TransformOperations& target);
  void TransitionSizeTo(base::TimeTicks monotonic_time,
                        int target_property,
                        const gfx::SizeF& current,
                        const gfx::SizeF& target);
  void TransitionColorTo(base::TimeTicks monotonic_time,
                         int target_property,
                         SkColor current,
                         SkColor target);

  bool IsAnimatingProperty(int property) const;

  float GetTargetFloatValue(int target_property, float default_value) const;
  cc::TransformOperations GetTargetTransformOperationsValue(
      int target_property,
      const cc::TransformOperations& default_value) const;
  gfx::SizeF GetTargetSizeValue(int target_property,
                                const gfx::SizeF& default_value) const;
  SkColor GetTargetColorValue(int target_property, SkColor default_value) const;

 private:
  void StartAnimations(base::TimeTicks monotonic_time);
  template <typename ValueType>
  void TransitionValueTo(base::TimeTicks monotonic_time,
                         int target_property,
                         const ValueType& current,
                         const ValueType& target);
  cc::Animation* GetRunningAnimationForProperty(int target_property) const;
  cc::Animation* GetAnimationForProperty(int target_property) const;
  template <typename ValueType>
  ValueType GetTargetValue(int target_property,
                           const ValueType& default_value) const;

  cc::AnimationTarget* target_ = nullptr;
  Animations animations_;
  Transition transition_;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace vr

#endif  //  CHROME_BROWSER_VR_ANIMATION_PLAYER_H_
