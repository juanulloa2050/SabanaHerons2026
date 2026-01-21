option(HandleGameState)
{
  common_transition
  {
    if(theGameState.isInitial(false))  // false = exclude standby
      goto initial;
    else if(theGameState.state == GameState::standby)
      goto standby;
    else if(theGameState.isReady())
      goto ready;
    else if(theGameState.isSet())
      goto set;
    else if(theGameState.isPlaying())
      goto playing;
    else if(theGameState.isFinished())
      goto finished;
    FAIL("Unknown game state.");
  }

  // The default state is "playing".
  initial_state(playing)
  {
    action
    {
      theArmContactSkill();
      if(theStrategyStatus.role != PositionRole::toRole(PositionRole::goalkeeper))
        theArmObstacleAvoidanceSkill();
      if(!select_option(playingOptions)) //@playingOptions
        executeRequest();
    }
  }

  state(initial)
  {
    action
    {
      theLookAtAnglesSkill({.pan = 0_deg,
                            .tilt = 0_deg,
                            .speed = 150_deg});
      theStandSkill({.high = true});
    }
  }

  state(standby)
  {
    action
    {
      // In standby, the robot stands and looks at the referee (handled by HandleRefereeSignal)
      // If the robot is not looking at the referee, it looks slightly upward to search for referee
      theLookAtAnglesSkill({.pan = 0_deg,
                            .tilt = -15_deg,  // Look slightly upward to search for referee
                            .speed = 150_deg});
      theStandSkill({.high = true});
    }
  }

  state(ready)
  {
    action
    {
      theArmContactSkill();
      if(theStrategyStatus.role != PositionRole::toRole(PositionRole::goalkeeper))
        theArmObstacleAvoidanceSkill();
      if(theSkillRequest.skill == SkillRequest::walk)
        theWalkToPointReadySkill({.target = theSkillRequest.target});
      else
      {
        theLookActiveSkill({.ignoreBall = true});
        theStandSkill();
      }
    }
  }

  state(set)
  {
    action
    {
      if(!theLibDemo.isOneVsOneDemoActive)
      {
        const Vector2f targetOnField = theGameState.isPenaltyKick() ?
                                       Vector2f(theGameState.isForOwnTeam() ?
                                                theFieldDimensions.xPosOpponentPenaltyMark :
                                                theFieldDimensions.xPosOwnPenaltyMark, 0.f) :
                                       Vector2f::Zero();
        theLookAtPointSkill({.target = (Vector3f() << theRobotPose.inverse() * targetOnField, theBallSpecification.radius).finished()});
      }
      else
        theLookActiveSkill({.ignoreBall = true});
      theStandSkill({.high = true});
    }
  }

  state(finished)
  {
    action
    {
      theLookForwardSkill();
      theStandSkill();
    }
  }
}
