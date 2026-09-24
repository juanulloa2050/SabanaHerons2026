# Final Reinforcement Learning Integration in SabanaHerons

## Summary

The project incorporated reinforcement learning (RL) policies into
`SabanaHerons2026`'s soccer stack without replacing B-Human's locomotion or
skills. RL decides **which high-level skill to request** — for example
walking, shooting, passing, dribbling, or blocking — and SabanaHerons keeps
full physical execution: it turns that decision into a `SkillRequest`, then
into a `MotionRequest`, and finally into the NAO's joint movements.

The `RL/` repository is the experimental base: it is where the SimRobot
environments are built, observations are generated, PPO/MAPPO is trained and
evaluated, and models are exported. `SabanaHerons2026/` is the operational
product: it holds the C++ runtime, the ONNX models, the safety rules, the
scenarios, and the deployment options. In competition the robot needs neither
Python nor training; it runs local inference with ONNX Runtime.

## What was built

In `RL/` an environment connected to B-Human via `pybh` was created,
compatible with both embedded and visible SimRobot. The environment can
reset and update the ball, the robot, teammates, and opponents; read
B-Human's perception and internal state; apply rewards and curricula; and
train policies with hybrid actions:

```text
action = discrete skill + four continuous parameters
```

The eight field-player skills are `stand`, `walk`, `shoot`, `pass`,
`dribble`, `block`, `mark`, and `observe`. The process evolved from a single
striker with 26 observations to team policies with 47 observations, roles,
and teammate context. Heuristic teachers, imitation pre-training, stability
repairs, scenario-based evaluation, and ONNX exporters compatible with the
NAO's runtime were also added. The goalkeeper uses an independent policy
with 64 observations and 12 specific skills.

In `SabanaHerons2026/` the two ways of consuming that work were implemented:

1. **External control for training and testing.** Python writes actions and
   receives observations through `RLSharedState`. The `4v4_RL2D` and
   `4v4_RL3D` scenarios use `RLSkillProvider` to feed those actions into
   B-Human's normal flow.
2. **Embedded PPO for matches and the real robot.** `StrategyBehaviorControl`
   loads the ONNX model and decides every frame without depending on Python.
   This is the path used by the match scenarios and by deployment to the
   robots.

## How it works inside SabanaHerons

The production flow is:

```text
B-Human representations
  (pose, ball, obstacles, team, and game state)
              |
              v
PPOObservationEncoder / GKObservationEncoder
              |
              v
ONNX model -> skill logits + parameters
              |
              v
SkillGate + legal-action mask
              |
              v
PPOActionDecoder / GKActionDecoder
              |
              v
SkillRequest -> SkillBehaviorControl -> MotionRequest
              |
              v
WalkingEngine / MotionEngine -> NAO joints
```

The main integration lives in
`Src/Modules/BehaviorControl/StrategyBehaviorControl`. Every cycle,
SabanaHerons first computes its usual B-Human strategy. Then, if RL is
enabled, the player is active, the match is in `playing`, and the player
belongs to the allowed set, it attempts to replace the `SkillRequest` with
PPO's output. The field PPO explicitly excludes the goalkeeper; the
goalkeeper policy is evaluated separately and only for the player flagged as
goalkeeper.

The network does not control motors directly. The encoders reproduce the
normalization used during training; the model produces eight logits and four
parameters; the *gates* remove unsafe or incoherent actions before the
`argmax`; and the decoder builds a valid B-Human request. For example, a
`walk` is anchored to a tactical pose, a `pass` receives a valid teammate,
and a `shoot` only becomes available when the ball, the angle, perception,
and the shooting opening all clear their thresholds. This keeps a raw
numeric network output from turning into a physical command without
validation.

For field players there are four variants:

| Mode | Main model | Use inside SabanaHerons |
| --- | --- | --- |
| `striker_base` | `ppo_striker_hsl2026.onnx` | Original striker, 26-value observation and approach-dribble-shoot chain. |
| `baseline_attack` | defender model | Defense/support with walk, pass, and block, driven by possession/threat gates. |
| `mixed_attack` | `ppo_team_hsl2026_v4_2.onnx` | Striker with a 47-value team observation. |
| `complete` | `ppo_team_hsl2026_v5_merged.onnx` | Unified 47-value policy for all field players. |

