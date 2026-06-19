/**
 * @file SkillRequest.cpp
 *
 * This file implements functions to generate the request from the strategy layer to the skill layer.
 *
 * @author Arne Hasselbring
 */

#include "SkillRequest.h"

SkillRequest SkillRequest::Builder::empty()
{
  return SkillRequest();
}

SkillRequest SkillRequest::Builder::stand()
{
  SkillRequest request;
  request.skill = SkillRequest::stand;
  return request;
}

SkillRequest SkillRequest::Builder::walkTo(const Pose2f& target)
{
  SkillRequest request;
  request.skill = SkillRequest::walk;
  request.target = target;
  return request;
}

SkillRequest SkillRequest::Builder::shoot()
{
  SkillRequest request;
  request.skill = SkillRequest::shoot;
  return request;
}

SkillRequest SkillRequest::Builder::passTo(int target)
{
  SkillRequest request;
  request.skill = SkillRequest::pass;
  request.passTarget = target;
  return request;
}

SkillRequest SkillRequest::Builder::dribbleTo(Angle target)
{
  SkillRequest request;
  request.skill = SkillRequest::dribble;
  request.target.rotation = target;
  return request;
}

SkillRequest SkillRequest::Builder::block(const Vector2f& player)
{
  SkillRequest request;
  request.skill = SkillRequest::block;
  request.target.translation = player;
  return request;
}

SkillRequest SkillRequest::Builder::mark(const Vector2f& player)
{
  SkillRequest request;
  request.skill = SkillRequest::mark;
  request.target.translation = player;
  return request;
}

SkillRequest SkillRequest::Builder::observe(const Vector2f& point)
{
  SkillRequest request;
  request.skill = SkillRequest::observe;
  request.target.translation = point;
  return request;
}

SkillRequest SkillRequest::Builder::interceptBall(unsigned interceptionMethods, bool allowDive)
{
  SkillRequest request;
  request.skill = SkillRequest::interceptBall;
  request.interceptionMethods = interceptionMethods;
  request.allowDive = allowDive;
  return request;
}

SkillRequest SkillRequest::Builder::keeperDive(MotionRequest::Dive::Request diveRequest)
{
  SkillRequest request;
  request.skill = SkillRequest::keeperDive;
  request.diveRequest = diveRequest;
  return request;
}
