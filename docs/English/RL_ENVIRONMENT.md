# RL Training Environment (`RL/`)

## Summary

This document describes the **simulation environment** used by `RL/` to
train the PPO/MAPPO policies that are later exported to ONNX and integrated
into `SabanaHerons2026` (see
[FINAL_RL_INTEGRATION_SABANAHERONS.md](./FINAL_RL_INTEGRATION_SABANAHERONS.md)
for the production half). The focus here is exclusively the experimental
side: how the observation is built, how the action is decoded, how the
reward is computed, and how each episode's world is generated.

`RL/` and `SabanaHerons2026/` are sibling repos under the same parent
directory. The RL environment does not simulate its own physics: it opens a
real **SimRobot** instance (B-Human's simulator) through the C++ binding
`pybh` and uses shared memory to send actions and read observations. RL never
controls motors directly; it only requests a high-level `SkillRequest`
(`walk`, `shoot`, `dribble`, ...) that B-Human turns into a real
`MotionRequest`.

```text
RealSimRobotEnv (PettingZoo ParallelEnv)
        |
        v
PyBHBackend.step(StructuredSkillAction)
        |
        v
pybh.rl_set_action()   <- shared-memory boundary
        |
        v
RLSkillProvider.cpp -> SkillRequest -> SkillBehaviorControl -> MotionEngine -> SimRobot
```

Observations travel the reverse path: `pybh.rl_get_obs()` ->
`BackendObservation` -> `RealSimRobotEnv._encode_obs()` -> the float vector
consumed by the policy.

## Backend: `PyBHBackend` (`pybh_backend.py`)

`PyBHBackend` encapsulates the entire lifecycle of the SimRobot scene:

- **Modes**: `simrobot` (SimRobot embedded in the same Python process,
  headless) and `simrobot_visible` (an external instance with a window,
  useful for live debugging).
- **Profiles**: `2d` and `3d`, each pointing to a different `.ros2d`/`.ros2`
  file under `SabanaHerons2026`'s `Config/Scenes/` (`SIMROBOT_SCENE_PRESETS` /
  `SIMROBOT_VISIBLE_SCENE_PRESETS` in `config.py`). The visible 3D profile has
  extra variants (`3d_visible`, `3d_visible_ci60`) that tune the camera frame
  rate so it doesn't collide with `obs_timeout_ms`.
- **`frames_per_step`**: how many SimRobot frames advance per `step()` before
  an observation is returned (controls how many times B-Human reacts per RL
  decision).

The public lifecycle is:

| Method | What it does |
| --- | --- |
| `start()` | Loads or connects to the SimRobot scene; in 3D it runs a Qt/OpenGL "preflight" of variants before starting. |
| `reset(world_spec)` | Repositions the ball, own robot, teammates and opponents according to a `WorldSpec`; waits for the observation to settle. |
| `step(action)` | Translates the structured action into the bridge's format, calls `rl_set_action`, advances `frames_per_step` frames, and reads `rl_get_obs`. |
| `set_dynamic_world(...)` | Repositions dynamic obstacles mid-episode (curriculum with moving opponents). |
| `read_obs()` | Reads the current state without advancing the simulation. |
| `close()` | Unloads the scene or closes the visible process. |

### `WorldSpec` — reset contract

```python
@dataclass(frozen=True)
class WorldPlayerSpec:
    number: int
    x: float
    y: float
    theta: float = 0.0
    upright: bool = True

@dataclass(frozen=True)
class WorldSpec:
    ball_x: float
    ball_y: float
    robot_x: float
    robot_y: float
    robot_theta: float
    teammates: tuple[WorldPlayerSpec, ...] = ()
    opponents: tuple[WorldPlayerSpec, ...] = ()
```

Every training world — single-agent curriculum, production scenarios, or
multi-agent worlds — is expressed as a `WorldSpec`. This is the only thing
`PyBHBackend.reset()` needs to know where to place every piece on the field;
the curriculum (see below) only decides *which* `WorldSpec` to sample for each
episode, not how it gets applied.

