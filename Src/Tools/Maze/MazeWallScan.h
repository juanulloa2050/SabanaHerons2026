/**
 * @file Tools/Maze/MazeWallScan.h
 *
 * Finds the floor-to-wall boundary in a grey image, column by column. Free of representations and modules
 * (a plain buffer and a projection callback) so that it can be unit tested.
 *
 * The rules give light walls and a matte uniform floor whose colour is not fixed, so the boundary is a
 * brightness step, not a colour: in a column that is scanned from the bottom row up to the horizon row, a
 * candidate boundary is a row where the mean of `smoothRows` rows above it is at least `minGradient` grey
 * levels brighter than the mean of `smoothRows` rows below it (a dark-to-bright step going up). Within a run
 * of such rows the strongest one is taken, the lowest one on a tie. The boundary is the last floor row.
 *
 * A candidate is accepted only if the floor above it is really a wall, which rejects floor markings. The
 * goal patch is darker than the floor, so its far edge is a dark-to-bright step like a wall boundary:
 *  1. Floor reference. The brightness of the floor is the median over the scanned columns of the mean of the
 *     bottom `floorReferenceRows` rows. The wall brightness (the mean of the rows right above the candidate)
 *     has to exceed it by at least `minWallContrast`. Beyond the far edge of the patch there is floor again,
 *     which has no contrast to the reference.
 *  2. Vertical extent. Above a true wall boundary the column stays wall up to a height of
 *     `minWallCheckHeightMm` (below the AprilTags, whose lower edge is at 220 mm or more): every row up to
 *     that height has to exceed the floor reference by `minWallContrast / 2`. A narrow bright marking on
 *     the floor ends after a few rows and fails.
 * Walls darker than the floor are not detected. The minimum supported contrast between wall and floor is
 * `minGradient` (default 12 grey levels). Limitation: the floor reference assumes that the floor is the
 * majority of the bottom rows, so a robot that stands on the goal patch and looks across it would take the
 * patch for the floor.
 */

#pragma once

#include <functional>
#include <vector>

namespace Maze
{
  namespace WallScan
  {
    /** A grey image as a plain buffer; `stride` is the distance between two rows in bytes. */
    struct GreyImage
    {
      const unsigned char* data = nullptr;
      int width = 0, height = 0, stride = 0;
    };

    struct Params
    {
      int columnStep = 8;                  /**< Distance between the scanned columns in pixels. */
      int smoothRows = 3;                  /**< Rows averaged on each side of a candidate. */
      int columnHalfWidth = 1;             /**< Columns on each side of the scanned one that are averaged. */
      float minGradient = 12.f;            /**< Least brightness step of a boundary in grey levels. */
      float minWallContrast = 8.f;         /**< Least brightness of the wall above the floor reference in grey levels. */
      float minWallCheckHeightMm = 150.f;  /**< Height up to which the column has to stay wall. */
      int floorReferenceRows = 8;          /**< Bottom rows that give the floor reference. */
    };

    /** The boundary of one scanned column. */
    struct ColumnBoundary
    {
      int column = 0;      /**< Scanned column. */
      int row = -1;        /**< Last floor row, -1 if the column has no accepted boundary. */
      float strength = 0.f; /**< Brightness step of the boundary in grey levels. */

      [[nodiscard]] bool found() const
      {
        return row >= 0;
      }
    };

    /**
     * Maps a floor point to the image row of the point `heightMm` above it.
     * @param column Image column of the floor point.
     * @param row Image row of the floor point.
     * @param rowAtHeight Receives the image row of the point `heightMm` above the floor point (may be negative).
     * @return Whether the projection is defined.
     */
    using HeightProjection = std::function<bool(int column, int row, float heightMm, int& rowAtHeight)>;

    /**
     * Scans the columns `columnStep / 2`, `columnStep / 2 + columnStep`, ... from the bottom row up to `horizonRow`.
     * @param horizonRow Topmost row of a boundary; rows above are not searched.
     * @param projection Used for the vertical extent check; a boundary for which it is undefined is rejected.
     * @return One entry per scanned column, in increasing column order.
     */
    [[nodiscard]] std::vector<ColumnBoundary> scanColumns(const GreyImage& image, int horizonRow,
                                                          const HeightProjection& projection, const Params& params = Params());
  }
}
