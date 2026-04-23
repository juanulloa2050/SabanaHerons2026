/**
 * @file RLSim2D.h
 *
 * Lightweight headless 2D simulation state for pybh-based RL loops.
 * The motion limits mirror SimulatedRobot2D closely enough to make
 * walk/shoot rollouts behave like the 2D simulator without requiring
 * a full SimRobot runtime inside the Python module.
 */

#pragma once

#include "Math/Geometry.h"
#include "Math/Pose2f.h"
#include "Platform/BHAssert.h"
#include "Representations/BehaviorControl/SkillRequest.h"
#include "Representations/Infrastructure/GroundTruthWorldState.h"
#include "Tools/Modeling/BallPhysics.h"
#include <algorithm>
#include <cmath>
#include <string>

struct RLSim2DState
{
  bool enabled = false;
  bool initialized = false;

  float ballX = 0.f;
  float ballY = 0.f;
  float ballVx = 0.f;
  float ballVy = 0.f;

  float robotX = 0.f;
  float robotY = 0.f;
  float robotTheta = 0.f;

  int leftPhase = -1; // -1 unknown, 0 false, 1 true
  std::string lastSkill = "stand";
};

namespace RLSim2D
{
  inline constexpr float dt = 0.033f;
  inline constexpr float baseWalkPeriod = 0.25f;
  inline const Pose2f maxSpeed = Pose2f(70_deg, 250.f, 180.f);
  inline constexpr float maxSpeedBackwards = 200.f;
  inline constexpr float ballFriction = -0.30f; // matches Config/*/ballSpecification.cfg
  inline constexpr float shootDistance = 6000.f;

  inline void reset(RLSim2DState& state, float ballX, float ballY,
                    float robotX, float robotY, float robotTheta)
  {
    state.enabled = true;
    state.initialized = true;
    state.ballX = ballX;
    state.ballY = ballY;
    state.ballVx = 0.f;
    state.ballVy = 0.f;
    state.robotX = robotX;
    state.robotY = robotY;
    state.robotTheta = robotTheta;
    state.leftPhase = -1;
    state.lastSkill = "stand";
  }

  inline GroundTruthWorldState toWorldState(const RLSim2DState& state)
  {
    GroundTruthWorldState ws;
    GroundTruthWorldState::GroundTruthBall ball;
    ball.position = Vector3f(state.ballX, state.ballY, 50.f);
    ball.velocity = Vector3f(state.ballVx, state.ballVy, 0.f);
    ws.balls.push_back(ball);
    ws.ownPose = Pose2f(state.robotTheta, state.robotX, state.robotY);
    return ws;
  }

  inline Rangea rotationRange(bool isLeftPhase)
  {
    constexpr float insideTurnRatio = 0.33f;
    const float innerTurn = 2.f * insideTurnRatio * maxSpeed.rotation * baseWalkPeriod;
    const float outerTurn = 2.f * (1.f - insideTurnRatio) * maxSpeed.rotation * baseWalkPeriod;
    return Rangea(isLeftPhase ? -innerTurn : -outerTurn, isLeftPhase ? outerTurn : innerTurn);
  }

  inline void translationRectangle(float rotation, Vector2f& backRight, Vector2f& frontLeft)
  {
    const float noTranslationFromRotation = 24_deg;
    const float tFactor = std::max(0.f, 1.f - std::max(0.f, std::abs(rotation) / noTranslationFromRotation));

    backRight = Vector2f(
      std::max(-1000.f, tFactor * -maxSpeedBackwards * baseWalkPeriod),
      std::max(-1000.f, tFactor * -2.f * maxSpeed.translation.y() * baseWalkPeriod));
    frontLeft = Vector2f(
      std::min(1000.f, tFactor * maxSpeed.translation.x() * baseWalkPeriod),
      std::min(1000.f, tFactor * 2.f * maxSpeed.translation.y() * baseWalkPeriod));

    backRight.x() = std::min(backRight.x(), -.01f);
    frontLeft.x() = std::max(frontLeft.x(), .01f);
    backRight.y() = std::min(backRight.y(), -.01f);
    frontLeft.y() = std::max(frontLeft.y(), .01f);
  }

