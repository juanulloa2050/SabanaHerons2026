/**
 * @file WallPerceptScore.h
 *
 * The result of the simulation evaluation of the wall perceptor, see WallPerceptEvaluator. It exists so that the
 * evaluator is a module of its own that a scene can switch on (`mr WallPerceptScore WallPerceptEvaluator`); it is
 * never used on the robot.
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include <string>

STREAMABLE(WallPerceptScore,
{,
  (int)(0) frames,         /**< Scored wall percepts so far. */
  (std::string) summary,   /**< The metrics, one per line, see Tools/Maze/MazeWallScore.h. */
});
