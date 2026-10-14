/**
 * @file Tools/Maze/MazeFile.h
 *
 * Reading, validating and writing `.maze` files (17 lines of 17 characters, see
 * Util/MazeTools/generate_maze.py) and the FNV-1a checksum used by expected.csv.
 */

#pragma once

#include "MazeGraph.h"
#include <cstdint>
#include <string>

namespace Maze
{
  /** A fully known maze together with the start of the run. */
  struct MazeDescription
  {
    MazeGraph graph; /**< Every edge is open or wall. */
    int startCell = 0;
    Heading startHeading = Heading::north; /**< The only open side of the start cell. */
  };

  namespace MazeFile
  {
    /**
     * Parses and validates `text` with the same rules as `validate` in generate_maze.py.
     * @param error Receives a short message for the first broken rule if parsing fails.
     * @return Whether `maze` was filled.
     */
    [[nodiscard]] bool parse(const std::string& text, MazeDescription& maze, std::string& error);

    /** Reads the file at `path` and parses it. */
    [[nodiscard]] bool load(const std::string& path, MazeDescription& maze, std::string& error);

    /** The 17 lines of the maze, each ended by '\n'. Edges that are not open are written as walls. */
    [[nodiscard]] std::string toText(const MazeDescription& maze);

    [[nodiscard]] bool save(const std::string& path, const MazeDescription& maze, std::string& error);

    /** FNV-1a, 32 bit, over the bytes of `data`. */
    [[nodiscard]] std::uint32_t fnv1a32(const std::string& data);

    /** `value` as exactly 8 lowercase hexadecimal digits. */
    [[nodiscard]] std::string toHex(std::uint32_t value);
  }
}
