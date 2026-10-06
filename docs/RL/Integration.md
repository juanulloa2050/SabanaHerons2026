# Reinforcement Learning in SabanaHerons2026

This document describes how the reinforcement-learning (RL) policies trained
by Sabana Herons are integrated into the B-Human-based soccer stack and run on
the NAO. How the policies are trained is covered in
[Environment.md](Environment.md) and [Training.md](Training.md).

## Design principle

RL does **not** replace B-Human's locomotion, motion engines, or skills. The
policy decides **which high-level skill to request** (walk, shoot, pass,
dribble, block, ...) and the normal B-Human pipeline executes it:

```text
policy decision -> SkillRequest -> SkillBehaviorControl -> MotionRequest
                -> WalkingEngine / MotionEngine -> JointRequest -> NAO
```

The work is split across two repositories:

- **RL repository (Python, developed separately).** SimRobot-based training
  environments, heuristic teachers, behavioral cloning, PPO/MAPPO training,
  evaluation, and ONNX export.
- **This repository (C++).** The runtime that consumes those policies: the
  `pybh` bridge used during training, the embedded ONNX inference used in
  matches, the safety gates, the scenarios, and the deployment options. During
  a match the robot runs no Python and does no training; it runs local
  inference only.

## Two ways the policies are consumed

1. **External control (training and testing).** Python writes actions and
   reads observations through `RLSharedState`
   ([Src/Libs/Python/Controller](../../Src/Libs/Python/Controller)). The
   `4v4_RL2D` and `4v4_RL3D` scenarios use `RLSkillProvider`
   ([Src/Modules/Infrastructure/RLSkillProvider](../../Src/Modules/Infrastructure/RLSkillProvider))
   to inject those actions into B-Human's normal behavior flow.
2. **Embedded inference (matches and real robots).** `StrategyBehaviorControl`
   loads the ONNX model and decides every frame without Python. This is the
   path used by the match scenarios and by deployment.

## Embedded inference pipeline

```text
B-Human representations
  (robot pose, ball model, obstacles, teammates, game state)
              |
              v
PPOObservationEncoder / GKObservationEncoder
              |
              v
ONNX model  ->  skill logits + continuous parameters
              |
              v
PPOSkillGate / GKSkillGate  (legal-action mask, applied before argmax)
              |
              v
PPOActionDecoder / GKActionDecoder
              |
              v
SkillRequest -> SkillBehaviorControl -> MotionRequest -> joints
```

The building blocks live in [Src/Libs/RL](../../Src/Libs/RL); the integration
lives in
[StrategyBehaviorControl](../../Src/Modules/BehaviorControl/StrategyBehaviorControl).
Every cycle the classical B-Human strategy is computed first. If RL is enabled,
the game is in `playing`, and the player is eligible, the `SkillRequest` is
replaced by the policy's decision. The field-player policy always excludes the
goalkeeper; the goalkeeper policy runs separately and only on the goalkeeper.

The network never commands motors directly:

- the **encoders** reproduce exactly the normalization used during training;
- the **model** outputs skill logits plus four continuous parameters;
- the **gates** remove unsafe or incoherent skills before the `argmax`;
- the **decoders** turn the decision into a valid B-Human `SkillRequest`.

For example, `walk` is anchored to a tactical pose, `pass` receives a valid
teammate, and `shoot` is only available when ball perception, angle, and the
shooting opening clear their thresholds.

## Field-player policies

Field policies share one action contract: eight skills (`stand`, `walk`,
`shoot`, `pass`, `dribble`, `block`, `mark`, `observe`) and four continuous
parameters. Four variants are available:

| Mode | Scenario | Model | Description |
| --- | --- | --- | --- |
| `striker_base` | `4v4_StrikerBase` | `ppo_striker_hsl2026.onnx` | First striker; 26-value observation; approach → dribble → shoot chain. |
| `baseline_attack` | `4v4_BaselineAttack` | `ppo_defender_hsl2026_param_repair.onnx` | Defense/support with walk, pass and block, driven by possession/threat gates. |
| `mixed_attack` | `4v4_MixedAttack` | `ppo_team_hsl2026_v4_2.onnx` | Striker with the 47-value team observation. |
| `complete` | `4v4_Complete` | `ppo_team_hsl2026_v5_merged.onnx` | One 47-value policy for all field players (used in our matches). |

The `complete` mode is the most integrated. A C++ role coordinator assigns
`striker`, `open_support`, and `off_ball_support` dynamically, with
hysteresis to avoid constant switching; it computes smoothed support
positions, appends the role one-hot to the observation, and decodes the same
network's output differently for the striker and for the supporters. The
learned policy makes the decision, while the C++ side contributes
coordination, geometry, team communication, and safety constraints.

