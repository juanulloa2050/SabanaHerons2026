/**
 * @file Module.cpp
 *
 * This file implements Python bindings for simulating B-Human instances.
 *
 * @author Arne Hasselbring
 */

#include "Controller.h"
#include "RLSharedState.h"
#include "SimRobotHost.h"
#include "Framework/Settings.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include "Platform/File.h"
#include "Platform/Time.h"
#include "Representations/Infrastructure/GroundTruthWorldState.h"
#include "Streaming/Enum.h"
#include "Streaming/FunctionList.h"

#ifdef slots
#undef slots
#endif

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace py = pybind11;

namespace
{
struct BoundWorldPlayer
{
  int number;
  float x;
  float y;
  float theta;
  bool upright;
};

std::vector<BoundWorldPlayer> parseWorldPlayers(const py::iterable& players, const char* label, bool allowNonUpright)
{
  std::vector<BoundWorldPlayer> parsed;
  std::unordered_set<int> seenNumbers;
  for(py::handle item : players)
  {
    const py::sequence values = py::reinterpret_borrow<py::sequence>(item);
    if(values.size() != 5)
      throw std::runtime_error(std::string(label) + " entries must have 5 values: (number, x, y, theta, upright)");

    BoundWorldPlayer player{
      values[0].cast<int>(),
      values[1].cast<float>(),
      values[2].cast<float>(),
      values[3].cast<float>(),
      values[4].cast<bool>(),
    };

    if(player.number < 1 || player.number > RLSharedStateBridge::maxWorldPlayersPerTeam)
      throw std::runtime_error(std::string(label) + " player number must be within 1..20");
    if(!seenNumbers.insert(player.number).second)
      throw std::runtime_error(std::string(label) + " contains duplicate player number " + std::to_string(player.number));
    if(!allowNonUpright && !player.upright)
      throw std::runtime_error(std::string(label) + " does not support upright=false in this backend");

    parsed.push_back(player);
  }

  if(parsed.size() > RLSharedStateBridge::maxWorldPlayersPerTeam)
    throw std::runtime_error(std::string(label) + " supports at most 20 players");
  return parsed;
}

void appendWorldPlayers(std::vector<GroundTruthWorldState::GroundTruthPlayer>& destination, const std::vector<BoundWorldPlayer>& players)
{
  for(const BoundWorldPlayer& player : players)
  {
    GroundTruthWorldState::GroundTruthPlayer converted;
    converted.number = player.number;
    converted.pose = Pose2f(player.theta, player.x, player.y);
    converted.upright = player.upright;
    destination.push_back(converted);
  }
}

template<typename T, std::size_t N>
void copyWorldPlayers(std::array<T, N>& destination, int& count, const std::vector<BoundWorldPlayer>& players)
{
  count = static_cast<int>(players.size());
  for(std::size_t i = 0; i < destination.size(); ++i)
    destination[i] = T{};
  for(std::size_t i = 0; i < players.size(); ++i)
  {
    destination[i].number = players[i].number;
    destination[i].x = players[i].x;
    destination[i].y = players[i].y;
    destination[i].theta = players[i].theta;
    destination[i].upright = players[i].upright;
  }
}
}

// Required to extract version info.
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

