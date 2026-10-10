# RL Training Flow

## Summary

This document describes how the policy that runs on the environment
described in [Environment.md](Environment.md) is trained, evaluated, and
exported. All of this work lives in our RL repository; the final output of
this flow is an `.onnx` + `.json` manifest pair that this code base consumes
at inference time (see [Integration.md](Integration.md)). File names refer to
modules of the RL repository (in its package layout, under `rlfootball/` and
`tools/export/`).

The full end-to-end pipeline is:

```text
teacher_policy.py (heuristic expert)
        |
        v
teacher_production_dataset.py  -> samples.npz (demonstrations)
        |
        v
train_bc.py  -> imitation warm-start (checkpoint .pt)
        |
        v
train.py  -> PPO/MAPPO through the curriculum (stages -1..4, then 6..8)
        |    with BC-replay regularization + auto stage promotion
        v
behavioral_evaluator.py -> pass/fail on 5 production scenarios
        |
        v
train_production.py  -> targeted repair fine-tune (stage 4 fixed)
        |
        v
tools/export_ppo_policy*.py -> ONNX + JSON manifest
        |
        v
Config/NeuralNets/RLPolicy/  (in this repository, consumed by StrategyBehaviorControl)
```

## Algorithm: MAPPO on `skrl` (`train.py`)

### Networks

`PolicyNetwork` is a hybrid policy: a shared trunk of two 128-unit linear
layers with ELU activation, feeding two heads:

- `skill_head`: categorical logits over the 8 skills (`SKILL_ORDER`).
- `param_head`: the (`tanh`-squashed) mean of 4 continuous residual
  parameters, with a learned but clamped standard deviation (`log_std`
  clamped to `[-2.0, -0.7]`, i.e. std between ~0.14 and ~0.50) — this keeps
  the entropy bonus needed to explore the discrete skill from also inflating
  noise on the continuous targets, which need precision so the dribble
  doesn't lose control of the ball.

`ValueNetwork` is a simple MLP (128-128-1, ELU) over the same observation
vector (today `state == observation`; the state space exists so
`skrl`/CTDE can differentiate them in the future without changing the
contract).

### Action-time masking

Before sampling the skill, `PolicyNetwork._masked_skill_logits` combines two
masks: the **stage mask** (`active_skill_names` from the current
`StageSpec`) and the **gate mask** (`gate_skill_mask_from_obs`, derived from
`obs[23:26]`/`obs[41:44]`, see [Environment.md](Environment.md)). The result always leaves
at least `walk`/`stand` available. Continuous parameters are also masked per
skill (`ACTION_PARAM_MASKS`) so "dead" dimensions accumulate neither log-prob
nor gradient.

### Run configuration (`ScriptRunConfig`)

`train.py` **does not use command-line flags**: the entry point
(`if __name__ == "__main__": main(_run_cfg_from_env())`) builds a
`ScriptRunConfig` from defaults (`IDE_RUN`, meant for running from the IDE)
and overrides them with `RL_*` environment variables (`RL_RUN_MODE`,
`RL_STAGE_ID`, `RL_TRAIN_STEPS`, `RL_ROLLOUT_STEPS`, `RL_BC_WARM_START`,
`RL_PARALLEL_PLAYER_NUMBERS`, etc. — see `_run_cfg_from_env` for the full
list). Real example:

```bash
RL_RUN_MODE=train \
RL_STAGE_ID=0 \
RL_TRAIN_STEPS=8000 \
PYTHONPATH=/path/to/SabanaHerons2026/Make/Python \
.venv/bin/python train.py
```

There are two modes:

- **`smoke`** (`run_smoke`): a handful of steps to validate that `act`,
  `record_transition`, and `post_interaction` work end-to-end against real
  SimRobot, with no intent to learn anything.
- **`train`** (`run_training`): the actual training loop.

### `run_training(...)`

