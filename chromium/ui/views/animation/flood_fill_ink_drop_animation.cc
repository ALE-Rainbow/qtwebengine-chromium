// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_animation.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace {

// The minimum radius to use when scaling the painted layers. Smaller values
// were causing visual anomalies.
const float kMinRadius = 1.f;

// All the sub animations that are used to animate each of the InkDropStates.
// These are used to get time durations with
// GetAnimationDuration(InkDropSubAnimations). Note that in general a sub
// animation defines the duration for either a transformation animation or an
// opacity animation but there are some exceptions where an entire InkDropState
// animation consists of only 1 sub animation and it defines the duration for
// both the transformation and opacity animations.
enum InkDropSubAnimations {
  // HIDDEN sub animations.

  // The HIDDEN sub animation that is fading out to a hidden opacity.
  HIDDEN_FADE_OUT,

  // The HIDDEN sub animation that transform the circle to a small one.
  HIDDEN_TRANSFORM,

  // ACTION_PENDING sub animations.

  // The ACTION_PENDING sub animation that fades in to the visible opacity.
  ACTION_PENDING_FADE_IN,

  // The ACTION_PENDING sub animation that transforms the circle to fill the
  // bounds.
  ACTION_PENDING_TRANSFORM,

  // ACTION_TRIGGERED sub animations.

  // The ACTION_TRIGGERED sub animation that is fading out to a hidden opacity.
  ACTION_TRIGGERED_FADE_OUT,

  // ALTERNATE_ACTION_PENDING sub animations.

  // The ALTERNATE_ACTION_PENDING animation has only one sub animation which
  // animates
  // the circleto fill the bounds at visible opacity.
  ALTERNATE_ACTION_PENDING,

  // ALTERNATE_ACTION_TRIGGERED sub animations.

  // The ALTERNATE_ACTION_TRIGGERED sub animation that is fading out to a hidden
  // opacity.
  ALTERNATE_ACTION_TRIGGERED_FADE_OUT,

  // ACTIVATED sub animations.

  // The ACTIVATED sub animation that is fading in to the visible opacity.
  ACTIVATED_FADE_IN,

  // The ACTIVATED sub animation that transforms the circle to fill the entire
  // bounds.
  ACTIVATED_TRANSFORM,

  // DEACTIVATED sub animations.

  // The DEACTIVATED sub animation that is fading out to a hidden opacity.
  DEACTIVATED_FADE_OUT,
};

// Duration constants for InkDropStateSubAnimations. See the
// InkDropStateSubAnimations enum documentation for more info.
int kAnimationDurationInMs[] = {
    200,  // HIDDEN_FADE_OUT
    300,  // HIDDEN_TRANSFORM
    0,    // ACTION_PENDING_FADE_IN
    240,  // ACTION_PENDING_TRANSFORM
    300,  // ACTION_TRIGGERED_FADE_OUT
    200,  // ALTERNATE_ACTION_PENDING
    300,  // ALTERNATE_ACTION_TRIGGERED_FADE_OUT
    150,  // ACTIVATED_FADE_IN
    200,  // ACTIVATED_TRANSFORM
    300,  // DEACTIVATED_FADE_OUT
};

// Returns the InkDropState sub animation duration for the given |state|.
base::TimeDelta GetAnimationDuration(InkDropSubAnimations state) {
  return base::TimeDelta::FromMilliseconds(
      (views::InkDropAnimation::UseFastAnimations()
           ? 1
           : views::InkDropAnimation::kSlowAnimationDurationFactor) *
      kAnimationDurationInMs[state]);
}

}  // namespace

namespace views {

FloodFillInkDropAnimation::FloodFillInkDropAnimation(
    const gfx::Rect& clip_bounds,
    const gfx::Point& center_point,
    SkColor color)
    : clip_bounds_(clip_bounds),
      center_point_(center_point),
      root_layer_(ui::LAYER_NOT_DRAWN),
      circle_layer_delegate_(
          color,
          std::max(clip_bounds_.width(), clip_bounds_.height()) / 2.f),
      ink_drop_state_(InkDropState::HIDDEN) {
  root_layer_.set_name("FloodFillInkDropAnimation:ROOT_LAYER");
  root_layer_.SetMasksToBounds(true);
  root_layer_.SetBounds(clip_bounds);

  const int painted_size_length =
      2 * std::max(clip_bounds_.width(), clip_bounds_.height());

  painted_layer_.SetBounds(gfx::Rect(painted_size_length, painted_size_length));
  painted_layer_.SetFillsBoundsOpaquely(false);
  painted_layer_.set_delegate(&circle_layer_delegate_);
  painted_layer_.SetVisible(true);
  painted_layer_.SetOpacity(1.0);
  painted_layer_.SetMasksToBounds(false);
  painted_layer_.set_name("FloodFillInkDropAnimation:PAINTED_LAYER");

  root_layer_.Add(&painted_layer_);

  SetStateToHidden();
}

FloodFillInkDropAnimation::~FloodFillInkDropAnimation() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  AbortAllAnimations();
}

void FloodFillInkDropAnimation::SnapToActivated() {
  InkDropAnimation::SnapToActivated();
  SetOpacity(kVisibleOpacity);
  painted_layer_.SetTransform(GetMaxSizeTargetTransform());
}

ui::Layer* FloodFillInkDropAnimation::GetRootLayer() {
  return &root_layer_;
}

