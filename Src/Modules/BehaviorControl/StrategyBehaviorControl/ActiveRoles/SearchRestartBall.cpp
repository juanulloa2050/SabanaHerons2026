/**
 * @file SearchRestartBall.cpp
 */

// Sabana Herons: role that searches the ball at the predicted restart position.

#include "SearchRestartBall.h"
#include "Tools/BehaviorControl/Strategy/Agent.h"

SkillRequest SearchRestartBall::execute(const Agent& self, const Agents&)
{
  const Vector2f target = theRestartBallSearchContext.rememberedPositionOnField;
  const float distance = (self.currentPosition - target).norm();
  if(distance < 350.f)
    return SkillRequest::Builder::observe(target);
  return SkillRequest::Builder::walkTo(Pose2f((target - self.currentPosition).angle(), target));
}
