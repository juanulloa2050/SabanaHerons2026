/**
 * @file RLSkillProvider.cpp
 *
 * Python ↔ BHuman in-memory bridge via RLSharedState.
 *
 * Frame protocol (N = GameState.playerNumber):
 *   1. Python: pybh.rl_set_action(N, skill, x, y, theta)
 *   2. Python: controller.update()
 *   3. update(SkillRequest) reads action → provides SkillRequest
 *   4. update(SkillRequest) writes ball+robot obs → shared state
 *   5. Python: pybh.rl_get_obs(N)
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
  {
    std::lock_guard<std::mutex> lock(io.mutex);
    skill = io.skill;
    tx    = io.targetX;
    ty    = io.targetY;
    tt    = io.targetTheta;
  }

  if(skill == "shoot")
    skillRequest = SkillRequest::Builder::shoot();
  else if(skill == "walkTo")
    skillRequest = SkillRequest::Builder::walkTo(Pose2f(tt, tx, ty));
  else if(skill == "dribble")
    skillRequest = SkillRequest::Builder::dribbleTo(tt);
  else
    skillRequest = SkillRequest::Builder::stand();

  // Write observation back to Python
  float bx = 0.f, by = 0.f;
  if(!theGroundTruthWorldState.balls.empty())
  {
    bx = theGroundTruthWorldState.balls[0].position.x();
    by = theGroundTruthWorldState.balls[0].position.y();
  }
  const Pose2f& rp = theGroundTruthWorldState.ownPose;

  {
    std::lock_guard<std::mutex> lock(io.mutex);
    io.ballX      = bx;
    io.ballY      = by;
    io.robotX     = rp.translation.x();
    io.robotY     = rp.translation.y();
    io.robotTheta = static_cast<float>(rp.rotation);
    io.frame      = theFrameInfo.time;
    io.obsReady   = true;
  }
  io.obsSignal.post();  // wake up any Python thread waiting for this observation
}

void RLSkillProvider::update(StrategyStatus& strategyStatus)
{
  strategyStatus = StrategyStatus();
}
