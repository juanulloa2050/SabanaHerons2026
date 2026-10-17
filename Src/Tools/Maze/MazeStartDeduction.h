/**
 * @file Tools/Maze/MazeStartDeduction.h
 *
 * Which corner and which way the robot started, deduced from what it sees, free of representations and
 * modules so that it can be unit tested. The start is a corner with walls on three sides and the robot faces
 * the open side, which leaves 8 hypotheses `(cell, heading)`: 4 corners times 2 open sides.
 *
 * Until the start is known the localizer and the mapper work in the frame of the first alive hypothesis (the
 * working hypothesis). The deduction itself works in the start-relative frame, which does not depend on any
 * hypothesis:
 *  - continuous: origin at the centre of the start cell, x forward (the start heading), y left, mm and rad;
 *  - grid: a maze frame whose start cell is (0, 0) and whose north is forward, so east is right and the
 *    rows and columns are unbounded (a cell outside the 8x8 grid is representable, never clamped).
 * A hypothesis is a rigid transform from the start-relative frame to the maze frame (roadmap 4.5).
 *
 * Evidence removes hypotheses: a decoded tag whose id and face do not fit, `open` evidence on an edge that
 * would be outer boundary or outside the grid, and a robot cell outside the grid. The last hypothesis is never
 * removed, the contradiction is counted in `conflicts` instead. A given start is a deduction restricted to one
 * hypothesis, so contradictions are only counted. Non-finite evidence (weights, tag poses) is ignored.
 *
 * The observations are logged in start-relative form so that `replay` can fuse them into a fresh map under
 * any hypothesis, as if that hypothesis had been the frame from the start.
 */

#pragma once

#include "MazeGraph.h"
#include "MazeMapFusion.h"
#include "MazeTagLayout.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include <array>
#include <map>
#include <set>
#include <vector>

namespace Maze
{
  /** A start of the robot: the corner cell and the maze heading it faces. */
  struct Hypothesis
  {
    int cell = 0;
    Heading heading = Heading::east;

    [[nodiscard]] bool operator==(const Hypothesis& other) const { return cell == other.cell && heading == other.heading; }
  };

  /** A grid cell, in the start-relative frame or in the maze frame; `Maze::inside` tells whether it is in the maze. */
  struct GridCell
  {
    int row = 0;
    int col = 0;

    [[nodiscard]] bool operator==(const GridCell& other) const { return row == other.row && col == other.col; }
    [[nodiscard]] bool operator<(const GridCell& other) const { return row != other.row ? row < other.row : col < other.col; }
  };

  struct DeductionParameters
  {
    FusionParameters fusion;                              /**< Open evidence has to reach `fusion.threshold`, accepted like the fusion does (rate limit, clock restart). */
    float maxFaceAngleError = Angle::fromDegrees(45.f); /**< Largest angle between the measured tag normal and the normal of a face that explains the tag (as the localizer). */
    float tagPositionTolerance = 200.f;                 /**< Largest distance (mm) between the tag and the centre of the face that explains it. */
  };

  class StartDeduction
  {
  public:
    static constexpr int hypothesisCount = 8;

    /** The 8 hypotheses in a fixed order: corners 0, 7, 56 and 63, each with its two headings in `Heading` order. */
    [[nodiscard]] static const std::array<Hypothesis, hypothesisCount>& hypotheses();

    /** The index in `hypotheses()` of a start, -1 if it is not a corner facing into the maze. */
    [[nodiscard]] static int indexOf(int cell, Heading heading);

    /** Whether `cell` is a value of the `start.cell` setting: -1 (deduce) or a corner. */
    [[nodiscard]] static bool isValidStartCell(int cell);

    /** Rigid transform from the start-relative frame to the maze frame (mm, rad). */
    [[nodiscard]] static Pose2f startToMaze(const Hypothesis& hypothesis);

    /** Grid cell and heading from the start-relative grid to the maze grid, and back. The result may be outside the maze. */
    [[nodiscard]] static GridCell toMaze(const Hypothesis& hypothesis, GridCell cell);
    [[nodiscard]] static GridCell toStart(const Hypothesis& hypothesis, GridCell cell);
    [[nodiscard]] static Heading toMaze(const Hypothesis& hypothesis, Heading heading);
    [[nodiscard]] static Heading toStart(const Hypothesis& hypothesis, Heading heading);

    /** The start-relative grid cell that contains a start-relative position (mm). */
    [[nodiscard]] static GridCell cellOf(const Vector2f& position);