PYBIND11_MODULE(controller, m)
{
#ifdef VERSION_INFO
  m.attr("__version__") = TOSTRING(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif

  m.attr("__name__") = "pybh.controller";
  m.doc() = R"bhdoc(
B-Human Controller

Python bindings for simulating B-Human instances.
)bhdoc";

  FunctionList::execute();

  m.def("get_bh_dir", &File::getBHDir, R"bhdoc(Returns the path to the B-Human root directory.

Returns:
    The path to the B-Human root directory.
)bhdoc");

  m.def("simrobot_load_scene",
    [](const std::string& scene_path)
    {
      return SimRobotHost::instance().loadScene(scene_path);
    },
    "Load a SimRobot scene inside the Python process.",
    py::arg("scene_path"));

  m.def("simrobot_unload_scene",
    []()
    {
      SimRobotHost::instance().unloadScene();
    },
    "Unload the currently active SimRobot scene.");

  m.def("simrobot_step",
    [](unsigned int steps)
    {
      return SimRobotHost::instance().step(steps);
    },
    "Advance the active SimRobot scene by a number of simulation steps.",
    py::arg("steps") = 1u);

  m.def("simrobot_is_loaded",
    []()
    {
      return SimRobotHost::instance().isLoaded();
    },
    "Return whether a SimRobot scene is currently active.");

  m.def("simrobot_last_error",
    []()
    {
      return SimRobotHost::instance().lastError();
    },
    "Return the last SimRobot load/runtime error.");

  m.def("simrobot_get_robot_pose",
    [](const std::string& robot_name) -> py::object
    {
      float x = 0.f;
      float y = 0.f;
      float theta = 0.f;
      if(!SimRobotHost::instance().getRobotPose(robot_name, x, y, theta))
        return py::none();

      py::dict d;
      d["x"] = x;
      d["y"] = y;
      d["theta"] = theta;
      return std::move(d);
    },
    "Read the pose of a robot body from the active SimRobot 2D scene.",
    py::arg("robot_name"));

  m.def("simrobot_set_robot_pose",
    [](const std::string& robot_name, float x, float y, float theta)
    {
      return SimRobotHost::instance().setRobotPose(robot_name, x, y, theta);
    },
    "Teleport a robot body in the active SimRobot 2D scene.",
    py::arg("robot_name"), py::arg("x"), py::arg("y"), py::arg("theta"));

  m.def("simrobot_get_ball_state",
    []() -> py::object
    {
      float x = 0.f;
      float y = 0.f;
      float vx = 0.f;
      float vy = 0.f;
      if(!SimRobotHost::instance().getBallState(x, y, vx, vy))
        return py::none();

      py::dict d;
      d["x"] = x;
      d["y"] = y;
      d["vx"] = vx;
      d["vy"] = vy;
      return std::move(d);
    },
    "Read the ball pose and velocity from the active SimRobot 2D scene.");

  m.def("simrobot_set_ball_state",
    [](float x, float y, float vx, float vy)
    {
      return SimRobotHost::instance().setBallState(x, y, vx, vy);
    },
    "Teleport the ball and set its planar velocity in the active SimRobot 2D scene.",
    py::arg("x"), py::arg("y"), py::arg("vx") = 0.f, py::arg("vy") = 0.f);

  py::class_<Controller>(m, "Controller", "An interface to control a set of robots.")
    .def(pybind11::init<>())
    .def("add_player", &Controller::addPlayer, R"bhdoc(Adds a player to the controller.

Args:
    name: The name of the player (only influences thread names but not the config search path).
    team_number: The team number of the player.
    field_player_color: The jersey color of the field players in this player's team.
    goalkeeper_color: The jersey color of the goalkeeper in this player's team.
    player_number: The (jersey) number of the player.
    location: The location in the configuration file search path.
    scenario: The scenario in the configuration file search path.
)bhdoc", py::arg("name"), py::arg("team_number"), py::arg("field_player_color"), py::arg("goalkeeper_color"), py::arg("player_number"), py::arg("location"), py::arg("scenario"))
    .def("start", &Controller::start, "Starts all robot threads.")
    .def("stop", &Controller::stop, "Stops all robot threads.")
    .def("update", &Controller::update, "Triggers a frame in all robots.");

  // Inject world state into a specific robot (0-based index) before update().
  m.def("set_world_state",
    [](Controller& ctrl, int robot_index,
       float ball_x, float ball_y,
       float robot_x, float robot_y, float robot_theta)
    {
      GroundTruthWorldState ws;
      GroundTruthWorldState::GroundTruthBall ball;
      ball.position = Vector3f(ball_x, ball_y, 50.f);
      ball.velocity = Vector3f::Zero();
      ws.balls.push_back(ball);
      ws.ownPose = Pose2f(robot_theta, robot_x, robot_y);
      ctrl.setWorldState(robot_index, ws);
    },
    "Inject world state into a robot before controller.update().",
    py::arg("controller"), py::arg("robot_index"),
    py::arg("ball_x"), py::arg("ball_y"),
    py::arg("robot_x"), py::arg("robot_y"),
    py::arg("robot_theta") = 0.f);

  m.def("set_world_state_extended",
    [](Controller& ctrl, int robot_index,
       float ball_x, float ball_y,
       float robot_x, float robot_y, float robot_theta,
       py::iterable teammates, py::iterable opponents)
    {
      GroundTruthWorldState ws;
      GroundTruthWorldState::GroundTruthBall ball;
      ball.position = Vector3f(ball_x, ball_y, 50.f);
      ball.velocity = Vector3f::Zero();
      ws.balls.push_back(ball);
      ws.ownPose = Pose2f(robot_theta, robot_x, robot_y);
      appendWorldPlayers(ws.ownTeamPlayers, parseWorldPlayers(teammates, "teammates", true));
      appendWorldPlayers(ws.opponentTeamPlayers, parseWorldPlayers(opponents, "opponents", true));
      ctrl.setWorldState(robot_index, ws);
    },
    "Inject world state, teammates, and opponents into a robot before controller.update().",
    py::arg("controller"), py::arg("robot_index"),
    py::arg("ball_x"), py::arg("ball_y"),
    py::arg("robot_x"), py::arg("robot_y"),
    py::arg("robot_theta") = 0.f,
    py::arg("teammates") = py::tuple(),
    py::arg("opponents") = py::tuple());

  m.def("rl_reset_world",
    [](int player_number,
       float ball_x, float ball_y,
       float robot_x, float robot_y, float robot_theta) -> unsigned int
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      while(io.tryWaitObs()) {}
      io.lock();
      io.resetBallX = ball_x;
      io.resetBallY = ball_y;
      io.resetRobotX = robot_x;
      io.resetRobotY = robot_y;
      io.resetRobotTheta = robot_theta;
      io.resetTeammateCount = 0;
      io.resetOpponentCount = 0;
      const unsigned int requestId = ++io.worldRequestSerial;
      io.resetRequestId = requestId;
      io.resetPending = true;
      io.obsReady = false;
      io.unlock();
      return requestId;
    },
    "Request that a visible SimRobot instance reset the world state for a player.",
    py::arg("player_number"),
    py::arg("ball_x"), py::arg("ball_y"),
    py::arg("robot_x"), py::arg("robot_y"),
    py::arg("robot_theta") = 0.f);

  m.def("rl_reset_world_extended",
    [](int player_number,
       float ball_x, float ball_y,
       float robot_x, float robot_y, float robot_theta,
       py::iterable teammates, py::iterable opponents) -> unsigned int
    {
      const std::vector<BoundWorldPlayer> parsedTeammates = parseWorldPlayers(teammates, "teammates", false);
      const std::vector<BoundWorldPlayer> parsedOpponents = parseWorldPlayers(opponents, "opponents", false);

      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      while(io.tryWaitObs()) {}
      io.lock();
      io.resetBallX = ball_x;
      io.resetBallY = ball_y;
      io.resetRobotX = robot_x;
      io.resetRobotY = robot_y;
      io.resetRobotTheta = robot_theta;
      copyWorldPlayers(io.resetTeammates, io.resetTeammateCount, parsedTeammates);
      copyWorldPlayers(io.resetOpponents, io.resetOpponentCount, parsedOpponents);
      const unsigned int requestId = ++io.worldRequestSerial;
      io.resetRequestId = requestId;
      io.resetPending = true;
      io.selfLocatorResetPending = true;
      io.obsReady = false;
      io.unlock();
      return requestId;
    },
    "Request that a visible SimRobot instance reset the world state for a player, teammates, and opponents.",
    py::arg("player_number"),
    py::arg("ball_x"), py::arg("ball_y"),
    py::arg("robot_x"), py::arg("robot_y"),
    py::arg("robot_theta") = 0.f,
    py::arg("teammates") = py::tuple(),
    py::arg("opponents") = py::tuple());

  m.def("rl_set_dynamic_world",
    [](int player_number,
       py::object ball,
       py::object own_pose,
       py::object teammates,
       py::object opponents) -> unsigned int
    {
      const bool hasBall = !ball.is_none();
      const bool hasOwnPose = !own_pose.is_none();
      const bool hasTeammates = !teammates.is_none();
      const bool hasOpponents = !opponents.is_none();
      std::vector<BoundWorldPlayer> parsedTeammates;
      std::vector<BoundWorldPlayer> parsedOpponents;
      float ballX = 0.f;
      float ballY = 0.f;
      float robotX = 0.f;
      float robotY = 0.f;
      float robotTheta = 0.f;

      if(hasBall)
      {
        const py::sequence values = py::reinterpret_borrow<py::sequence>(ball);
        if(values.size() != 2)
          throw std::runtime_error("ball must have 2 values: (x, y)");
        ballX = values[0].cast<float>();
        ballY = values[1].cast<float>();
      }

      if(hasOwnPose)
      {
        const py::sequence values = py::reinterpret_borrow<py::sequence>(own_pose);
        if(values.size() != 3)
          throw std::runtime_error("own_pose must have 3 values: (x, y, theta)");
        robotX = values[0].cast<float>();
        robotY = values[1].cast<float>();
        robotTheta = values[2].cast<float>();
      }

      if(hasTeammates)
        parsedTeammates = parseWorldPlayers(py::reinterpret_borrow<py::iterable>(teammates), "teammates", false);
      if(hasOpponents)
        parsedOpponents = parseWorldPlayers(py::reinterpret_borrow<py::iterable>(opponents), "opponents", false);

      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      while(io.tryWaitObs()) {}
      io.lock();
      io.dynamicHasBall = hasBall;
      io.dynamicBallX = ballX;
      io.dynamicBallY = ballY;
      io.dynamicHasRobotPose = hasOwnPose;
      io.dynamicRobotX = robotX;
      io.dynamicRobotY = robotY;
      io.dynamicRobotTheta = robotTheta;
      io.dynamicApplyTeammates = hasTeammates;
      io.dynamicApplyOpponents = hasOpponents;
      if(hasTeammates)
        copyWorldPlayers(io.dynamicTeammates, io.dynamicTeammateCount, parsedTeammates);
      else
        copyWorldPlayers(io.dynamicTeammates, io.dynamicTeammateCount, std::vector<BoundWorldPlayer>{});
      if(hasOpponents)
        copyWorldPlayers(io.dynamicOpponents, io.dynamicOpponentCount, parsedOpponents);
      else
        copyWorldPlayers(io.dynamicOpponents, io.dynamicOpponentCount, std::vector<BoundWorldPlayer>{});
      const unsigned int requestId = ++io.worldRequestSerial;
      io.dynamicRequestId = requestId;
      io.dynamicPending = true;
      io.obsReady = false;
      io.unlock();
      return requestId;
    },
    "Apply an intra-episode visible SimRobot world update for a player.",
    py::arg("player_number"),
    py::arg("ball") = py::none(),
    py::arg("own_pose") = py::none(),
    py::arg("teammates") = py::none(),
    py::arg("opponents") = py::none());

  m.def("rl_get_world_status",
    [](int player_number) -> py::dict
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      io.lock();
      py::dict d;
      d["result_request_id"] = io.worldResultRequestId;
      d["result_ok"] = io.worldResultOk;
      d["result_error"] = std::string(io.worldResultError);
      d["reset_request_id"] = io.resetRequestId;
      d["reset_applied_id"] = io.resetAppliedId;
      d["reset_pending"] = io.resetPending;
      d["dynamic_request_id"] = io.dynamicRequestId;
      d["dynamic_applied_id"] = io.dynamicAppliedId;
      d["dynamic_pending"] = io.dynamicPending;
      io.unlock();
      return d;
    },
    "Return the latest visible SimRobot world-request status for a player.",
    py::arg("player_number"));

  m.def("rl_mark_embedded_reset_context",
    [](int player_number, float ball_x, float ball_y)
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      io.lock();
      io.resetBallX = ball_x;
      io.resetBallY = ball_y;
      io.resetAppliedFrame = Time::getCurrentSystemTime();
      io.unlock();
    },
    "Seed reset-ball context for embedded SimRobot resets.",
    py::arg("player_number"), py::arg("ball_x"), py::arg("ball_y"));

  m.def("rl_mark_embedded_dynamic_context",
    [](int player_number, py::object ball)
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      io.lock();
      if(ball.is_none())
      {
        io.dynamicHasBall = false;
      }
      else
      {
        const py::sequence values = py::reinterpret_borrow<py::sequence>(ball);
        if(values.size() != 2)
          throw std::runtime_error("ball must have 2 values: (x, y)");
        io.dynamicHasBall = true;
        io.dynamicBallX = values[0].cast<float>();
        io.dynamicBallY = values[1].cast<float>();
      }
      io.dynamicAppliedFrame = Time::getCurrentSystemTime();
      io.unlock();
    },
    "Seed dynamic-ball context for embedded SimRobot updates.",
    py::arg("player_number"), py::arg("ball") = py::none());

  // RL API — direct in-memory bridge to RLSkillProvider (no subprocess, no JSON)
  m.def("rl_set_action",
    [](int player_number, const std::string& skill,
       float target_x, float target_y, float target_theta,
       int pass_target)
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      io.lock();
      io.setSkill(skill);
      io.targetX     = target_x;
      io.targetY     = target_y;
      io.targetTheta = target_theta;
      io.passTarget  = pass_target;
      io.obsReady    = false;
      io.unlock();
    },
    "Set RL action in shared state before controller.update().",
    py::arg("player_number"), py::arg("skill"),
    py::arg("target_x") = 0.f, py::arg("target_y") = 0.f,
    py::arg("target_theta") = 0.f, py::arg("pass_target") = -1);

  // Visible-SimRobot variant: posts the action WITHOUT clearing obsReady.
  // In visible mode the semaphore in rl_get_obs guarantees frame freshness,
  // so clearing obsReady here only creates a race: if frame N+1 already
  // posted (obsReady=true, sem+1) before this call, the clear causes
  // rl_get_obs to discard the posted semaphore count and wait an extra frame,
  // doubling step latency from 33ms to 66ms.
  m.def("rl_set_visible_action",
    [](int player_number, const std::string& skill,
       float target_x, float target_y, float target_theta,
       int pass_target)
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      io.lock();
      io.setSkill(skill);
      io.targetX     = target_x;
      io.targetY     = target_y;
      io.targetTheta = target_theta;
      io.passTarget  = pass_target;
      // obsReady is intentionally NOT cleared here — see comment above.
      io.unlock();
    },
    "Set RL action for a visible SimRobot slot (does not clear obsReady).",
    py::arg("player_number"), py::arg("skill"),
    py::arg("target_x") = 0.f, py::arg("target_y") = 0.f,
    py::arg("target_theta") = 0.f, py::arg("pass_target") = -1);

  m.def("rl_get_obs",
    [](int player_number, unsigned int timeout_ms) -> py::dict
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      // Release the GIL during the blocking semaphore wait so that Python
      // threads for different player slots can overlap their waits concurrently.
      // pthread_mutex_lock/unlock inside the loop are pure C calls — safe without GIL.
      {
        py::gil_scoped_release release;
        if(timeout_ms == 0)
        {
          do
          {
            io.waitForObs(0);
            io.lock();
            const bool ready = io.obsReady;
            io.unlock();
            if(ready)
              break;
          }
          while(true);
        }
        else
        {
          const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
          while(std::chrono::steady_clock::now() < deadline)
          {
            const auto now = std::chrono::steady_clock::now();
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            if(!io.waitForObs(static_cast<unsigned int>(remaining > 0 ? remaining : 1)))
              break;
            io.lock();
            const bool ready = io.obsReady;
            io.unlock();
            if(ready)
              break;
          }
        }
      } // GIL re-acquired here before py::dict construction
      io.lock();
      py::dict d;
      d["ball_x"]      = io.ballX;
      d["ball_y"]      = io.ballY;
      d["robot_x"]     = io.robotX;
      d["robot_y"]     = io.robotY;
      d["robot_theta"] = io.robotTheta;
      d["estimated_ball_x"] = io.estimatedBallX;
      d["estimated_ball_y"] = io.estimatedBallY;
      d["estimated_robot_x"] = io.estimatedRobotX;
      d["estimated_robot_y"] = io.estimatedRobotY;
      d["estimated_robot_theta"] = io.estimatedRobotTheta;
      d["simrobot_robot_x"] = io.simRobotX;
      d["simrobot_robot_y"] = io.simRobotY;
      d["simrobot_robot_theta"] = io.simRobotTheta;
      d["frame"]       = io.frame;
      d["reset_applied_frame"] = io.resetAppliedFrame;
      d["dynamic_applied_frame"] = io.dynamicAppliedFrame;
      d["obs_ready"]   = io.obsReady;
      d["obs_export_mode"] = io.obsExportMode;
      d["corrected_exported_robot_pose"] = io.correctedExportedRobotPose;
      d["corrected_exported_ball"] = io.correctedExportedBall;
      d["ball_export_source"] = io.ballExportSource;
      d["ball_export_fresh"] = io.ballExportFresh;
      d["natural_time_since_ball_seen"] = io.naturalTimeSinceBallSeen;
      d["natural_ball_seen_percentage"] = io.naturalBallSeenPercentage;
      d["natural_ball_consistent_with_game_state"] = io.naturalBallConsistentWithGameState;
      d["ball_rel_x"] = io.ballRelX;
      d["ball_rel_y"] = io.ballRelY;
      d["ball_end_rel_x"] = io.ballEndRelX;
      d["ball_end_rel_y"] = io.ballEndRelY;
      d["ball_vel_x"] = io.ballVelX;
      d["ball_vel_y"] = io.ballVelY;
      d["time_since_ball_seen"] = io.timeSinceBallSeen;
      d["time_since_ball_disappeared"] = io.timeSinceBallDisappeared;
      d["ball_seen_percentage"] = io.ballSeenPercentage;
      d["ball_consistent_with_game_state"] = io.ballConsistentWithGameState;
      d["can_score_now"] = io.canScoreNow;
      d["shot_quality_no_obstacles"] = io.shotQualityNoObstacles;
      d["shot_opening_with_obstacles"] = io.shotOpeningWithObstacles;
      d["pass_options_count"] = io.passOptionsCount;
      d["nearest_teammate_dist"] = io.nearestTeammateDist;
      d["nearest_opponent_dist"] = io.nearestOpponentDist;
      d["nearest_uncertain_obstacle_dist"] = io.nearestUncertainObstacleDist;
      d["nearest_teammate_front_dist"] = io.nearestTeammateFrontDist;
      d["nearest_opponent_front_dist"] = io.nearestOpponentFrontDist;
      d["nearest_uncertain_front_dist"] = io.nearestUncertainFrontDist;
      py::list teammateX;
      py::list teammateY;
      py::list teammateTheta;
      py::list teammateAgeMs;
      py::list teammateActivity;
      for(int i = 0; i < io.teammateCount && i < static_cast<int>(io.teammateX.size()); ++i)
      {
        teammateX.append(io.teammateX[static_cast<std::size_t>(i)]);
        teammateY.append(io.teammateY[static_cast<std::size_t>(i)]);
        teammateTheta.append(io.teammateTheta[static_cast<std::size_t>(i)]);
        teammateAgeMs.append(io.teammateAgeMs[static_cast<std::size_t>(i)]);
        teammateActivity.append(io.teammateActivity[static_cast<std::size_t>(i)]);
      }
      d["teammate_x_mm"] = teammateX;
      d["teammate_y_mm"] = teammateY;
      d["teammate_theta"] = teammateTheta;
      d["teammate_age_ms"] = teammateAgeMs;
      d["teammate_activity"] = teammateActivity;
      d["global_team_ball_x_mm"] = io.globalTeamBallX;
      d["global_team_ball_y_mm"] = io.globalTeamBallY;
      d["global_team_ball_age_ms"] = io.globalTeamBallAgeMs;
      d["global_team_ball_valid"] = io.globalTeamBallValid;
      d["goal_opponent_x_mm"] = io.goalOpponentX;
      d["goal_opponent_y_mm"] = io.goalOpponentY;
      d["obstacle_count"] = io.debugObstacleCount;
      d["obstacle_teammate_count"] = io.debugObstacleTeammateCount;
      d["obstacle_opponent_count"] = io.debugObstacleOpponentCount;
      d["obstacle_uncertain_count"] = io.debugObstacleUncertainCount;
      d["obstacle_front_count"] = io.debugObstacleFrontCount;
      d["nearest_obstacle_dist"] = io.debugNearestObstacleDist;
      d["nearest_obstacle_angle"] = io.debugNearestObstacleAngle;
      d["nearest_front_obstacle_dist"] = io.debugNearestFrontObstacleDist;
      d["nearest_opponent_angle"] = io.debugNearestOpponentAngle;
      d["obstacle_field_percept_count"] = io.debugObstacleFieldPerceptCount;
      d["obstacle_field_percept_opponent_count"] = io.debugObstacleFieldPerceptOpponentCount;
      d["obstacle_field_percept_teammate_count"] = io.debugObstacleFieldPerceptTeammateCount;
      d["obstacle_field_percept_unknown_count"] = io.debugObstacleFieldPerceptUnknownCount;
      d["obstacle_accepted_percept_count"] = io.debugObstacleAcceptedPerceptCount;
      d["obstacle_hypothesis_count"] = io.debugObstacleHypothesisCount;
      d["obstacle_published_count"] = io.debugObstaclePublishedCount;
      d["obstacle_max_seen_count"] = io.debugObstacleMaxSeenCount;
      d["obstacle_min_percepts"] = io.debugObstacleMinPercepts;
      d["obstacle_camera"] = io.debugObstacleCamera;
      d["obstacle_frame"] = io.debugObstacleFrame;
      d["obstacle_arm_contact"] = io.debugObstacleArmContact;
      d["obstacle_foot_contact"] = io.debugObstacleFootContact;
      d["requested_skill"] = io.getSkill();
      d["requested_pass_target"] = io.passTarget;
      d["motion_request"] = io.debugMotionRequest;
      d["provider_motion_request"] = io.debugProviderMotionRequest;
      d["provider_call_count"] = io.debugProviderCallCount;
      d["provider_target_x"] = io.debugProviderTargetX;
      d["provider_target_y"] = io.debugProviderTargetY;
      d["provider_target_theta"] = io.debugProviderTargetTheta;
      d["skill_behavior_skill_request"] = io.debugSkillBehaviorSkillRequest;
      d["skill_behavior_motion_request"] = io.debugSkillBehaviorMotionRequest;
      d["skill_behavior_call_count"] = io.debugSkillBehaviorCallCount;
      d["skill_behavior_walk_target_x"] = io.debugSkillBehaviorWalkTargetX;
      d["skill_behavior_walk_target_y"] = io.debugSkillBehaviorWalkTargetY;
      d["skill_behavior_walk_target_theta"] = io.debugSkillBehaviorWalkTargetTheta;
      d["zweikampf_active"] = io.debugZweikampfActive;
      d["zweikampf_call_count"] = io.debugZweikampfCallCount;
      d["motion_obstacle_avoidance_x"] = io.debugMotionObstacleAvoidanceX;
      d["motion_obstacle_avoidance_y"] = io.debugMotionObstacleAvoidanceY;
      d["motion_obstacle_path_count"] = io.debugMotionObstaclePathCount;
      d["motion_obstacle_first_x"] = io.debugMotionObstacleFirstX;
      d["motion_obstacle_first_y"] = io.debugMotionObstacleFirstY;
      d["motion_obstacle_first_radius"] = io.debugMotionObstacleFirstRadius;
      d["motion_obstacle_first_clockwise"] = io.debugMotionObstacleFirstClockwise;
      d["motion_engine_input_request"] = io.debugMotionEngineInputRequest;
      d["motion_engine_effective_request"] = io.debugMotionEngineEffectiveRequest;
      d["motion_engine_phase"] = io.debugMotionEnginePhase;
      d["motion_engine_force_sit_down"] = io.debugMotionEngineForceSitDown;
      d["motion_engine_gyro_offset_finished"] = io.debugMotionEngineGyroOffsetFinished;
      d["motion_engine_gyro_bad"] = io.debugMotionEngineGyroBad;
      d["motion_engine_inertial_angle_x"] = io.debugMotionEngineInertialAngleX;
      d["motion_engine_inertial_angle_y"] = io.debugMotionEngineInertialAngleY;
      d["motion_engine_fall_state"] = io.debugMotionEngineFallState;
      d["walk_to_pose_call_count"] = io.debugWalkToPoseCallCount;
      d["walk_to_pose_target_x"] = io.debugWalkToPoseTargetX;
      d["walk_to_pose_target_y"] = io.debugWalkToPoseTargetY;
      d["walk_to_pose_target_theta"] = io.debugWalkToPoseTargetTheta;
      d["walk_to_pose_step_x"] = io.debugWalkToPoseStepX;
      d["walk_to_pose_step_y"] = io.debugWalkToPoseStepY;
      d["walk_to_pose_step_theta"] = io.debugWalkToPoseStepTheta;
      d["walk_to_pose_avoidance_x"] = io.debugWalkToPoseAvoidanceX;
      d["walk_to_pose_avoidance_y"] = io.debugWalkToPoseAvoidanceY;
      d["walk_to_pose_path_count"] = io.debugWalkToPosePathCount;
      d["walk_to_pose_first_obstacle_x"] = io.debugWalkToPoseFirstObstacleX;
      d["walk_to_pose_first_obstacle_y"] = io.debugWalkToPoseFirstObstacleY;
      d["walk_to_pose_first_obstacle_radius"] = io.debugWalkToPoseFirstObstacleRadius;
      d["walk_to_pose_first_obstacle_clockwise"] = io.debugWalkToPoseFirstObstacleClockwise;
      d["libwalk_target_angle"] = io.debugLibWalkTargetAngle;
      d["libwalk_angle_left"] = io.debugLibWalkAngleLeft;
      d["libwalk_angle_right"] = io.debugLibWalkAngleRight;
      d["libwalk_angle_offset_left"] = io.debugLibWalkAngleOffsetLeft;
      d["libwalk_angle_offset_right"] = io.debugLibWalkAngleOffsetRight;
      d["libwalk_selected_offset"] = io.debugLibWalkSelectedOffset;
      d["libwalk_obstacle_count"] = io.debugLibWalkObstacleCount;
      d["libwalk_active_obstacle_count"] = io.debugLibWalkActiveObstacleCount;
      d["libwalk_disable_obstacle_avoidance"] = io.debugLibWalkDisableObstacleAvoidance;
      d["motion_engine_ground_contact"] = io.debugMotionEngineGroundContact;
      d["motion_phase"] = io.debugExecutedPhase;
      d["motion_speed_x"] = io.debugMotionSpeedX;
      d["motion_speed_y"] = io.debugMotionSpeedY;
      d["motion_speed_rot"] = io.debugMotionSpeedRot;
      d["joint_l_hip_pitch"] = io.debugLHipPitch;
      d["joint_l_knee_pitch"] = io.debugLKneePitch;
      d["joint_r_hip_pitch"] = io.debugRHipPitch;
      d["joint_r_knee_pitch"] = io.debugRKneePitch;
      io.unlock();
      return d;
    },
    "Read BHuman observation from shared state after controller.update().",
    py::arg("player_number"), py::arg("timeout_ms") = 500u);

  auto teamColorType = py::enum_<Settings::TeamColor>(m, "TeamColor");
  FOREACH_ENUM(Settings::TeamColor, teamColor)
    teamColorType.value(TypeRegistry::getEnumName(teamColor), teamColor);
}
