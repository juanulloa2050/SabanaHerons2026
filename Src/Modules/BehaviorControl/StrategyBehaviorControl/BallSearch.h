/**
 * @file BallSearch.h
 *
 * This file declares a ball search behavior.
 *
 * During restart situations (corner kick, goal kick, kick-in, free kicks) the search
 * is driven by the rule-based placement candidates from RestartBallSearchContext:
 * with the limited ball detection range (~2-4 m), robots must get a camera close to
 * every spot where the rules may have put the ball. ObservePoint stops about 1.2 m
 * short of its target and scans, which is inside the reliable detection range.
 *
 * @author Arne Hasselbring
 */

#pragma once

#include "Debugging/Annotation.h"
#include "Tools/BehaviorControl/Strategy/Agent.h"
#include "Tools/BehaviorControl/Strategy/BehaviorBase.h"
#include "Representations/BehaviorControl/SkillRequest.h"
#include "Math/Geometry.h"
#include <algorithm>
#include <array>
#include <regex> // not needed in header, but would otherwise be broken by CABSL

class BallSearch;

#include "Representations/BehaviorControl/Skills.h"
#ifdef __INTELLISENSE__
#define INTELLISENSE_PREFIX BallSearch::
#endif
#include "Tools/Cabsl.h"

class BallSearch : public Cabsl<BallSearch>, public BehaviorBase
{
public:
  bool ballUnknown = false;
  const int refereeBallPlacementDelay = 5000;
  const float refereeBallPlacementAccuracy = 400.f;
  /** Whether the team ball model is close to one of the current restart candidates. */
  bool teamBallNearRestartCandidate = false;
  /** Whether the own ball model is close to one of the current restart candidates. */
  bool ownBallNearRestartCandidate = false;

  const int soloSwapInterval = 8000; /**< A solo searcher switches to the other candidate at this interval (in ms). */
  const int kickInSweepInterval = 6000; /**< During kick-in search, the observed point moves along the touchline at this interval (in ms). */
  const float kickInSweepStep = 2000.f; /**< Distance between consecutive sweep points along the touchline. */
  const int freeKickOrbitDelay = 8000; /**< Time observing the remembered foul position before orbiting around it (in ms). */
  const int freeKickOrbitInterval = 5000; /**< Time per orbit vantage point around the foul position (in ms). */
  const float freeKickOrbitRadius = 1500.f; /**< Radius of the orbit around the foul position. */

  BallSearch();

  void preProcess() override;

  void postProcess() override;

  SkillRequest execute(const Agent& agent, const Agents& otherAgents);

  SkillRequest decidePositionForSearch(const Vector2f leftPosition, const Vector2f rightPosition, const Agent& agent, const Agents& otherAgents);

private:
  ActivationGraph activationGraph;

  // skill request for ballSearch behavior
  SkillRequest skillRequest;
  Agents agents;
  const float groundLineXOffset = 50.f;
  const float minRadius = 20.f;
  float initialRadius;
  const Agent* agent;

  /** Time since the current restart started (in ms), 0 outside restarts. */
  int timeInRestart() const
  {
    return theGameState.isFreeKick() || theGameState.isPenaltyKick() ?
           std::max(0, theFrameInfo.getTimeSince(theGameState.timeWhenStateStarted)) : 0;
  }

  /** Whether no other field player can take part in the search. */
  bool isSoloSearcher() const
  {
    for(const Agent* other : agents)
      if(!other->isGoalkeeper && other->isUpright)
        return false;
    return true;
  }

  /** Whether this robot is the searcher closest to the given position (deterministic across robots). */
  bool isClosestSearcher(const Vector2f& position) const
  {
    const float ownDistance = (agent->currentPosition - position).squaredNorm();
    for(const Agent* other : agents)
    {
      if(other->isGoalkeeper || !other->isUpright)
        continue;
      const float otherDistance = (other->currentPosition - position).squaredNorm();
      if(otherDistance < ownDistance ||
         (otherDistance == ownDistance && other->number < agent->number))
        return false;
    }
    return true;
  }

  /** Search request for a restart with two placement candidates (corners, goal area corners, touchlines). */
  SkillRequest twoCandidateSearch(Vector2f first, Vector2f second)
  {
    if(theGameState.isKickIn())
    {
      // The x where the ball went out is uncertain: sweep the observed point along the
      // touchline over time (0, +step, -step, +2*step, -2*step, ...).
      const int phase = timeInRestart() / kickInSweepInterval;
      const float offset = (phase + 1) / 2 * ((phase & 1) ? kickInSweepStep : -kickInSweepStep);
      const float minX = theFieldDimensions.xPosOwnGroundLine + 700.f;
      const float maxX = theFieldDimensions.xPosOpponentGroundLine - 700.f;
      first.x() = std::clamp(first.x() + offset, minX, maxX);
      second.x() = std::clamp(second.x() + offset, minX, maxX);
    }

    // With no teammate to cover the second candidate, alternate between both over time,
    // starting with the more likely one.
    if(isSoloSearcher())
      return SkillRequest::Builder::observe((timeInRestart() / soloSwapInterval) % 2 == 0 ? first : second);

    return decidePositionForSearch(first, second, *agent, agents);
  }

