/**
 * @file RLSkillProvider.cpp
 *
 * Python ↔ BHuman in-memory bridge via RLSharedState.
 *
 * Frame protocol (N = GameState.playerNumber):
 *   1. Python: pybh.rl_set_action(N, skill, x, y, theta)
 *   2. Python: controller.update()
 *   3. update(SkillRequest) reads action → provides SkillRequest
 */

#include "RLSkillProvider.h"
#include "Python/Controller/RLSharedState.h"

MAKE_MODULE(RLSkillProvider);

void RLSkillProvider::update(SkillRequest& skillRequest)
{
  const int n = theGameState.playerNumber > 0 ? theGameState.playerNumber : 1;
  RLPlayerIO& io = RLSharedState::instance().player(n);

  std::string skill;
  float tx, ty, tt;
  int passTarget;
  {
    io.lock();
    skill = io.getSkill();
    tx    = io.targetX;
    ty    = io.targetY;
    tt    = io.targetTheta;
    passTarget = io.passTarget;
    io.unlock();
  }

  if(skill == "walkTo" || skill == "walk")
    skillRequest = SkillRequest::Builder::walkTo(Pose2f(tt, tx, ty));
  else if(skill == "shoot")
    skillRequest = SkillRequest::Builder::shoot();
  else if(skill == "pass")
    skillRequest = SkillRequest::Builder::passTo(passTarget);
  else if(skill == "dribble")
    skillRequest = SkillRequest::Builder::dribbleTo(tt);
  else if(skill == "block")
    skillRequest = SkillRequest::Builder::block(Vector2f(tx, ty));
  else if(skill == "mark")
    skillRequest = SkillRequest::Builder::mark(Vector2f(tx, ty));
  else if(skill == "observe")
    skillRequest = SkillRequest::Builder::observe(Vector2f(tx, ty));
  else
    skillRequest = SkillRequest::Builder::stand();
}

void RLSkillProvider::update(StrategyStatus& strategyStatus)
{
  strategyStatus = StrategyStatus();
}