### `BackendObservation`

This is the object the bridge produces on every `step`/`reset`: robot pose,
ball perception (relative position, predicted end position, velocity, time
since last seen, natural-vs-exported/corrected trust flags), obstacle
distances, and bridge status flags (`obs_ready`, `requested_skill`,
`requested_pass_target`). The environment never exposes this raw object to the
policy: it always goes through `_encode_obs()`.

## `RealSimRobotEnv` (`environment.py`)

This is a PettingZoo `ParallelEnv` with a single logical agent, `player_1`
(the team multi-agent environment is built *on top of* this one, see below —
it does not replace it).

```python
observation_spaces = {"player_1": Box(-inf, inf, shape=(OBS_SIZE,))}  # OBS_SIZE = 41 (47 with the team extension)
action_spaces      = {"player_1": Box(low=[0, -1, -1, -1, 0], high=[7, 1, 1, 1, max_pass_target])}
```

### `reset(seed, options)`

1. If production scenario resets are enabled
   (`RL_PRODUCTION_SCENARIO_RESETS=1`) and no explicit `stage_id`/`world_spec`
   was requested, it samples a weighted `ProductionScenario` (see the
   curriculum section).
2. Resolves the episode's `stage_id` (`_sample_episode_stage_id`) — normally
   the configured stage, unless an "interleave" probability toward the finish
   stage kicks in (see below).
3. Samples a `WorldSpec` for that stage (`_sample_episode_world_spec`): a mix
   of the stage's generic reset, a finish sequence (`finish_sequence`), a
   pre-shot approach (`preshoot_dribble`), or a pure finish (`finish`),
   depending on per-stage probabilities.
4. Calls `backend.reset(world_spec)`, resets the episode's `SkillGate`, and
   encodes the first observation.

### `step(actions)`

The flow inside a step, in order:

1. Decodes the raw action (a `dict` or a 5-float `np.ndarray`) into a
   `StructuredSkillAction`.
2. `_decode_residual_policy_action`: the continuous parameters the policy
   emits are **residuals** relative to a rule-computed anchor
   (`build_fallback_action_targets`), not absolute field coordinates. This
   drastically bounds the search space (the policy only has to learn a fine
   adjustment on top of a reasonable geometric approximation).
3. *Handoff override*: if the policy requests `walk` but the geometric
   conditions for taking over the ball are already met
   (`_should_force_dribble_handoff`), the environment forces `dribble`
   instead, so the walk→dribble transition moment is not missed.
4. *Target repair* (`_repair_execution_targets`): fixes targets that would
   drive the robot off the field or into a degenerate pose.
5. `project_action`: projects the final action into the backend's valid
   ranges.
6. `backend.step(...)` advances the real simulation and returns the new
   `BackendObservation`.
7. If the stage has dynamic obstacles, it updates their positions
   (`get_dynamic_world_update` / `backend.set_dynamic_world`).
8. Computes `done` (goal, timeout, robot fallen/stuck) and builds the
   `StepContext` with all the step's evidence (executed vs. requested skill,
   skill history, whether there was a collision, whether the "takeover" is
   committed with post-takeover grace, etc.).
9. `reward.evaluate_step(step_ctx, stage.reward_weights)` computes the reward
   (see the Reward section).
10. `SkillGate.step(obs)` recomputes which skills are armed for the *next*
    step and bakes them into the encoded observation.
11. `_encode_obs(obs)` produces the final vector; the episode metrics tracker
    is updated (used by evaluation and stage promotion).

## Observation space (`observation.py`, `OBS_SIZE = 41`, team extension to 47)

`ObsIndex` is an `IntEnum` that fixes the exact order of every column.
Breaking this order invalidates any already-trained checkpoint (`.pt`) or
ONNX model.

