/**
 * @file OracledMazePerceptsProvider.h
 *
 * Simulation only: provides the AprilTags and the walls that the robot would see in the maze,
 * computed from the ground truth robot pose and a `.maze` file. This lets the behavior, the
 * mapping and the safe mode be built before the real detectors exist.
 *
 * The model is an approximation. Occlusion is approximated by a look-ahead corridor: from the
 * robot's cell, along the gaze snapped to the nearest maze heading, up to `maxRange` cells and not
 * beyond a front wall (walls are higher than the camera, so nothing is visible past a corner).
 * Tags are seen on the wall faces of those cells when they are inside the horizontal field of view
 * and face the camera. The vertical field of view is ignored.
 *
 * Frames: maze frame mm + `mazeOriginOnField` = field (SimRobot world) mm; x is east, y is north
 * and a rotation of 0 looks east. The tag layout comes from `mazeTagLayout.cfg`.
 */

#pragma once

#include "Framework/Module.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Representations/Perception/MazePercepts/AprilTagPercept.h"
#include "Representations/Perception/MazePercepts/WallPercept.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeTagLayout.h"
#include <string>
#include <vector>

MODULE(OracledMazePerceptsProvider,
{,
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  REQUIRES(FrameInfo),
  REQUIRES(GroundTruthRobotPose),
  PROVIDES(AprilTagPercept),
  PROVIDES(WallPercept),
  LOADS_PARAMETERS(
  {,
    (std::string) mazeFile,        /**< The maze, relative to the repository root. Written next to the scene by maze_to_scene.py. */
    (Vector2f) mazeOriginOnField,  /**< Field position (mm) of the south-west corner of the maze. */
    (int) maxRange,                /**< Cells ahead of the robot's cell that are seen. */
    (float) dropoutRate,           /**< Probability to miss one observation (a tag, a cell, a segment); 1 gives empty percepts. */
    (float) tagPositionStdDev,     /**< Standard deviation of the tag position per axis in mm. */
    (Angle) tagRotationStdDev,     /**< Standard deviation of the tag rotation per axis. */
    (float) wallDistanceStdDev,    /**< Standard deviation of wall and cell distances in mm. */
    (Angle) maxTagViewAngle,       /**< Largest angle between the tag normal and the direction to the camera. */
  }),
});

/**
 * @class OracledMazePerceptsProvider
 * Fake maze perception for the simulator.
 */
class OracledMazePerceptsProvider : public OracledMazePerceptsProviderBase
{
public:
  OracledMazePerceptsProvider();

private:
  /** One cell of the look-ahead. */
  struct LookAheadCell
  {
    int ahead;                                /**< Cells between the robot's cell and this one. */
    int row, col;                             /**< Position of the cell in the maze. */
    Maze::EdgeState left, right, front;       /**< Edges relative to the gaze direction. */
  };

  /** The robot pose in the maze frame and the gaze it looks along. */
  struct View
  {
    Vector2f robot;           /**< Robot position in the maze frame in mm. */
    float robotRotation;      /**< Robot heading in the maze frame in rad, 0 is east. */
    Vector2f camera;          /**< Camera position in the maze frame in mm. */
    float cameraYaw;          /**< Camera gaze in the maze frame in rad. */
    int row, col;             /**< The robot's cell. */
    Maze::Heading gaze;       /**< The gaze snapped to a maze heading. */
    std::vector<LookAheadCell> cells;
  };

  void update(AprilTagPercept& aprilTagPercept) override;
  void update(WallPercept& wallPercept) override;

  /** Loads the maze again if `mazeFile` changed. @return Whether a maze is available. */
  bool updateMaze();

  /** Fills `view`. @return Whether the robot is inside the maze. */
  bool computeView(View& view) const;

  /** Maze frame to robot frame (rotation and translation only). */
  Vector2f toRobot(const View& view, const Vector2f& mazePoint) const;

  /** Whether the tag on `face` of the cell is seen; fills `detection` if so. */
  bool observeTag(const View& view, int cell, Maze::Heading face, AprilTagPercept::Detection& detection) const;

  bool dropped() const;

  Maze::MazeDescription maze;
  Maze::MazeTagLayout layout;
  std::string loadedMazeFile;   /**< The path that `maze` was read from; empty if none. */
  bool mazeAvailable = false;
};
