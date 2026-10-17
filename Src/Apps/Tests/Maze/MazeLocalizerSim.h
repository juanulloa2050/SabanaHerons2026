/**
 * @file Apps/Tests/Maze/MazeLocalizerSim.h
 *
 * Test-only simulation of a walking robot for the acceptance runs of MazeLocalizer and MazeMapper. A true pose
 * moves through a real maze along the actions of the planner; the sim derives from it what the sensors would
 * report (noisy odometry, a gyro that drifts, AprilTag detections and the wall look-ahead of the oracle) and
 * feeds the pure cores exactly like the modules do (same helpers: PlayingTracker, mapActions, and the shared
 * `StartAnchoredMap` with the start deduction, which also moves the localizer into the frame of the working start hypothesis).
 *
 * With `startGiven = false` the start is deduced: the localizer and the map work in the frame of the working hypothesis,
 * which is not the frame of the true maze until the start is known. The error statistics compare the estimate with the truth
 * moved into that frame.
 *
 * What it does not model: a real walk (the true pose follows the cell centres exactly, no sway, no stopping
 * errors), image processing (the oracle sees every tag in reach), the head (it looks along the robot x-axis
 * unless a scan is configured) and timing jitter of the percept threads (one percept per frame).
 */

#pragma once

#include "Math/RotationMatrix.h"
#include "Tools/Maze/MazeFile.h"
#include "Tools/Maze/MazeHeading.h"
#include "Tools/Maze/MazeLocalization.h"
#include "Tools/Maze/MazeMapFusion.h"
#include "Tools/Maze/MazeOracle.h"
#include "Tools/Maze/MazePlanner.h"
#include "Tools/Maze/MazeSession.h"
#include "Tools/Maze/MazeStartAnchoredMap.h"
#include "Tools/Maze/MazeTagLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace Maze
{
  namespace Sim
  {
    /** Errors of the simulated sensors; the defaults are the oracle cfg values and the 4.5 roadmap noise budget. */
    struct Noise
    {
      float odometryTranslationBias = 0.05f;   /**< Systematic relative error of the walked distance (5 %). */
      float odometryTranslationNoise = 0.05f;  /**< Std. dev. of the relative error of each frame's translation. */
      float odometryRotationPerMetre = Angle::fromDegrees(2.f); /**< Std. dev. of the odometry rotation error per metre (ignored with the gyro). */
      float gyroDriftDegPerMinute = 3.f;       /**< Linear gyro drift. */
      float gyroNoiseDeg = 0.05f;              /**< Std. dev. of the gyro yaw per frame. */
      float gyroReference = 0.4f;              /**< The gyro does not know the maze: its yaw is the true one plus this offset (rad). */
      float tagPositionStdDev = 5.f;           /**< mm per axis, like oracledMazePerceptsProvider.cfg. */
      float tagRotationStdDev = Angle::fromDegrees(1.f);
      float wallDistanceStdDev = 10.f;         /**< mm. */
      float dropoutRate = 0.f;                 /**< Probability to miss one observation; 1 gives empty percepts. */
    };

    struct Config
    {
      MazeDescription maze;
      MazeTagLayout layout;
      Noise noise;
      LocalizationParameters localization;
      FusionParameters fusion;
      Oracle::Camera camera = {Vector2f(50.f, 0.f), 0.f, 1.07f}; /**< Upper camera of the NAO, looking along the robot x-axis. */
      float headScanAmplitude = 0.f;           /**< Head yaw scan amplitude in rad (sine with `headScanPeriodMs`); 0 looks straight ahead. */
      float headScanPeriodMs = 4000.f;
      int maxRange = 2;                        /**< Cells ahead that the oracle sees. */
      float maxTagViewAngle = Angle::fromDegrees(70.f);
      bool useGyro = true;
      bool startGiven = true;                  /**< The start of the maze is given (`start.cell`); otherwise the mapper deduces it. */
      bool noTagsInStartCell = false;          /**< No tag detection while the robot is in the start cell, e.g. no tag within sight of the start. */
      bool observeWhileMoving = true;          /**< False: the map learns only while the robot stands. */
      LocalizationQuality worstAcceptedQuality = LocalizationQuality::poor;
      float maxOffsetFromCentre = 150.f;       /**< Like the mapper: the look-ahead is used only this close (mm, per axis) to the centre of the estimated cell. */
      unsigned frameMs = 33;                   /**< Period of the odometry and gyro (about 30 Hz). */
      unsigned perceptPeriodMs = 100;          /**< Period of the tag and wall percepts (the vision runs at about 10 Hz). */
      unsigned seed = 1;
      MazeCosts costs;                         /**< Durations of the primitive motions. */
    };

    constexpr float centreZoneMm = 125.f; /**< Half edge of the middle of a cell for `Stats::wrongCellFrames`. */

    /** Statistics over a run. */
    struct Stats
    {
      unsigned frames = 0;                     /**< Odometry/gyro frames. */
      unsigned perceptFrames = 0;              /**< Frames with a tag and wall percept; the counters below count these. */
      unsigned framesWithTag = 0;              /**< Percept frames in which the oracle showed at least one tag. */
      unsigned fixes = 0;                      /**< Accepted tag fixes. */
      unsigned rejections = 0;
      unsigned framesWithFix = 0;              /**< Percept frames with at least one accepted fix. */
      unsigned skippedLookAheads = 0;          /**< Look-aheads not used because the estimate was near a cell boundary. */
      unsigned wrongCellFrames = 0;            /**< Frames in the middle of a cell (within `centreZoneMm` of the centre) in which the estimated cell differs from the true one. */
      float maxPositionError = 0.f;
      float sumPositionError = 0.f;
      float maxHeadingError = 0.f;
      std::array<unsigned, headingCount> framesByHeading = {};   /**< By true heading. */
      std::array<unsigned, headingCount> fixFramesByHeading = {};
      std::array<unsigned, headingCount> tagFramesByHeading = {};

      float meanPositionError() const { return frames ? sumPositionError / static_cast<float>(frames) : 0.f; }
    };

    /** A walking robot, its sensors and the localization/mapping cores. */
    class WalkingRobot
    {
    public:
      explicit WalkingRobot(const Config& config) : config(config), rng(config.seed), anchored(anchoredParameters(config)), localization(config.localization)
      {
        truth = Pose2f(angleOf(config.maze.startHeading), Oracle::cellCentreOf(cellRow(config.maze.startCell), cellCol(config.maze.startCell)));
        anchored.layout = config.layout;
        localizerFrame = anchored.anchor();
        if(config.startGiven && !giveStart(config.maze.startCell, config.maze.startHeading))
          throw std::invalid_argument("the start of the maze is not a corner facing into the maze; use placeAt to start elsewhere");
      }

      /** Gives the start like `start.cell` of handleMaze.cfg, before the first frame; it may differ from the true start of the maze. */
      bool giveStart(int cell, Heading heading)
      {
        if(!anchored.setGiven(cell, heading))
          return false;
        localizerFrame = anchored.anchor();
        return true;
      }

      /** Ground truth. */
      Pose2f truth;
      unsigned time = 1000;
      bool playing = true;
      FallDownState::State body = FallDownState::upright; /**< The body state: `pickedUp` is a pick-up; fallen, falling, ... are falls, which do not end the attempt. */
      Config config;
      Stats stats;
      std::mt19937 rng;
      StartAnchoredMap anchored;   /**< The map and the start deduction, as MazeMapper runs them. */
      MazeLocalization localization;
      unsigned resets = 0;
      std::function<void(const WalkingRobot&)> onFrame; /**< Called after every frame, e.g. to watch the quality. */

      /** The map in the maze frame of the working start hypothesis (the true one once the start is known). */
      [[nodiscard]] const MazeMapFusion& fusion() const { return anchored.map(); }

      /** The start of the true maze. */
      [[nodiscard]] Hypothesis trueStart() const { return {config.maze.startCell, config.maze.startHeading}; }

      /** The start whose maze frame the localizer works in. */
      [[nodiscard]] const Hypothesis& localizerStart() const { return localizerFrame; }

      /** The ground truth in the maze frame of the localizer; the same as `truth` when that is the true frame. */
      [[nodiscard]] Pose2f truthInLocalizerFrame() const
      {
        if(localizerFrame == trueStart())
          return truth;
        return StartDeduction::startToMaze(localizerFrame) * StartDeduction::startToMaze(trueStart()).inverse() * truth;
      }

      /** True cell and heading of the robot. */
      [[nodiscard]] bool trueCell(int& row, int& col) const { return Oracle::cellContaining(truth.translation, row, col); }
      [[nodiscard]] Heading trueHeading() const { return headingOf(truth.rotation); }

      [[nodiscard]] float positionError() const { return (localization.pose.translation - truthInLocalizerFrame().translation).norm(); }
      [[nodiscard]] float headingError() const { return std::abs(Angle::normalize(localization.pose.rotation - truthInLocalizerFrame().rotation)); }

      /** The estimated cell equals the true one (in the frame of the localizer). */
      [[nodiscard]] bool cellCorrect() const
      {
        int tr, tc, er, ec;
        return Oracle::cellContaining(truthInLocalizerFrame().translation, tr, tc) && localization.cell(er, ec) && tr == er && tc == ec;
      }

      /** One frame; the true pose first moves by `motion` (robot frame). */
      void tick(const Pose2f& motion = Pose2f())
      {
        const Pose2f before = truth;
        truth = truth * motion;
        time += config.frameMs;
        frame(before.inverse() * truth);
        if(onFrame)
          onFrame(*this);
      }

      /** Stands still for `frames` frames. */
      void stand(int frames)
      {
        for(int i = 0; i < frames; ++i)
          tick();
      }

      /** Executes one primitive motion of the planner at the speed of the planner costs. */
      void perform(Action action)
      {
        int durationMs = config.costs.forwardMs;
        Pose2f total;
        switch(action)
        {
          case Action::forward: total = Pose2f(0.f, Vector2f(cellSizeMm, 0.f)); break;
          case Action::turnLeft: total = Pose2f(pi / 2.f, Vector2f::Zero()); durationMs = config.costs.turn90Ms; break;
          case Action::turnRight: total = Pose2f(-pi / 2.f, Vector2f::Zero()); durationMs = config.costs.turn90Ms; break;
          default: total = Pose2f(pi, Vector2f::Zero()); durationMs = config.costs.turn180Ms; break;
        }
        const int ticks = std::max(1, static_cast<int>(std::lround(static_cast<float>(durationMs) / static_cast<float>(config.frameMs))));
        const Pose2f step(total.rotation / static_cast<float>(ticks), total.translation / static_cast<float>(ticks));
        for(int i = 0; i < ticks; ++i)
          tick(step);
        // Snap the truth onto the exact cell grid: the sum of the small steps carries a float error.
        if(action != Action::forward)
          truth.rotation = Angle::normalize(angleOf(headingOf(truth.rotation)));
      }

      /** The true pose is carried to a place and orientation, e.g. a pick-up. */
      void carryTo(int cell, Heading heading)
      {
        truth = Pose2f(angleOf(heading), Oracle::cellCentreOf(cellRow(cell), cellCol(cell)));
      }

      /**
       * Starts the run somewhere else than at the start of the maze: the truth and the localizer are put at `cell` facing `heading`,
       * as if the robot had been positioned there. Only for a given start, whose frame is the frame of the true maze; call it
       * before the first frame, which would otherwise put the pose at the start.
       */
      void placeAt(int cell, Heading heading)
      {
        carryTo(cell, heading);
        localization.reset(cell, heading, Angle::normalize(truth.rotation + config.noise.gyroReference + gyroDrift()), time);
        (void) localizerSession.update(isActive(playing, body)); // The first frame is over.
      }

      /** Moves the robot's state to not playing (pick-up, Standby) or back to playing; the frame runs with the new state. */
      void setPlaying(bool value)
      {
        playing = value;
        tick();
      }

      /** Lifts the robot (a pick-up while the game keeps playing) or puts it down; the frame runs with the new state. */
      void setPickedUp(bool value)
      {
        body = value ? FallDownState::pickedUp : FallDownState::upright;
        tick();
      }

      /** Sets the body state (for example `fallen`); the frame runs with the new state. */
      void setBody(FallDownState::State value)
      {
        body = value;
        tick();
      }

      /** The accumulated gyro drift in rad at the current time. */
      [[nodiscard]] float gyroDrift() const
      {
        return Angle::fromDegrees(config.noise.gyroDriftDegPerMinute) * static_cast<float>(time) / 60000.f;
      }

      /** The localizer's cell heading. */
      [[nodiscard]] Heading estimatedHeading() const { return localization.heading(); }

    private:
      float normal(float mean, float stdDev)
      {
        return stdDev > 0.f ? std::normal_distribution<float>(mean, stdDev)(rng) : mean;
      }

      bool dropped()
      {
        return config.noise.dropoutRate > 0.f && std::bernoulli_distribution(std::min(1.f, config.noise.dropoutRate))(rng);
      }

      Oracle::Camera cameraNow() const
      {
        Oracle::Camera camera = config.camera;
        if(config.headScanAmplitude > 0.f)
          camera.yawInRobot = config.headScanAmplitude * std::sin(2.f * pi * static_cast<float>(time) / config.headScanPeriodMs);
        return camera;
      }

      void frame(const Pose2f& trueDelta)
      {
        const Noise& noise = config.noise;
        ++stats.frames;

        // Sensors: the odometry delta of the frame and the gyro yaw.
        Pose2f delta = trueDelta;
        delta.translation *= (1.f + noise.odometryTranslationBias) * std::max(0.f, normal(1.f, noise.odometryTranslationNoise));
        delta.rotation += normal(0.f, noise.odometryRotationPerMetre) * trueDelta.translation.norm() / 1000.f;
        const float gyroYaw = Angle::normalize(truth.rotation + noise.gyroReference + gyroDrift() + normal(0.f, Angle::fromDegrees(noise.gyroNoiseDeg)));

        // MazeLocalizer::step: follow the working start hypothesis of the last frame, then reset if the robot became active.
        if(!(localizerFrame == anchored.anchor()))
        {
          localization.transform(StartDeduction::startToMaze(anchored.anchor()) * StartDeduction::startToMaze(localizerFrame).inverse());
          localizerFrame = anchored.anchor();
        }
        if(localizerSession.update(isActive(playing, body)).resetPose())
        {
          localization.reset(localizerFrame.cell, localizerFrame.heading, gyroYaw, time);
          ++resets;
        }
        localization.predict(delta, gyroYaw, config.useGyro);

        Percept percept;
        if(time - lastPerceptTime >= config.perceptPeriodMs)
        {
          lastPerceptTime = time;
          percept.present = true;
          sense(percept);
        }
        mapperUpdate(percept, trueDelta);

        // Errors.
        int trueRow = 0, trueCol = 0;
        const bool trueInside = trueCell(trueRow, trueCol);
        const float positionError = this->positionError(), headingError = this->headingError();
        stats.maxPositionError = std::max(stats.maxPositionError, positionError);
        stats.sumPositionError += positionError;
        stats.maxHeadingError = std::max(stats.maxHeadingError, headingError);
        // Near a border between two cells a small error legitimately puts the estimate in the neighbour: judge the middle of a cell.
        const Vector2f fromCentre = truth.translation - Oracle::cellCentreOf(trueRow, trueCol);
        if(trueInside && std::max(std::abs(fromCentre.x()), std::abs(fromCentre.y())) <= centreZoneMm && !cellCorrect())
          ++stats.wrongCellFrames;
      }

      struct AprilTagDetection
      {
        int id;
        Pose3f tagInRobot;
      };

      /** What the vision delivers in a percept frame. */
      struct Percept
      {
        bool present = false;
        bool inMaze = false;
        Oracle::View view;
        std::vector<AprilTagDetection> detections;
      };

      /** The vision frame: oracle view and tag detections, then the tag fixes of MazeLocalizer. */
      void sense(Percept& percept)
      {
        const Noise& noise = config.noise;
        ++stats.perceptFrames;
        const Oracle::Camera camera = cameraNow();
        Oracle::View& view = percept.view;
        std::vector<AprilTagDetection>& detections = percept.detections;
        percept.inMaze = noise.dropoutRate < 1.f && Oracle::computeView(config.maze.graph, truth, camera, config.maxRange, view);
        int startRow = 0, startCol = 0;
        const bool hidden = config.noTagsInStartCell && trueCell(startRow, startCol) && cellId(startRow, startCol) == config.maze.startCell;
        if(percept.inMaze && !hidden)
          for(const Oracle::VisibleTag& tag : Oracle::visibleTags(config.maze.graph, config.layout, view, camera, config.maxRange, config.maxTagViewAngle))
            if(!dropped())
              detections.push_back({tag.cell, noisyTag(tag.tagInRobot)});

        const Heading truthHeading = trueHeading();
        const std::size_t h = static_cast<std::size_t>(truthHeading);
        ++stats.framesByHeading[h];
        if(!detections.empty())
        {
          ++stats.framesWithTag;
          ++stats.tagFramesByHeading[h];
        }
        bool fixedThisFrame = false;
        const bool startKnown = anchored.status().known; // Under a start that is not known a tag would pull the pose to the wrong place.
        for(const AprilTagDetection& detection : detections)
        {
          if(!startKnown)
            break;
          if(localization.fixWithTag(detection.id, detection.tagInRobot, time, config.layout).accepted)
          {
            ++stats.fixes;
            fixedThisFrame = true;
          }
          else
            ++stats.rejections;
        }
        if(fixedThisFrame)
        {
          ++stats.framesWithFix;
          ++stats.fixFramesByHeading[h];
        }

      }

      /** MazeMapper::step, every frame; the observations only when a percept arrived. */
      void mapperUpdate(const Percept& percept, const Pose2f& trueDelta)
      {
        const bool active = isActive(playing, body);
        if(mapperSession.update(active).newAttempt)
          anchored.newAttempt();
        const MapActions actions = mapActions(active, qualityAccepted(localization.quality(time), config.worstAcceptedQuality));
        if(actions.markVisited)
          anchored.markVisited(localization.pose);
        if(actions.observe)
        {
          anchored.observePosition(localization.pose);
          if(percept.present && percept.inMaze && (config.observeWhileMoving || trueDelta.translation.norm() + std::abs(trueDelta.rotation) < 1e-3f))
          {
            if(anchored.lookAheadUsable(localization.pose))
              observeWalls(percept.view);
            else
              ++stats.skippedLookAheads;
            for(const AprilTagDetection& detection : percept.detections)
              anchored.observeTag(localization.pose, detection.id, detection.tagInRobot, time);
          }
        }
        anchored.endFrame();
      }

      Pose3f noisyTag(const Pose3f& exact)
      {
        RotationMatrix rotation(exact.rotation);
        rotation.rotateX(normal(0.f, config.noise.tagRotationStdDev));
        rotation.rotateY(normal(0.f, config.noise.tagRotationStdDev));
        rotation.rotateZ(normal(0.f, config.noise.tagRotationStdDev));
        Pose3f result;
        result.rotation = rotation;
        result.translation = Vector3f(normal(exact.translation.x(), config.noise.tagPositionStdDev), normal(exact.translation.y(), config.noise.tagPositionStdDev),
                                      normal(exact.translation.z(), config.noise.tagPositionStdDev));
        return result;
      }

      /** The wall percept of the oracle as MazeMapper turns it into cell observations. */
      void observeWalls(const Oracle::View& view)
      {
        // lookAheadTurns of OracledMazePerceptsProvider, and the gaze the mapper reconstructs from the localized heading.
        int turns = static_cast<int>(std::lround(Angle::normalize(Oracle::angleOf(view.gaze) - view.robotRotation) / (pi / 2.f)));
        if(turns < -1)
          turns += 4;
        const Heading gaze = gazeHeading(localization.heading(), turns);
        std::vector<CellObservation> cells;
        for(const Oracle::LookAheadCell& cell : view.cells)
        {
          if(dropped())
            continue;
          CellObservation observation;
          observation.ahead = cell.ahead;
          observation.left = cell.left;
          observation.right = cell.right;
          observation.front = cell.front;
          observation.distance = std::max(0.f, normal((Oracle::cellCentreOf(cell.row, cell.col) - view.robot).norm(), config.noise.wallDistanceStdDev));
          cells.push_back(observation);
        }
        anchored.observeCellsAhead(localization.pose, gaze, cells, time);
      }

      static AnchoredMapParameters anchoredParameters(const Config& config)
      {
        AnchoredMapParameters parameters;
        parameters.fusion = config.fusion;
        parameters.maxFaceAngleError = config.localization.maxFaceAngleError;
        parameters.maxOffsetFromCentre = config.maxOffsetFromCentre;
        return parameters;
      }

      Hypothesis localizerFrame; /**< The start whose maze frame the localizer works in. */
      unsigned lastPerceptTime = 0;
      PlayingTracker localizerSession;
      PlayingTracker mapperSession;
    };

    /** The actions of the optimal route from the start of a fully known maze. */
    inline std::vector<Action> optimalRoute(const MazeDescription& maze, const MazeCosts& costs = MazeCosts())
    {
      MazePlanner planner(costs);
      planner.computeCostToGo(maze.graph, UnknownPolicy::asWall);
      MazeState start;
      start.row = cellRow(maze.startCell);
      start.col = cellCol(maze.startCell);
      start.heading = maze.startHeading;
      return planner.routeFromTable(start).actions;
    }

    /**
     * The actions of an exploration run in a fully known maze with the sensing model of MazeExplorationSim (the
     * four edges of a cell are learned on arrival, unknown edges count as open, replanned before every action),
     * until a goal cell is entered. Empty if the goal cannot be reached.
     */
    inline std::vector<Action> explorationRoute(const MazeDescription& real, const MazeCosts& costs = MazeCosts())
    {
      std::vector<Action> actions;
      MazePlanner planner(costs);
      MazeGraph known;
      MazeState state;
      state.row = cellRow(real.startCell);
      state.col = cellCol(real.startCell);
      state.heading = real.startHeading;
      const auto learn = [&]()
      {
        for(int h = 0; h < headingCount; ++h)
          known.set(state.row, state.col, static_cast<Heading>(h), real.graph.get(state.row, state.col, static_cast<Heading>(h)));
      };
      learn();
      for(int steps = 0; !isGoal(state.row, state.col); ++steps)
      {
        Action action = Action::forward;
        planner.computeCostToGo(known, UnknownPolicy::asOpen);
        MazeState next;
        MazeCost cost;
        if(steps >= 1000 || !planner.nextAction(state, action) || !planner.apply(known, UnknownPolicy::asOpen, state, action, next, cost))
          return {};
        actions.push_back(action);
        state = next;
        if(action == Action::forward)
          learn();
      }
      return actions;
    }

    /** Result of `explore`. */
    struct Exploration
    {
      bool reachedGoal = false;
      std::string error;
      std::vector<int> visitedCells;   /**< True cells in the order they were entered, the start first. */
      int cellMismatches = 0;          /**< Checkpoints at which the estimated cell was wrong. */
      int steps = 0;
      int firstWatchStep = -1;        /**< Index in `visitedCells` of the first entry of `watchCell`, -1 if never. */
    };

    /**
     * Explores with the planner (unknown edges as open, replanned before every action) until a goal cell is
     * entered. The planner state comes from the localizer estimate. `learnOnArrival` reproduces the model of
     * MazeExplorationSim (the four edges of a cell are learned when it is entered, nothing else); otherwise
     * the map is the one `MazeMapFusion` collects from the oracle.
     * @param standFrames Frames spent standing at every cell centre before the next decision.
     */
    inline Exploration explore(WalkingRobot& run, bool learnOnArrival, int standFrames = 0, int watchCell = -1)
    {
      constexpr int stepGuard = 400;
      Exploration result;
      MazePlanner planner(run.config.costs);
      MazeGraph arrival;
      const MazeGraph& real = run.config.maze.graph;
      const auto learn = [&](int row, int col)
      {
        for(int h = 0; h < headingCount; ++h)
          arrival.set(row, col, static_cast<Heading>(h), real.get(row, col, static_cast<Heading>(h)));
      };
      int row, col;
      (void) run.trueCell(row, col);
      learn(row, col);
      result.visitedCells.push_back(cellId(row, col));
      bool moving = false;

      for(; result.steps < stepGuard; ++result.steps)
      {
        run.stand(standFrames);
        (void) run.trueCell(row, col);
        if(isGoal(row, col))
        {
          result.reachedGoal = true;
          return result;
        }
        int er, ec;
        (void) run.localization.cell(er, ec);
        MazeState state;
        state.row = er;
        state.col = ec;
        state.heading = run.localization.heading();
        state.moving = moving;

        planner.computeCostToGo(learnOnArrival ? arrival : run.fusion().graph, UnknownPolicy::asOpen);
        Action action = Action::forward;
        if(!planner.nextAction(state, action))
        {
          result.error = "goal unreachable";
          return result;
        }
        if(action == Action::forward && real.get(row, col, run.trueHeading()) != EdgeState::open)
        {
          result.error = "forward through a wall";
          return result;
        }
        run.perform(action);
        moving = action == Action::forward;
        if(action == Action::forward)
        {
          (void) run.trueCell(row, col);
          learn(row, col);
          result.visitedCells.push_back(cellId(row, col));
          if(cellId(row, col) == watchCell && result.firstWatchStep < 0)
            result.firstWatchStep = static_cast<int>(result.visitedCells.size()) - 1;
          if(!run.cellCorrect())
            ++result.cellMismatches;
        }
      }
      result.error = "step guard exceeded";
      return result;
    }

    /** A layout parsed from the text of a cfg file (the caller made sure that the enums are registered). */
    inline std::string layoutText(const std::string& mode, const std::string& faces, const std::string& walls = "")
    {
      return "mode = " + mode + "; faces = [" + faces + "]; walls = [" + walls + "]; centreHeight = 300; size = 150;";
    }
  }
}
