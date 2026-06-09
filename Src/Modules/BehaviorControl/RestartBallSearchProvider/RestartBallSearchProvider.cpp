/**
 * @file RestartBallSearchProvider.cpp
 */

#include "RestartBallSearchProvider.h"
#include "Math/BHMath.h"
#include "Streaming/TypeRegistry.h"
#include <limits>

MAKE_MODULE(RestartBallSearchProvider);

namespace
{
  bool isBetterCandidate(const RestartBallSearchProvider::Candidate& lhs,
                         const RestartBallSearchProvider::Candidate& rhs,
                         RestartBallSearchType expectedType)
  {
    const bool lhsValid = lhs.context.valid &&
                          (expectedType == restartSearchNone || lhs.context.restartType == expectedType);
    const bool rhsValid = rhs.context.valid &&
                          (expectedType == restartSearchNone || rhs.context.restartType == expectedType);
    if(lhsValid != rhsValid)
      return lhsValid;
    if(!lhsValid)
      return false;
    if(lhs.context.sourceTimestamp != rhs.context.sourceTimestamp)
      return lhs.context.sourceTimestamp > rhs.context.sourceTimestamp;
    if(lhs.holderRobotNumber == rhs.holderRobotNumber)
      return false;
    if(lhs.holderRobotNumber < 0)
      return true;
    if(rhs.holderRobotNumber < 0)
      return false;
    return lhs.holderRobotNumber < rhs.holderRobotNumber;
  }
}

void RestartBallSearchProvider::update(RestartBallSearchContext& restartBallSearchContext)
{
  const RestartBallSearchType restartType = currentRestartType();
  const Candidate liveCandidate = makeLiveCandidate();
  const bool liveBallReacquired = liveCandidate.context.valid &&
                                  liveCandidate.context.fromLiveBall &&
                                  theFrameInfo.getTimeSince(liveCandidate.context.sourceTimestamp) <= reacquiredBallMaxAge;

  if(restartType == restartSearchNone)
  {
    localContext = liveCandidate.context;
  }
  else if(restartType != lastRestartType)
  {
    clearLocalContext(restartType);

    if(restartType == restartSearchOwnPenaltyKick || restartType == restartSearchOpponentPenaltyKick)
      localContext = makePenaltyCandidate(restartType).context;
    else if(liveCandidate.context.valid)
    {
      localContext = liveCandidate.context;
      localContext.restartType = restartType;
      localContext.frozenForCurrentRestart = true;
    }
    else if(const Candidate dropInCandidate = makeDropInCandidate(restartType); dropInCandidate.context.valid)
    {
      localContext = dropInCandidate.context;
      localContext.frozenForCurrentRestart = true;
    }
  }
  else if(localContext.frozenForCurrentRestart && liveBallReacquired)
  {
    localContext = liveCandidate.context;
    localContext.restartType = restartType;
  }

  if(localContext.valid && localContext.sourceTimestamp &&
     theFrameInfo.getTimeSince(localContext.sourceTimestamp) > frozenContextMaxAge &&
     localContext.restartType != restartSearchOwnPenaltyKick &&
     localContext.restartType != restartSearchOpponentPenaltyKick)
  {
    localContext.valid = false;
    localContext.frozenForCurrentRestart = false;
  }

  std::vector<Candidate> candidates;
  candidates.reserve(theReceivedTeamMessages.messages.size() + 1);
  candidates.push_back({localContext, theGameState.playerNumber});
  for(const ReceivedTeamMessage& message : theReceivedTeamMessages.messages)
    candidates.push_back(makeCandidateFromBehaviorStatus(message.number, message.theBehaviorStatus));

  restartBallSearchContext = selectNewestCandidate(candidates, restartType).context;
  restartBallSearchContext.ownerRobotNumber = -1;

  if(restartBallSearchContext.valid)
  {
    restartBallSearchContext.regionIndex = positionToRegion(restartBallSearchContext.rememberedPositionOnField);
    restartBallSearchContext.rememberedPositionOnField = clipToPlayableField(restartBallSearchContext.rememberedPositionOnField);
  }

  lastRestartType = restartType;

  DECLARE_DEBUG_DRAWING("module:RestartBallSearchProvider:context", "drawingOnField");
  COMPLEX_DRAWING("module:RestartBallSearchProvider:context")
  {
    if(restartBallSearchContext.valid)
    {
      CROSS("module:RestartBallSearchProvider:context",
            restartBallSearchContext.rememberedPositionOnField.x(),
            restartBallSearchContext.rememberedPositionOnField.y(),
            150, 20, Drawings::solidPen,
            restartBallSearchContext.frozenForCurrentRestart ? ColorRGBA::red : ColorRGBA::yellow);
      DRAW_TEXT("module:RestartBallSearchProvider:context",
                restartBallSearchContext.rememberedPositionOnField.x() + 100.f,
                restartBallSearchContext.rememberedPositionOnField.y() - 100.f,
                120, ColorRGBA::white,
                TypeRegistry::getEnumName(restartBallSearchContext.restartType)
                << " r=" << restartBallSearchContext.regionIndex
                << " src=" << restartBallSearchContext.sourceRobotNumber
                << " frozen=" << restartBallSearchContext.frozenForCurrentRestart);
    }
  }
}

