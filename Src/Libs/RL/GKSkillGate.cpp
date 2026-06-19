#include "GKSkillGate.h"

#include <cmath>

namespace
{
  // Mask constants (defaults from goalkeeper_teacher.py; RL_GK_* env in Python).
  constexpr float standRadiusMm = 60.f;
  constexpr float walkRadiusMm = 125.f;
  constexpr float genuflectRadiusMm = 200.f;
  constexpr float jumpRadiusMm = 600.f;
  constexpr float interceptTimeMinS = 0.02f;
  constexpr float interceptTimeMaxS = 3.0f;
  constexpr float isectNorm = 1200.f; // BALL_CATCH_MAX_WALK_DISTANCE_MM (de-normalizes obs[21])
  constexpr bool keeperJumpingOn = true;

  std::size_t idx(const RLGK::GKSkillType s) { return static_cast<std::size_t>(s); }
}

std::array<float, RLGK::gkSkillCount> RLGK::GKSkillGate::mask(const std::array<float, gkObsSize>& obs) const
{
  std::array<float, gkSkillCount> m{};
  m.fill(0.f);

  using S = GKSkillType;
  m[idx(S::stand)] = 1.f;
  m[idx(S::walkGoaliePose)] = 1.f;
  m[idx(S::observe)] = (obs[28] >= 0.5f || obs[32] >= 0.5f) ? 1.f : 0.f;

  const bool rolling = obs[19] >= 0.5f;
  // obs[20] is time-to-intercept normalized by 4 s; keep the window in the same unit.
  const float tMin = interceptTimeMinS / 4.f;
  const float tMax = interceptTimeMaxS / 4.f;
  const bool tOk = obs[20] >= tMin && obs[20] <= tMax;
  const bool fresh = obs[27] >= 0.5f;
  const float y = obs[21] * isectNorm; // de-normalized predicted crossing (mm)
  const bool insidePenalty = obs[24] >= 0.5f;
  const bool nearLeft = obs[22] >= 0.5f;
  const bool nearRight = obs[23] >= 0.5f;
  const bool stable = obs[37] <= 0.4f;
  const bool interceptBase = rolling && tOk && fresh && stable;

  if(interceptBase && std::abs(y) <= standRadiusMm)
    m[idx(S::interceptCenter)] = 1.f;
  if(interceptBase && std::abs(y) <= walkRadiusMm)
    m[idx(S::interceptLateral)] = 1.f;
  if(interceptBase && y > 0.f && y <= genuflectRadiusMm)
    m[idx(S::interceptLowLeft)] = 1.f;
  if(interceptBase && y < 0.f && y >= -genuflectRadiusMm)
    m[idx(S::interceptLowRight)] = 1.f;
  if(keeperJumpingOn && interceptBase && insidePenalty && y > 0.f && y <= jumpRadiusMm && !nearLeft)
    m[idx(S::interceptJumpLeft)] = 1.f;
  if(keeperJumpingOn && interceptBase && insidePenalty && y < 0.f && y >= -jumpRadiusMm && !nearRight)
    m[idx(S::interceptJumpRight)] = 1.f;

  const bool controlled = obs[25] >= 0.5f;
  if(controlled)
  {
    m[idx(S::clear)] = 1.f;
    m[idx(S::pass)] = 1.f;
    m[idx(S::dribbleOut)] = 1.f;
  }
  return m;
}