## Goalkeeper policy

The goalkeeper has an independent contract defined in
[GKCommon.h](../../Src/Libs/RL/GKCommon.h): a 64-value observation (38 active
features, zero-padded), 12 skills, and 4 parameters. It is evaluated only on
the goalkeeper and is a no-op on field players.

| Skill | B-Human request |
| --- | --- |
| `stand` | stand |
| `walk_goalie_pose` | walk to the goalie pose |
| `observe` | observe a point |
| `intercept_center`, `intercept_lateral` | intercept ball (no dive) |
| `intercept_low_l/r` | keeper dive, low (squat, arms back) |
| `intercept_jump_l/r` | keeper dive, jump |
| `clear` | shoot |
| `pass` | pass to the most advanced teammate (clear if none) |
| `dribble_out` | dribble along a heading |

`GKObservationEncoder` builds its features from existing B-Human
representations, so no extra representation is required. `GKSkillGate` masks
illegal skills before the `argmax`; intercepts are only armed while the robot
is upright. Whether the dive is actually executed is still controlled by
`keeperJumpingOn` in `behaviorParameters.cfg`, which the `Dive` skill checks
for every request, including RL ones.

The goalkeeper's save rate was measured in the RL repository with an
analytical benchmark; dive reach on the real NAO should be calibrated before
treating that number as on-robot performance.

## Configuration and models

Models and their JSON manifests live in
[Config/NeuralNets/RLPolicy](../../Config/NeuralNets/RLPolicy). A manifest
records the observation size, skill order, normalization, source checkpoint,
and hash. When replacing a model, keep the feature order, skill order,
normalization, and output format unchanged, or the C++ encoder/decoder will
silently disagree with the network.

Each scenario's `strategyBehaviorControl.cfg` selects the variant
(`enableEmbeddedPPO`, `embeddedPPORole`, the model paths,
`enableEmbeddedGK`, the stand watchdog). `4v4_Full` is the classical
reference without field-player RL. The location for every 4v4 match is
`4v4_Full`; scenario and RL mode are independent choices.

## Deployment

`Make/Common/deploy` copies the binary, the configuration, the models, and
`libonnxruntime` to the robot, and patches the scenario configuration
according to the RL options:

```bash
./deploy Release \
  -r 1 <goalkeeper-ip> -r 2 <player-ip> -r 3 <player-ip> -r 4 <player-ip> \
  -t <team-number> -s 4v4_Complete -l 4v4_Full \
  --rl-complete 2,3,4 --rl-gk on --goalkeeper-dive on
```

| Option | Effect |
| --- | --- |
| `--rl-disable` | No field-player RL. |
| `--rl-striker-base <players>` | `striker_base` policy. |
| `--rl-defender <players>` | `baseline_attack` (defender) policy. |
| `--rl-mixed-attack <players>` | `mixed_attack` policy (v4.2). |
| `--rl-complete <players>` | `complete` policy (merged v5). |
| `--rl-gk on\|off` | Goalkeeper policy. |
| `--goalkeeper-dive on\|off` | Sets `keeperJumpingOn`. |

The deploy dialog (`rlModes` and `goalkeeperDivingEnabled` in
`Config/teams.cfg`) translates into the same options.

Note: `--rl-complete` selects the merged model but does not restrict it to
the listed players. With the merged model configured, the policy runs on
every active field player and the goalkeeper is always excluded.

## Safety and fallback

The policy only takes control during active play. Initial, ready, set,
stopped, and penalized states, and players that are not enabled, keep the
normal behavior. If the model fails to load, inference fails, no legal action
exists, or the `stand` watchdog fires, `StrategyBehaviorControl` keeps or
restores the classical B-Human behavior. The watchdog can force a walk and
applies a cooldown before retrying the policy.

The log shows the current state:

```text
[RL] Embedded PPO model loaded
[RL] mode=EmbeddedPPOActive
[RL] Embedded PPO action
[RL] Embedded PPO inference failed
[RL] mode=EmbeddedPPOFallback
[RL] Embedded GK model loaded
```

A minimal check after changing a model: build `SimulatedNao` and `Nao`,
confirm the ONNX loads, confirm `EmbeddedPPOActive` (or the goalkeeper mode)
on the expected players, and verify that decisions produce `SkillRequest`s
and actual movement in SimRobot.

## Result

The final system is hybrid: B-Human keeps handling game rules, perception,
localization, communication, skills, and locomotion, while RL replaces part
of the strategic decision in a controlled way. Training and experimentation
stay in the RL repository; only validated policies move here as ONNX files,
and the on-robot path always has the classical behavior as a fallback.
