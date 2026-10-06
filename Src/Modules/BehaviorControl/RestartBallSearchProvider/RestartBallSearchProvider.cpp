/**
 * @file RestartBallSearchProvider.cpp
 */

#include "RestartBallSearchProvider.h"
#include "Math/BHMath.h"
#include "Streaming/TypeRegistry.h"
#include <algorithm>

MAKE_MODULE(RestartBallSearchProvider);

void RestartBallSearchProvider::update(RestartBallSearchContext& restartBallSearchContext)
{
  updateBallMemory();

  const RestartBallSearchType restartType = currentRestartType();

  restartBallSearchContext.restartType = restartType;
  restartBallSearchContext.candidates.clear();
  restartBallSearchContext.fromLiveBall = false;
  restartBallSearchContext.fromDropInFallback = false;
  restartBallSearchContext.ownerRobotNumber = -1;

  if(restartType == restartSearchNone)
  {
    // Outside restarts, the context just shares the local ball memory with teammates.
    restartBallSearchContext.frozenForCurrentRestart = false;
    restartBallSearchContext.valid = ballMemory.valid &&
                                     theFrameInfo.getTimeSince(ballMemory.timestamp) <= static_cast<int>(memoryMaxAgeForRanking);
    restartBallSearchContext.fromLiveBall = restartBallSearchContext.valid;
    restartBallSearchContext.rememberedPositionOnField = ballMemory.position;
    restartBallSearchContext.sourceTimestamp = ballMemory.timestamp;
    restartBallSearchContext.sourceRobotNumber = ballMemory.sourceRobotNumber;
  }
  else
  {
    // During a restart, the newest memory across the team ranks the rule-based candidates.
    const Memory memory = bestTeamMemory();
    restartBallSearchContext.frozenForCurrentRestart = true;
    restartBallSearchContext.rememberedPositionOnField = memory.position;
    restartBallSearchContext.sourceTimestamp = memory.timestamp;
    restartBallSearchContext.sourceRobotNumber = memory.sourceRobotNumber;
    restartBallSearchContext.candidates = computeCandidates(restartType, memory);
    restartBallSearchContext.valid = !restartBallSearchContext.candidates.empty();
    restartBallSearchContext.fromDropInFallback = RestartBallSearchContext::isRefereePlaced(restartType) &&
                                                  dropInObservationIsFresh();
  }

  restartBallSearchContext.regionIndex = restartBallSearchContext.valid ?
                                         positionToRegion(!restartBallSearchContext.candidates.empty() ?
                                                          restartBallSearchContext.candidates.front() :
                                                          restartBallSearchContext.rememberedPositionOnField) :
                                         -1;

  DECLARE_DEBUG_DRAWING("module:RestartBallSearchProvider:context", "drawingOnField");
  COMPLEX_DRAWING("module:RestartBallSearchProvider:context")
  {
    if(restartBallSearchContext.valid)
    {
      for(std::size_t i = 0; i < restartBallSearchContext.candidates.size(); ++i)
      {
        const Vector2f& candidate = restartBallSearchContext.candidates[i];
        CROSS("module:RestartBallSearchProvider:context", candidate.x(), candidate.y(),
              150, 20, Drawings::solidPen, i == 0 ? ColorRGBA::red : ColorRGBA::orange);
      }
      CROSS("module:RestartBallSearchProvider:context",
            restartBallSearchContext.rememberedPositionOnField.x(),
            restartBallSearchContext.rememberedPositionOnField.y(),
            100, 20, Drawings::solidPen, ColorRGBA::yellow);
      DRAW_TEXT("module:RestartBallSearchProvider:context",
                restartBallSearchContext.rememberedPositionOnField.x() + 100.f,
                restartBallSearchContext.rememberedPositionOnField.y() - 100.f,
                120, ColorRGBA::white,
                TypeRegistry::getEnumName(restartBallSearchContext.restartType)
                << " src=" << restartBallSearchContext.sourceRobotNumber);
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

void RestartBallSearchProvider::updateBallMemory()
{
  if(theTeammatesBallModel.isValid &&
     isWithinPlayableField(theTeammatesBallModel.position) &&
     (!ballMemory.valid || theTeammatesBallModel.timeWhenLastSeen > ballMemory.timestamp))
  {
    ballMemory.valid = true;
    ballMemory.position = theTeammatesBallModel.position;
    ballMemory.timestamp = theTeammatesBallModel.timeWhenLastSeen;
    ballMemory.sourceRobotNumber = 0;
  }

  const unsigned ownBallTimestamp = theFrameInfo.time - static_cast<unsigned>(std::max(0, theFieldBall.timeSinceBallWasSeen));
  if(theFieldBall.ballWasSeen(500) &&
     isWithinPlayableField(theFieldBall.positionOnField) &&
     (!ballMemory.valid || ownBallTimestamp > ballMemory.timestamp))
  {
    ballMemory.valid = true;
    ballMemory.position = theFieldBall.positionOnField;
    ballMemory.timestamp = ownBallTimestamp;
    ballMemory.sourceRobotNumber = theGameState.playerNumber;
  }
}

RestartBallSearchProvider::Memory RestartBallSearchProvider::bestTeamMemory() const
{
  Memory best = ballMemory;
  for(const ReceivedTeamMessage& message : theReceivedTeamMessages.messages)
  {
    const BehaviorStatus& status = message.theBehaviorStatus;
    if(!status.restartMemoryValid)
      continue;
    if(!best.valid ||
       status.restartMemoryTimestamp > best.timestamp ||
       (status.restartMemoryTimestamp == best.timestamp && status.restartMemorySourceRobot >= 0 &&
        (best.sourceRobotNumber < 0 || status.restartMemorySourceRobot < best.sourceRobotNumber)))
    {
      best.valid = true;
      best.position = status.restartMemoryPositionOnField;
      best.timestamp = status.restartMemoryTimestamp;
      best.sourceRobotNumber = status.restartMemorySourceRobot;
    }
  }
  return best;
}

bool RestartBallSearchProvider::dropInObservationIsFresh() const
{
  return theBallDropInModel.lastTimeWhenBallOutWasObserved != 0 &&
         theFrameInfo.getTimeSince(theBallDropInModel.lastTimeWhenBallOutWasObserved) <= static_cast<int>(dropInMaxAge);
}

std::vector<Vector2f> RestartBallSearchProvider::computeCandidates(const RestartBallSearchType restartType, const Memory& memory) const
{
  std::vector<Vector2f> candidates;

  switch(restartType)
  {
    case restartSearchOwnCorner:
      candidates = {Vector2f(theFieldDimensions.xPosOpponentGroundLine - lineInset, theFieldDimensions.yPosLeftSideline - lineInset),
                    Vector2f(theFieldDimensions.xPosOpponentGroundLine - lineInset, theFieldDimensions.yPosRightSideline + lineInset)};
      rankBySide(candidates, restartType, memory);
      break;
    case restartSearchOpponentCorner:
      candidates = {Vector2f(theFieldDimensions.xPosOwnGroundLine + lineInset, theFieldDimensions.yPosLeftSideline - lineInset),
                    Vector2f(theFieldDimensions.xPosOwnGroundLine + lineInset, theFieldDimensions.yPosRightSideline + lineInset)};
      rankBySide(candidates, restartType, memory);
      break;
    case restartSearchOwnGoalKick:
      candidates = {Vector2f(theFieldDimensions.xPosOwnGoalArea, theFieldDimensions.yPosLeftGoalArea),
                    Vector2f(theFieldDimensions.xPosOwnGoalArea, theFieldDimensions.yPosRightGoalArea)};
      rankBySide(candidates, restartType, memory);
      break;
    case restartSearchOpponentGoalKick:
      candidates = {Vector2f(theFieldDimensions.xPosOpponentGoalArea, theFieldDimensions.yPosLeftGoalArea),
                    Vector2f(theFieldDimensions.xPosOpponentGoalArea, theFieldDimensions.yPosRightGoalArea)};
      rankBySide(candidates, restartType, memory);
      break;
    case restartSearchOwnKickIn:
    case restartSearchOpponentKickIn:
    {
      // The x coordinate where the ball left the field is the best estimate for where it is put back in.
      float x = 0.f;
      if(dropInObservationIsFresh())
        x = theBallDropInModel.outPosition.x();
      else if(memory.valid && theFrameInfo.getTimeSince(memory.timestamp) <= static_cast<int>(memoryMaxAgeForRanking))
        x = memory.position.x();
      x = std::clamp(x, theFieldDimensions.xPosOwnGroundLine + kickInXMargin, theFieldDimensions.xPosOpponentGroundLine - kickInXMargin);
      candidates = {Vector2f(x, theFieldDimensions.yPosLeftSideline - lineInset),
                    Vector2f(x, theFieldDimensions.yPosRightSideline + lineInset)};
      rankBySide(candidates, restartType, memory);
      break;
    }
    case restartSearchOwnDirectFreeKick:
    case restartSearchOpponentDirectFreeKick:
    case restartSearchOwnIndirectFreeKick:
    case restartSearchOpponentIndirectFreeKick:
      // The ball stays where the foul happened, so the last known position is the candidate.
      if(memory.valid && theFrameInfo.getTimeSince(memory.timestamp) <= static_cast<int>(memoryMaxAgeForFreeKick))
        candidates = {clipToPlayableField(memory.position)};
      break;
    case restartSearchOwnPenaltyKick:
      candidates = {Vector2f(theFieldDimensions.xPosOpponentPenaltyMark, 0.f)};
      break;
    case restartSearchOpponentPenaltyKick:
      candidates = {Vector2f(theFieldDimensions.xPosOwnPenaltyMark, 0.f)};
      break;
    default:
      break;
  }

  return candidates;
}

void RestartBallSearchProvider::rankBySide(std::vector<Vector2f>& candidates, const RestartBallSearchType, const Memory& memory) const
{
  if(candidates.size() < 2)
    return;

  // The side where the ball went out (or was last known) is searched first.
  float referenceY = 0.f;
  bool haveReference = false;
  if(dropInObservationIsFresh())
  {
    referenceY = theBallDropInModel.outPosition.y();
    haveReference = true;
  }
  else if(memory.valid && theFrameInfo.getTimeSince(memory.timestamp) <= static_cast<int>(memoryMaxAgeForRanking))
  {
    referenceY = memory.position.y();
    haveReference = true;
  }

  if(haveReference && referenceY < 0.f && candidates[0].y() > candidates[1].y())
    std::swap(candidates[0], candidates[1]);
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
  return Vector2f(std::clamp(position.x(), theFieldDimensions.xPosOwnGroundLine + lineInset, theFieldDimensions.xPosOpponentGroundLine - lineInset),
                  std::clamp(position.y(), theFieldDimensions.yPosRightSideline + lineInset, theFieldDimensions.yPosLeftSideline - lineInset));
}

int RestartBallSearchProvider::positionToRegion(const Vector2f& rawPosition) const
{
  const float width = (theFieldDimensions.xPosOpponentFieldBorder - theFieldDimensions.xPosOwnFieldBorder) / 4.f;
  const float height = (theFieldDimensions.yPosLeftFieldBorder - theFieldDimensions.yPosRightFieldBorder) / 4.f;
  const int x = std::clamp(static_cast<int>((rawPosition.x() - theFieldDimensions.xPosOwnFieldBorder) / width), 0, 3);
  const int y = std::clamp(static_cast<int>((rawPosition.y() - theFieldDimensions.yPosRightFieldBorder) / height), 0, 3);
  return y * 4 + x;
}
