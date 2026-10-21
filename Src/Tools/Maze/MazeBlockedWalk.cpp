/**
 * @file Tools/Maze/MazeBlockedWalk.cpp
 *
 * Implementation of the blocked walk detector, see the header.
 */

#include "MazeBlockedWalk.h"
#include <cmath>

namespace Maze
{
  void BlockedWalkDetector::reset()
  {
    samples.clear();
    hasTime = false;
    gyroAlarm = false;
    bumperAlarm = false;
    deviation = 0.f;
    cause = BlockedCause::none;
  }

  bool BlockedWalkDetector::update(const BlockedWalkInput& input)
  {
    if(hasTime && input.timeMs < lastTime)
      reset(); // The clock went back: a new session, the old samples say nothing.
    hasTime = true;
    lastTime = input.timeMs;

    samples.push_back({input.timeMs, input.gyroZ});
    while(!samples.empty() && input.timeMs - samples.front().time >= parameters.gyroWindowMs)
      samples.pop_front();

    deviation = 0.f;
    if(samples.size() >= 3)
    {
      float mean = 0.f;
      for(const Sample& sample : samples)
        mean += sample.gyroZ;
      mean /= static_cast<float>(samples.size());
      float variance = 0.f;
      for(const Sample& sample : samples)
        variance += (sample.gyroZ - mean) * (sample.gyroZ - mean);
      deviation = std::sqrt(variance / static_cast<float>(samples.size()));
    }

    if(blocked())
      return true;

    const bool walking = input.walkCommanded && input.forwardSpeed >= parameters.minWalkSpeed;

    const bool gyro = walking && deviation > parameters.gyroStdThreshold;
    if(gyro && !gyroAlarm)
      gyroSince = input.timeMs;
    gyroAlarm = gyro;
    if(gyroAlarm && input.timeMs - gyroSince >= parameters.blockedTimeoutMs)
      cause = BlockedCause::gyro;

    const bool bumper = walking && input.bumperContact;
    if(bumper && !bumperAlarm)
      bumperSince = input.timeMs;
    bumperAlarm = bumper;
    if(bumperAlarm && input.timeMs - bumperSince >= parameters.bumperHoldMs)
      cause = BlockedCause::bumper;

    return blocked();
  }
}