1. Builds the environment (`make_env_config` + `build_env`) and the MAPPO
   agent (`build_mappo`), which instantiates `PolicyNetwork`/`ValueNetwork`
   and a rollout memory (`skrl`'s `RandomMemory`).
2. If `bc_warm_start=True`, loads an imitation checkpoint
   (`load_bc_warm_start`) as the starting point — with zero-padding in the
   first layer if the checkpoint was trained with fewer observation columns
   than the current layout (e.g. 26 → 41/47), so prior work isn't lost every
   time the observation vector is extended.
3. Clones that policy as a frozen reference (`clone_frozen_policy_reference`)
   for the BC-replay regularization described below.
4. Runs its own loop (not `skrl`'s `SequentialTrainer.train()`): the single-`MultiAgent` path in this version of `skrl`
   falls through the single-agent code and doesn't tolerate PettingZoo's
   dict-shaped return, so `train.py` keeps its own collection + update loop
   using `skrl`'s real `MAPPO` agent (`agent.act`, `agent.record_transition`,
   `agent.post_interaction`).
5. Every `eval_interval` steps it runs `evaluate_agent` (deterministic
   episodes against fixed scenarios), aggregates metrics
   (`_aggregate_eval_metrics`), and checks `promote_stage(...)` to decide
   whether to move up a stage — requiring `RL_PROMOTION_CONFIRM_EVALS`
   (default 2) consecutive passing evaluations.
6. Every `checkpoint_interval` steps (and on stage promotion) it saves a
   checkpoint with `save_checkpoint`.

Default hyperparameters (`TrainConfig` in `config.py`): `rollout_steps=256`,
`learning_epochs=4`, `mini_batches=4`, `learning_rate=1e-4`,
`discount_factor=0.99`, `gae_lambda=0.95`, `entropy_loss_scale=0.03`,
`ratio_clip=0.2`, `eval_interval=checkpoint_interval=5000`, `eval_episodes=8`.

### Checkpoints and telemetry

Checkpoints land at `runs/<experiment_name>/checkpoint_stage<S>_step<N>.pt`
and contain `policy_state_dict`, `value_state_dict`, `timestep`, `stage_id`,
and that evaluation's `eval_summary` — enough to resume training or evaluate
without rerunning anything. If `save_telemetry=True`, the same
`runs/<experiment_name>/` directory also gets `run_config.json` (the full run
configuration), `smoke_steps.csv`/`smoke_summary.csv`, `eval_summary.csv`,
and `eval_episodes.csv`, plus plots if `save_plots=True`.

## Heuristic teacher (`teacher_policy.py`)

`stage_aware_teacher_action(...)` is a rule-based expert (not learned) that
decides the "correct" skill given the current state: approach by walking,
dribble when the ball is controlled, shoot when the angle and distance to
goal are good, sidestep a frontal obstacle (from stage 2 onward), and
actively re-acquire the ball if it's been too long since it was seen on the
*natural* channel (`NATURAL_BLIND_MAX_AGE_MS`). It uses the same `SkillGate`
as the policy, so its decisions are consistent with what the policy can
actually execute (it never labels "shoot" when the gate wouldn't allow it).
This teacher has two uses:

- Generating demonstration datasets for Behavioral Cloning.
- Contributing a soft bias (`reward.heuristic_bias`) inside the reward,
  without forcing the action.

## Behavioral Cloning warm start (`train_bc.py`)

Before any PPO, `train_bc.py` trains `PolicyNetwork`/`ValueNetwork` purely by
imitation on a dataset of teacher demonstrations (`samples.npz`). Training is
organized into **phases by stage group** (`PHASES`, e.g.
`basics_stage_minus1_0` covers stages -1 and 0 for a configurable number of
epochs), mirroring the same difficulty progression as the RL curriculum.

The loss combines:

- **Skill cross-entropy**, masked to the stage's active skills
  (`bc_ce_safe_logits`) so the network isn't penalized for not predicting a
  skill that stage doesn't even allow.
- **Continuous-parameter MSE**, only over the demonstrated skill's live
  dimensions (`ACTION_PARAM_MASKS`).

It can start from scratch or from an existing checkpoint
(`RL_BC_INIT_CHECKPOINT`) using the same zero-padding in the first layer that
PPO's warm start uses. The result lands in `runs/bc_warm_start/`.

## Production imitation datasets (`teacher_production_dataset.py`, `production_curriculum.py`)

For the production fine-tune a targeted dataset is generated: the teacher is
run over the `ProductionScenario` worlds (hand-built worlds reproducing real
hard situations — a clean 2.5-3 m shooting band, the same band with a static
opponent in the lane, a boundary pre-shot angle, dynamic full-field pressure,
blocked-corridor repair — plus "topup" variants, and, for the team stages,
`team_*`/`defense_*` scenarios). `teacher_production_dataset.py` filters out
bad episodes (`_episode_kept`) and checks that the skill label is consistent
with the gate's bucket at that frame (`_gate_bucket`/`_gate_label_consistent`)
so the dataset never teaches a skill the real gate would have blocked. The
result is merged into a `samples.npz` + `summary.json`, which feeds both
`train_bc.py` and the BC-replay regularizer described next.

### Multi-agent teacher (stages 6–8)

The team datasets are generated by `teacher_multiagent_dataset.py` with the
rule-based team teacher `multiagent_teacher.py`
(`stage_aware_team_teacher_action`). Roles, coordination targets, and the
shoot-vs-pass decision follow the rules in
[Coordination.md](Coordination.md); the teacher adds the per-role skill
choice:

| Role | Heuristic |
| --- | --- |
| Striker | Single-agent teacher (approach with `walk`, `dribble` when the ball is controlled, `shoot` when the shot is armed). While the ball is controlled and `pass_armed` is set by `smash_or_pass`, the teacher labels `pass` to the chosen teammate. |
| Open support (attack) | `walk` to the smoothed open-lane target. `stand` within 250 mm of it only while it is a live pass option: the team ball is valid, a teammate is within 600 mm of it, and no opponent in front is closer than 700 mm. Otherwise the support repositions. |
| Open support (defense) | `walk` to a second defensive lane between the ball and the own goal (four-slot defensive grid); `stand` within 250 mm. |
| Off-ball support (attack) | Same rule toward the triangle target, with a 350 mm hold tolerance. If `dribble_armed` (the ball reaches it), it acts as striker for that frame. |
| Defender cover (off-ball support in defense) | `walk` to the cover point on a 1600 mm circle around the own goal, facing the ball; `stand` within 350 mm unless its own ball is stale (> 1000 ms) while the team ball is valid. |
| Block | Defender in defense, team ball valid and at x ≤ −500 mm, robot within 900 mm of its cover point, ball ahead with 0 < x < 1800 mm (robot frame). Lateral parameter = ball y (robot frame) / 900 mm. |
| Mark | Same guards as block, an opponent in front closer than 1500 mm and the ball not in front. Lateral parameter = opponent y (robot frame) / 900 mm. |
| Recovery / clearance | Striker controlling the ball at x < −1500 mm: (1) if `pass_armed`, `pass` to the most up-field teammate at least 600 mm ahead of the ball, within 400–4000 mm and with no opponent in front closer than 800 mm; (2) otherwise, if the ball is central (\|y\| < 1500 mm), a clearance `shoot`; (3) otherwise `dribble` (or `walk`) toward the wing at x = 0. |

The clearance `shoot` exists only as a training label. To keep it consistent
with the mask, the generator sets `shoot_armed` in the stored observation for
those rows. On the robot, `shoot` is armed only with the ball within 3000 mm
of the opponent goal
([PPOSkillGate.cpp](../../Src/Libs/RL/PPOSkillGate.cpp)), so a clearance from
the own third cannot be executed: recovery goes through `pass` or `dribble`.
The defender's `block` is handled the same way: when the teacher arms it,
`dribble_armed` is set in the stored observation.

### Label consistency

- **Gate and role mask.** Every candidate label is passed through the
  training-time mask (`gate_skill_mask_from_obs`, see
  [Coordination.md](Coordination.md#role-conditioned-mask)) evaluated on the
  stored 47-value observation, with the team gate bits and role one-hot
  already set. Labels the mask forbids are not recorded, so every stored row
  has `label_consistent = 1`. Rollout actions are still executed so that the
  episode continues.
- **Team communication.** Teammate poses and the team ball are refreshed every
  600 ms with growing ages in between, and the team ball expires after
  2000 ms, to match the stale, step-wise data the robots receive.
- **Audit.** `teacher_multiagent_dataset_audit.py` checks:
  - size: minimum rows and episodes, 47-value observations;
  - consistency: the consistency flags in `summary.json` and `label_consistent`;
  - coverage: striker and open-support rows present;
  - striker gates: minimum `dribble_armed`/`shoot_armed` rows, at least one
    `pass_armed` row, `dribble` ≥ 15 % and `shoot` ≥ 3 % of striker labels;
  - observation liveness: teammate block non-zero, team ball distinct from the
    goal vector;
  - diversity of coordination targets, and at least one kept episode per
    scenario;
  - stage 8: defensive rows present, a non-zero `block` share, and the
    defender's `stand` share with a valid team ball below 0.5.

  `block` and `mark` labels are validated by the generator-side mask filter
  above.

### Teacher-to-student action encoding

The teacher produces absolute field targets. The dataset stores what the
student outputs: a residual with respect to the same anchor the decoder uses
at execution time.

| Label | Anchor | Stored parameters |
| --- | --- | --- |
| Striker `walk` / `dribble`; off-ball escape `dribble` | ball-based anchor (`build_fallback_action_targets`) | residual |
| Support `walk` | smoothed coordination target, heading toward it | residual |
| `block`, `mark` | — | lateral offset in `target_y`, clipped to ±1 |
| `stand`, `shoot`, `pass` | — | zeros; `pass` keeps its target |

Residual = (absolute − anchor) / range, clipped to [−1, 1], with ranges 0 mm
in x (the x residual is always 0), 450 mm in y, and 0.75 rad in heading. The
same ranges are used by the C++ decoder
([PPOActionDecoder.cpp](../../Src/Libs/RL/PPOActionDecoder.cpp)) and are
recorded as `residual_ranges` in the model manifests.

### Episode discard criteria

Each scenario is attempted up to twice its requested number of episodes. An
attempt is discarded when:

| Criterion | Threshold |
| --- | --- |
| Ball misplaced after reset | farther than 800 mm from the requested position |
| Backend failure | a simulator step error; transient errors (stale visible frames, observation timeout, frame rewind) are retried up to 3 times first |
| Robot out of the field (fallen and parked) | \|x\| > 4600 mm or \|y\| > 3050 mm for 5 consecutive steps |
| Sustained teammate collision | two RL agents closer than 350 mm for 8 consecutive steps; single-frame crossings are ignored because each agent runs in its own simulation |
| Role thrashing | more than 10 role changes in the episode |


### Datasets

Corpus sizes reported in the paper:

| Dataset | Rows | Episodes | Positive labels |
| --- | --- | --- | --- |
| Single-agent, 26 values (`dataset_20260527_220501_production_merged`) | 17,815 | — | 16,402 positive: `walk` 5,858; `dribble` 6,332; `shoot` 4,212 |
| Multi-agent attack, stage 7 (`dataset_stage7_20260611_182346`) | 105,624 | 45 | `pass` 2,015; `shoot` 1,677 |
| Cooperation / striker | 434,639 | — | `pass` 16,914; `shoot` 5,702 |
| Defender, stage 8 (`dataset_stage8_20260618_211245`) | 33,999 | 29 | `mark` 673; `block` 2,817 |
| Final merge (`dataset_merged_v5_20260618`) | 139,623 | 74 | `block` 2,817; `mark` 673; `pass` 2,015; `shoot` 2,565 |

The stage-8 defender corpus was collected with the opponent features
`obs[18]` and `obs[21]` filled from simulator ground truth
(`RL_TRAIN_GT_OPPONENT_OBS=1`, `tools/dataset/collect_defense_gt.sh`), because
off-ball opponent perception in simulation was unreliable; on the robot these
features come from vision. The final merge is the concatenation of the attack
and defender corpora
(`merge_multiagent_teacher_datasets.py`; 105,624 + 33,999 rows,
45 + 29 episodes).

## BC regularization during PPO (`BCReplayRegularizer` / `BCCompetenceGate`, `train.py`)

While PPO trains on-policy, a background distillation loss periodically pulls
the policy back toward the frozen BC snapshot
(`clone_frozen_policy_reference`), replaying batches from the demonstration
dataset. This prevents the skill collapse PPO can induce on its own (we observed
`policy_shoot_rate = 0.000`, a local optimum where the agent stops attempting
to shoot). This loss's weight (`beta`) decays with
competence measured across evaluations (`BCCompetenceGate`: from
`bc_beta_max` to `bc_beta_min` with a half-life measured in number of
evaluations), so the imitation anchor relaxes as the policy proves it matches
or beats the teacher. `bc_skill_loss_scale` and `bc_param_loss_scale`
separately control how much the discrete vs. continuous part of that
distillation weighs.

## Multi-agent curriculum — stages 6/7/8

Stages 6 (`multiagent_warmup_2v0`), 7 (`multiagent_adversarial_3v3`), and 8
(`multiagent_defender_3v3`) are trained on `TeamSlotSimRobotEnv` (see
[Environment.md](Environment.md)) with `train_multiagent.py` (exploratory) and
`train_multiagent_production.py` (which sets
`RL_PRODUCTION_SCENARIO_SET=team`/`defense` to sample
`production_curriculum.py`'s team scenarios). They inherit stage 4's reward
weights; team-specific terms (pass attempts/completions, holding an open
support position, team goal bonus, penalty for switching roles too often,
teammate separation) are added at runtime when the info packet carries a
`role_id`, without touching the single-agent `RewardWeights` dataclass.

## Production fine-tune (`train_production.py`)

This is the entry point for *repairing* an almost-ready checkpoint, not for
climbing the curriculum from scratch: it fixes `stage_id=4` and disables
auto-promotion (`auto_promote=False`). Its real CLI:

```bash
python train_production.py \
  --checkpoint runs/production_finetune_v1/checkpoint_stage4_step35000.pt \
  --dataset runs/fine_tuning_production_dataset/.../samples.npz \
  [--output-dataset ...] [--prepare-only]
```

The rest of the run's parameters (training steps, evaluation cadence, the BC
`beta` schedule, visible-reset timeouts, number of parallel players) are
controlled with `RL_PROD_*` variables, with defaults already tuned from
previous runs (for example `bc_beta_max=0.15` instead of the generic `0.08`,
because a previous run turned the BC signal off too fast and the agent never
got to commit to shooting). Before training, it prepares the replay dataset
(`prepare_stage4_replay_dataset`) and enables production-scenario-weighted
resets with an explicit mix tuned from `behavioral_evaluator.py` failures
(for example `dynamic_full_field_pressure` is set to weight `0` because it
turned out to be the most expensive episode and the least useful). It runs
with 4 parallel visible SimRobot instances (players 1-4) — 6 saturates
Qt/OpenGL on this machine.

## Behavioral evaluation (`behavioral_evaluator.py`)

Loads a checkpoint and runs it deterministically on 5 hand-built situations
(the same `ProductionScenario`s from `production_curriculum.py`):
`finish_band_2p5_3m_clean`, `finish_band_2p5_3m_static_lane`,
`preshoot_bad_angle_boundary`, `dynamic_full_field_pressure`, and
`blocked_corridor_repair`. Each has its own pass criteria, for example for
the clean finish:

```python
clean_takeover_rate  >= 0.50
clean_finish_rate    >= 0.60
shoot_finish_rate    >= 0.25
goal_rate >= 0.10  OR  ball_progress_post_takeover_mean >= 0.015
collision_rate       <= 0.12
policy_shoot_rate    >= 0.02
```

CLI:

```bash
python behavioral_evaluator.py \
  --checkpoint runs/production_readiness_v2/checkpoint_stage4_step30000.pt \
  --episodes 4 --seed 4200 [--capture-steps]
```

The current bar before promoting a checkpoint to export is
`behavioral_evaluator ≥ 3/5` scenarios passing.

## ONNX export (`tools/export_ppo_policy*.py`)

The exporter rebuilds the network directly from the saved `state_dict`'s
tensors (`ActorExport`, with the same `trunk`, `skill_head`, and `param_head`
weights as `PolicyNetwork`), without going through `skrl`'s class, and exports
it with `torch.onnx.export` (input `obs`, outputs `skill_logits` +
`param_mean`, `opset_version=17`):

```bash
python tools/export_ppo_policy.py \
  --checkpoint runs/production_stability_repair_v1/checkpoint_stage4_step5000.pt \
  --output ppo_striker_hsl2026.onnx
```

Alongside the `.onnx` it writes a `.json` manifest with the model's input
observation size, the environment's actual output size (to know how much
padding there is), the skill order, the active skills, the checkpoint's and
the ONNX's own hash, the export commit, and the normalization contract — the
same manifest that [Integration.md](Integration.md) describes as a
requirement for keeping the contract between training and the C++ runtime.
There are three exporter variants: `export_ppo_policy.py` (single-agent
striker), `export_ppo_policy_team.py` (47-observation team policy), and
`export_goalkeeper_policy.py` (goalkeeper, an independent 64-observation /
12-skill model).

## Policy lineage (deployed models)

| Model | How it was obtained |
| --- | --- |
| `ppo_striker_hsl2026` | Single-agent striker, 26-value observation, curriculum stages -1..4, production fine-tune plus parameter/stability repairs. |
| `ppo_defender_hsl2026_param_repair` | Defender/support policy (walk, pass, block) trained on the stage-8 defense dataset, then a parameter-head repair. |
| `ppo_team_hsl2026_v4_2` | Team striker, 47-value observation. MAPPO on stage 7 warm-started from and KL-anchored to the earlier team model (v2b), pass-rich dataset, BC beta 0.30 → 0.05, 12 000 steps. Pass rate ≈ 0.99 in evaluation. |
| `ppo_team_hsl2026_v5_merged` | One policy for all field roles. Warm start and KL anchor on v4.2, merged attack + defense corpus, stage 8, 10 000 steps. Keeps 96.7 % skill agreement with v4.2 on attack data, adds 93.2 % recall on `mark`, 0 gate violations. |
| `ppo_goalkeeper_hsl2026_gk_closedloop_elite` | Separate 64-value / 12-skill model: heuristic teacher → mirror-augmented dataset → BC → closed-loop PPO rewarded on goals actually prevented. 93.3 % saves over 20 scenarios in an analytical benchmark (reach windows still to be calibrated on the real NAO). |

The details of each step are in the RL repository's development notes (docs
37, 38, and 39).

## Invariants and known friction points

- **`OBS_SIZE` and `SKILL_ORDER` are frozen**: changing either invalidates
  every already-exported `.pt` and `.onnx`. When the observation vector grew
  (26 → 41 → 47), compatibility was solved with zero-padding in the first
  layer (`_load_state_dict_obs_padded`), never by retraining from scratch.
- **Stage promotion is monotonic** (never demotes) and requires several
  consecutive passing evaluations before advancing, so a lucky
  perception evaluation doesn't push the agent into a stage it can't yet
  hold.
- **`train.py` is configured via environment variables, not CLI flags**;
  only `train_production.py`, `behavioral_evaluator.py`, and the `tools/`
  scripts use `argparse`.
- Changes to `RLSharedState`, `RLSkillProvider.cpp`, or `Module.cpp` in
  this repository require rebuilding both `pybh` **and**
  `SimulatedNao` before training again; if they end up misaligned,
  `obs_ready` never returns to `True` and training never starts, even though
  there's no Python-side error.

## References

- [Environment.md](Environment.md) — the environment contract (observation,
  action, gate, and reward) this flow consumes.
- [Integration.md](Integration.md) — how the exported ONNX is consumed on the
  robot.
- `docs/README.md` in the RL repository — index of the detailed development
  notes (striker v4.2, merged v5, and goalkeeper transplants are documented in
  its docs 37, 38, and 39).
