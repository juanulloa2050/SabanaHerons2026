/**
 * @file Tools/Maze/MazeStartConfig.h
 *
 * The `start` setting of `handleMaze.cfg`: the corner the robot starts in (`cell`: -1 to deduce it from the tags, or
 * 0, 7, 56, 63) and the maze heading it faces (`heading`, needed because a corner has two open sides). The file also
 * holds the settings of the behaviour, so it is read through `StartFile`, which declares `start` only; the other
 * top-level entries of the file are ignored (and the behaviour can read its own the same way). Free of modules so that it can be unit tested.
 */

#pragma once

#include "MazeGraph.h"
#include "MazeTagLayout.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include <string>

namespace Maze
{
  STREAMABLE(StartConfig,
  {,
    (int)(-1) cell,                  /**< -1 deduces the start, otherwise the corner cell: 0, 7, 56 or 63. */
    (TagFace)(south) heading,        /**< The maze heading the robot faces at the start (a `TagFace` names a maze direction); only used if `cell` is given. */
  });

  /** What the setting means. */
  struct StartChoice
  {
    bool given = false;              /**< The start is known; otherwise it is deduced. */
    int cell = -1;
    Heading heading = Heading::south;
  };

  /**
   * Validates the setting. A `cell` that is neither -1 nor a corner, or a `heading` that does not point into the
   * maze from that corner, is an error; the start is then deduced.
   * @param error Receives a message if the setting is invalid, otherwise it is cleared.
   */
  [[nodiscard]] StartChoice resolveStart(const StartConfig& config, std::string& error);

  /** The layout of `handleMaze.cfg` as far as the start is concerned. */
  STREAMABLE(StartFile,
  {
    /**
     * Reads the start setting from a config file; other entries of the file are not read.
     * @return Whether the file was found and read.
     */
    static bool load(const std::string& name, StartFile& file),

    (StartConfig) start,
  });
}