    explicit StartDeduction(const DeductionParameters& parameters = DeductionParameters()) : parameters(parameters) {}

    /**
     * Restricts the deduction to one start. Observations that contradict it are counted, never followed. The
     * observation log, the edge evidence, the visited cells and the conflicts start over, and `revision` grows
     * if the working hypothesis changes, so a caller that follows the revision re-anchors to the given start.
     * @return Whether it is one of the 8 hypotheses; if not, nothing changes.
     */
    bool setGiven(int cell, Heading heading);

    /**
     * Adds a decoded tag. It removes every alive hypothesis under which the tag is not on a face of cell `id`
     * that the layout allows. One decoded tag is full evidence.
     * @param position Centre of the tag in the start-relative frame (mm).
     * @param normal Normal of the tag in the start-relative frame, out of the wall into the cell (the z-axis of the tag).
     * @param time Time of the percept in ms, for the replay.
     * @return The number of hypotheses removed.
     */
    int observeTag(int id, const Vector2f& position, const Vector2f& normal, unsigned time, const MazeTagLayout& layout);

    /**
     * Adds one observation of the edge on side `side` of a start-relative cell. `open` evidence on an edge that is
     * outer boundary or outside the maze under a hypothesis removes it once it reaches the threshold; `wall` never does.
     * @return The number of hypotheses removed.
     */
    int observeEdge(GridCell cell, Heading side, bool isWall, float weight, unsigned time = untimed);

    /**
     * Adds a look-ahead like `MazeMapFusion::observeCellsAhead`, from a start-relative cell and gaze, weighing the
     * walls with `weights`. Unknown entries are skipped.
     * @return The number of hypotheses removed.
     */
    int observeCellsAhead(GridCell robot, Heading gaze, const std::vector<CellObservation>& cells, const MazeMapFusion& weights, unsigned time = untimed);

    /**
     * Notes the start-relative cell of the robot (visited for the replay). A cell outside the maze under a hypothesis removes it.
     * @return The number of hypotheses removed.
     */
    int observePosition(GridCell cell);

    /**
     * Fuses the whole log into `fusion`, which should be fresh, under `hypothesis`: the same evidence the mapper
     * would have fused had that hypothesis been its frame from the start. Edges outside the maze are dropped. Sets
     * `fusion.startCell`; the attempt counter is left alone.
     */
    void replay(const Hypothesis& hypothesis, MazeMapFusion& fusion, const MazeTagLayout& layout) const;

    [[nodiscard]] bool alive(int index) const { return aliveMask[static_cast<std::size_t>(index)]; }
    [[nodiscard]] int aliveCount() const;

    /** Whether one hypothesis is left. */
    [[nodiscard]] bool known() const { return aliveCount() == 1; }

    /** The first alive hypothesis, whose frame the localizer and the mapper use. */
    [[nodiscard]] const Hypothesis& working() const { return hypotheses()[static_cast<std::size_t>(workingIndex())]; }
    [[nodiscard]] int workingIndex() const;

    DeductionParameters parameters;
    int conflicts = 0; /**< Observations that would have removed the last alive hypothesis (or the given one). */
    int revision = 0;  /**< Grows whenever the working hypothesis changes. */

  private:
    /** One logged observation in start-relative form. */
    struct Entry
    {
      bool isTag = false;
      GridCell cell;             /**< Edge: the cell the edge is on. */
      Heading side = Heading::north; /**< Edge: the side of `cell`. */
      bool isWall = false;
      float weight = 0.f;
      int id = 0;                /**< Tag: its id. */
      float normalAngle = 0.f;   /**< Tag: angle of its normal in the start-relative frame. */
      unsigned time = untimed;
    };

    /** Open evidence of one edge and the time of its last accepted observation. */
    struct EdgeEvidence
    {
      float open = 0.f;
      unsigned last = 0;
      bool hasLast = false;
    };

    /** Removes the hypotheses in `doomed` unless that would leave none; counts a conflict then. */
    int remove(const std::array<bool, hypothesisCount>& doomed);

    [[nodiscard]] bool acceptedByClock(EdgeEvidence& evidence, unsigned time) const;
    [[nodiscard]] bool edgeOnBoundary(const Hypothesis& hypothesis, GridCell cell, Heading side) const;

    std::array<bool, hypothesisCount> aliveMask = {true, true, true, true, true, true, true, true};
    std::vector<Entry> log;
    std::set<GridCell> visitedCells;
    std::map<std::array<int, 3>, EdgeEvidence> edges; /**< Keyed by the canonical edge: row, column, horizontal. */
  };
}