| Range | Block | Content |
| --- | --- | --- |
| `[0:3]` | Own pose | `robot_x`, `robot_y`, `robot_theta` |
| `[3:13]` | Ball perception | relative position, predicted end position, velocity, time since seen/disappeared, seen %, consistency with game state |
| `[13:17]` | Scoring | `can_score_now`, shot quality with no obstacles, shot opening with obstacles, pass options count |
| `[17:23]` | Proximity | distance to the nearest teammate/opponent/uncertain obstacle (global and frontal) |
| `[23:26]` | **Skill gate (Design A)** | `shoot_armed`, `dribble_armed`, `shoot_arm_progress` |
| `[26:41]` | Multi-agent context | 3 teammate relative positions, team ball estimate, teammate-engaging-ball flags, opponent goal vector |
| `[41:44]` | Team gate (Design A extension) | `pass_armed`, `observe_armed`, `pass_arm_progress` |
| `[44:47]` | Role one-hot | `is_striker`, `is_open_support`, `is_off_ball_support` (exactly one is 1.0 per RL agent; the goalkeeper always reads 0 on all three) |

Columns 26-46 were added after the original 0-25; that's why old checkpoints
(26 dims) are padded to the current observation with zeros in the network's
first layer instead of retraining from scratch (see
`RL_TRAINING_FLOW.md`).

## Action space and contract (`action.py`)

```python
class SkillType(IntEnum):
    stand, walk, shoot, pass_, dribble, block, mark, observe = range(8)
```

`SKILL_ORDER` fixes the public order (`pass_` is exposed as `"pass"` because
`pass` is a reserved word in Python). The flat action seen by
`skrl`/PettingZoo is a 5-float vector: `[skill_type, target_x, target_y,
target_theta, pass_target]`, where `target_x/y/theta` are **residuals
normalized to [-1, 1]** on top of the geometric anchor, not field
coordinates.

Not every continuous parameter is relevant to every skill. `ACTION_PARAM_MASKS`
fixes which dimensions are actually "alive" per skill (for example `shoot`
uses no continuous parameter, `pass` only uses `pass_target`, `walk`/`dribble`
use `y` and optionally `theta`). This mask is shared between the policy loss,
the BC loss, and the backend decoding, so a "dead" dimension is genuinely
pinned to zero across all three paths.

## `SkillGate` — stateful forbid-invalid masking (`gate.py`)

The gate is the piece that decides, every step, which skills are *safe/possible*
right now. Its semantics are strictly **forbid-invalid**: it can only *remove*
a skill that is unsafe or impossible right now (shooting when not set up,
dribbling without controlling the ball). `walk` and `stand` are never
removed, so the policy always has a safe fallback and the mask can never trap
it.

Robustness mechanisms against perception noise:

- **Hysteresis**: separate tight-enter / loose-exit thresholds per skill, so
  it doesn't oscillate near the boundary.
- **Temporal debounce**: the enter condition must hold for `N` consecutive
  frames before arming (`shoot_arm_frames`, `dribble_arm_frames`).
- **Uncertainty gating**: arming `shoot` requires fresh *natural* ball
  perception (not the estimated/exported one); `dribble` uses a wider grace
  window to tolerate `walkTo`'s head momentarily losing sight of the ball
  while approaching (avoids the see→engage→lose-sight deadlock).

The resulting `GateDecision` (`shoot_armed`, `dribble_armed`,
`shoot_arm_progress`, and in the team environment also `pass_armed`,
`observe_armed`, `is_striker`/`is_open_support`/`is_off_ball_support`) is
baked directly into the observation (`obs[23:26]` and `obs[41:47]`). This is
what the code calls **Design A**: the valid-action mask used by the policy in
`train.py` (`gate_skill_mask_from_obs`) is a pure function of the stored
observation vector, so PPO's log-prob recompute at update time reproduces
exactly the same mask used when the action was sampled, without having to
store the mask separately in the rollout buffer. The same gate also feeds the
heuristic teacher, the imitation dataset's labels, and the guardrail that
runs embedded on the robot — a single owner of "what is safe right now."