RestartBallSearchType RestartBallSearchProvider::currentRestartType() const
{
  switch(theGameState.state)
  {
    case GameState::ownCornerKick: return restartSearchOwnCorner;
    case GameState::opponentCornerKick: return restartSearchOpponentCorner;
    case GameState::ownGoalKick: return restartSearchOwnGoalKick;
    case GameState::opponentGoalKick: return restartSearchOpponentGoalKick;
    case GameState::ownThrowIn:
    case GameState::ownKickIn: return restartSearchOwnKickIn;
    case GameState::opponentThrowIn:
    case GameState::opponentKickIn: return restartSearchOpponentKickIn;
    case GameState::ownDirectFreeKick:
    case GameState::ownPushingFreeKick: return restartSearchOwnDirectFreeKick;
    case GameState::opponentDirectFreeKick:
    case GameState::opponentPushingFreeKick: return restartSearchOpponentDirectFreeKick;
    case GameState::ownIndirectFreeKick: return restartSearchOwnIndirectFreeKick;
    case GameState::opponentIndirectFreeKick: return restartSearchOpponentIndirectFreeKick;
    case GameState::ownPenaltyKick: return restartSearchOwnPenaltyKick;
    case GameState::opponentPenaltyKick: return restartSearchOpponentPenaltyKick;
    default: return restartSearchNone;
  }
}

bool RestartBallSearchProvider::isTouchlineOrGoalLineRestart(const RestartBallSearchType type) const
{
  return type == restartSearchOwnCorner ||
         type == restartSearchOpponentCorner ||
         type == restartSearchOwnGoalKick ||
         type == restartSearchOpponentGoalKick ||
         type == restartSearchOwnKickIn ||
         type == restartSearchOpponentKickIn;
}

bool RestartBallSearchProvider::isWithinPlayableField(const Vector2f& position) const
{
  return position.x() >= theFieldDimensions.xPosOwnFieldBorder &&
         position.x() <= theFieldDimensions.xPosOpponentFieldBorder &&
         position.y() >= theFieldDimensions.yPosRightFieldBorder &&
         position.y() <= theFieldDimensions.yPosLeftFieldBorder;
}

Vector2f RestartBallSearchProvider::clipToPlayableField(const Vector2f& position) const
{
  return Vector2f(std::clamp(position.x(), theFieldDimensions.xPosOwnFieldBorder, theFieldDimensions.xPosOpponentFieldBorder),
                  std::clamp(position.y(), theFieldDimensions.yPosRightFieldBorder, theFieldDimensions.yPosLeftFieldBorder));
}

int RestartBallSearchProvider::positionToRegion(const Vector2f& rawPosition) const
{
  const Vector2f position = clipToPlayableField(rawPosition);
  const float width = (theFieldDimensions.xPosOpponentFieldBorder - theFieldDimensions.xPosOwnFieldBorder) / 4.f;
  const float height = (theFieldDimensions.yPosLeftFieldBorder - theFieldDimensions.yPosRightFieldBorder) / 4.f;
  const int x = std::clamp(static_cast<int>((position.x() - theFieldDimensions.xPosOwnFieldBorder) / width), 0, 3);
  const int y = std::clamp(static_cast<int>((position.y() - theFieldDimensions.yPosRightFieldBorder) / height), 0, 3);
  return y * 4 + x;
}

RestartBallSearchProvider::Candidate RestartBallSearchProvider::makeLiveCandidate() const
{
  Candidate candidate;
  candidate.holderRobotNumber = theGameState.playerNumber;
  candidate.context.restartType = currentRestartType();

  if(theTeammatesBallModel.isValid &&
     theFrameInfo.getTimeSince(theTeammatesBallModel.timeWhenLastSeen) <= liveBallMaxAge &&
     isWithinPlayableField(theTeammatesBallModel.position))
  {
    candidate.context.valid = true;
    candidate.context.fromLiveBall = true;
    candidate.context.rememberedPositionOnField = clipToPlayableField(theTeammatesBallModel.position);
    candidate.context.sourceTimestamp = theTeammatesBallModel.timeWhenLastSeen;
    candidate.context.sourceRobotNumber = 0;
    candidate.context.regionIndex = positionToRegion(candidate.context.rememberedPositionOnField);
  }

  const unsigned ownBallTimestamp = theFrameInfo.time - static_cast<unsigned>(std::max(0, theFieldBall.timeSinceBallWasSeen));
  if(theFieldBall.ballWasSeen(static_cast<int>(liveBallMaxAge)) &&
     isWithinPlayableField(theFieldBall.positionOnField) &&
     (!candidate.context.valid || ownBallTimestamp > candidate.context.sourceTimestamp))
  {
    candidate.context.valid = true;
    candidate.context.fromLiveBall = true;
    candidate.context.rememberedPositionOnField = clipToPlayableField(theFieldBall.positionOnField);
    candidate.context.sourceTimestamp = ownBallTimestamp;
    candidate.context.sourceRobotNumber = theGameState.playerNumber;
    candidate.context.regionIndex = positionToRegion(candidate.context.rememberedPositionOnField);
  }

  return candidate;
}

