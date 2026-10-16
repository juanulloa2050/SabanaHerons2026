/**
 * @file Tools/Maze/MazeTagLayout.cpp
 *
 * Implementation of the AprilTag layout of the maze.
 */

#include "MazeTagLayout.h"
#include "Streaming/InStreams.h"
#include <algorithm>

namespace Maze
{
  bool MazeTagLayout::hasTag(int cell, Heading face, const MazeGraph& maze) const
  {
    if(cell < 0 || cell >= cellCount)
      return false;
    const std::vector<Heading> candidates = candidateFaces(cell);
    if(std::find(candidates.begin(), candidates.end(), face) == candidates.end())
      return false;
    return maze.get(cellRow(cell), cellCol(cell), face) == EdgeState::wall;
  }

  std::vector<Heading> MazeTagLayout::candidateFaces(int cell) const
  {
    std::vector<Heading> result;
    if(cell < 0 || cell >= cellCount)
      return result;
    switch(mode)
    {
      case TagLayoutMode::allFaces:
        for(int h = 0; h < headingCount; ++h)
          result.push_back(static_cast<Heading>(h));
        break;
      case TagLayoutMode::faces:
        // Walk the faces in maze order so that the result does not depend on the order in the file.
        for(int h = 0; h < headingCount; ++h)
          if(std::find(this->faces.begin(), this->faces.end(), toTagFace(static_cast<Heading>(h))) != this->faces.end())
            result.push_back(static_cast<Heading>(h));
        break;
      case TagLayoutMode::walls:
        for(int h = 0; h < headingCount; ++h)
          if(std::any_of(this->walls.begin(), this->walls.end(), [&](const TagWall& wall)
                         { return wall.cell == cell && toHeading(wall.face) == static_cast<Heading>(h); }))
            result.push_back(static_cast<Heading>(h));
        break;
      default:
        break;
    }
    return result;
  }

  bool MazeTagLayout::load(const std::string& name, MazeTagLayout& layout)
  {
    InMapFile stream(name);
    if(!stream.exists())
      return false;
    stream >> layout;
    return true;
  }
}