## Reward (`reward.py`)

`RewardWeights` is a frozen dataclass with ~37 terms (progress toward the
ball, alignment, entering/holding the "takeover", walk/dribble/shoot quality,
collision penalty, skill "thrash" penalty, timeout, goal bonus, perception
honesty terms — `stale_ball`/`collapsed_ball`/`rescued_ball`/`corrected_ball`/
`lost_trusted_ball` — that penalize the agent for relying on a ball it
wouldn't actually see that way on the real robot, etc.). Each `StageSpec` in
the curriculum carries its own weight set, so the shape of the reward
deliberately changes stage by stage (see `RL_TRAINING_FLOW.md`).

`evaluate_step(step_ctx, weights)` is the central function: it classifies the
step into zones (`walk_zone`, `dribble_zone`, `shoot_zone`, the "takeover"
envelope), computes a soft heuristic bias toward the preferred skill
(`heuristic_bias`), computes each term, weighs them, and returns a
`StepEvaluation` with the full breakdown (useful for debugging why an episode
got the reward it got).

## Curriculum and world generation (`curriculum.py`, `production_curriculum.py`)

Each `StageSpec` fixes: the initial ball-robot distance range, whether the
field is full or bounded, the obstacle mode (`none`/`static`/`dynamic`), which
skills are active, and that stage's reward weights.

| Stage | Name | Active skills | Obstacles | Description |
| --- | --- | --- | --- | --- |
| -1 | `stage_0a_walk_approach` | walk, stand | none | Pre-stage: only walking toward the ball, no skill switching. |
| 0 | `stage_0_clean_handoff` | walk, dribble, stand | none | Clean walk→dribble handoff from a realistic approach distance, no shoot. |
| 1 | `stage_1_finish_after_handoff` | walk, dribble, shoot, stand | none | Full walk→dribble→shoot chain with no obstacles. |
| 2 | `stage_2` | walk, dribble, shoot, stand | static | Same as 1, with static obstacles and a collision penalty. |
| 3 | `stage_3` | walk, dribble, shoot, stand | dynamic (scripted) | Temporal robustness, full field, moving obstacles. |
| 4 | `stage_4_production` | walk, dribble, shoot, stand | dynamic | Deployment target: real finishing without skill collapse. |
| 6 | `multiagent_warmup_2v0` | + pass | static | 1 B-Human goalkeeper + 2 RL field players, minimal opponent exposure; warms up the role one-hot and the pass head. |
| 7 | `multiagent_adversarial_3v3` | + pass | dynamic, full field | 1 B-Human goalkeeper + 3 RL vs. a full B-Human team. |
| 8 | `multiagent_defender_3v3` | + pass, block | dynamic, full field | Same as 7 with `block` enabled for the off-ball defensive role. |

Stages 6-8 inherit stage 4's reward weights; team-specific terms (pass
attempt/completion, holding an open support position, team goal bonus, role
"thrash" penalty, teammate separation) are applied at runtime only when the
info packet carries a `role_id`, so as not to break the `RewardWeights`
dataclass's backward compatibility.

On top of each episode's `WorldSpec`, besides the stage's generic reset there
are specialized generators that activate with a certain probability
(`stageN_finish_interleave_probability` in `config.py`, tunable per stage):
`sample_finish_world_spec` (pure finish), `sample_finish_sequence_world_spec`
(a full sequence from farther away), and `sample_preshoot_dribble_world_spec`
(the approach right before the shot). This keeps the agent from only ever
seeing the stage's "generic" reset and never practicing the specific closing
phase it struggles with most.

