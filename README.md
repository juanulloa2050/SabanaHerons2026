# Sabana-Herons Code Release

SabanaHerons2026 is based on the [B-Human 2023 code release](https://wiki.b-human.de/coderelease2023/). Building on that foundation, Sabana-Herons developed its own HSL adaptations, team strategies, ball detection, reinforcement-learning integration, and robot-operation tools.

This release brings together the team's previous SabanaHerons2024 developments and its 2026 contributions. The sections below describe those additions and how they work within the team's system.

## Improvements

1. **HSL 2026 migration**: The project now includes the current HSL GameController protocol and the main HSL restart behavior updates, including stop play, direct and indirect free kicks, throw-ins, goal kicks, corner kicks, penalty kicks, and updated kick-off restrictions.
2. **3v3 and 4v4 full-field strategies**: The team now has dedicated `3v3_Full` and `4v4_Full` scenarios and locations for HSL-style play, with updated field dimensions and set-play behavior.
3. **Trionda ball detection**: The fork includes classic/Trionda perception variants, an external detector bridge, and on-robot YOLO ONNX inference. The current `4v4_Complete` camera configuration selects `YoloBallDetector` with `yolo_ball_best1.onnx`, followed by ball-percept filtering and state estimation.
4. **New whistle implementation**: The baseline includes the current whistle recognizer and tuning used in the latest deployed match baseline.
5. **Web control and operational tooling**: The codebase includes the current web control, camera streaming, and recording workflow used for robot operation and data collection.
6. **Behavior and match fixes**: The baseline also includes the current kick-off, set-play, and field-behavior fixes that were merged into the deployed branch.

## Ball Detection Baseline

The current `4v4_Complete` configuration uses:

`Config/NeuralNets/BallDetector/yolo_ball_best1.onnx`

Check the selected scenario's `yoloBallDetector.cfg`, its module providers in `threads.cfg`, and robot-specific overrides for the effective detector. Earlier notes referring to `Trionda Final Model` or `yolo_ball.onnx` describe historical baselines; they do not select the model for a current deployment.

## Run this code

The project builds on B-Human's framework. Their [2023 documentation](https://wiki.b-human.de/coderelease2023/) provides the foundation for building and running it, together with the Sabana-Herons configurations and deployment options described here.

The build and deploy flow still follows the B-Human-style workflow. In practice, the team currently uses the configured scenarios and locations inside `Config/Scenarios` and `Config/Locations`, then deploys with `Make/Common/deploy`.

## RL and common SimRobot scenes

This repository contains the C++ SimRobot/pybh bridge and embedded policy runtime.
The sibling `RL` checkout also contains a persistent sidecar integration with a
different contract, described below.

The recent SimRobot work here was mainly about keeping common scenes from
freezing. The safe debugging order is:

1. compare against `master`
2. confirm the scene really leaves `standby` and reaches `playing`
3. only then inspect higher-level RL code

The core invariant that must stay intact is:

```text
Python -> RLSharedState -> SkillRequest -> SkillBehaviorControl ->
MotionRequest -> MotionEngine / WalkingEngine -> JointRequest -> SimRobot
```

Practical guardrails:

- do not assume manual `F5` for visible RL scenes
- do not break common scenario game-state flow when enabling RL
- do not fix freezes by bypassing the normal B-Human motion path

## Hybrid RL vs B-Human test

Selective RL override is available through:

- `PYBH_RL_OVERRIDE_TEAM`
- `PYBH_RL_ACTIVE_PLAYERS`

That override lives in `StrategyBehaviorControl`, so only the requested jersey
numbers are replaced by RL while the rest of the team keeps normal B-Human
behavior.

There is also a dedicated mixed scene for observing duels and `Zweikampf`:

```text
Config/Scenes/RLvsBH3v3_3D.ros2
Config/Scenes/RLvsBH3v3_3D.con
```

Layout:

- own team `24`
- `robot1`: B-Human goalkeeper
- `robot2` and `robot3`: RL-controlled field players
- opponent team: three B-Human players

The console enables `Drawings/Zweikampf` so the scene can be used together with
the Python runner from the `RL` repo to inspect when the duel skill gets
entered.

## What changed in the fork

The earlier SabanaHerons2024 work includes robot identities, networks and calibrations, 5v5 migration, attack/defense/Tortuga strategies, kickoff and dribble tuning, spoken feedback, and Windows deployment tools. `ImageTransform.h` caches source row pointers for bilinear patch sampling; `UKFPose2D.cpp` reorganizes landmark mean/covariance accumulation. This review does not claim measured performance gains for those changes.

HSL integration uses GameController protocol v20. `GameStateProvider` and `GameState` handle stopped play, dropped ball, distinct direct/indirect free kicks, throw-ins, cautions and permanent player removal. Legacy SPL names are source aliases, not v18 packet compatibility. The simulated GameController exposes HSL division/restart commands. Sent-off players no longer occupy setup-pose slots or count as active opponents.

Behavior adapts dropped-ball setup, opponent goal-kick exclusion over the full penalty area, restart selection, and direct-goal permissions. Kickoff restrictions survive the transition to `playing` and use completed-touch metadata shared by teammates. Teams with at most two active robots first send the ball outside the center circle. Penalty-taking logic prevents another kick after its completed touch. The final 4v4 kickoff uses a lateral pass with adjusted receiving positions.

`RestartBallSearchProvider` creates rule-based ball-placement candidates; remembered local/team ball data ranks them. `RestartBallSearchContext`, `SearchRestartBall`, and `BallSearchAreasProvider` integrate those candidates into team search. `TeammatesBallModelProvider` keeps a bounded prediction with age/contributor metadata. Search hypotheses and predictions are distinct from fresh visual sightings. During SET the robot searches with its head while maintaining body posture; ball tracking smooths and bounds head targets.

`SelfLocator` seeds penalty-return particles on the touchline at the own penalty-mark height. A confident pre-penalty side hint reduces wrong-side symmetry; without it both sides remain possible. That hint is remembered locally, not sent by the GameController. RL episode teleports reset localization explicitly rather than continuously forcing ground truth.

Perception includes the extended classic `BallPerceptor`, Trionda blob-candidate modules, the experimental external `YoloBallBridge`, and asynchronous on-robot `YoloBallDetector`. The current YOLO path retains capture-associated camera geometry, expires stale results, and feeds `BallPerceptFilter` plus the existing `BallStateEstimator`. `RawBallPatch` and `CameraStreamer` support dataset capture.

`WhistleRecognizer` adds three configurable spectral profiles and temporal gates, retaining the `Whistle` representation. Referee perception adds RGB patch preparation and geometric keypoint filtering; automatic entry into the legacy SPL referee-signal sequence remains disabled in `HandleRefereeSignal.h`.

Stability changes include Debug waiting without a connected client to avoid busy-spin, increased startup connection retries, nonnegative scanline starts, safe handling of `off/ignore` joint-angle sentinels, and a fallback when a `Zweikampf` field-line intersection fails.

### Representative commits

| Contribution | Commits |
| --- | --- |
| Inherited team/tactics and calculations | `8732007f`, `3206a6a3`, `f8e0d4ad`, `48343bf3`, `bb6d3edf`, `50b33ce8`, `7850f97c` |
| HSL rules and protocol integration | `fcbc54da`, `2d8d3a6f`, `c3d26822` |
| Restart search, localization and 4v4 behavior | `6d14a4ed`, `fe50d0a8`, `4938990b`, `3293a3d5`, `26217d8a`, `fe753f47`, `107f51cc` |
| Ball detection and models | `ad1fe9e3`, `600034fb`, `712b3a41`, `0de53e3f`, `5ef33ae7`, `ca6bbbfb` |
| Whistle and runtime stability | `cacef4cb`, `a0cd56ac`, `d4d75152`, `d4d30e43`, `23afa5ba`, `490cd276` |
| RL bridge and embedded policies | `4a9029f4`, `e6003730`, `2eb15c9f`, `6d990c98`, `9b8b919c`, `5e8684ce`, `ad6a8642`, `d2261cf4`, `eeae9775` |
| Deployment and operation | `941943dd`, `b721ff68`, `f139dcb2`, `8c22db44`, `12a1d78a`, `abe6ced9`, `698350fa` |

## Embedded reinforcement learning

`Src/Libs/RL` supplies encoders, model loaders, legal-action gates and decoders. `StrategyBehaviorControl` calculates the classical request first and replaces it after a valid enabled-policy decision. Field-player PPO requires active play and excludes the goalkeeper. Load/inference failures preserve the classical path; the stand watchdog can force walking or fall back with a cooldown according to configuration.

| Field mode | Included contract/model |
| --- | --- |
| `striker_base` | 26 observations; `ppo_striker_hsl2026.onnx` |
| `baseline_attack` | Defender/support policy; `ppo_defender_hsl2026_param_repair.onnx` is included |
| `mixed_attack` | 47 observations; `ppo_team_hsl2026_v4_2.onnx` |
| `complete` | 47 observations; `ppo_team_hsl2026_v5_merged.onnx`, with C++ role coordination and support anchors |

Field policies use eight skills and four parameters. The independent goalkeeper policy uses 64 inputs (38 active features plus padding), 12 skills, and its own encoder/gate/decoder. Keeper interception and dives become `SkillRequest`s; `Dive.cpp` enforces `keeperJumpingOn`, including for RL requests. The networks do not generate joint commands.

Models and available manifests reside in `Config/NeuralNets/RLPolicy`. Preserve feature/skill order, normalization and output format when replacing a model. Not every model has an equivalent JSON manifest.

`RLSharedState` synchronizes external commands, resets, observations and motion diagnostics per player. Extensions to pybh, `PythonConsole`, `SimRobotHost`, `LocalConsole` and `SimulatedRobot` manage stepping/world updates. Dedicated RL scenarios translate commands through `RLSkillProvider`. Opt-in simulator abstract-motion/root-assist helpers are experimental aids; their use does not establish physical NAO execution.

### How the sibling RL repository complements this code

The sibling RL project provides CFG parsing, a multiagent environment, MAPPO training, positioning/handoff/takeover rewards, a persistent B-Human subprocess bridge with JSON-line transport, execution traces, smoke tests, and demonstrated-trajectory generation.

Relevant files there include `bhuman_cfg_parser.py`, `config.py`, `environment.py`, `wrapper.py`, `reward.py`, `bhuman_sidecar.py`, `bridge/bridge_protocol.py`, `bridge/bhuman_bridge.cpp`, `runtime_paths.py`, `train.py`, and `trajectory_guidance_project`.

Its positioning/handoff action contract differs from the embedded PPO contract of eight skills and four parameters. Exact training/export provenance for every bundled ONNX was not established, and compatibility of that persistent bridge with this master was not compiled or tested. The RL code distinguishes `real`, `contract_validation`, and `python_stub` backends; stub/contractual results do not establish real B-Human execution.

## Deployment details

The deploy script and GUI add RL mode selection, an independent keeper-policy toggle and a dive switch. The reviewed `Config/teams.cfg` preset selects team 49, scenario `4v4_Complete`, location `4v4_Full`, `gk` for the keeper, `complete` for field players and diving enabled.

- The scenario enables field-player PPO but sets `enableEmbeddedGK = false`; the preset or `--rl-gk on` activates the keeper during deployment.
- `--rl-complete` receives a player list but uses it to select the merged model, without persisting that list as a runtime filter. A configured merged model has priority; an empty internal list permits all eligible field players.

Intermediate RL/deploy changes were reverted in `4099c50f`, `2804bea2`, `9e3fe7ae`, `f58f0d6c` and `b9664e99`. `ad6a8642` reapplied v4.2 and addressed a robot-deploy configuration failure; subsequent commits added merged v5 and the keeper. This README describes the final code rather than counting reverted attempts as separate features.

`Util/KeyboardControl` contains web/keyboard operation, camera streaming, recording and the NAO watcher. These tools support robot operation and data collection independently of match strategy. Build extensions are in `Make/CMake`, the root CMake entry point and `Make/Python` for pybh packaging.

## Validation status

The prior HSL checklist retained open checks for real GameController timings, stop/timeout behavior, dropped-ball and ball-free semantics, penalty extensions/shootout, competition-ball measurements, communications limits and field/robot smoke tests. Implemented support does not imply those validations are complete.

## License

See [License.txt](License.txt) for the licensing terms and upstream attribution.
