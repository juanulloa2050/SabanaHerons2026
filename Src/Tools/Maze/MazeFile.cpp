/**
 * @file Tools/Maze/MazeFile.cpp
 *
 * Implementation of the `.maze` file reader and writer.
 */

#include "MazeFile.h"
#include <array>
#include <fstream>
#include <sstream>
#include <vector>

namespace Maze
{
  namespace MazeFile
  {
    namespace
    {
      constexpr int textSize = 2 * size + 1;
      constexpr std::array<int, 4> cornerCells = {0, size - 1, size * (size - 1), size * size - 1};

      Heading heading(int h)
      {
        return static_cast<Heading>(h);
      }

      /** Position of the wall character on side `h` of cell (row, col) in the text grid. */
      int wallLine(int row, Heading h)
      {
        return 2 * row + 1 + rowDelta[static_cast<std::size_t>(h)];
      }

      int wallColumn(int col, Heading h)
      {
        return 2 * col + 1 + colDelta[static_cast<std::size_t>(h)];
      }

      bool fail(std::string& error, const std::string& message)
      {
        error = message;
        return false;
      }

      int reachableCells(const MazeGraph& graph, int startCell)
      {
        std::vector<bool> seen(cellCount, false);
        std::vector<int> stack = {startCell};
        seen[static_cast<std::size_t>(startCell)] = true;
        int count = 0;
        while(!stack.empty())
        {
          const int cell = stack.back();
          stack.pop_back();
          ++count;
          for(int h = 0; h < headingCount; ++h)
          {
            const int row = cellRow(cell), col = cellCol(cell);
            const int nextRow = row + rowDelta[static_cast<std::size_t>(h)], nextCol = col + colDelta[static_cast<std::size_t>(h)];
            if(inside(nextRow, nextCol) && graph.get(row, col, heading(h)) == EdgeState::open && !seen[static_cast<std::size_t>(cellId(nextRow, nextCol))])
            {
              seen[static_cast<std::size_t>(cellId(nextRow, nextCol))] = true;
              stack.push_back(cellId(nextRow, nextCol));
            }
          }
        }
        return count;
      }
    }

