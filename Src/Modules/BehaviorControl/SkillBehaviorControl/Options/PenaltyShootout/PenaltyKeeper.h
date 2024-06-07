option(PenaltyKeeper)
{
  // Martin Kroker, 14.04.2013?
  auto penaltyKeeperShouldCatchBall = [&]
  {
    const Vector2f& ballVelocityRel = theBallModel.estimate.velocity;
    // Return true if the ball is moving.
    return ballVelocityRel.x() != 0.f;
  };

initial_state(initial)
  {
    transition
    {
      if(penaltyKeeperShouldCatchBall())
        goto intercept;
    }
    action
    {
      theLookForwardSkill();
    }
  }

  state(intercept)
  {
    action
    {
        theInterceptBallSkill({.interceptionMethods = bit(Interception::jumpRight),
                                  .allowGetUp = !theGameState.isPenaltyShootout(),
                                  .allowDive = theBehaviorParameters.keeperJumpingOn});
    }
  }
}