/**
 * @file Tools/Maze/MazeStartAnchoredMap.h
 *
 * The wall map of the mapper together with the deduction of the start, free of representations and modules so that
 * MazeMapper and the offline simulation run the same per-frame logic.
 *
 * The map lives in the maze frame of the working hypothesis (`anchor`), like the pose of the localizer. Every
 * input of a frame is given in that frame: the pose of the robot, the gaze, the tag detections. The inputs go to the
 * deduction in start-relative form (`toStart(anchor, ...)`) and to the map as they are. When the evidence changes the
 * working hypothesis, `endFrame` rebuilds the map by replaying the logged observations under the new one, so that the
 * result is the map that the mapper would have had if that hypothesis had been its frame from the start.
 *
 * The inputs of a frame are always converted with the anchor of the frame, which only changes in `endFrame`. The
 * deduction also sees the frames in which the pose is outside the maze of the working hypothesis (which proves that
 * hypothesis wrong, and whose observations belong to another hypothesis); the map only learns from the others.
 *
 * Once the start is known only one hypothesis is left, the log is freed and nothing is logged again. A given start never logs.
 */

#pragma once

#include "MazeLocalization.h"
#include "MazeMapFusion.h"
#include "MazeStartDeduction.h"
#include "MazeTagLayout.h"
#include "Math/Pose2f.h"
#include "Math/Pose3f.h"
#include <vector>

namespace Maze
{
  struct AnchoredMapParameters
  {
    FusionParameters fusion;
    float maxFaceAngleError = Angle::fromDegrees(45.f); /**< Largest angle between the measured and the expected tag normal; as in the localizer. */
    float maxOffsetFromCentre = 150.f;                  /**< The look-ahead and the position evidence are used only this close (mm, per axis) to the centre of the cell. */
  };

  /** What the mapper tells about the start, the content of `MazeStart`. */
  struct StartStatus
  {
    bool given = false;
    bool known = false;
    Hypothesis working;
    int aliveCount = StartDeduction::hypothesisCount;
    unsigned aliveMask = (1u << StartDeduction::hypothesisCount) - 1u;
    int revision = 0;
    int conflicts = 0;
    Evidence lastConflict = Evidence::none; /**< The kind of the latest observation that contradicted the start. */
    bool logOverflowed = false;
  };

  class StartAnchoredMap
  {
  public:
    /** The log reports a conflict at most this often (ms). */
    static constexpr unsigned conflictWarningInterval = 2000;

    explicit StartAnchoredMap(const AnchoredMapParameters& parameters = AnchoredMapParameters());

    /** Applies the parameters to the map and the deduction; call it before the frame if they may have changed. */
    void setParameters(const AnchoredMapParameters& newParameters);

    /**
     * The start is given: the deduction is skipped and the map is rebuilt in its frame. Call it before any observation.
     * @return Whether it is a corner facing into the maze; if not, nothing changes.
     */
    bool setGiven(int cell, Heading heading);

    /** Starts the next attempt: the counter grows and the map is kept. */
    void newAttempt() { fusion.newAttempt(); }

    /** The robot stands in the cell of `pose`: it counts as visited, and a cell outside the maze under a hypothesis removes it. */
    void markVisited(const Pose2f& pose);

    /** Evidence from the position alone: `pose` close to the centre of a cell that is outside the maze under a hypothesis removes it. */
    void observePosition(const Pose2f& pose);

    /** Whether the look-ahead from `pose` is used: the cell of the estimate is unambiguous. */
    [[nodiscard]] bool lookAheadUsable(const Pose2f& pose) const;

    /** Adds a look-ahead along `gaze` seen from `pose`; the caller checks `lookAheadUsable`. @return The number of edges that changed. */
    int observeCellsAhead(const Pose2f& pose, Heading gaze, const std::vector<CellObservation>& cells, unsigned time);

    /**
     * The robot was blocked by the edge on side `heading` of cell (`row`, `col`), in the frame of the anchor: a wall for good (see
     * `MazeMapFusion::observeBlocked`). It is logged like a tag, so a rebuild under another hypothesis keeps it.
     * @return Whether an edge of the map changed.
     */
    bool observeBlocked(int row, int col, Heading heading);

    /**
     * Adds a decoded tag seen from `pose`. The deduction takes every detection; the map only those whose face the
     * working hypothesis explains, and only while the pose is inside the maze.
     * @return The number of edges that changed.
     */
    int observeTag(const Pose2f& pose, int id, const Pose3f& tagInRobot, unsigned time);

    /**
     * Ends the frame: if the working hypothesis changed, rebuilds the map under the new one. Call it once per frame.
     * @return Whether the map was rebuilt (the planner has to replan, and the localizer has to move its pose).
     */
    bool endFrame();

    /** Whether new conflicts arrived and none was reported within `conflictWarningInterval`. */
    [[nodiscard]] bool takeConflictWarning(unsigned time);

    /** Whether the log overflowed since this was last asked; true once. */
    [[nodiscard]] bool takeLogOverflowWarning();

    [[nodiscard]] const MazeMapFusion& map() const { return fusion; }
    [[nodiscard]] const StartDeduction& deduction() const { return startDeduction; }
    [[nodiscard]] StartStatus status() const;

    /** The hypothesis whose frame the map and the inputs are in. */
    [[nodiscard]] const Hypothesis& anchor() const { return anchored; }

    MazeTagLayout layout;

  private:
    /** Replaces the map by the replay of the log under the working hypothesis. */
    void rebuild();

    /** The start-relative cell of a pose in the frame of the anchor. */
    [[nodiscard]] GridCell startCellOf(const Pose2f& pose) const;

    AnchoredMapParameters parameters;
    MazeMapFusion fusion;
    StartDeduction startDeduction;
    Hypothesis anchored;
    bool given = false;
    int reportedConflicts = 0;
    unsigned lastConflictWarning = 0;
    bool conflictWarned = false;
    bool overflowReported = false;
  };
}