    bool parse(const std::string& text, MazeDescription& maze, std::string& error)
    {
      // Same line splitting as generate_maze.parse: CRLF -> LF, drop trailing newlines.
      std::string normalized;
      for(std::size_t i = 0; i < text.size(); ++i)
        if(text[i] != '\r' || i + 1 >= text.size() || text[i + 1] != '\n')
          normalized += text[i];
      while(!normalized.empty() && normalized.back() == '\n')
        normalized.pop_back();
      std::vector<std::string> lines = {""};
      for(const char c : normalized)
        if(c == '\n')
          lines.emplace_back();
        else
          lines.back() += c;

      if(static_cast<int>(lines.size()) != textSize)
        return fail(error, "expected " + std::to_string(textSize) + " lines, found " + std::to_string(lines.size()));
      for(int i = 0; i < textSize; ++i)
        if(static_cast<int>(lines[static_cast<std::size_t>(i)].size()) != textSize)
          return fail(error, "line " + std::to_string(i) + " has length " + std::to_string(lines[static_cast<std::size_t>(i)].size()) + ", expected " + std::to_string(textSize));
      const auto at = [&lines](int line, int col) { return lines[static_cast<std::size_t>(line)][static_cast<std::size_t>(col)]; };

      for(int i = 0; i < textSize; ++i)
        for(int j = 0; j < textSize; ++j)
        {
          const std::string where = "line " + std::to_string(i) + " col " + std::to_string(j);
          if(i % 2 == 0 && j % 2 == 0 && at(i, j) != '+')
            return fail(error, "corner missing at " + where);
          if(i % 2 != j % 2 && at(i, j) != ' ' && at(i, j) != (i % 2 == 0 ? '-' : '|'))
            return fail(error, std::string("invalid wall character '") + at(i, j) + "' at " + where);
        }

      std::vector<int> starts, goals;
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
        {
          const char c = at(2 * row + 1, 2 * col + 1);
          if(c != 'S' && c != 'G' && c != ' ')
            return fail(error, std::string("invalid cell character '") + c + "' at cell (" + std::to_string(row) + "," + std::to_string(col) + ")");
          if(c == 'S')
            starts.push_back(cellId(row, col));
          else if(c == 'G')
            goals.push_back(cellId(row, col));
        }
      if(starts.size() != 1)
        return fail(error, "expected exactly one S, found " + std::to_string(starts.size()));
      if(goals != std::vector<int>{27, 28, 35, 36})
        return fail(error, "G must be in exactly cells 27, 28, 35, 36");
      const int startCell = starts[0];
      bool corner = false;
      for(const int c : cornerCells)
        corner = corner || c == startCell;
      if(!corner)
        return fail(error, "start cell " + std::to_string(startCell) + " is not in a corner");

      MazeDescription result;
      result.startCell = startCell;
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
          for(int h = 0; h < headingCount; ++h)
          {
            const bool isOpen = at(wallLine(row, heading(h)), wallColumn(col, heading(h))) == ' ';
            const bool border = !inside(row + rowDelta[static_cast<std::size_t>(h)], col + colDelta[static_cast<std::size_t>(h)]);
            if(border && isOpen)
              return fail(error, "border open at line " + std::to_string(wallLine(row, heading(h))) + " col " + std::to_string(wallColumn(col, heading(h))));
            result.graph.set(row, col, heading(h), isOpen ? EdgeState::open : EdgeState::wall);
          }

      int openSides = 0;
      for(int h = 0; h < headingCount; ++h)
        if(result.graph.get(cellRow(startCell), cellCol(startCell), heading(h)) == EdgeState::open)
        {
          ++openSides;
          result.startHeading = heading(h);
        }
      if(openSides != 1)
        return fail(error, "start has " + std::to_string(headingCount - openSides) + " walls, expected 3");

      for(const int cell : goals)
        for(int h = 0; h < headingCount; ++h)
        {
          const int row = cellRow(cell), col = cellCol(cell);
          if(isGoal(row + rowDelta[static_cast<std::size_t>(h)], col + colDelta[static_cast<std::size_t>(h)]) && result.graph.get(row, col, heading(h)) != EdgeState::open)
            return fail(error, "wall inside goal block at line " + std::to_string(wallLine(row, heading(h))) + " col " + std::to_string(wallColumn(col, heading(h))));
        }

      const int unreachable = cellCount - reachableCells(result.graph, startCell);
      if(unreachable != 0)
        return fail(error, std::to_string(unreachable) + " cells unreachable from start");

      maze = result;
      return true;
    }

    bool load(const std::string& path, MazeDescription& maze, std::string& error)
    {
      std::ifstream file(path, std::ios::binary);
      if(!file)
        return fail(error, "cannot open " + path);
      std::ostringstream content;
      content << file.rdbuf();
      return parse(content.str(), maze, error);
    }

    std::string toText(const MazeDescription& maze)
    {
      std::vector<std::string> lines(textSize, std::string(textSize, ' '));
      for(int i = 0; i < textSize; i += 2)
        for(int j = 0; j < textSize; j += 2)
          lines[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = '+';
      for(int row = 0; row < size; ++row)
        for(int col = 0; col < size; ++col)
        {
          lines[static_cast<std::size_t>(2 * row + 1)][static_cast<std::size_t>(2 * col + 1)] =
            cellId(row, col) == maze.startCell ? 'S' : isGoal(row, col) ? 'G' : ' ';
          for(int h = 0; h < headingCount; ++h)
            if(maze.graph.get(row, col, heading(h)) != EdgeState::open)
              lines[static_cast<std::size_t>(wallLine(row, heading(h)))][static_cast<std::size_t>(wallColumn(col, heading(h)))] =
                h % 2 == 0 ? '-' : '|';
        }
      std::string text;
      for(const std::string& line : lines)
        text += line + '\n';
      return text;
    }

    bool save(const std::string& path, const MazeDescription& maze, std::string& error)
    {
      std::ofstream file(path, std::ios::binary);
      file << toText(maze);
      file.flush();
      return file ? true : fail(error, "cannot write " + path);
    }

    std::uint32_t fnv1a32(const std::string& data)
    {
      std::uint32_t hash = 0x811C9DC5u;
      for(const char c : data)
        hash = (hash ^ static_cast<std::uint8_t>(c)) * 0x01000193u;
      return hash;
    }

    std::string toHex(std::uint32_t value)
    {
      std::string text(8, '0');
      for(int i = 7; i >= 0; --i, value >>= 4)
        text[static_cast<std::size_t>(i)] = "0123456789abcdef"[value & 0xF];
      return text;
    }
  }
}
