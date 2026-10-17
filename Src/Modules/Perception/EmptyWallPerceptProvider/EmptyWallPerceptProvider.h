/**
 * @file EmptyWallPerceptProvider.h
 *
 * Placeholder for the wall perceptor (MZ-019): provides a `WallPercept` that never sees a wall, so
 * that the module graph of the real robot resolves for MazeMapper. In simulation the scene replaces it
 * with the oracle (`mr WallPercept OracledMazePerceptsProvider` in Maze.con). Remove it from
 * `threads.cfg` when MZ-019 provides the real percept.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Perception/MazePercepts/WallPercept.h"

MODULE(EmptyWallPerceptProvider,
{,
  REQUIRES(FrameInfo),
  PROVIDES(WallPercept),
});

/**
 * @class EmptyWallPerceptProvider
 * A wall percept without walls.
 */
class EmptyWallPerceptProvider : public EmptyWallPerceptProviderBase
{
  void update(WallPercept& wallPercept) override;
};
