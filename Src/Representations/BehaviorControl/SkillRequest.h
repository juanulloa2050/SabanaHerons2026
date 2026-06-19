/**
 * @file SkillRequest.h
 *
 * This file declares the representation of the request from the strategy layer to the skill layer.
 *
 * @author Arne Hasselbring
 */

#pragma once

#include "Math/Pose2f.h"
#include "Representations/MotionControl/MotionRequest.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"

STREAMABLE(SkillRequest,
{
  ENUM(Type,
  {,
    none,
    stand,
    walk,
    shoot,
    pass,
    dribble,
    block,
    mark,
    observe,
    interceptBall,
    keeperDive,
  });

  struct Builder
  {
    // All coordinates/angles/directions are in field coordinates.
    static SkillRequest empty();
    static SkillRequest stand();
    static SkillRequest walkTo(const Pose2f& target);
    static SkillRequest shoot();
    static SkillRequest passTo(int target);
    static SkillRequest dribbleTo(Angle target);
    static SkillRequest block(const Vector2f& player);
    static SkillRequest mark(const Vector2f& player);
    static SkillRequest observe(const Vector2f& point);
    static SkillRequest interceptBall(unsigned interceptionMethods, bool allowDive = true);
    static SkillRequest keeperDive(MotionRequest::Dive::Request diveRequest);
  },

  (Type)(none) skill, /**< The skill that shall run. */
  (Pose2f) target, /**< The target pose, object or direction (in field coordinates). */
  (int)(-1) passTarget, /**< The number of the passed-to player. */
  (unsigned)(0) interceptionMethods, /**< Bitset of Interception::Method values for goalkeeper ball interception. */
  (bool)(true) allowDive, /**< Whether keyframe dive/genuflect interception is allowed. */
  (MotionRequest::Dive::Request)(MotionRequest::Dive::prepare) diveRequest, /**< Exact goalkeeper keyframe dive/genuflect request. */
});
