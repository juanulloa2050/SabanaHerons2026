/**
 * @file Tools/Maze/MazeTagLayout.h
 *
 * Which wall faces carry an AprilTag. The organisers have not said yet which walls carry tags
 * and at what height (question Q1), so no module hard-codes a layout: they all ask this model,
 * which is read from `mazeTagLayout.cfg`. Faces are absolute (maze frame, north is the top row).
 * The tag id of a cell is its cell index, see `Maze::cellId`.
 */

#pragma once

#include "MazeGraph.h"
#include "Streaming/AutoStreamable.h"
#include "Streaming/Enum.h"
#include <string>
#include <vector>

namespace Maze
{
  /** How the layout selects the wall faces that carry a tag. */
  ENUM(TagLayoutMode,
  {,
    allFaces, /**< A tag on every wall face of every cell. */
    faces,    /**< A tag only on the faces listed in `faces`. */
    walls,    /**< A tag only on the (cell, face) pairs listed in `walls`. */
  });

  /** Face of a cell in the maze frame; same order as `Maze::Heading`. */
  ENUM(TagFace,
  {,
    north,
    east,
    south,
    west,
  });

  [[nodiscard]] constexpr Heading toHeading(TagFace face)
  {
    return static_cast<Heading>(static_cast<unsigned char>(face));
  }

  [[nodiscard]] constexpr TagFace toTagFace(Heading heading)
  {
    return static_cast<TagFace>(static_cast<unsigned char>(heading));
  }

  STREAMABLE(TagWall,
  {,
    (int)(0) cell,           /**< Cell index (row * 8 + column), which is also the tag id. */
    (TagFace)(north) face,   /**< Face of that cell. */
  });

  STREAMABLE(MazeTagLayout,
  {
    /**
     * Whether the face `face` of cell `cell` carries a tag: the layout lists it and the maze has a wall there.
     * Edges that are not a wall (open or unknown) never carry a tag.
     */
    bool hasTag(int cell, Heading face, const MazeGraph& maze) const;

    /** The faces that may carry the tag with id `cell` according to the layout, regardless of the maze. */
    std::vector<Heading> candidateFaces(int cell) const;

    /**
     * Reads a layout from a config file.
     * @param name Name of the config file, searched like any other configuration file.
     * @param layout Receives the layout if the file exists.
     * @return Whether the file was found and read.
     */
    static bool load(const std::string& name, MazeTagLayout& layout),

    (TagLayoutMode)(allFaces) mode,                                       /**< How the faces are selected. */
    (std::vector<TagFace>)({north COMMA east COMMA south COMMA west}) faces, /**< Faces used when `mode` is `faces`. */
    (std::vector<TagWall>) walls,                                          /**< Pairs used when `mode` is `walls`. */
    (float)(300.f) centreHeight,                                           /**< Height of the tag centre above the floor in mm. */
    (float)(150.f) size,                                                   /**< Edge of the black outer square in mm. */
  });
}
