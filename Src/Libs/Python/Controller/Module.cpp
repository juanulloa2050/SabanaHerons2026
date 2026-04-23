/**
 * @file Module.cpp
 *
 * This file implements Python bindings for simulating B-Human instances.
 *
 * @author Arne Hasselbring
 */

#include "Controller.h"
#include "RLSharedState.h"
#include "Framework/Settings.h"
#include "Math/Eigen.h"
#include "Math/Pose2f.h"
#include "Platform/File.h"
#include "Representations/Infrastructure/GroundTruthWorldState.h"
#include "Streaming/Enum.h"
#include "Streaming/FunctionList.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

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

  // RL API — direct in-memory bridge to RLSkillProvider (no subprocess, no JSON)
  m.def("rl_set_action",
    [](int player_number, const std::string& skill,
       float target_x, float target_y, float target_theta)
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      std::lock_guard<std::mutex> lock(io.mutex);
      io.skill       = skill;
      io.targetX     = target_x;
      io.targetY     = target_y;
      io.targetTheta = target_theta;
      io.obsReady    = false;
    },
    "Set RL action in shared state before controller.update().",
    py::arg("player_number"), py::arg("skill"),
    py::arg("target_x") = 0.f, py::arg("target_y") = 0.f,
    py::arg("target_theta") = 0.f);

  m.def("rl_get_obs",
    [](int player_number, unsigned int timeout_ms) -> py::dict
    {
      RLPlayerIO& io = RLSharedState::instance().player(player_number);
      // Wait for RLSkillProvider to post the observation (with timeout)
      io.obsSignal.wait(timeout_ms);
      std::lock_guard<std::mutex> lock(io.mutex);
      py::dict d;
      d["ball_x"]      = io.ballX;
      d["ball_y"]      = io.ballY;
      d["robot_x"]     = io.robotX;
      d["robot_y"]     = io.robotY;
      d["robot_theta"] = io.robotTheta;
      d["frame"]       = io.frame;
      d["obs_ready"]   = io.obsReady;
      return d;
    },
    "Read BHuman observation from shared state after controller.update().",
    py::arg("player_number"), py::arg("timeout_ms") = 500u);

  auto teamColorType = py::enum_<Settings::TeamColor>(m, "TeamColor");
  FOREACH_ENUM(Settings::TeamColor, teamColor)
    teamColorType.value(TypeRegistry::getEnumName(teamColor), teamColor);
}
