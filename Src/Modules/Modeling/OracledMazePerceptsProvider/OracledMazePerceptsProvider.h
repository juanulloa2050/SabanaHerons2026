/**
 * @file OracledMazePerceptsProvider.h
 *
 * Simulation only: provides the AprilTags and the walls that the robot would see in the maze,
 * computed from the ground truth robot pose and a `.maze` file. This lets the behavior, the
 * mapping and the safe mode be built before the real detectors exist.
 *
 * The model is an approximation that ignores the vertical field of view.
 *
 * Tags: every wall face that the layout tags is a candidate, wherever it is. A tag is reported when it
 * is within `maxRange` cells, the whole black square (both side edges) is inside the horizontal field of
 * view, it faces the camera within `maxTagViewAngle`, and no wall blocks the 2D line from the camera to
 * the tag centre or to either side edge (walls are higher than the camera). A wall blocks as its centre
 * line extended by half its thickness at both ends, so a ray grazing a wall end counts as blocked.
 *
 * Walls (`WallPercept`) keep the look-ahead corridor: from the robot's cell, along the gaze snapped to
 * the nearest maze heading, up to `maxRange` cells and not beyond a front wall.
 *
 * Frames: the maze frame is in mm, x is east, y is north and a rotation of 0 looks east. Its origin is
 * the south-west corner of the maze. `mazePoseOnField` is the pose of that frame in the frame of the
 * ground truth robot pose, which is not the SimRobot world for first-team robots (it is rotated by pi). The tag layout comes from `mazeTagLayout.cfg`.
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
#include "Tools/Maze/MazeOracle.h"
#include "Tools/Maze/MazeTagLayout.h"
#include <filesystem>
#include <string>

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
    (Pose2f) mazePoseOnField,      /**< Pose of the maze frame (mm) in the frame of the ground truth robot pose. */
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
  void update(AprilTagPercept& aprilTagPercept) override;
  void update(WallPercept& wallPercept) override;

  /**
   * Loads the maze if `mazeFile` changed, if the file was written since it was read or, while no
   * maze is available, once per `reloadIntervalMs`. @return Whether a maze is available.
   */
  bool updateMaze();

  /** The camera of the current frame as the oracle geometry needs it. */
  Maze::Oracle::Camera camera() const;

  /** Fills `view` from the ground truth. @return Whether the robot is inside the maze. */
  bool computeView(Maze::Oracle::View& view) const;

  /** The detection of a visible tag with the configured noise. */
  AprilTagPercept::Detection detect(const Maze::Oracle::VisibleTag& tag) const;

  bool dropped() const;

  static constexpr unsigned reloadIntervalMs = 1000; /**< Time between two looks at the maze file. */

  Maze::MazeDescription maze;
  Maze::MazeTagLayout layout;
  std::string loadedMazeFile;                   /**< The path that `maze` was read from; empty if none. */
  std::filesystem::file_time_type loadedWriteTime; /**< Modification time of the file when it was read. */
  unsigned lastLookTime = 0;                    /**< Frame time of the last look at the file. */
  bool mazeAvailable = false;
  bool warned = false;                          /**< The failure of the current episode was reported. */
};