  /** Search request for a restart whose ball stays in place (free kicks): observe the
      remembered foul position, later orbit around it to handle occlusion and drift. */
  SkillRequest singleCandidateSearch(const Vector2f& candidate)
  {
    if(!isClosestSearcher(candidate))
      return SkillRequest::Builder::walkTo(Pose2f((candidate - agent->basePose.translation).angle(), agent->basePose.translation));

    const int elapsed = timeInRestart();
    if(elapsed <= freeKickOrbitDelay)
      return SkillRequest::Builder::observe(candidate);

    // Orbit vantage points around the foul position, starting on the own-goal side
    // (defensively sound and usually the side the robot comes from).
    const int orbitStep = (elapsed - freeKickOrbitDelay) / freeKickOrbitInterval;
    const Angle baseAngle = (Vector2f(theFieldDimensions.xPosOwnGroundLine, 0.f) - candidate).angle();
    const Angle orbitAngle = Angle(baseAngle + 90_deg * (1 + orbitStep / 2) * ((orbitStep & 1) ? 1.f : -1.f)).normalize();
    Vector2f target = candidate + Vector2f(freeKickOrbitRadius, 0.f).rotated(orbitAngle);
    theFieldDimensions.clipToField(target);
    return SkillRequest::Builder::observe(target);
  }

  option(Root)
  {
    const Pose2f goalCenterOnFieldWithOffset = Pose2f(0.f, theFieldDimensions.xPosOwnGroundLine + groundLineXOffset, 0.f);
    const Pose2f goalCenterRelativeWithOffset = theRobotPose.inverse() * goalCenterOnFieldWithOffset;
    const bool restartCandidatesAvailable = (theGameState.isFreeKick() || theGameState.isPenaltyKick()) &&
                                            theRestartBallSearchContext.valid &&
                                            !theRestartBallSearchContext.candidates.empty();
    //the initial ballSearch is used in non-standard situations
    initial_state(initial)
    {
      transition
      {
        ANNOTATION("BallSearch", "is active");
        if(agent->isGoalkeeper)
          goto goalkeeper;
        else if(restartCandidatesAvailable)
          goto restartSearch;
        else
          goto gridSearch;
      }
    }
    // if the ball is lost, the robot will use the ballSearchAreas Grid to search the ball.
    state(gridSearch)
    {
      transition
      {
        if(agent->isGoalkeeper)
          goto goalkeeper;
        else if(restartCandidatesAvailable)
          goto restartSearch;
      }
      action
      {
        skillRequest = SkillRequest::Builder::observe(theBallSearchAreas.cellToSearchNext(*agent));
      }
    }
    // search at the spots where the rules may have placed the ball for the current restart
    state(restartSearch)
    {
      transition
      {
        if(agent->isGoalkeeper)
          goto goalkeeper;
        else if(!restartCandidatesAvailable)
          goto gridSearch;
      }
      action
      {
        const std::vector<Vector2f>& candidates = theRestartBallSearchContext.candidates;
        if(candidates.size() >= 2)
          skillRequest = twoCandidateSearch(candidates[0], candidates[1]);
        else
          skillRequest = singleCandidateSearch(candidates[0]);
      }
    }
    //BallSearch Behavior for the Goalkeeper
    state(goalkeeper)
    {
      transition
      {
        {
          if((theGameState.isCornerKick() || theGameState.isGoalKick()) && theFrameInfo.getTimeSince(theGameState.timeWhenStateStarted) < 10000)
            goto goalkeeperStandardSituation;
          else if(theFieldBall.timeSinceBallWasSeen > 10000 && theRobotPose.translation.x() < theFieldDimensions.xPosOwnGroundLine)
            goto goalkeeperWalkToTarget;
          else if(theFieldBall.timeSinceBallWasSeen > 10000 && (std::abs(goalCenterRelativeWithOffset.translation.angle()) > 45_deg))
          {
            initialRadius = (theRobotPose.translation - Vector2f(theFieldDimensions.xPosOwnGroundLine, 0.f)).norm();;
            goto goalkeeperAvoidBall;
          }
          else if(theFieldBall.timeSinceBallWasSeen > 10000)
            goto goalkeeperTurnToTarget;
        }
      }
      action
      {
        skillRequest = SkillRequest::Builder::walkTo(agent->basePose);
      }
    }
    state(goalkeeperStandardSituation)
    {
      transition
      {
        if((!theGameState.isCornerKick() && !theGameState.isGoalKick()) || theFrameInfo.getTimeSince(theGameState.timeWhenStateStarted) > 10000)
        {
          goto goalkeeper;
        }
      }
      action
      {
        skillRequest = SkillRequest::Builder::stand();
      }
    }
    state(goalkeeperWalkToTarget)
    {
      action
      {
        skillRequest = SkillRequest::Builder::walkTo(goalCenterOnFieldWithOffset);
      }
    }
    state(goalkeeperTurnToTarget)
    {
      transition
      {
        if((std::abs(goalCenterRelativeWithOffset.translation.angle()) < 10_deg))
          goto goalkeeperWalkToTarget;
      }
      action
      {
        skillRequest = SkillRequest::Builder::observe((theRobotPose * goalCenterRelativeWithOffset).translation);
      }
    }
    state(goalkeeperAvoidBall)
    {
      transition
      {
        const float GoalkeeperToGoalLineCenter = (theRobotPose.translation - Vector2f(theFieldDimensions.xPosOwnGroundLine, 0.f)).norm();
        if(GoalkeeperToGoalLineCenter > initialRadius + minRadius)
          goto goalkeeperTurnToTarget;
      }
      action
      {
        const Vector2f goalCenterRelative(theRobotPose.inverse() * Vector2f(theFieldDimensions.xPosOwnGroundLine, 0.f));
        const Vector2f offsetRelative(goalCenterRelative.normalized(-200.f));
        const Vector2f offsetAbsolute(theRobotPose * offsetRelative);
        skillRequest = SkillRequest::Builder::walkTo(offsetAbsolute);
      }
    }
  }
};
#undef action
#undef transition
