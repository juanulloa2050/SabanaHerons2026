option(HandleGameState)
{
  common_transition
  {
    // Explicit check for standby state (must be before isInitial since standby is included in isInitial by default)
    if(theGameState.state == GameState::standby)
      goto standby;
    else if(theGameState.isInitial(/* orStandby: */ false))
      goto initial;
    else if(theGameState.isReady())
    {
      OUTPUT_TEXT("[HandleGameState] Transitioning to READY state - robot will position on field");
      goto ready;
    }
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

  /**
   * Standby state: Robot must remain still to avoid "Illegal Motion in Standby" penalty.
   * The robot waits for the referee gesture detection (via InitialToReady).
   * When the transition is detected, the GameStateProvider will change to ready state.
   */
  state(standby)
  {
    action
    {
      // Robot must stand still during standby to avoid penalty
      theLookAtAnglesSkill({.pan = 0_deg,
                            .tilt = 0_deg,
                            .speed = 150_deg});
      theStandSkill({.high = true});
    }
  }

  state(ready)
  {
    transition
    {
      if(state_time == 0)
        OUTPUT_TEXT("[HandleGameState] Entered READY state - executing WalkToPointReady or Stand");
    }
    action
    {
      theArmContactSkill();
      if(theStrategyStatus.role != PositionRole::toRole(PositionRole::goalkeeper))
        theArmObstacleAvoidanceSkill();
      if(theSkillRequest.skill == SkillRequest::walk)
      {
        OUTPUT_TEXT("[HandleGameState] READY: Walking to position on field");
        theWalkToPointReadySkill({.target = theSkillRequest.target});
      }
      else
      {
        OUTPUT_TEXT("[HandleGameState] READY: Standing in position");
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
