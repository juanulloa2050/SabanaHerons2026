#pragma once

#include "Math/Eigen.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"

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
{,
  (RestartBallSearchType) restartType,
  (bool)(false) valid,
  (bool)(false) frozenForCurrentRestart,
  (bool)(false) fromLiveBall,
  (bool)(false) fromDropInFallback,
  (int)(-1) regionIndex,
  (Vector2f)(Vector2f::Zero()) rememberedPositionOnField,
  (unsigned)(0) sourceTimestamp,
  (int)(-1) sourceRobotNumber,
  (int)(-1) ownerRobotNumber,
});
