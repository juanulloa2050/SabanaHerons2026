# Sabana Herons — Code Release 2026

This is the code that team **Sabana Herons** (Universidad de La Sabana,
Colombia) used in the **RoboCup 2026 Humanoid Soccer League (HSL)** with NAO
V6 robots, 4v4 on the HSL Small field.

**It is based on the [B-Human Code Release 2023](https://github.com/bhuman/BHumanCodeRelease/releases/tag/coderelease2023).**
B-Human's framework, motion (walking, kicks, get-up), perception, localization,
team communication, behavior architecture, and tools (SimRobot, deploy dialog)
are the foundation of everything here. On top of it we:

1. **migrated the code from the SPL to the HSL 2026 rules** and the HSL
   GameController protocol v20;
2. **adapted the team strategies** to 4v4 and 3v3 on the HSL Small field,
   including set plays, kick-offs, dropped balls, and penalty kicks;
3. **replaced the ball detection** with a YOLO network running on the robot
   (ONNX), since the HSL ball is not the SPL ball;
4. **rewrote the whistle recognizer** with three spectral profiles;
5. **integrated reinforcement learning (RL)**: policies trained in SimRobot
   decide which skill each player executes, and run embedded on the NAO;
6. **added operation tools** for deployment, remote control, camera
   streaming, and dataset collection.

The RL training itself was developed in a separate Python repository (see
[Reinforcement learning](#5-reinforcement-learning)).

If you have used the B-Human 2023 release before, everything you know still
applies: build, deploy, SimRobot, and the module/representation architecture
are unchanged. The [B-Human 2023 documentation](https://wiki.b-human.de/coderelease2023/)
is the reference for all of that.

---

## What we changed

### 1. HSL 2026 rules and GameController

The SPL GameController protocol (v18) was replaced by the HSL protocol (v20).
`GameStateProvider` and `GameState` handle Stop Play, direct and indirect free
kicks, throw-ins, goal kicks, corner kicks, penalty kicks, dropped balls,
cautions, and sent-off players. The behavior enforces the HSL restart rules:
no direct goal from kick-off (two-touch rule, or a touch outside the center
circle when two or fewer robots are active), the 45 s free-kick limit,
opponent goal kicks with the whole penalty area cleared, and HSL penalty-kick
placement. The simulated GameController in SimRobot exposes the HSL commands.

Robots returning from a penalty are seeded by `SelfLocator` on the touchline
at the height of their own penalty mark, using a side hint remembered before
the penalty to break the left/right symmetry. For restarts, a new
`RestartBallSearchProvider` predicts where the ball will be placed, so robots
search there instead of waiting to see it.

Details: [docs/HSL2026_Migration.md](docs/HSL2026_Migration.md).

### 2. Strategies for 4v4 and 3v3

New strategies, tactics, setup poses, and locations for HSL-sized fields:
`4v4_Full` and `3v3_Full` (field dimensions of the HSL 2026 Small field). In
the final 4v4 configuration the goalkeeper dives, and the kick-off is a short
lateral pass to receivers placed away from the center circle. The classical
4v4 behavior (no RL) is the `4v4_Full` scenario.

### 3. Ball detection

The HSL uses a FIFA-style ball instead of the SPL ball, so we added:

- `YoloBallDetector`: a YOLOv8n detector (320×320) exported to ONNX and run
  asynchronously on the NAO with ONNX Runtime, with a lightweight image-space
  Kalman tracker that compensates for camera motion; its output feeds
  B-Human's `BallPerceptFilter` and `BallStateEstimator`;
- Trionda-specific candidate generation and classification modules, kept as
  an alternative to the network;
- `CameraStreamer` and `RawBallPatch` to collect images and ball patches for
  training.

The scenario used in matches selects `YoloBallDetector` with
`Config/NeuralNets/BallDetector/yolo_ball_320.onnx`.
Details: [docs/BallDetection.md](docs/BallDetection.md).

### 4. Whistle recognition

`WhistleRecognizer` was rewritten around three independent profiles
(standard referee whistle, hand-squeeze whistle, and mouth whistle), each
using Goertzel band energy, SNR, spectral flatness, and temporal gates
(onset confirmation, hang-over, gap filling). It still provides B-Human's
`Whistle` representation, so the rest of the code is unchanged.

### 5. Reinforcement learning

The policies decide **which skill** to execute (stand, walk, shoot, pass,
dribble, block, mark, observe) and four continuous parameters; B-Human still
executes every skill and every motion. On the robot,
`StrategyBehaviorControl` computes the classical decision first and replaces
it with the policy's decision only during active play, after a legal-action
gate. If a model is missing or inference fails, the classical behavior is
used.

| Policy | Observation | Used in |
| --- | --- | --- |
| Striker (`striker_base`) | 26 values | `4v4_StrikerBase` |
| Defender/support (`baseline_attack`) | 26 values | `4v4_BaselineAttack` |
| Team striker v4.2 (`mixed_attack`) | 47 values | `4v4_MixedAttack` |
| Merged field-player brain v5 (`complete`) | 47 values + role | `4v4_Complete` (our match configuration) |
| Goalkeeper | 64 values, 12 skills | any scenario, enabled with `--rl-gk on` |

In the `complete` mode one network serves all field players: a C++ role
coordinator assigns striker, open support, and off-ball support roles, and the
role is part of the observation.

The training pipeline (in the separate RL repository) is: rule-based teachers
derived from B-Human's behavior → behavioral cloning → PPO/MAPPO in SimRobot
with a curriculum and a KL anchor to the cloned policy → scenario-based
evaluation → ONNX export. SimRobot is driven from Python through the `pybh`
bindings and `RLSharedState`, so the policies are trained against the real
B-Human behavior, motion, and perception stack.

| Document | Content |
| --- | --- |
| [docs/RL/Integration.md](docs/RL/Integration.md) | How the policies run on the robot, models, deploy options, fallback |
| [docs/RL/Environment.md](docs/RL/Environment.md) | Training environment: observation, action, skill gate, reward, curriculum |
| [docs/RL/Training.md](docs/RL/Training.md) | BC + PPO/MAPPO training, evaluation, ONNX export, model lineage |

Training with the RL repository requires the multi-agent observation fields
of `RLSharedState`, which are in the `rl-simrobot3d` branch of this
repository; `master` contains everything needed to run the trained policies.

### 6. Tools and robustness

- `Make/Common/deploy` and the deploy dialog select the RL mode per player,
  the goalkeeper policy, and goalkeeper diving (see `Config/teams.cfg`).
- [Util/KeyboardControl](Util/KeyboardControl/README.md): control robots from
  a phone or browser, live camera view, recording, and ball dataset
  collection.
- Stability fixes: the debug thread no longer busy-waits without a client,
  more connection retries at startup, safe handling of `off`/`ignore` joint
  sentinels, non-negative scan-line starts, and a fallback in `Zweikampf` when
  a field-line intersection fails.

### Earlier work (2024–2025)

This code base also contains the team's earlier work: robot identities,
calibrations and network profiles, the 5v5 migration and the
attack/defensive/"Tortuga" strategies of 2024, spoken feedback, a Windows
deploy dialog, the referee-gesture pipeline (disabled by default in
`HandleRefereeSignal.h`), and optimizations in `ImageTransform.h` (polar
transform for the ball CNN) and `UKFPose2D`.

---

## Getting started

Building, deploying, and running SimRobot work exactly as in B-Human 2023;
follow the [B-Human 2023 documentation](https://wiki.b-human.de/coderelease2023/).
After cloning, initialize the submodules:

```bash
git submodule update --init
```

### Scenarios and locations

| Scenario | Description |
| --- | --- |
| `4v4_Complete` | Our match configuration: merged RL brain for field players, YOLO ball detector |
| `4v4_Full` | Classical B-Human-style 4v4 behavior, no field-player RL |
| `4v4_NoRLBlock` | Classical 4v4 with its own blocking formation; the deploy script disables all RL policies |
| `4v4_StrikerBase`, `4v4_BaselineAttack`, `4v4_MixedAttack` | Earlier RL policies, for comparison |
| `3v3_Full`, `3v3_RL_TeamV42`, `3v3_RL_MergedV5` | 3v3 variants |
| `4v4_RL2D`, `4v4_RL3D` | Scenarios driven by the Python RL environment |

Use the location `4v4_Full` (or `3v3_Full`) on the HSL Small field. Other
scenarios and locations come from B-Human or from our earlier SPL work.

### Deploying

```bash
cd Make/Common
./deploy Release \
  -r 1 <goalkeeper-ip> -r 2 <player-ip> -r 3 <player-ip> -r 4 <player-ip> \
  -t <team-number> -s 4v4_Complete -l 4v4_Full \
  --rl-complete 2,3,4 --rl-gk on --goalkeeper-dive on
```

Use `--rl-disable --rl-gk off` with `-s 4v4_Full` for the classical behavior.
All RL options are listed in [docs/RL/Integration.md](docs/RL/Integration.md#deployment).
The robots, network profiles, and team number in `Config/` are ours; replace
them with your own.

### Simulation

The usual B-Human scenes work (`Config/Scenes/*.ros2`). `RLvsBH3v3_3D.ros2`
plays RL-controlled field players against a B-Human team; the `RL*` scenes
are used by the Python training environment.

---

## Repository layout

Only the folders that differ from B-Human 2023 are listed.

| Path | Content |
| --- | --- |
| `Src/Libs/RL` | Observation encoders, skill gates, action decoders, ONNX policy wrappers |
| `Src/Libs/Python/Controller` | `pybh` extensions and `RLSharedState` (Python ↔ SimRobot) |
| `Src/Modules/BehaviorControl/StrategyBehaviorControl` | Embedded RL, role coordinator, 4v4/3v3 behavior, restart search |
| `Src/Modules/Perception/BallPerceptors` | `YoloBallDetector`, Trionda modules, external detector bridge |
| `Src/Modules/Modeling/WhistleRecognizer` | Three-profile whistle recognizer |
| `Src/Modules/Infrastructure/RLSkillProvider` | Applies actions from Python during training |
| `Src/Modules/Infrastructure/CameraStreamer` | Camera streaming for data collection |
| `Config/NeuralNets/RLPolicy` | RL policies (ONNX) and manifests |
| `Config/NeuralNets/BallDetector` | YOLO ball detector models |
| `Config/Scenarios`, `Config/Locations` | HSL 4v4/3v3 and RL scenarios |
| `Util/KeyboardControl` | Web/keyboard control and data collection tools |
| `docs/` | Documentation of our contributions |

## Known limitations

- The goalkeeper policy's save rate comes from a simulated benchmark; dive
  reach was not calibrated on the real robot.
- `--rl-complete` selects the merged model for all eligible field players; it
  does not restrict it to the listed player numbers.
