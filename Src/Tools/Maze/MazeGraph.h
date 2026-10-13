/**
 * @file Tools/Maze/MazeGraph.h
 *
 * An 8x8 maze as a grid graph. Every wall slot between two cells (or between a cell and the
 * outside) is one edge that is unknown, open or a wall, so both neighbours see the same edge.
 * Pure standard C++ so that it can be used by any module and by the unit tests.
 */

#pragma once

#include <array>
#include <cstddef>

namespace Maze
{
  constexpr int size = 8; /**< Cells per side. */
  constexpr int cellCount = size * size;
  constexpr float cellSizeMm = 500.f;

  enum class EdgeState : unsigned char
  {
    unknown,
    open,
    wall,
  };

  /** Clockwise from north. The row index grows towards the south. */
  enum class Heading : unsigned char
  {
    north,
    east,
    south,
    west,
  };

  constexpr int headingCount = 4;
  constexpr std::array<int, headingCount> rowDelta = {-1, 0, 1, 0};
  constexpr std::array<int, headingCount> colDelta = {0, 1, 0, -1};

  /** Heading rotated clockwise by `quarterTurns` * 90 degrees. */
  [[nodiscard]] constexpr Heading turned(Heading heading, int quarterTurns)
  {
    return static_cast<Heading>((static_cast<int>(heading) + quarterTurns % headingCount + headingCount) % headingCount);
  }

  [[nodiscard]] constexpr bool inside(int row, int col)
  {
    return row >= 0 && row < size && col >= 0 && col < size;
  }

  /** Cell index, which is also the tag id of the cell (roadmap 4.5). */
  [[nodiscard]] constexpr int cellId(int row, int col)
  {
    return row * size + col;
  }

  [[nodiscard]] constexpr int cellRow(int id)
  {
    return id / size;
  }

  [[nodiscard]] constexpr int cellCol(int id)
  {
    return id % size;
  }

  /** The goal is the central 2x2 block: cells 27, 28, 35 and 36. */
  [[nodiscard]] constexpr bool isGoal(int row, int col)
  {
    return (row == 3 || row == 4) && (col == 3 || col == 4);
  }

  /** Cell centre on the field in mm: x grows with the column, y towards the top row (roadmap 4.5). */
  struct CellCentre
  {
    float x, y;
  };

  [[nodiscard]] constexpr CellCentre cellCentre(int row, int col)
  {
    return {(static_cast<float>(col) + 0.5f) * cellSizeMm, (static_cast<float>(size) - 0.5f - static_cast<float>(row)) * cellSizeMm};
  }

  /** How edges that are still unknown are treated when asking whether one can pass. */
  enum class UnknownPolicy : unsigned char
  {
    asOpen, /**< Exploration: optimistic. */
    asWall, /**< Speed run: only what has been seen. */
  };

  class MazeGraph
  {
  public:
    /** The outer border is wall, every inner edge is unknown. */
    MazeGraph()
    {
      for(int i = 0; i < size; ++i)
      {
        set(0, i, Heading::north, EdgeState::wall);
        set(size - 1, i, Heading::south, EdgeState::wall);
        set(i, 0, Heading::west, EdgeState::wall);
        set(i, size - 1, Heading::east, EdgeState::wall);
      }
    }

    /** Edge on side `heading` of cell (row, col); the neighbour on that side reads the same edge. */
    [[nodiscard]] EdgeState get(int row, int col, Heading heading) const
    {
      return *edge(row, col, heading);
    }

    void set(int row, int col, Heading heading, EdgeState state)
    {
      *edge(row, col, heading) = state;
    }

    /** Whether one can leave (row, col) towards `heading` when unknown edges count as `policy`. */
    [[nodiscard]] bool canPass(int row, int col, Heading heading, UnknownPolicy policy) const
    {
      const EdgeState state = get(row, col, heading);
      return state == EdgeState::open || (state == EdgeState::unknown && policy == UnknownPolicy::asOpen);
    }

    /** This graph turned clockwise by 90 degrees: cell (r, c) moves to (c, size - 1 - r). */
    [[nodiscard]] MazeGraph rotated90() const
    {
      MazeGraph result;
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
          for(int h = 0; h < headingCount; ++h)
          {
            const Heading heading = static_cast<Heading>(h);
            result.set(col, size - 1 - row, turned(heading, 1), get(row, col, heading));
          }
      return result;
    }

    [[nodiscard]] bool operator==(const MazeGraph& other) const
    {
      return horizontal == other.horizontal && vertical == other.vertical;
    }

  private:
    [[nodiscard]] const EdgeState* edge(int row, int col, Heading heading) const
    {
      switch(heading)
      {
        case Heading::north: return &horizontal[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
        case Heading::south: return &horizontal[static_cast<std::size_t>(row + 1)][static_cast<std::size_t>(col)];
        case Heading::west: return &vertical[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
        default: return &vertical[static_cast<std::size_t>(row)][static_cast<std::size_t>(col + 1)];
      }
    }

    [[nodiscard]] EdgeState* edge(int row, int col, Heading heading)
    {
      return const_cast<EdgeState*>(static_cast<const MazeGraph*>(this)->edge(row, col, heading));
    }

    /** horizontal[r][c] is the edge above cell (r, c); row `size` is the bottom border. */
    std::array<std::array<EdgeState, size>, size + 1> horizontal = {};
    /** vertical[r][c] is the edge left of cell (r, c); column `size` is the right border. */
    std::array<std::array<EdgeState, size + 1>, size> vertical = {};
  };
}
