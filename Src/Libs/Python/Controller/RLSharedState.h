/**
 * @file RLSharedState.h
 *
 * Thread-safe shared state between the Python RL loop and BHuman modules.
 * Python writes actions; RLSkillProvider writes observations.
 * Indexed by player number (1-based, up to MAX_PLAYERS).
 */

#pragma once

#include "RLSim2D.h"
#include "Platform/Semaphore.h"
#include <array>
#include <mutex>
#include <string>

struct RLPlayerIO
{
  std::mutex mutex;

  // Python → BHuman (action): set before controller.update()
  std::string skill = "stand";  // "stand" | "walkTo" | "shoot" | "dribble"
  float targetX     = 0.f;      // mm, field coords
  float targetY     = 0.f;
  float targetTheta = 0.f;      // radians

  // BHuman → Python (observation): written by RLSkillProvider each frame
  float ballX       = 0.f;
  float ballY       = 0.f;
  float robotX      = 0.f;
  float robotY      = 0.f;
  float robotTheta  = 0.f;
  unsigned int frame = 0;
  bool obsReady     = false;

  // Optional headless 2D simulation state used by pybh when Python wants
  // B-Human cognition plus a C++ physics backend rooted in SabanaHerons.
  RLSim2DState sim2D;

  // Semaphore posted by RLSkillProvider when observation is ready.
  // Python waits on this after controller.update().
  Semaphore obsSignal;
};

class RLSharedState
{
public:
  static constexpr int MAX_PLAYERS = 6;

  static RLSharedState& instance()
  {
    static RLSharedState inst;
    return inst;
  }

  RLPlayerIO& player(int number)
  {
    const int idx = (number >= 1 && number <= MAX_PLAYERS) ? number - 1 : 0;
    return players[idx];
  }

private:
  std::array<RLPlayerIO, MAX_PLAYERS> players;
};
