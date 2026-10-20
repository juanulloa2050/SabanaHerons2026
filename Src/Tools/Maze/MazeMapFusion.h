/**
 * @file Tools/Maze/MazeMapFusion.h
 *
 * The wall map of the maze localizer/mapper, free of representations and modules so that it can be unit
 * tested. Every wall slot (edge) collects signed evidence, positive for wall and negative for open, weighted
 * by how reliable the observation is. An edge leaves `unknown` once consistent evidence reaches `threshold`.
 * Once decided, contrary evidence has to reach the threshold again to flip it (hysteresis): the evidence
 * of a decided edge only counts what speaks against it, and confirmations cancel that again.
 * The outer border is always a wall and never changes.
 *
 * A face on which a tag was seen is a wall for good: open observations of the wall percept for that edge are ignored from
 * then on (a perceptor that sees through a tagged face is wrong), for as long as the fusion lives, as its map does. Other
 * open evidence, such as the robot's own position, still counts.
 *
 * Independence: Cognition repeats the same scene at 60 Hz, so consecutive frames are not independent evidence.
 * Timed observations are rate-limited per edge: an edge takes at most one observation every
 * `minObservationInterval` ms, the others are dropped, not accumulated.
 */

#pragma once

#include "MazeGraph.h"
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace Maze
{
  struct FusionParameters
  {
    float threshold = 3.f;            /**< Evidence needed to decide an edge or to flip it; 3 observations of full weight. */
    float fullWeightDistance = 500.f; /**< Wall observations up to this distance (mm) have full weight. */
    float zeroWeightDistance = 2500.f; /**< Wall observations from this distance (mm) have no weight; linear in between. */
    float tagWeight = 1.f;            /**< Weight of a decoded tag at any distance. */
    float minObservationInterval = 250.f; /**< An edge accepts at most one timed observation per this many ms (wall percepts and tags alike). */
    float clockRestartThreshold = 10000.f; /**< A timed observation older than the edge's last one is dropped, unless it is more than this many ms older (the clock restarted: a new session). */
  };

  /** The `time` of an observation that is not subject to the per-edge rate limit. */
  constexpr unsigned untimed = std::numeric_limits<unsigned>::max();

  /**
   * Whether a position (maze frame, mm) is within `maxOffset` of the centre of cell (`row`, `col`), measured per axis. Near a cell
   * boundary the cell of the estimate is ambiguous, so what the oracle or the camera shows may belong to the neighbour.
   */
  [[nodiscard]] inline bool nearCellCentre(float x, float y, int row, int col, float maxOffset)
  {
    const CellCentre centre = cellCentre(row, col);
    return std::abs(x - centre.x) <= maxOffset && std::abs(y - centre.y) <= maxOffset;
  }

  /** One cell of a look-ahead, the plain equivalent of `WallPercept::CellAhead`. */
  struct CellObservation
  {
    int ahead = 0;                        /**< Cells between the robot's cell and this one. */
    EdgeState left = EdgeState::unknown;  /**< Edge left of the gaze direction. */
    EdgeState right = EdgeState::unknown; /**< Edge right of the gaze direction. */
    EdgeState front = EdgeState::unknown; /**< Edge in front of the cell, in the gaze direction. */
    float distance = 0.f;                 /**< Distance from the robot to the cell centre in mm. */
  };

  class MazeMapFusion
  {
  public:
    explicit MazeMapFusion(const FusionParameters& parameters = FusionParameters()) : parameters(parameters) {}

    /** Weight of a wall observation made from `distanceMm` away. */
    [[nodiscard]] float wallWeight(float distanceMm) const;

    /**
     * Adds one observation of the edge on side `heading` of a cell.
     * @param time Time of the observation in ms; `untimed` skips the rate limit of the edge.
     * @return Whether the state of the edge changed (the planner has to replan).
     */
    bool observe(int row, int col, Heading heading, bool isWall, float weight, unsigned time = untimed);

    /**
     * Adds a look-ahead. Cell k ahead is the robot's cell moved k steps along `gaze`; "left" is
     * `turned(gaze, -1)`, "right" `turned(gaze, 1)` and "front" the gaze. Unknown entries and cells
     * outside the maze are skipped.
     * @return The number of edges whose state changed.
     */
    int observeCellsAhead(int robotRow, int robotCol, Heading gaze, const std::vector<CellObservation>& cells, unsigned time = untimed);

    /** A decoded tag on `face` of `cell` means that this face is a wall; `observeCellsAhead` then ignores open for that edge. */
    bool observeTag(int cell, Heading face, unsigned time = untimed);

    void markVisited(int row, int col);
    [[nodiscard]] bool visited(int row, int col) const;

    /** Starts the next attempt: the counter grows and the map is kept. */
    void newAttempt() { ++attempt; }

    /** Same map: edges, evidence, visited cells and start cell. The attempt counter is not part of it. */
    [[nodiscard]] bool operator==(const MazeMapFusion& other) const;

    FusionParameters parameters;
    MazeGraph graph;               /**< The believed edges. */
    int startCell = 0;
    int attempt = 1;               /**< Number of the current attempt, 1 for the first. */

  private:
    /** Index of the evidence of an edge; the two sides of a shared edge have the same index. */
    [[nodiscard]] static std::size_t index(int row, int col, Heading heading);

    std::array<float, cellCount * headingCount> evidence = {}; /**< Per edge: unknown, signed total; decided, only the contrary part. */
    std::array<unsigned, cellCount * headingCount> lastObservation = {}; /**< Per edge: time of the last accepted timed observation. */
    std::array<bool, cellCount * headingCount> hasObservation = {};
    std::array<bool, cellCount * headingCount> tagged = {}; /**< Per edge: a tag was seen on one of its faces. */
    std::array<bool, cellCount> visitedCells = {};
  };
}