**Production scenarios** (`production_curriculum.py`): a `ProductionScenario`
is a `WorldSpec` with a name, a `stage_id`, and a world function, designed to
reproduce exactly the situations used by `behavioral_evaluator.py` and the
production imitation dataset. They are enabled with
`RL_PRODUCTION_SCENARIO_RESETS=1`, filtered by set
(`RL_PRODUCTION_SCENARIO_SET=eval|topup|team|defense|all`), and weighted by
name via `RL_PRODUCTION_SCENARIO_WEIGHTS` (see `RL_TRAINING_FLOW.md`
for how this is used in the final fine-tune).

`promote_stage(stage_id, metrics, history)` is the auto-curriculum gate: each
stage defines its own thresholds (healthy ball semantics, stable behavior,
"takeover" success rate, shoot rate, etc.) that the aggregated evaluation
metrics must clear before moving up a stage. Promotion is **monotonic**
(never demotes), and in `train.py` it must be confirmed over several
consecutive evaluations (`RL_PROMOTION_CONFIRM_EVALS`, default 2) so a lucky
evaluation doesn't over-promote the agent.

## Multi-agent environments (`parallel_environment.py`, `team_parallel_environment.py`)

`ParallelSlotSimRobotEnv` is the layer that lets several players be trained in
parallel: it creates an independent `RealSimRobotEnv` ("slot") per player
number, each with its own visible SimRobot instance, and exposes the union as
a single PettingZoo `ParallelEnv`. `reset`/`step` dispatch to each slot in
separate threads so each instance's simulation time isn't serialized.

`TeamSlotSimRobotEnv` adds, on top of `ParallelSlotSimRobotEnv`, the team
coordination layer:

- **Role assignment**: each RL agent receives `striker`, `open_support`, or
  `off_ball_support` per step, with hysteresis to avoid constant switching.
- **Cross-slot observation injection**: each slot sees, in `obs[26:41]`, its
  teammates' positions and a team ball estimate built from the other slots'
  observations (`_inject_for`).
- **Pass decision**: when the role is `striker` and `shoot`/`dribble` are
  armed, it evaluates `smash_or_pass` with teammates' positions to decide
  whether to shoot or pass, and arms `pass_armed` in the team gate.
- **Team metrics**: pass attempts/completions, defensive-coverage-holding
  steps, role changes, goalkeeper-defender collisions, etc.
  (`_team_metrics`), used by evaluation and by the curriculum of stages 6-8.

The goalkeeper is a completely separate model (64-value observation, 12 of
its own skills) and does not go through this field environment; it is trained
and evaluated with its own stack (`goalkeeper_action.py`,
`train_goalkeeper_ppo*.py`, `run_goalkeeper_production.py`).

## Hard invariants

- **`OBS_SIZE = 41`/47 and `SKILL_ORDER` cannot change** without invalidating
  every already-trained and deployed checkpoint (`.pt`) or ONNX model.
- **Design A has a single owner**: the forbid-invalid mask at PPO update time
  is derived only from `obs[23:26]`/`obs[41:44]` (`gate_skill_mask_from_obs`
  in `train.py`); it must not be recomputed anywhere else in the codebase.
- **Max 4 visible SimRobot slots in parallel** — 6 saturates Qt/OpenGL and
  crashes the run.
- Any change to `RLSharedState`, `RLSkillProvider.cpp`, or `Module.cpp` on the
  `SabanaHerons2026/` side requires rebuilding both `pybh` **and**
  `SimulatedNao`; if they end up misaligned, `obs_ready` never returns to
  `True` even though the rest of the pipeline compiles without errors.

## References

- `RL/docs/README.md` — historical index of the RL/B-Human/SimRobot integration.
- `RL/CLAUDE.md` — up-to-date architecture map and operational commands.
- [FINAL_RL_INTEGRATION_SABANAHERONS.md](./FINAL_RL_INTEGRATION_SABANAHERONS.md)
  — how this environment's exported ONNX is consumed in production inside
  `SabanaHerons2026`.
- [RL_TRAINING_FLOW.md](./RL_TRAINING_FLOW.md) — how the policy that runs on
  this environment is trained, evaluated, and exported.
