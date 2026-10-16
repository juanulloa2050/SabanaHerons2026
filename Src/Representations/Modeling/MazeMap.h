/**
 * @file MazeMap.h
 *
 * The wall map that the robot believes in, provided by MazeMapper and kept for the whole process:
 * every wall slot (edge) is unknown, open or a wall, which cells were visited, the start cell and the
 * number of the current attempt. The outer border is always a wall.
 *
 * Edges are stored flat so that the representation streams and logs plainly:
 * `horizontal[r * 8 + c]` is the edge above cell (r, c) for r = 0..8 (row 8 is the bottom border) and
 * `vertical[r * 9 + c]` is the edge left of cell (r, c) for c = 0..8 (column 8 is the right border), as
 * in `Maze::MazeGraph`. `toGraph` rebuilds a `Maze::MazeGraph` for the planner.
 *
 * A planner replans when `changeCounter` differs from the value it planned with.
 */

#pragma once

#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include "Tools/Maze/MazeGraph.h"
#include <array>

STREAMABLE(MazeMap,
{
  /** What is known about one wall slot; same order as `Maze::EdgeState`. */
  ENUM(Edge,
  {,
    unknown, /**< Not decided yet. */
    open,    /**< Free to pass. */
    wall,    /**< A wall. */
  });

  static constexpr int horizontalCount = (Maze::size + 1) * Maze::size; /**< Edges above the cells, including the bottom border. */
  static constexpr int verticalCount = Maze::size * (Maze::size + 1);   /**< Edges left of the cells, including the right border. */

  /** The edge on side `heading` of cell (row, col); the neighbour on that side reads the same edge. */
  Edge get(int row, int col, Maze::Heading heading) const;

  /** Sets the edge on side `heading` of cell (row, col). Unlike `Maze::MazeGraph::set` this also writes the border. */
  void set(int row, int col, Maze::Heading heading, Edge edge);

  /** Copies the edges of a graph, including the border. */
  void setEdges(const Maze::MazeGraph& graph);

  /** The believed edges as a graph for the planner. */
  Maze::MazeGraph toGraph() const;

  /** Whether the cell is one of the four goal cells. */
  static bool isGoal(int cell) { return Maze::isGoal(Maze::cellRow(cell), Maze::cellCol(cell)); }

  /** Number of inner edges that are still unknown. */
  int unknownEdges() const;

  /** Draws walls, open edges, unknown edges and visited cells in the maze frame. */
  void draw() const,

  (std::array<Edge, horizontalCount>)({}) horizontal, /**< Edge above each cell, row by row; initially unknown. */
  (std::array<Edge, verticalCount>)({}) vertical,     /**< Edge left of each cell, row by row; initially unknown. */
  (std::array<bool, Maze::cellCount>)({}) visited,    /**< Cells the robot has stood in, indexed by cell number. */
  (int)(0) startCell,                             /**< Cell the attempts start in. */
  (int)(1) attempt,                               /**< Number of the current attempt, 1 for the first. */
  (unsigned)(0) changeCounter,                    /**< Grows whenever an edge changed its state; a planner replans when it differs. */
  (unsigned)(0) lastChangeTime,                   /**< Time of the last edge change in ms. */
});
