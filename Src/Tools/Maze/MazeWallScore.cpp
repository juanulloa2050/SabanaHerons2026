/**
 * @file Tools/Maze/MazeWallScore.cpp
 *
 * Implementation of the wall percept scorer, see the header for the metrics.
 */

#include "MazeWallScore.h"
#include "MazeOracle.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Maze
{
  namespace WallScore
  {
    namespace
    {
      constexpr float quarterTurn = pi / 2.f;

      float ratio(int part, int whole)
      {
        return whole > 0 ? static_cast<float>(part) / static_cast<float>(whole) : 0.f;
      }

      /** Distance of `point` to the segment a to b. */
      float distanceToSegment(const Vector2f& point, const Vector2f& a, const Vector2f& b)
      {
        const Vector2f along = b - a;
        const float t = std::clamp((point - a).dot(along) / along.squaredNorm(), 0.f, 1.f);
        return (point - (a + along * t)).norm();
      }

      /** Distance of `point` (maze frame) to the nearest wall of the maze. */
      float distanceToNearestWall(const MazeGraph& maze, const Vector2f& point)
      {
        float nearest = 1e9f;
        for(int row = 0; row < size; ++row)
          for(int col = 0; col < size; ++col)
            for(int h = 0; h < headingCount; ++h)
            {
              const Heading heading = static_cast<Heading>(h);
              const int r = row + rowDelta[static_cast<std::size_t>(h)], c = col + colDelta[static_cast<std::size_t>(h)];
              // Every shared edge once, from its north or west cell.
              if(maze.get(row, col, heading) != EdgeState::wall || ((heading == Heading::south || heading == Heading::east) && inside(r, c)))
                continue;
              const Vector2f direction = Oracle::directionOf(heading), along(-direction.y(), direction.x());
              const Vector2f middle = Oracle::cellCentreOf(row, col) + direction * Oracle::halfCellMm;
              nearest = std::min(nearest, distanceToSegment(point, middle - along * Oracle::halfCellMm, middle + along * Oracle::halfCellMm));
            }
        return nearest;
      }

      ErrorStats statsOf(std::vector<float> samples)
      {
        ErrorStats stats;
        stats.count = static_cast<int>(samples.size());
        if(samples.empty())
          return stats;
        float sum = 0.f;
        for(const float sample : samples)
          sum += sample;
        stats.mean = sum / static_cast<float>(samples.size());
        float squares = 0.f;
        for(const float sample : samples)
          squares += (sample - stats.mean) * (sample - stats.mean);
        stats.standardDeviation = std::sqrt(squares / static_cast<float>(samples.size()));
        std::sort(samples.begin(), samples.end());
        stats.percentile95 = samples[std::min(samples.size() - 1, static_cast<std::size_t>(0.95f * static_cast<float>(samples.size())))];
        return stats;
      }
    }

    float Summary::edgeAccuracy() const { return ratio(wallCorrect + openCorrect, edgesKnown()); }
    float Summary::visitAccuracy() const { return ratio(visitSidesCorrect, visitSidesKnown); }
    float Summary::visitCoverage() const { return ratio(visitSidesKnown, visitSides); }
    float Summary::aheadAccuracy() const { return ratio(aheadSidesCorrect, aheadSidesKnown); }
    float Summary::aheadCoverage() const { return ratio(aheadSidesKnown, aheadSides); }

    void Scorer::add(const Frame& frame)
    {
      int row = 0, col = 0;
      if(!Oracle::cellContaining(frame.truePose.translation, row, col))
      {
        ++totals.skippedFrames;
        return;
      }
      int believedRow = -1, believedCol = -1;
      if(!Oracle::cellContaining(frame.believedPosition, believedRow, believedCol) || believedRow != row || believedCol != col ||
         Oracle::snapToHeading(Angle::normalize(frame.believedRotation)) != Oracle::snapToHeading(Angle::normalize(frame.truePose.rotation)))
      {
        ++totals.poseMismatchFrames;
        return;
      }
      ++totals.frames;
      ++(frame.standing ? totals.standingFrames : totals.walkingFrames);

      if(visiting && (visitRow != row || visitCol != col))
        finishVisit();
      if(!visiting)
      {
        visiting = true;
        visitRow = row;
        visitCol = col;
      }

      const Heading gaze = Oracle::snapToHeading(Angle::normalize(frame.truePose.rotation + static_cast<float>(frame.lookAheadTurns) * quarterTurn));
      const Heading leftHeading = turned(gaze, -1), rightHeading = turned(gaze, 1);

      // Frame edges, the visit votes, the next cell ahead and the range.
      const bool nextVisible = frame.range >= 1 && maze.get(row, col, gaze) == EdgeState::open &&
                               inside(row + rowDelta[static_cast<std::size_t>(gaze)], col + colDelta[static_cast<std::size_t>(gaze)]);
      if(nextVisible)
        totals.aheadSides += 2;
      for(const WallView::Cell& cell : frame.cellsAhead)
      {
        if(cell.ahead > frame.range)
          ++totals.cellsBeyondRange;
        const int r = row + rowDelta[static_cast<std::size_t>(gaze)] * cell.ahead, c = col + colDelta[static_cast<std::size_t>(gaze)] * cell.ahead;
        if(!inside(r, c))
          continue;
        const std::pair<Heading, EdgeState> edges[] = {{leftHeading, cell.left}, {rightHeading, cell.right}, {gaze, cell.front}};
        for(int e = 0; e < 3; ++e)
        {
          const auto& [heading, seen] = edges[e];
          if(seen == EdgeState::unknown)
          {
            ++totals.edgesUnknown;
            continue;
          }
          const EdgeState real = maze.get(r, c, heading);
          const bool right = seen == real;
          (seen == EdgeState::wall ? (right ? totals.wallCorrect : totals.wallWrong) : (right ? totals.openCorrect : totals.openWrong))++;
          Votes& vote = votesOf(r, c, heading);
          (seen == EdgeState::wall ? vote.wall : vote.open)++;
          vote.latest = seen;
          if(cell.ahead == 1 && nextVisible && e < 2)
          {
            ++totals.aheadSidesKnown;
            totals.aheadSidesCorrect += right;
          }
        }
      }

      // Boundary points against the true walls.
      auto& samples = frame.standing ? standingSamples : walkingSamples;
      for(const Vector2f& point : frame.boundaryPoints)
      {
        const int bin = static_cast<int>(std::floor((point.norm() - 250.f) / 500.f));
        if(bin < 0 || bin >= distanceBins)
          continue;
        samples[static_cast<std::size_t>(bin)].push_back(distanceToNearestWall(maze, frame.truePose.translation + Oracle::rotated(point, frame.truePose.rotation)));
      }
    }

    Scorer::Votes& Scorer::votesOf(int row, int col, Heading heading)
    {
      const EdgeIndex::Slot slot = EdgeIndex::slotOf(row, col, heading);
      return slot.horizontal ? horizontalVotes[slot.index] : verticalVotes[slot.index];
    }

    const Scorer::Votes& Scorer::votesOf(int row, int col, Heading heading) const
    {
      const EdgeIndex::Slot slot = EdgeIndex::slotOf(row, col, heading);
      return slot.horizontal ? horizontalVotes[slot.index] : verticalVotes[slot.index];
    }

    void Scorer::countVisit(Summary& summary, std::vector<std::string>* describe) const
    {
      ++summary.visits;
      summary.visitSides += headingCount;
      for(int h = 0; h < headingCount; ++h)
      {
        const Votes& vote = votesOf(visitRow, visitCol, static_cast<Heading>(h));
        if(vote.wall + vote.open == 0)
          continue;
        const EdgeState decided = vote.wall > vote.open ? EdgeState::wall : vote.open > vote.wall ? EdgeState::open : vote.latest;
        ++summary.visitSidesKnown;
        const EdgeState real = maze.get(visitRow, visitCol, static_cast<Heading>(h));
        summary.visitSidesCorrect += decided == real;
        if(describe && decided != real)
        {
          constexpr const char* names[] = {"north", "east", "south", "west"};
          char line[160];
          std::snprintf(line, sizeof(line), "cell %d,%d %s: votes wall %d open %d, truth %s", visitRow, visitCol, names[h], vote.wall, vote.open,
                        real == EdgeState::wall ? "wall" : "open");
          describe->push_back(line);
        }
      }
    }

    void Scorer::finishVisit()
    {
      if(!visiting)
        return;
      countVisit(totals, &wrongSides);
      visiting = false;
    }

    Summary Scorer::summary() const
    {
      Summary result = totals;
      if(visiting)
        countVisit(result);
      for(int bin = 0; bin < distanceBins; ++bin)
      {
        result.standingErrors[static_cast<std::size_t>(bin)] = statsOf(standingSamples[static_cast<std::size_t>(bin)]);
        result.walkingErrors[static_cast<std::size_t>(bin)] = statsOf(walkingSamples[static_cast<std::size_t>(bin)]);
      }
      return result;
    }

    std::string Scorer::text() const
    {
      const Summary s = summary();
      char line[256];
      std::string out;
      const auto append = [&](const char* format, auto... args)
      {
        std::snprintf(line, sizeof(line), format, args...);
        out += line;
      };
      append("frames %d (standing %d, walking %d, pose mismatch %d, outside %d), edges known %d unknown %d accuracy %.4f (wall %d/%d, open %d/%d)\n",
             s.frames, s.standingFrames, s.walkingFrames, s.poseMismatchFrames, s.skippedFrames, s.edgesKnown(), s.edgesUnknown, s.edgeAccuracy(),
             s.wallCorrect, s.wallCorrect + s.wallWrong, s.openCorrect, s.openCorrect + s.openWrong);
      append("(a) visits %d sides %d known %d correct %d accuracy %.4f coverage %.4f\n", s.visits, s.visitSides, s.visitSidesKnown,
             s.visitSidesCorrect, s.visitAccuracy(), s.visitCoverage());
      append("(b) ahead sides %d known %d correct %d accuracy %.4f coverage %.4f\n", s.aheadSides, s.aheadSidesKnown,
             s.aheadSidesCorrect, s.aheadAccuracy(), s.aheadCoverage());
      append("(c) cells beyond range %d\n", s.cellsBeyondRange);
      for(const std::string& side : wrongSides)
        out += "(a) wrong " + side + "\n";
      for(int bin = 0; bin < distanceBins; ++bin)
      {
        const ErrorStats& st = s.standingErrors[static_cast<std::size_t>(bin)];
        const ErrorStats& wa = s.walkingErrors[static_cast<std::size_t>(bin)];
        append("(d) %.1f m standing n %d mean %.1f std %.1f p95 %.1f | walking n %d mean %.1f std %.1f p95 %.1f\n", 0.5f + 0.5f * static_cast<float>(bin),
               st.count, st.mean, st.standardDeviation, st.percentile95, wa.count, wa.mean, wa.standardDeviation, wa.percentile95);
      }
      return out;
    }
  }
}
