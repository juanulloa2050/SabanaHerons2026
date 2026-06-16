/**
 * @file RestartBallSearchProvider.h
 *
 * Provides rule-based ball placement candidates for restart situations (corner kick,
 * goal kick, kick-in, free kicks). For referee-placed restarts the candidates are the
 * spots where the rules put the ball; the team-shared memory of the last known ball
 * position is only used to rank them. For in-place free kicks the memory itself is
 * the candidate, since the ball stays at the foul position.
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
    (unsigned)(15000) memoryMaxAgeForFreeKick, /**< Memory older than this is not used as the free kick search candidate. */
    (unsigned)(25000) memoryMaxAgeForRanking, /**< Memory older than this does not influence candidate ranking. */
    (unsigned)(10000) dropInMaxAge, /**< Drop-in observations older than this do not influence candidates. */
    (float)(150.f) lineInset, /**< Candidates on field lines are placed this far inside the field. */
    (float)(700.f) kickInXMargin, /**< Kick-in candidates keep this distance from the ground lines along the touchline. */
  }),
};

class RestartBallSearchProvider : public RestartBallSearchProviderBase
{
public:
  RestartBallSearchProvider() = default;

private:
  struct Memory
  {
    bool valid = false;
    Vector2f position = Vector2f::Zero();
    unsigned timestamp = 0;
    int sourceRobotNumber = -1;
  };

  Memory ballMemory; /**< Continuously updated local memory of the last known ball position. */

  void update(RestartBallSearchContext& restartBallSearchContext) override;

  RestartBallSearchType currentRestartType() const;
  void updateBallMemory();
  Memory bestTeamMemory() const;
  std::vector<Vector2f> computeCandidates(RestartBallSearchType restartType, const Memory& memory) const;
  void rankBySide(std::vector<Vector2f>& candidates, RestartBallSearchType restartType, const Memory& memory) const;
  bool dropInObservationIsFresh() const;
  bool isWithinPlayableField(const Vector2f& position) const;
  Vector2f clipToPlayableField(const Vector2f& position) const;
  int positionToRegion(const Vector2f& position) const;
});
