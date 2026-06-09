/**
 * @file RestartBallSearchProvider.h
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/BehaviorControl/RestartBallSearchContext.h"
#include "Representations/Communication/ReceivedTeamMessages.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/BallDropInModel.h"
#include "Representations/Modeling/TeammatesBallModel.h"

MODULE(RestartBallSearchProvider,
{,
  REQUIRES(BallDropInModel),
  REQUIRES(FieldBall),
  REQUIRES(FieldDimensions),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(ReceivedTeamMessages),
  REQUIRES(TeammatesBallModel),
  PROVIDES(RestartBallSearchContext),
  DEFINES_PARAMETERS(
  {,
    (unsigned)(4000) liveBallMaxAge,
    (unsigned)(6000) frozenContextMaxAge,
    (unsigned)(4000) dropInMaxAge,
    (unsigned)(1000) reacquiredBallMaxAge,
  }),
};

class RestartBallSearchProvider : public RestartBallSearchProviderBase
{
public:
  RestartBallSearchProvider() = default;
  struct Candidate
  {
    RestartBallSearchContext context;
    int holderRobotNumber = -1;
  };

private:
  RestartBallSearchContext localContext;
  RestartBallSearchType lastRestartType = restartSearchNone;

  void update(RestartBallSearchContext& restartBallSearchContext) override;

  RestartBallSearchType currentRestartType() const;
  bool isTouchlineOrGoalLineRestart(RestartBallSearchType type) const;
  bool isWithinPlayableField(const Vector2f& position) const;
  Vector2f clipToPlayableField(const Vector2f& position) const;
  int positionToRegion(const Vector2f& position) const;
  Candidate makeLiveCandidate() const;
  Candidate makeDropInCandidate(RestartBallSearchType restartType) const;
  Candidate makePenaltyCandidate(RestartBallSearchType restartType) const;
  Candidate makeCandidateFromBehaviorStatus(int holderRobotNumber, const BehaviorStatus& behaviorStatus) const;
  Candidate selectNewestCandidate(const std::vector<Candidate>& candidates, RestartBallSearchType expectedType) const;
  void clearLocalContext(RestartBallSearchType restartType);
});