RestartBallSearchProvider::Candidate RestartBallSearchProvider::makeDropInCandidate(const RestartBallSearchType restartType) const
{
  Candidate candidate;
  candidate.holderRobotNumber = theGameState.playerNumber;
  candidate.context.restartType = restartType;

  if(!isTouchlineOrGoalLineRestart(restartType) ||
     !theBallDropInModel.isValid ||
     theFrameInfo.getTimeSince(theBallDropInModel.lastTimeWhenBallWentOut) > dropInMaxAge)
    return candidate;

  const Vector2f position = !theBallDropInModel.dropInPositions.empty() ?
                            theBallDropInModel.dropInPositions.front() :
                            theBallDropInModel.outPosition;
  if(!isWithinPlayableField(position))
    return candidate;

  candidate.context.valid = true;
  candidate.context.frozenForCurrentRestart = true;
  candidate.context.fromDropInFallback = true;
  candidate.context.rememberedPositionOnField = clipToPlayableField(position);
  candidate.context.sourceTimestamp = theBallDropInModel.lastTimeWhenBallWentOut;
  candidate.context.sourceRobotNumber = theGameState.playerNumber;
  candidate.context.regionIndex = positionToRegion(candidate.context.rememberedPositionOnField);
  return candidate;
}

RestartBallSearchProvider::Candidate RestartBallSearchProvider::makePenaltyCandidate(const RestartBallSearchType restartType) const
{
  Candidate candidate;
  candidate.holderRobotNumber = theGameState.playerNumber;
  candidate.context.restartType = restartType;
  candidate.context.valid = true;
  candidate.context.frozenForCurrentRestart = true;
  candidate.context.sourceTimestamp = theFrameInfo.time;
  candidate.context.sourceRobotNumber = theGameState.playerNumber;
  candidate.context.rememberedPositionOnField =
    Vector2f(restartType == restartSearchOwnPenaltyKick ? theFieldDimensions.xPosOpponentPenaltyMark : theFieldDimensions.xPosOwnPenaltyMark, 0.f);
  candidate.context.regionIndex = positionToRegion(candidate.context.rememberedPositionOnField);
  return candidate;
}

RestartBallSearchProvider::Candidate RestartBallSearchProvider::makeCandidateFromBehaviorStatus(const int holderRobotNumber,
                                                                                                 const BehaviorStatus& behaviorStatus) const
{
  Candidate candidate;
  candidate.holderRobotNumber = holderRobotNumber;
  candidate.context.restartType = behaviorStatus.restartMemoryType;
  candidate.context.valid = behaviorStatus.restartMemoryValid;
  candidate.context.frozenForCurrentRestart = behaviorStatus.restartMemoryFrozen;
  candidate.context.fromLiveBall = behaviorStatus.restartMemoryFromLiveBall;
  candidate.context.fromDropInFallback = behaviorStatus.restartMemoryFromDropInFallback;
  candidate.context.regionIndex = behaviorStatus.restartMemoryRegionIndex;
  candidate.context.rememberedPositionOnField = behaviorStatus.restartMemoryPositionOnField;
  candidate.context.sourceTimestamp = behaviorStatus.restartMemoryTimestamp;
  candidate.context.sourceRobotNumber = behaviorStatus.restartMemorySourceRobot;
  return candidate;
}

RestartBallSearchProvider::Candidate RestartBallSearchProvider::selectNewestCandidate(const std::vector<Candidate>& candidates,
                                                                                      const RestartBallSearchType expectedType) const
{
  Candidate best;
  for(const Candidate& candidate : candidates)
    if(isBetterCandidate(candidate, best, expectedType))
      best = candidate;

  if(best.context.valid &&
     best.context.restartType == restartSearchNone &&
     expectedType != restartSearchNone)
    best.context.restartType = expectedType;
  return best;
}

void RestartBallSearchProvider::clearLocalContext(const RestartBallSearchType restartType)
{
  localContext = RestartBallSearchContext();
  localContext.restartType = restartType;
}
