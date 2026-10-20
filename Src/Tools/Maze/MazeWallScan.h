/**
 * @file Tools/Maze/MazeWallScan.h
 *
 * Finds the floor-to-wall boundary in a grey image, column by column. Free of representations and modules
 * (a plain buffer and a projection callback) so that it can be unit tested.
 *
 * The rules give light walls and a matte uniform floor of unknown colour, and the simulator renders walls darker than
 * the floor, so the boundary is a brightness step in either direction, not a colour and not a polarity: in a column that
 * is scanned from the bottom row up to the horizon row, a candidate boundary is a row where the mean of `smoothRows`
 * rows above it differs by at least `minGradient` grey levels from the mean of `smoothRows` rows below it. Within a run
 * of such rows with the same sign the strongest one is taken, the lowest one on a tie. The boundary is the last floor
 * row. The sign of the step is the direction in which the wall differs from the floor (polarity "auto").
 *
 * A candidate is accepted only if what lies above it is a wall, which rejects floor markings. The blue goal patch (2x2
 * cells, 1000 mm deep) has about the grey of a wall, so both of its edges look like boundaries:
 *  1. Floor reference. The brightness of the floor is the median over the scanned columns of the mean of the bottom
 *     `floorReferenceRows` rows. The region right above the candidate has to differ from it by at least
 *     `minWallContrast`, in the direction of the step. Beyond the far edge of the patch there is floor again, which
 *     has no contrast to the reference.
 *  2. Floor visible. The bottom rows of the column and the rows below the boundary have to look like the
 *     floor: they differ from the floor reference by at most `maxFloorDeviation`. The rows looked at start `smoothRows`
 *     rows under the step window, because the edge of a wall is a ramp of several rows in the image. A column that shows
 *     wall at its bottom (its floor is below the image) has no boundary, which also rules out the steps of a wall, such
 *     as the edges of an AprilTag whose white border is as bright as the floor.
 *  3. Vertical extent. Above a true wall boundary the column stays wall up to `minWallCheckHeightMm`, a height
 *     near the camera but below the wall top (walls are 600 mm or higher). The rows up to that height must not look
 *     like floor again: at least `minWallRowFraction` of them differ from the floor reference by half the contrast
 *     or more, in either direction (so an AprilTag, black and white and 150 mm tall from 220 mm up, does not break
 *     a wall), and no run of more than `maxFloorRunRows` floor-like rows occurs. A patch ends 1000 mm behind its
 *     near edge, and the floor behind it shows as such a run, as does a narrow marking. Rows beyond the top of the
 *     image are not checked, but `minCheckRows` have to remain.
 * 4. Not coloured. Walls and floor have no colour by the rules, a floor marking such as the goal patch has. With a
 *     saturation buffer and `maxWallSaturation` > 0, a candidate is rejected if the `saturationRows` rows above it (after
 *     the `smoothRows` of the edge ramp) are more saturated than the floor reference (the median of the bottom rows) by more
 *     than that. It catches what the checks above cannot: a patch that continues to the top of the image, or that touches a
 *     wall, looks like a wall in the grey image. The grey gradient stays the detector; colour only vetoes.
 * A column in which a step was met but no boundary accepted is `rejected`: its floor is not known to be free, so it
 * must not give clear (open) evidence.
 * 5. Standing on a marking. Bottom rows that are coloured (more saturated than `maxWallSaturation`) are a floor marking such as
 *     the goal patch under the robot, not the floor. The far edge of the patch (patch to floor) is then a step to a region
 *     that a patch-valued floor reference takes for a wall, and the colour veto does not fire (the region above is less
 *     coloured than the bottom). The floor reference is then the median over the columns that are not coloured if they are
 *     at least half of them; otherwise the image gives no boundary and rejects every column (no clear evidence either).
 * Limitations: a patch edge that is below the lowest visible floor row cannot be rejected, because the patch is then
 * the floor reference; a patch that continues to the top of the image (its far edge is not in the picture) looks like a
 * wall from its near edge; and a patch with a grey between floor and wall that touches a wall is indistinguishable
 * from a wall that starts at the patch edge. The minimum supported contrast between wall and floor is
 * `minGradient` (default 12 grey levels). The floor reference assumes that the floor is the majority of the bottom
 * rows, unless they are coloured (5.); a patch that differs from the floor by more than `maxFloorDeviation` in grey makes
 * the columns that end on it give no boundary.
 */

#pragma once

