/**
 * @file Tools/Maze/MazeEdgeIndex.h
 *
 * Flat storage of the wall slots (edges) of the maze, as `MazeMap` streams them: `horizontal[r * 8 + c]` is
 * the edge above cell (r, c) for r = 0..8 (row 8 is the bottom border) and `vertical[r * 9 + c]` is the edge
 * left of cell (r, c) for c = 0..8 (column 8 is the right border), the same layout as `Maze::MazeGraph`.
 * The functions are templates over the edge type so that a streamable enum with the order of
 * `Maze::EdgeState` (unknown, open, wall) can use them. Out-of-range cells are handled safely: a read
 * returns `unknown` and a write is ignored. Pure standard C++ so that it can be unit tested.
 */

#pragma once

#include "MazeGraph.h"
#include <array>
#include <cstddef>

namespace Maze
{
  namespace EdgeIndex
  {
    constexpr int horizontalCount = (size + 1) * size; /**< Edges above the cells, including the bottom border. */
    constexpr int verticalCount = size * (size + 1);   /**< Edges left of the cells, including the right border. */

    /** Where the edge on one side of a cell lives. */
    struct Slot
    {
      bool valid = false;      /**< False for a cell outside the maze or an invalid heading. */
      bool horizontal = false; /**< In the horizontal array, otherwise in the vertical one. */
      std::size_t index = 0;
    };

    /** The slot of the edge on side `heading` of (row, col); the neighbour on that side has the same slot. */
    [[nodiscard]] constexpr Slot slotOf(int row, int col, Heading heading)
    {
      if(!inside(row, col))
        return {};
      switch(heading)
      {
        case Heading::north: return {true, true, static_cast<std::size_t>(row * size + col)};
        case Heading::south: return {true, true, static_cast<std::size_t>((row + 1) * size + col)};
        case Heading::west: return {true, false, static_cast<std::size_t>(row * (size + 1) + col)};
        case Heading::east: return {true, false, static_cast<std::size_t>(row * (size + 1) + col + 1)};
      }
      return {};
    }

    /** The edge on side `heading` of (row, col), `unknown` outside the maze. */
    template<typename Edge>
    [[nodiscard]] Edge get(const std::array<Edge, horizontalCount>& horizontal, const std::array<Edge, verticalCount>& vertical,
                           int row, int col, Heading heading)
    {
      const Slot slot = slotOf(row, col, heading);
      if(!slot.valid)
        return static_cast<Edge>(static_cast<int>(EdgeState::unknown));
      return slot.horizontal ? horizontal[slot.index] : vertical[slot.index];
    }

    /** Sets the edge on side `heading` of (row, col), including the border; ignored outside the maze. */
    template<typename Edge>
    void set(std::array<Edge, horizontalCount>& horizontal, std::array<Edge, verticalCount>& vertical,
             int row, int col, Heading heading, Edge edge)
    {
      const Slot slot = slotOf(row, col, heading);
      if(slot.valid)
        (slot.horizontal ? horizontal[slot.index] : vertical[slot.index]) = edge;
    }

    /** Copies the edges of a graph, including the border. */
    template<typename Edge>
    void setEdges(std::array<Edge, horizontalCount>& horizontal, std::array<Edge, verticalCount>& vertical, const MazeGraph& graph)
    {
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
          for(int h = 0; h < headingCount; ++h)
          {
            const Heading heading = static_cast<Heading>(h);
            set(horizontal, vertical, row, col, heading, static_cast<Edge>(static_cast<int>(graph.get(row, col, heading))));
          }
    }

    /** The edges as a graph; the border of the graph is always a wall. */
    template<typename Edge>
    [[nodiscard]] MazeGraph toGraph(const std::array<Edge, horizontalCount>& horizontal, const std::array<Edge, verticalCount>& vertical)
    {
      MazeGraph graph;
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
          for(int h = 0; h < headingCount; ++h)
          {
            const Heading heading = static_cast<Heading>(h);
            graph.set(row, col, heading, static_cast<EdgeState>(static_cast<int>(get(horizontal, vertical, row, col, heading))));
          }
      return graph;
    }

    /** Number of inner edges that are still unknown; each shared edge counts once. */
    template<typename Edge>
    [[nodiscard]] int unknownCount(const std::array<Edge, horizontalCount>& horizontal, const std::array<Edge, verticalCount>& vertical)
    {
      const Edge unknown = static_cast<Edge>(static_cast<int>(EdgeState::unknown));
      int count = 0;
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
        {
          // The north and west side of every cell; the south and east border are always walls.
          count += get(horizontal, vertical, row, col, Heading::north) == unknown ? 1 : 0;
          count += get(horizontal, vertical, row, col, Heading::west) == unknown ? 1 : 0;
        }
      return count;
    }
  }
}
