#pragma once

#include "Math/Eigen.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include <vector>

ENUM(RestartBallSearchType,
{,
  restartSearchNone,
  restartSearchOwnCorner,
  restartSearchOpponentCorner,
  restartSearchOwnGoalKick,
  restartSearchOpponentGoalKick,
  restartSearchOwnKickIn,
  restartSearchOpponentKickIn,
  restartSearchOwnDirectFreeKick,
  restartSearchOpponentDirectFreeKick,
  restartSearchOwnIndirectFreeKick,
  restartSearchOpponentIndirectFreeKick,
  restartSearchOwnPenaltyKick,
  restartSearchOpponentPenaltyKick,
});

STREAMABLE(RestartBallSearchContext,
{
  /** Whether the ball is placed by the referee at a rule-defined spot for this restart
      (corner kick, goal kick, kick-in). For free kicks the ball stays at the foul position. */
  static bool isRefereePlaced(RestartBallSearchType type)
  {
    return type == restartSearchOwnCorner ||
           type == restartSearchOpponentCorner ||
           type == restartSearchOwnGoalKick ||
           type == restartSearchOpponentGoalKick ||
           type == restartSearchOwnKickIn ||
           type == restartSearchOpponentKickIn;
  },

  (RestartBallSearchType) restartType,
  (bool)(false) valid,
  (bool)(false) frozenForCurrentRestart,
  (bool)(false) fromLiveBall,
  (bool)(false) fromDropInFallback,
  (int)(-1) regionIndex,
  (Vector2f)(Vector2f::Zero()) rememberedPositionOnField, /**< Team-consensus memory of the last known ball position (shared via BehaviorStatus, used to rank candidates and as the free kick candidate). */
  (std::vector<Vector2f>) candidates, /**< Rule-based ball placement candidates for the current restart, ranked most likely first. */
  (unsigned)(0) sourceTimestamp,
  (int)(-1) sourceRobotNumber,
  (int)(-1) ownerRobotNumber,
});