#include <cstddef>
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
      float maxFloorDeviation = 30.f;      /**< Largest difference in grey levels between the bottom rows of a column, or the rows below a boundary, and the floor reference. */
      float minWallContrast = 8.f;         /**< Least brightness of the wall above the floor reference in grey levels. */
      float minWallCheckHeightMm = 550.f;  /**< Height up to which the column has to stay wall. */
      float minWallRowFraction = 0.75f;    /**< Part of the rows up to that height that have to keep the wall contrast. */
      int maxFloorRunRows = 4;             /**< Longest run of floor-like rows allowed above a boundary within the check height. */
      int minCheckRows = 3;                /**< Rows that have to remain for the check after cutting at the image top. */
      int floorReferenceRows = 8;          /**< Bottom rows that give the floor reference. */
      float maxWallSaturation = 0.f;       /**< Most that the saturation above a boundary may exceed the saturation of the floor, in levels of the saturation buffer; 0 or less switches the veto off. */
      int saturationRows = 8;              /**< Rows above a boundary (leaving out the ramp of the edge, `smoothRows` rows) that give the saturation of the region above it. */
    };

    /** The boundary of one scanned column. */
    struct ColumnBoundary
    {
      int column = 0;      /**< Scanned column. */
      int row = -1;        /**< Last floor row, -1 if the column has no accepted boundary. */
      float strength = 0.f; /**< Size of the brightness step of the boundary in grey levels. */
      float bottom = 0.f;   /**< Mean brightness of the bottom `floorReferenceRows` rows of the column, the floor next to the robot. */
      bool rejected = false; /**< A brightness step of at least `minGradient` was met but not accepted as a boundary (a goal patch edge, a wall that fails a check): something is in the column, so it is not known to be free floor. */

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
     * @param floorReference Receives the floor reference (the median of the `bottom` values), if not null.
     * @param saturation Optional saturation of each pixel, in a buffer of the size of the image; used for the saturation veto.
     * @return One entry per scanned column, in increasing column order.
     */
    [[nodiscard]] std::vector<ColumnBoundary> scanColumns(const GreyImage& image, int horizonRow,
                                                          const HeightProjection& projection, const Params& params = Params(),
                                                          float* floorReference = nullptr, const GreyImage* saturation = nullptr);

    /**
     * The row of the topmost floor that is within `maxDistanceMm` of the camera, searched from the bottom row up.
     * @param floorDistance Distance in mm from the camera to the floor point of a row of the middle column; false if the
     *                      row is above the horizon.
     * @return The row, -1 if not even the bottom row sees floor that near.
     */
    [[nodiscard]] int horizonRow(int imageHeight, const std::function<bool(int row, float& distanceMm)>& floorDistance, float maxDistanceMm);

    /**
     * What the perceptor knows about the brightness of the floor at the bottom of the image. A column without a
     * boundary only counts as free floor if its bottom rows look like the floor: otherwise a wall closer than the
     * lowest visible floor row fills the column and would be taken for free floor.
     *
     * Learning: a frame teaches the floor when at least half of the columns have a boundary (then the median of the
     * bottoms is floor). The first lesson sets the brightness, later ones move it by `trackingRate` of the difference, so
     * the model follows a change of the lighting. After `maxAgeMs` without a lesson the model is forgotten. Until
     * a lesson, `isFloor` is false for every column (cold start: no clear points). `reset` forgets at once, for a
     * new attempt, a pick-up or a penalty. A time before the last lesson (the clock went back) forgets as well.
     */
    class FloorModel
    {
    public:
      struct Params
      {
        float tolerance = 8.f;       /**< Largest difference in grey levels between a column bottom and the floor. */
        float trackingRate = 0.1f;   /**< Part of the difference to a new floor reference that is followed per lesson. */
        unsigned maxAgeMs = 10000;   /**< Time without a lesson after which the floor is forgotten. */
      };

      Params params; /**< May change between calls. */

      void reset();

      /** Learns from one scan; call once per image. */
      void observe(const std::vector<ColumnBoundary>& columns, float floorReference, unsigned timeMs);

      /** Whether the floor is known at `timeMs`. */
      [[nodiscard]] bool known(unsigned timeMs) const;

      /** Whether a column bottom of this brightness is floor. */
      [[nodiscard]] bool isFloor(float bottomBrightness, unsigned timeMs) const;

      [[nodiscard]] float brightness() const { return floor; }

    private:
      bool learned = false;
      float floor = 0.f;
      unsigned lastLessonMs = 0;
    };
  }
}
