/**
 * @file SearchRestartBall.cpp
 */

/* SabanaHerons fork extension (B-Human 2023 base).
 * Execute the restart candidate search selected by the strategy using the existing
 * observation and walking skills.
 * Release overview and commit references: README.md.
 */

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