bool FloodFillInkDropAnimation::IsVisible() const {
  return root_layer_.visible();
}

void FloodFillInkDropAnimation::AnimateStateChange(
    InkDropState old_ink_drop_state,
    InkDropState new_ink_drop_state,
    ui::LayerAnimationObserver* animation_observer) {
  switch (new_ink_drop_state) {
    case InkDropState::HIDDEN:
      if (!IsVisible()) {
        SetStateToHidden();
      } else {
        AnimateToOpacity(kHiddenOpacity, GetAnimationDuration(HIDDEN_FADE_OUT),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
        const gfx::Transform transform = CalculateTransform(kMinRadius);
        AnimateToTransform(transform, GetAnimationDuration(HIDDEN_TRANSFORM),
                           ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                           gfx::Tween::EASE_IN_OUT, animation_observer);
      }
      break;
    case InkDropState::ACTION_PENDING: {
      DCHECK(old_ink_drop_state == InkDropState::HIDDEN);

      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ACTION_PENDING_FADE_IN),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ACTION_PENDING_TRANSFORM) -
                           GetAnimationDuration(ACTION_PENDING_FADE_IN),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN, animation_observer);

      AnimateToTransform(GetMaxSizeTargetTransform(),
                         GetAnimationDuration(ACTION_PENDING_TRANSFORM),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::FAST_OUT_SLOW_IN, animation_observer);
      break;
    }
    case InkDropState::ACTION_TRIGGERED: {
      DCHECK(old_ink_drop_state == InkDropState::HIDDEN ||
             old_ink_drop_state == InkDropState::ACTION_PENDING);
      if (old_ink_drop_state == InkDropState::HIDDEN) {
        AnimateStateChange(old_ink_drop_state, InkDropState::ACTION_PENDING,
                           animation_observer);
      }
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(ACTION_TRIGGERED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_PENDING: {
      DCHECK(old_ink_drop_state == InkDropState::ACTION_PENDING);
      AnimateToOpacity(kVisibleOpacity,
                       GetAnimationDuration(ALTERNATE_ACTION_PENDING),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToTransform(GetMaxSizeTargetTransform(),
                         GetAnimationDuration(ALTERNATE_ACTION_PENDING),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_TRIGGERED:
      DCHECK(old_ink_drop_state == InkDropState::ALTERNATE_ACTION_PENDING);
      AnimateToOpacity(kHiddenOpacity, GetAnimationDuration(
                                           ALTERNATE_ACTION_TRIGGERED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    case InkDropState::ACTIVATED: {
      AnimateToOpacity(kVisibleOpacity, GetAnimationDuration(ACTIVATED_FADE_IN),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToTransform(GetMaxSizeTargetTransform(),
                         GetAnimationDuration(ACTIVATED_TRANSFORM),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::DEACTIVATED:
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(DEACTIVATED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
  }
}

void FloodFillInkDropAnimation::SetStateToHidden() {
  painted_layer_.SetTransform(CalculateTransform(kMinRadius));
  root_layer_.SetOpacity(InkDropAnimation::kHiddenOpacity);
  root_layer_.SetVisible(false);
}

void FloodFillInkDropAnimation::AbortAllAnimations() {
  root_layer_.GetAnimator()->AbortAllAnimations();
  painted_layer_.GetAnimator()->AbortAllAnimations();
}

void FloodFillInkDropAnimation::AnimateToTransform(
    const gfx::Transform& transform,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = painted_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(preemption_strategy);
  animation.SetTweenType(tween);
  ui::LayerAnimationElement* element =
      ui::LayerAnimationElement::CreateTransformElement(transform, duration);
  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(element);

  if (animation_observer)
    sequence->AddObserver(animation_observer);

  animator->StartAnimation(sequence);
}

void FloodFillInkDropAnimation::SetOpacity(float opacity) {
  root_layer_.SetOpacity(opacity);
}

void FloodFillInkDropAnimation::AnimateToOpacity(
    float opacity,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = root_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  animation_settings.SetPreemptionStrategy(preemption_strategy);
  animation_settings.SetTweenType(tween);
  ui::LayerAnimationElement* animation_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  ui::LayerAnimationSequence* animation_sequence =
      new ui::LayerAnimationSequence(animation_element);

  if (animation_observer)
    animation_sequence->AddObserver(animation_observer);

  animator->StartAnimation(animation_sequence);
}

gfx::Transform FloodFillInkDropAnimation::CalculateTransform(
    float target_radius) const {
  const float target_scale = target_radius / circle_layer_delegate_.radius();
  const gfx::Point drawn_center_point =
      ToRoundedPoint(circle_layer_delegate_.GetCenterPoint());

  gfx::Transform transform = gfx::Transform();
  transform.Translate(center_point_.x(), center_point_.y());
  transform.Scale(target_scale, target_scale);
  transform.Translate(-drawn_center_point.x() - root_layer_.bounds().x(),
                      -drawn_center_point.y() - root_layer_.bounds().y());

  return transform;
}

gfx::Transform FloodFillInkDropAnimation::GetMaxSizeTargetTransform() const {
  // TODO(estade): get rid of this 2, but make the fade out start before the
  // active/action transform is done.
  return CalculateTransform(
      gfx::Vector2dF(clip_bounds_.width(), clip_bounds_.height()).Length() / 2);
}

}  // namespace views
