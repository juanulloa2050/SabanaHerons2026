# RL Training Flow

## Summary

This document describes how the policy that runs on the environment
described in [RL_ENVIRONMENT.md](./RL_ENVIRONMENT.md) is trained, evaluated,
and exported. All of this work lives in `RL/`; the final output of this flow
is an `.onnx` + `.json` manifest pair that `SabanaHerons2026` consumes at
inference time (see
[FINAL_RL_INTEGRATION_SABANAHERONS.md](./FINAL_RL_INTEGRATION_SABANAHERONS.md)).

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
SabanaHerons2026/Config/NeuralNets/RLPolicy/  (consumed by StrategyBehaviorControl)
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
`obs[23:26]`/`obs[41:44]`, see `RL_ENVIRONMENT.md`). The result always leaves
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
4. Runs its own loop (not `skrl`'s `SequentialTrainer.train()`): as documented
   in `RL/README.md`, the single-`MultiAgent` path in this version of `skrl`
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

## BC regularization during PPO (`BCReplayRegularizer` / `BCCompetenceGate`, `train.py`)

While PPO trains on-policy, a background distillation loss periodically pulls
the policy back toward the frozen BC snapshot
(`clone_frozen_policy_reference`), replaying batches from the demonstration
dataset. This prevents the skill collapse PPO can induce on its own (the case
documented in `CLAUDE.md`: `policy_shoot_rate = 0.000`, a local optimum where
the agent stops attempting to shoot). This loss's weight (`beta`) decays with
competence measured across evaluations (`BCCompetenceGate`: from
`bc_beta_max` to `bc_beta_min` with a half-life measured in number of
evaluations), so the imitation anchor relaxes as the policy proves it matches
or beats the teacher. `bc_skill_loss_scale` and `bc_param_loss_scale`
separately control how much the discrete vs. continuous part of that
distillation weighs.

## Multi-agent curriculum — stages 6/7/8

Stages 6 (`multiagent_warmup_2v0`), 7 (`multiagent_adversarial_3v3`), and 8
(`multiagent_defender_3v3`) are trained on `TeamSlotSimRobotEnv` (see
`RL_ENVIRONMENT.md`) with `train_multiagent.py` (exploratory) and
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
same manifest that `FINAL_RL_INTEGRATION_SABANAHERONS.md` documents as a
requirement for keeping the contract between training and the C++ runtime.
There are three exporter variants: `export_ppo_policy.py` (single-agent
striker), `export_ppo_policy_team.py` (47-observation team policy), and
`export_goalkeeper_policy.py` (goalkeeper, an independent 64-observation /
12-skill model).

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
- Changes to `RLSharedState`, `RLSkillProvider.cpp`, or `Module.cpp` on the
  `SabanaHerons2026/` side require rebuilding both `pybh` **and**
  `SimulatedNao` before training again; if they end up misaligned,
  `obs_ready` never returns to `True` and training never starts, even though
  there's no Python-side error.

## References

- [RL_ENVIRONMENT.md](./RL_ENVIRONMENT.md) — the environment contract
  (observation, action, gate, and reward) this flow consumes.
- [FINAL_RL_INTEGRATION_SABANAHERONS.md](./FINAL_RL_INTEGRATION_SABANAHERONS.md)
  — how the exported ONNX is consumed inside `SabanaHerons2026`.
- `RL/CLAUDE.md` — file map and operational commands.
- `RL/FOR_TRAIN_PRODUCTION.md` — quick-start guide and triage checklist for
  the production fine-tune.
- `RL/.docs/09-production-selfplay-and-multiagent-handoff.md` and
  `RL/.docs/10-graph-coordination-post-production-roadmap.md` — detailed
  handoffs for the multi-agent expansion.
