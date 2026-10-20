/**
 * @file MazeRunMetricsProvider.cpp
 *
 * Implementation of the measurement of simulated maze runs, see the header.
 */

#include "MazeRunMetricsProvider.h"
#include "Debugging/DebugRequest.h"
#include "Debugging/Debugging.h"
#include "Platform/File.h"
#include "Tools/Maze/MazeOracle.h"
#include <algorithm>
#include <cstdio>

MAKE_MODULE(MazeRunMetricsProvider);

namespace
{
  const char* statusName(Maze::RunStatus status)
  {
    switch(status)
    {
      case Maze::RunStatus::goal: return "goal";
      case Maze::RunStatus::timeout: return "timeout";
      default: return "not_started";
    }
  }

  unsigned nonNegative(int value)
  {
    return static_cast<unsigned>(std::max(value, 0));
  }
}

bool MazeRunMetricsProvider::loadMaze()
{
  if(tracker)
    return true;
  // One try: the maze file is written before SimRobot starts. A failure is said on the error stream, which is all a headless run shows.
  if(loadAttempted)
    return false;
  loadAttempted = true;
  Maze::MazeDescription maze;
  std::string error;
  if(!Maze::MazeFile::load(std::string(File::getBHDir()) + "/" + mazeFile, maze, error))
  {
    OUTPUT_WARNING("MazeRunMetricsProvider: " << mazeFile << " not found or invalid (" << error << "), nothing is measured.");
    std::fprintf(stderr, "MazeRunMetricsProvider: %s not found or invalid (%s), nothing is measured\n", mazeFile.c_str(), error.c_str());
    std::fflush(stderr);
    return false;
  }
  Maze::RunConfig config;
  config.attempts = std::clamp(attempts, 1, 2);
  config.startTapAfterMs = nonNegative(startTapAfterMs);
  config.attemptTimeoutMs = nonNegative(attemptTimeoutMs);
  config.returnTimeoutMs = nonNegative(returnTimeoutMs);
  config.settleMs = nonNegative(settleMs);
  config.contactRadius = contactRadius;
  config.hysteresis = hysteresis;
  config.stallDistance = stallDistance;
  config.stallAngle = stallAngle;
  tracker = std::make_unique<Maze::RunTracker>(maze, config);
  return true;
}

void MazeRunMetricsProvider::print(const Maze::AttemptResult& result) const
{
  std::fprintf(stderr, "MZ021_METRICS {\"attempt\":%d,\"reached_goal\":%s,\"time_s\":%.2f,\"wall_contacts\":%d,\"max_stall_s\":%.2f,\"status\":\"%s\"}\n",
               result.attempt, result.reachedGoal ? "true" : "false", static_cast<float>(result.timeMs) / 1000.f, result.wallContacts,
               static_cast<float>(result.maxStallMs) / 1000.f, statusName(result.status));
  std::fflush(stderr);
}

void MazeRunMetricsProvider::update(MazeRunMetrics& mazeRunMetrics)
{
  // The default is a module that does nothing, so that it is harmless in a normal Maze run.
  if(startTapAfterMs <= 0 || !loadMaze())
    return;

  Maze::RunFrame frame;
  frame.timeMs = theFrameInfo.time;
  frame.playing = theGameState.isPlaying();
  frame.pose = Maze::Oracle::robotInMaze(theGroundTruthRobotPose, mazePoseOnField);
  const Maze::RunUpdate result = tracker->update(frame);

  if(result.tap)
    Global::getDebugRequestTable().addRequest(DebugRequest("option:HandleMaze:headFront", true));
  for(const Maze::AttemptResult& finished : result.finished)
    print(finished);
  if(tracker->phase() == Maze::RunPhase::done && !doneAnnounced)
  {
    doneAnnounced = true;
    std::fprintf(stderr, "MZ021_DONE\n");
    std::fflush(stderr);
  }

  const Maze::AttemptResult& current = tracker->currentResult();
  mazeRunMetrics.state = static_cast<MazeRunMetrics::State>(static_cast<int>(tracker->phase()));
  mazeRunMetrics.attempt = current.attempt;
  mazeRunMetrics.reachedGoal = current.reachedGoal;
  mazeRunMetrics.timeMs = current.timeMs;
  mazeRunMetrics.wallContacts = current.wallContacts;
  mazeRunMetrics.maxStallMs = current.maxStallMs;
  mazeRunMetrics.status = static_cast<MazeRunMetrics::Status>(static_cast<int>(current.status));
}
