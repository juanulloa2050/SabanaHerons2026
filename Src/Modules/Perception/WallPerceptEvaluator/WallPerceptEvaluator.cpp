/**
 * @file WallPerceptEvaluator.cpp
 *
 * Implementation of the wall percept evaluation, see WallPerceptEvaluator.h.
 */

#include "WallPerceptEvaluator.h"
#include "Debugging/DebugRequest.h"
#include "Debugging/Debugging.h"
#include "Platform/File.h"
#include "Tools/Maze/MazeOracle.h"
#include <cstdio>

MAKE_MODULE(WallPerceptEvaluator);

WallPerceptEvaluator::~WallPerceptEvaluator()
{
  if(scorer)
    print("final");
}

bool WallPerceptEvaluator::loadMaze()
{
  if(mazeLoaded)
    return true;
  if(mazeFailed)
    return false;
  std::string error;
  if(!Maze::MazeFile::load(std::string(File::getBHDir()) + "/" + mazeFile, maze, error))
  {
    OUTPUT_WARNING("WallPerceptEvaluator: " << mazeFile << " not found or invalid (" << error << ").");
    mazeFailed = true;
    return false;
  }
  scorer = std::make_unique<Maze::WallScore::Scorer>(maze.graph);
  mazeLoaded = true;
  return true;
}

void WallPerceptEvaluator::print(const char* title)
{
  const std::string text = scorer->text();
  OUTPUT_TEXT("WallPerceptEvaluator " << title << " (empty percepts " << emptyPercepts << "):\n" << text);
  std::fprintf(stderr, "WallPerceptEvaluator: %s, new percepts %d, empty percepts %d\n", title, framesSeen, emptyPercepts);
  std::size_t begin = 0;
  while(begin < text.size())
  {
    const std::size_t end = text.find('\n', begin);
    std::fprintf(stderr, "WallPerceptEvaluator: %s\n", text.substr(begin, end - begin).c_str());
    begin = end == std::string::npos ? text.size() : end + 1;
  }
  std::fflush(stderr);
}

void WallPerceptEvaluator::update(WallPerceptScore& wallPerceptScore)
{
  if(theGameState.isPlaying() && !wasPlaying)
    playingSince = theFrameInfo.time;
  wasPlaying = theGameState.isPlaying();
  const bool firstTap = startTapAfterMs > 0 && !tapped && wasPlaying && theFrameInfo.getTimeSince(playingSince) >= startTapAfterMs;
  const bool nextTap = tapped && tapRepeatMs > 0 && theFrameInfo.getTimeSince(lastTapTime) >= tapRepeatMs;
  if(firstTap || nextTap)
  {
    tapped = true;
    lastTapTime = theFrameInfo.time;
    Global::getDebugRequestTable().addRequest(DebugRequest("option:HandleMaze:headFront", true));
  }
  if(!loadMaze())
    return;
  const WallPercept& percept = theUpperWallPercept;
  if(percept.timestamp != lastTimestamp)
  {
    lastTimestamp = percept.timestamp;
    ++framesSeen;
    if(percept.cellsAhead.empty())
      ++emptyPercepts;
    else
    {
      Maze::WallScore::Frame frame;
      const Pose2f truth = Maze::Oracle::robotInMaze(theGroundTruthRobotPose, mazePoseOnField);
      frame.truePose = truth;
      frame.believedPosition = theRobotPose.translation;
      frame.believedRotation = theRobotPose.rotation;
      frame.standing = theMotionInfo.executedPhase == MotionPhase::stand;
      frame.range = Maze::WallView::lookAheadRange(frame.standing, lookAheadRangeWalking, lookAheadRangeStanding);
      frame.lookAheadTurns = percept.lookAheadTurns;
      for(const WallPercept::CellAhead& cell : percept.cellsAhead)
      {
        Maze::WallView::Cell entry;
        entry.ahead = cell.ahead;
        entry.left = static_cast<Maze::EdgeState>(static_cast<int>(cell.left));
        entry.right = static_cast<Maze::EdgeState>(static_cast<int>(cell.right));
        entry.front = static_cast<Maze::EdgeState>(static_cast<int>(cell.front));
        entry.distance = cell.distance;
        frame.cellsAhead.push_back(entry);
      }
      frame.boundaryPoints = percept.boundaryPoints;
      scorer->add(frame);
    }
  }

  if(theFrameInfo.getTimeSince(lastSummaryTime) >= summaryIntervalMs)
  {
    lastSummaryTime = theFrameInfo.time;
    print("summary");
    wallPerceptScore.frames = framesSeen;
    wallPerceptScore.summary = scorer->text();
  }
}
