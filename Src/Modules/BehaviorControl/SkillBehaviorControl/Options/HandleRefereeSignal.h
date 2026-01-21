/**
 * @file HandleRefereeSignal.h
 *
 * This file defines an option that handles the referee detection in the
 * standby state. The robot looks at the referee when in standby state
 * and the referee is within a certain bearing angle.
 *
 * Based on BHumanCodeRelease implementation.
 *
 * @author Thomas Röfer
 */

/**
 * The detection of the referee signal. It only becomes active if the robot is
 * in the right place during standby state.
 */
option(HandleRefereeSignal)
{
  // Height to look at (in mm) - Referee is standing, approximately 1.6m to 1.8m tall
  // We aim at chest/shoulder height for best gesture recognition
  const float lookAtHeight = 1400.f;
  // Maximum bearing to the referee this option becomes active
  // Increased to allow more robots to look at referee
  const Angle maxBearing = 60_deg;

  const Vector2f refereeOnField(theFieldDimensions.xPosHalfWayLine,
                                (theFieldDimensions.yPosLeftSideline + theFieldDimensions.yPosLeftFieldBorder) / 2.f
                                * (theGameState.leftHandTeam ? 1 : -1));
  const Vector2f refereeOffset = theRobotPose.inverse() * refereeOnField;

  common_transition
  {
    if(theGameState.state != GameState::standby || std::abs(refereeOffset.angle()) >= maxBearing)
      goto inactive;
  }

  initial_state(inactive)
  {
    transition
    {
      if(theGameState.gameControllerActive
         && theGameState.state == GameState::standby
         && std::abs(refereeOffset.angle()) < maxBearing)
        goto lookAtReferee;
    }
  }

  state(lookAtReferee)
  {
    action
    {
      theStandSkill({.high = true});
      theLookAtPointSkill({.target = {refereeOffset.x(), refereeOffset.y(), lookAtHeight},
                           .camera = HeadMotionRequest::upperCamera});
      theOptionalImageRequest.sendImage = true;
    }
  }
}