/**
 * @file Tools/Maze/MazeStartConfig.cpp
 *
 * Implementation of the start setting, see the header.
 */

#include "MazeStartConfig.h"
#include "MazeStartDeduction.h"
#include "Streaming/InStreams.h"
#include "Streaming/TypeRegistry.h"

namespace Maze
{
  StartChoice resolveStart(const StartConfig& config, std::string& error)
  {
    error.clear();
    StartChoice choice;
    if(config.cell == -1)
      return choice;
    if(!StartDeduction::isValidStartCell(config.cell))
    {
      error = "start.cell " + std::to_string(config.cell) + " is neither -1 nor a corner (0, 7, 56, 63), the start is deduced.";
      return choice;
    }
    const Heading heading = toHeading(config.heading);
    if(StartDeduction::indexOf(config.cell, heading) < 0)
    {
      error = "start.heading " + std::string(TypeRegistry::getEnumName(config.heading)) + " does not point into the maze from cell " +
              std::to_string(config.cell) + ", the start is deduced.";
      return choice;
    }
    choice.given = true;
    choice.cell = config.cell;
    choice.heading = heading;
    return choice;
  }

  bool StartFile::load(const std::string& name, StartFile& file)
  {
    // The entries of the behaviour in the same file are top-level keys that `StartFile` does not declare; the map stream
    // only reports unused attributes of the records it enters, so they are ignored.
    InMapFile stream(name);
    if(!stream.exists())
      return false;
    stream >> file;
    return true;
  }
}
