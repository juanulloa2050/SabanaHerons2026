/**
 * @file BallSearch.cpp
 *
 * This file implements a ball search behavior.
 *
 * @author Arne Hasselbring, Sina Schreiber
 */

#include "BallSearch.h"
#include "Representations/BehaviorControl/Skills.h"

BallSearch::BallSearch() :
  Cabsl(&activationGraph)
{
}

void BallSearch::preProcess()
{
  const float usedRefereeBallPlacementAccuracy = refereeBallPlacementAccuracy + (ballUnknown ? 0.f : 200.f);

  teamBallNearRestartCandidate = false;
  ownBallNearRestartCandidate = false;
  if(theRestartBallSearchContext.valid)
  {
    for(const Vector2f& candidate : theRestartBallSearchContext.candidates)
    {
      if(theTeammatesBallModel.isValid &&
         (candidate - theTeammatesBallModel.position).squaredNorm() < sqr(usedRefereeBallPlacementAccuracy))
        teamBallNearRestartCandidate = true;
      if((candidate - theFieldBall.positionOnField).squaredNorm() < sqr(usedRefereeBallPlacementAccuracy))
        ownBallNearRestartCandidate = true;
    }
  }

  beginFrame(theFrameInfo.time);
}

void BallSearch::postProcess()
{
  endFrame();
  MODIFY("behavior:BallSearch", activationGraph);
}

SkillRequest BallSearch::execute(const Agent& agent, const Agents& otherAgents)
{
  this->agent = &agent;
  agents = otherAgents;
  // Execute behavior
  Root();
  this->agent = nullptr;

  return skillRequest;
}

SkillRequest BallSearch::decidePositionForSearch(const Vector2f leftPosition, const Vector2f rightPosition, const Agent& agent, const Agents& otherAgents)
{
  if(agent.isGoalkeeper)
    return SkillRequest::Builder::walkTo(agent.basePose);

  const bool leftPositionIsInVoronoi = Geometry::isPointInsidePolygon(leftPosition, agent.baseArea);
  const bool rightPositionIsInVoronoi = Geometry::isPointInsidePolygon(rightPosition, agent.baseArea);

  const float distanceToLeftPosition = (agent.currentPosition - leftPosition).squaredNorm();
  const float distanceToRightPosition = (agent.currentPosition - rightPosition).squaredNorm();

  if(leftPositionIsInVoronoi && rightPositionIsInVoronoi)
    return distanceToLeftPosition < distanceToRightPosition ? SkillRequest::Builder::observe(leftPosition) : SkillRequest::Builder::observe(rightPosition);
  else if(leftPositionIsInVoronoi)
    return SkillRequest::Builder::observe(leftPosition);
  else if(rightPositionIsInVoronoi)
    return SkillRequest::Builder::observe(rightPosition);

  bool isClosestToRightPosition = true;
  bool isClosestToLeftPosition = true;

  const Agent* agentWithBothPositions = nullptr;

  for(const Agent* agent : otherAgents)
  {
    if(agent->isGoalkeeper)
      continue;
    const bool isLeftPositionInsideVoronoiOfOtherAgent = Geometry::isPointInsidePolygon(leftPosition, agent->baseArea);
    const bool isRightPositionInsideVoronoiOfOtherAgent = Geometry::isPointInsidePolygon(rightPosition, agent->baseArea);

    float distanceFromOtherAgentToLeftPosition = (agent->currentPosition - leftPosition).squaredNorm();
    float distanceFromOtherAgentToRightPosition = (agent->currentPosition - rightPosition).squaredNorm();

    if(isLeftPositionInsideVoronoiOfOtherAgent && !isRightPositionInsideVoronoiOfOtherAgent)
      isClosestToLeftPosition = false;
    else if(isRightPositionInsideVoronoiOfOtherAgent && !isLeftPositionInsideVoronoiOfOtherAgent)
      isClosestToRightPosition = false;
    else if(isLeftPositionInsideVoronoiOfOtherAgent && isRightPositionInsideVoronoiOfOtherAgent)
    {
      agentWithBothPositions = agent;
      const bool otherAgentWillChooseLeftPosition = distanceFromOtherAgentToLeftPosition < distanceFromOtherAgentToRightPosition;
      isClosestToLeftPosition &= !otherAgentWillChooseLeftPosition;
      isClosestToRightPosition &= otherAgentWillChooseLeftPosition;
    }
    else
    {
      //Check if this robot is closest to one of the positions
      if(isClosestToLeftPosition && distanceFromOtherAgentToLeftPosition < distanceToLeftPosition)
        isClosestToLeftPosition = false;
      if(isClosestToRightPosition && distanceFromOtherAgentToRightPosition < distanceToRightPosition)
        isClosestToRightPosition = false;
    }
  }

  if(!isClosestToLeftPosition && !isClosestToRightPosition)
    return SkillRequest::Builder::walkTo(agent.basePose);
  else if(agentWithBothPositions)
  {
    if(isClosestToRightPosition)
      return SkillRequest::Builder::observe(rightPosition);
    else if(isClosestToLeftPosition)
      return SkillRequest::Builder::observe(leftPosition);
  }

  return SkillRequest::Builder::walkTo(agent.basePose);
}