The `complete` mode is the most integrated one. A C++ coordinator dynamically
assigns the `striker`, `open_support`, and `off_ball_support` roles; applies
hysteresis to avoid constant switching; computes smoothed support positions;
appends the role to the observation; and decodes the same network's output
differently for the striker and for supports. This way, the learned policy
participates in the decision while SabanaHerons contributes coordination,
geometry, team communication, and safety constraints.

The goalkeeper is independent from the field PPO. Its model can reposition,
observe, intercept centrally or laterally, perform low blocks and dives,
clear, pass, and dribble out. `GKSkillGate` masks invalid actions before
selecting one, and SabanaHerons picks the most advanced teammate whenever the
network requests a pass.

## Configuration, models, and deployment

The models and their manifests live under `Config/NeuralNets/RLPolicy/`. The
JSON manifests document observation size, skill order, normalization,
checkpoint, and hash; they must be kept alongside the ONNX file to preserve
the contract between training and C++.

The `4v4_StrikerBase`, `4v4_BaselineAttack`, `4v4_MixedAttack`, and
`4v4_Complete` scenarios select the variant. `4v4_Full` serves as a
configurable reference, and `--rl-disable` allows comparing against B-Human
without PPO. The field location must remain `4v4_Full`; scenario and RL mode
are distinct concepts.

The `Make/Common/deploy` script copies the binary, all configuration, the
models, and `libonnxruntime` to the NAO. A full deployment conceptually
looks like:

```bash
./deploy Release \
  -r 1 <goalkeeper-ip> -r 2 <player-ip> -r 3 <player-ip> -r 4 <player-ip> \
  -t 49 -s 4v4_Complete -l 4v4_Full \
  --rl-complete 2,3,4 --rl-gk on --goalkeeper-dive on
```

The current `Config/teams.cfg` preset combines `4v4_Complete`, `gk` mode for
the goalkeeper, and `complete` for field players. The graphical deployment
selector translates `rlModes` values into the same script options.

**Important operational detail:** in the current implementation, configuring
`embeddedPPOMergedTeamModelPath` gives absolute priority to `complete` mode.
The script receives a list in `--rl-complete`, but does not copy it into
`embeddedPPOPlayers`; with the internal list empty, the policy is applied to
every active field player and the goalkeeper is excluded. So today that
option selects the full-team mode, not an effective per-player restriction.

## Safety and failure behavior

The policy only takes control during active play. Initial, stopped,
penalized, and non-enabled-player states remain in their corresponding
normal flow. If the model fails to load, inference fails, no valid action
exists, or the `stand` watchdog triggers, `StrategyBehaviorControl` keeps or
recovers B-Human's behavior. The watchdog can force a walk or apply a
cooldown before retrying PPO.

The logs make the main states clearly distinguishable:

```text
[RL] Embedded PPO model loaded
[RL] mode=EmbeddedPPOActive
[RL] Embedded PPO action
[RL] Embedded PPO inference failed
[RL] mode=EmbeddedPPOFallback
[RL] Embedded GK model loaded
```

A minimal validation consists of building `SimRobot` and `Nao`, checking that
the ONNX loads, confirming `EmbeddedPPOActive` or the goalkeeper mode on the
expected players, and observing that decisions produce a `SkillRequest` and
real movement. Before replacing a model it must be verified in `RL/` with the
behavioral and stability evaluators, re-exported to ONNX, and it must keep
exactly the same skill order, normalization, gates, and decoder used during
training.

## Final result

SabanaHerons ended up as a hybrid system: B-Human continues managing game
rules, perception, localization, communication, skills, and locomotion; RL
replaces, in a controlled way, part of the strategic decision. This
separation allows training and experimenting in `RL/`, moving only validated
policies over as ONNX, and keeping an autonomous, fast on-robot path with a
fallback to classic behavior.

## References

- [RL_ENVIRONMENT.md](./RL_ENVIRONMENT.md) — the `RL/` training environment:
  backend, observation/action contract, skill gate, reward, and curriculum.
- [RL_TRAINING_FLOW.md](./RL_TRAINING_FLOW.md) — how the policy consumed here
  is trained, evaluated, and exported to ONNX.
