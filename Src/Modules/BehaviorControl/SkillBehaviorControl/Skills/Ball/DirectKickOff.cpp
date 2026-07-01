/**
 * @file DirectKickOff.cpp
 *
 * This ...
 *
 * @author Arne Hasselbring (from former KickoffStrikerCard.cpp in 2019).
 */

#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/BehaviorControl/Skills.h"
#include "Representations/Configuration/KickInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/MotionControl/MotionInfo.h"
#include "Tools/BehaviorControl/Framework/Skill/Skill.h"

SKILL_IMPLEMENTATION(DirectKickOffImpl,
{,
  IMPLEMENTS(DirectKickOff),
  CALLS(GoToBallAndKick),
  CALLS(LookLeftAndRight),
  CALLS(Stand),
  REQUIRES(ExtendedGameState),
  REQUIRES(FieldBall),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(KickInfo),
  REQUIRES(MotionInfo),
  REQUIRES(RobotPose),

});

class DirectKickOffImpl : public DirectKickOffImplBase
{
  void execute(const DirectKickOff&) override
  {
    if(theMotionInfo.lastKickTimestamp > theExtendedGameState.timeWhenStateStarted[GameState::ownKickOff])
    {
      theLookLeftAndRightSkill();
      theStandSkill();
      return;
    }

    if(theFrameInfo.getTimeSince(theExtendedGameState.timeWhenStateStarted[GameState::ownKickOff]) < 2000)
    {
      theLookLeftAndRightSkill();
      theStandSkill();
      return;
    }

    std::size_t numOfActiveOwnRobots = 0;
    for(const auto& playerState : theGameState.ownTeam.playerStates)
      if(playerState == GameState::active)
        ++numOfActiveOwnRobots;
    const bool limitedTeam = numOfActiveOwnRobots <= 3;

    // HSL kick-offs must not be aimed directly at the goal. With three or fewer active
    // robots, the first kick should safely leave the center circle so normal play can resume.
    // With more than three active robots, a teammate must provide the next scoring touch.
    if(!wasActive)
    {
      const Angle exitOffset = theFieldBall.positionOnField.y() >= 0.f ? (limitedTeam ? -55_deg : -75_deg) : (limitedTeam ? 55_deg : 75_deg);
      targetAngle = Angle::normalize(theRobotPose.rotation + exitOffset);
      kickType = exitOffset < 0_deg ? KickInfo::walkForwardsRightAlternative : KickInfo::walkForwardsLeftAlternative;
      wasActive = true;
    }

    theGoToBallAndKickSkill({.targetDirection = Angle::normalize(targetAngle - theRobotPose.rotation),
                             .kickType = kickType,
                             .lookActiveWithBall = true});
  }

  void reset(const DirectKickOff&) override
  {
    wasActive = false;
    kickType = KickInfo::walkForwardsRightAlternative;
  }

  void preProcess(const DirectKickOff&) override {}

  void preProcess() override {}

  bool wasActive; /**< Whether an in walk kick out of the center circle was already tried. */
  KickInfo::KickType kickType; /**< The kick type to try. */
  Angle targetAngle; /**< The target angle to which the kick-off should go (from the ball in field coordinates). */
};

MAKE_SKILL_IMPLEMENTATION(DirectKickOffImpl);