  inline Vector2f clipTargetToRectangle(const Vector2f& target, const Vector2f& backRight, const Vector2f& frontLeft)
  {
    if(Geometry::isPointInsideRectangle(backRight, frontLeft, target))
      return target;

    if(target.squaredNorm() <= 1e-6f)
      return Vector2f::Zero();

    Vector2f p1;
    Vector2f p2;
    VERIFY(Geometry::getIntersectionPointsOfLineAndRectangle(
      backRight, frontLeft, Geometry::Line(Vector2f(0.f, 0.f), target.normalized()), p1, p2));
    return p2;
  }

  inline void stepBall(RLSim2DState& state)
  {
    Vector2f ballPos(state.ballX, state.ballY);
    Vector2f ballVel(state.ballVx, state.ballVy);
    BallPhysics::propagateBallPositionAndVelocity(ballPos, ballVel, dt, ballFriction);
    state.ballX = ballPos.x();
    state.ballY = ballPos.y();
    state.ballVx = ballVel.x();
    state.ballVy = ballVel.y();
  }

  inline void stepWalkTo(RLSim2DState& state, const Pose2f& target)
  {
    const Pose2f robotPose(state.robotTheta, state.robotX, state.robotY);
    const Pose2f walkTarget = robotPose.inverse() * target;

    Angle modRotation = walkTarget.rotation;
    const float targetNorm = walkTarget.translation.norm();
    if(targetNorm > 600.f - std::abs(walkTarget.rotation) / pi * 400.f)
      modRotation = walkTarget.translation.angle();
    else if(targetNorm > 100.f)
    {
      const float factor = (targetNorm - 100.f) / 500.f;
      modRotation = (factor * walkTarget.translation.normalized() +
                    Vector2f::polar(1.f - factor, walkTarget.rotation)).angle();
    }

    const bool shouldBeLeft = walkTarget.translation.y() != 0.f ? walkTarget.translation.y() > 0.f : modRotation > 0.f;
    const bool isLeftPhase = state.leftPhase < 0 ? shouldBeLeft : !static_cast<bool>(state.leftPhase);
    const float stepRotation = rotationRange(isLeftPhase).clamped(modRotation);

    Vector2f backRight;
    Vector2f frontLeft;
    translationRectangle(stepRotation, backRight, frontLeft);
    Vector2f stepTranslation = clipTargetToRectangle(walkTarget.translation, backRight, frontLeft);
    if(isLeftPhase == (stepTranslation.y() < 0.f))
      stepTranslation.y() = 0.f;

    const float frac = dt / baseWalkPeriod;
    const Pose2f delta(stepRotation * frac, stepTranslation * frac);
    const Pose2f newPose = robotPose * delta;
    state.robotX = newPose.translation.x();
    state.robotY = newPose.translation.y();
    state.robotTheta = newPose.rotation;
    state.leftPhase = isLeftPhase ? 1 : 0;
  }

  inline void stepShoot(RLSim2DState& state)
  {
    const float speed = BallPhysics::velocityForDistance(shootDistance, ballFriction);
    state.ballVx = std::cos(state.robotTheta) * speed;
    state.ballVy = std::sin(state.robotTheta) * speed;
  }

  inline void stepDribble(RLSim2DState& state)
  {
    const float speed = BallPhysics::velocityForDistance(1200.f, ballFriction);
    state.ballVx = std::cos(state.robotTheta) * speed * 0.25f;
    state.ballVy = std::sin(state.robotTheta) * speed * 0.25f;
  }

  inline void stepFromSkill(RLSim2DState& state, const std::string& skill, const SkillRequest& skillRequest)
  {
    if(!state.enabled || !state.initialized)
      return;

    if(skillRequest.skill == SkillRequest::walk)
      stepWalkTo(state, skillRequest.target);
    else if(skillRequest.skill == SkillRequest::shoot && state.lastSkill != "shoot")
      stepShoot(state);
    else if(skillRequest.skill == SkillRequest::dribble)
      stepDribble(state);

    stepBall(state);
    state.lastSkill = skill;
  }
}
