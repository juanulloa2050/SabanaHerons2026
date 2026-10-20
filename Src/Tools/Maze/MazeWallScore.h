/**
 * @file Tools/Maze/MazeWallScore.h
 *
 * Scores the wall percept against the true maze, for the simulation evaluation of the wall perceptor (MZ-019). Free of
 * representations and modules so that it can be unit tested; WallPerceptEvaluator feeds it with one frame per new percept.
 *
 * A frame holds the true robot pose in the maze frame, the percept (look-ahead and boundary points) and the pose the
 * perceptor believes (the cell and the maze heading nearest to its rotation). Every reported known edge (wall or open) is mapped to its true maze edge with the
 * true cell and the true gaze, which is the gaze heading nearest to the true robot rotation turned by
 * `lookAheadTurns` quarter turns. If the believed cell or the believed heading differs from the true one, the frame is
 * counted in `poseMismatchFrames` and not scored: the perceptor then reports the right walls in a wrong cell or direction,
 * which is a localization error and no perception error. A frame whose true pose is outside the maze is skipped as well.
 *
 * Metrics (all over the scored frames):
 *  - Frame counts: `frames`, split into `standingFrames` and `walkingFrames`.
 *  - Frame edges. Every reported edge of every cell ahead: `wall`/`open` against the truth, `unknown` is counted apart.
 *    Accuracy is correct / (correct + wrong). `wallCorrect`, `wallWrong`, `openCorrect` and `openWrong` split it.
 *  - (a) Visited-cell sides. A visit is a run of scored frames with the same true cell. Every reported known edge of every
 *    scored frame, whatever its `ahead`, is a vote for the true edge it maps to, so the votes are the wall map that the
 *    percept would build over the whole run (a side wall of the cell is seen mostly from the cell before). At the end of a
 *    visit its 4 sides are read from the votes collected so far: per side the majority of the known values (a tie takes
 *    the latest), never known counts as unknown. A finished visit adds 4 sides to `visitSides`, the known ones to
 *    `visitSidesKnown`, those among them equal to the truth to `visitSidesCorrect`. Accuracy = correct / known,
 *    coverage = known / sides. `finishVisit` ends the open visit, `summary` counts it as if it ended now.
 *  - (b) Next cell ahead. For each scored frame in which the true front edge of the robot's cell (in the gaze) is open
 *    and the next cell is inside the maze, and the range is at least 1, the left and right edges of the cell
 *    `ahead == 1` are eligible (`aheadSides`). Those reported as wall or open count as known, those equal to the
 *    truth as correct. Accuracy = correct / known, coverage = known / eligible.
 *    `text` also lists the wrong sides of the finished visits (cell, side, the votes for wall and open, the truth).
 *  - (c) `cellsBeyondRange`: reported cells with `ahead` larger than the active range of the frame. It must be 0.
 *  - (d) Boundary point error. For each boundary point, the distance in mm to the nearest true wall segment (the 500 mm
 *    centre line of an edge). The points are binned by their distance from the robot, 500, 1000, 1500, 2000 and 2500 mm
 *    with +-250 mm, and split by `standing`. Per bin the number, the mean, the standard deviation and the 95th
 *    percentile of the error.
 */

#pragma once

#include "MazeEdgeIndex.h"
#include "MazeGraph.h"
#include "MazeWallView.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include <array>
#include <string>
#include <vector>

namespace Maze
{
  namespace WallScore
  {
    constexpr int distanceBins = 5;

    /** One new wall percept with the ground truth of its frame. */
    struct Frame
    {
      Pose2f truePose;                          /**< True robot pose in the maze frame (mm, rad). */
      Vector2f believedPosition;                /**< The perceptor's own position estimate in the maze frame (mm). */
      float believedRotation = 0.f;             /**< The perceptor's own heading estimate in the maze frame (rad). */
      bool standing = false;                    /**< The robot stands (otherwise it walks). */
      int range = 0;                            /**< Active look-ahead range in cells. */
      int lookAheadTurns = 0;                   /**< As `WallPercept::lookAheadTurns`. */
      std::vector<WallView::Cell> cellsAhead;   /**< As `WallPercept::cellsAhead`. */
      std::vector<Vector2f> boundaryPoints;     /**< Boundary floor points in the robot frame in mm. */
    };

    /** Error statistics of one distance bin. */
    struct ErrorStats
    {
      int count = 0;
      float mean = 0.f, standardDeviation = 0.f, percentile95 = 0.f; /**< mm */
    };

    struct Summary
    {
      int frames = 0;              /**< Scored frames. */
      int standingFrames = 0, walkingFrames = 0; /**< Scored frames split by `Frame::standing`. */
      int poseMismatchFrames = 0;  /**< Frames not scored because the believed cell or heading differs from the true one. */
      int skippedFrames = 0;       /**< Frames not scored because the true pose is outside the maze. */
      int wallCorrect = 0, wallWrong = 0, openCorrect = 0, openWrong = 0, edgesUnknown = 0;
      int visits = 0, visitSides = 0, visitSidesKnown = 0, visitSidesCorrect = 0;
      int aheadSides = 0, aheadSidesKnown = 0, aheadSidesCorrect = 0;
      int cellsBeyondRange = 0;
      std::array<ErrorStats, distanceBins> standingErrors, walkingErrors;

      [[nodiscard]] int edgesKnown() const { return wallCorrect + wallWrong + openCorrect + openWrong; }
      [[nodiscard]] float edgeAccuracy() const;
      [[nodiscard]] float visitAccuracy() const;
      [[nodiscard]] float visitCoverage() const;
      [[nodiscard]] float aheadAccuracy() const;
      [[nodiscard]] float aheadCoverage() const;
    };

    class Scorer
    {
    public:
      explicit Scorer(const MazeGraph& truth) : maze(truth) {}

      void add(const Frame& frame);

      /** Ends the open visit; the next scored frame starts a new one. */
      void finishVisit();

      /** The metrics so far; an open visit counts as if it ended now, but stays open. */
      [[nodiscard]] Summary summary() const;

      /** One line per metric, for the logs. */
      [[nodiscard]] std::string text() const;

    private:
      /** Votes for one edge: how often it was seen as wall and as open, and the latest value. */
      struct Votes
      {
        int wall = 0, open = 0;
        EdgeState latest = EdgeState::unknown;
      };

      /** Adds the open visit to `summary`: its 4 sides and the known and correct ones. A wrong side is described in `wrongSides` if `describe`. */
      void countVisit(Summary& summary, std::vector<std::string>* describe = nullptr) const;

      const MazeGraph& maze;
      Summary totals;
      bool visiting = false;
      int visitRow = 0, visitCol = 0;
      std::array<Votes, EdgeIndex::horizontalCount> horizontalVotes; /**< Votes per edge, in the layout of `EdgeIndex`. */
      std::array<Votes, EdgeIndex::verticalCount> verticalVotes;

      /** The votes of the edge on side `heading` of a cell. */
      Votes& votesOf(int row, int col, Heading heading);
      const Votes& votesOf(int row, int col, Heading heading) const;
      std::array<std::vector<float>, distanceBins> standingSamples, walkingSamples;
      std::vector<std::string> wrongSides; /**< The wrong sides of the finished visits, for the logs: cell, side, votes, truth. */
    };
  }
}
