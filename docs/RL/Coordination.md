# Team Coordination in the Merged Policy

This document describes the rule-based coordination that surrounds the merged
field-player policy (`complete` mode, `ppo_team_hsl2026_v5_merged.onnx`): role
assignment, support placement, the shoot-vs-pass arbitration, and the
role-conditioned action mask. The runtime code is in
[StrategyBehaviorControl.cpp](../../Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp).
The training-time counterparts are in the RL repository, in
`rlfootball/teachers/multiagent_teacher.py`,
`rlfootball/env/team_parallel_environment.py`, and `rlfootball/core/gate.py`
(branch `refactor/repo-layout`).

Every robot runs the coordinator locally, every frame, from its own world
model and the team messages it receives. The elected role is not transmitted
(see [Team communication](Integration.md#team-communication)).

## Per-frame sequence

| Step | Function | Output |
| --- | --- | --- |
| 1 | `ppoSkillGateV47.step` ([PPOSkillGate.cpp](../../Src/Libs/RL/PPOSkillGate.cpp)) | `shootArmed`, `dribbleArmed` |
| 2 | `assignTeamRoles` | own role: striker, open support, or off-ball support |
| 3 | `computeStrikerPassArmed` (striker only) | `passArmed` and pass target |
| 4 | `buildTeamContext` | teammates, team ball, role one-hot, gate bits for `obs[26:47]` |
| 5 | `computeMergedOpenLaneTarget` / `computeMergedTriangleTarget` + EMA (supports only) | coordination target |
| 6 | `encode47` ([PPOObservationEncoder.cpp](../../Src/Libs/RL/PPOObservationEncoder.cpp)), ONNX inference | skill logits + 4 parameters |
| 7 | `buildMergedTeamMask`; striker with `finishArmed` also masks `stand`/`walk` | legal skills |
| 8 | argmax, block-priority rule, stand watchdog | selected skill |
| 9 | role-conditioned decoding ([PPOActionDecoder.cpp](../../Src/Libs/RL/PPOActionDecoder.cpp)) | `SkillRequest` |

If inference fails or no skill is legal, the classical B-Human request is kept
(see [Integration.md](Integration.md#safety-and-fallback)). Coordinator state
(roles, streaks, EMA targets, the off-ball side lock) is reset by
`resetEmbeddedPPO` whenever the policy is not in control, for example outside
`playing`.

## Role coordinator

`assignTeamRoles` returns `0` = striker, `1` = open support, `2` = off-ball
support. Candidates are this robot and every non-goalkeeper teammate whose last
team message is at most 2000 ms old. All distances are measured to this
robot's own ball estimate (field coordinates).

1. **All-stale fallback.** If this robot has not seen the ball for more than
   1000 ms and every teammate's message is older than 1000 ms, the previous
   role is kept (striker if there is none).
2. **Proposed striker.** The candidate with the smallest distance to the ball;
   ties go to the lowest player number.
3. **Ball-control override.** If the proposed striker is this robot, the ball
   is closer than 350 mm, and it was seen at most 1000 ms ago, this robot
   becomes striker immediately, skipping the hysteresis.
4. **Hysteresis.** If a different robot was striker in the previous frame and
   is still a candidate, it stays striker while it is less than 200 mm farther
   from the ball than the new candidate. Beyond that gap the new candidate
   must be proposed for 7 consecutive frames before the switch.
5. **Support roles.** The phase is defensive when the ball is at x < −300 mm.

| Non-strikers | Defensive phase | Offensive phase |
| --- | --- | --- |
| 1 | off-ball support | open support |
| 2 or more | deepest robot (smallest x) is off-ball support; the others are open support | highest `forwardRating` is open support; the others are off-ball support |

| Constant | Value | C++ | Python (training) |
| --- | --- | --- | --- |
| Teammate message timeout | 2000 ms | `assignTeamRoles` | — |
| Stale ball / stale teammate | 1000 ms | `assignTeamRoles` | `TEAMMATE_FRESH_MAX_AGE_MS` |
| Ball-control radius | 350 mm | `assignTeamRoles` | `BALL_CONTROL_RADIUS_MM` |
| Minimum distance gap | 200 mm | `assignTeamRoles` | `ROLE_MIN_DISTANCE_GAP_MM` |
| Hysteresis streak | 7 frames | `assignTeamRoles` | `ROLE_HYSTERESIS_FRAMES` |
| Defensive phase | ball x < −300 mm | `assignTeamRoles` | `DEFENSE_PHASE_ENTER_X` |

During training, roles are assigned by `assign_roles` (RL repository,
`multiagent_teacher.py`), which runs over all simulated agents at once:

| Aspect | Training (`assign_roles`) | Robot (`assignTeamRoles`) |
| --- | --- | --- |
| Ball used for distances | team ball if valid, otherwise each agent's own ball | this robot's own ball |
| All-stale fallback | the agent with the freshest ball becomes striker, the rest off-ball support | previous roles are kept |
| Ball-control override | evaluated for any candidate | evaluated only when the candidate is this robot |
| Phase | sticky, enters defense at x < −300 mm and returns to attack at x > 300 mm | x < −300 mm, no dead band |

The training column refers to the teacher committed with the merged v5
policy (RL repository, commit `2a85d0f`).

## Support placement

Supports walk to a coordination target chosen from a fixed grid of six
candidate slots (field coordinates, mm). The y coordinate is mirrored to one
side of the field.

| Slot | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| x | 800 | 1200 | 400 | −400 | 2000 | 2800 |
| y | 1200 | 800 | 1400 | 1200 | 1300 | 1200 |

Each slot is scored as `forwardRating × separationPenalty`, and the best slot
becomes the raw target.

- **`forwardRating(p, ball)`** is 0 if the slot is closer than 400 mm or
  farther than 4000 mm from the ball (pass range); otherwise it is
  `clamp(p.x / 4500, 0, 1)`.
- **`separationPenalty(p)`** is `1 − exp(−d² / (2σ²))` with σ = 800 mm, where
  `d` is the distance from the slot to the nearest teammate considered.

| Role | Side | Teammates for separation | EMA α |
| --- | --- | --- | --- |
| Open support (`computeMergedOpenLaneTarget`) | side of the ball (sign of ball y) | striker | 0.15 |
| Off-ball support (`computeMergedTriangleTarget`) | opposite to the open support, locked on first use | striker, open support | 0.10 |

The raw target is smoothed with an exponential moving average,
`target ← α · raw + (1 − α) · target`, initialized with the first raw target.
The off-ball target completes a triangle with the striker and the open
support. The grid, the ratings, and both α values match the training
constants (`SUPPORT_SLOT_CANDIDATE_GRID`, `SIGMA_TEAM_MM`,
`EMA_ALPHA_OPEN_LANE`, `EMA_ALPHA_TRIANGLE`).

In the training environment, the defensive phase has its own targets: the
off-ball support covers the gap left by the goalkeeper on a 1600 mm circle
around the own goal (`defender_cover_target`), and the open support takes a
second defensive lane from a four-slot defensive grid
(`defense_support_target`). On the robot, supports use the grid above in both
phases, and defensive actions come from the `block` and `mark` skills.

For a support, `walk` is decoded by `decodeTeamWalkWithAnchor`: the smoothed
target is the anchor, and the policy adds a residual of up to ±450 mm in y and
±0.75 rad in heading (the x residual range is 0). `block`, `mark`, and
`observe` use the defender decoding, and `dribble` the ball-anchored decoding.

## Shoot-vs-pass arbitration

Passing is a skill the policy chooses, but only when the `pass` bit
(`obs[41]`) is armed. The policy then picks among the legal skills (`shoot`,
`pass`, `dribble`) with the argmax; while `finishArmed`, `stand` and `walk`
are masked for the striker.

**Training and dataset generation.** `pass_armed` is the output of
`smash_or_pass` (RL repository, `multiagent_teacher.py`), a port of B-Human's
`PlayBall::smashOrPass`:

1. Own value = shot opening with obstacles + 0.3 × `can_score_now`.
2. If a pass was committed less than 1.5 s ago
   (`LAST_PASS_TARGET_LIFETIME_MS = 1500`), the own value is reduced by a
   decision penalty that falls linearly from 0.20 (ball at 500 mm) to 0.05
   (ball at 2500 mm). The same penalty is applied to any pass target other
   than the previous one, which prevents switching back and forth.
3. If the own value is above `SHOOT_THRESHOLD`, the striker shoots. The
   default is 0.55; the v3/v4 dataset scripts and the v4.2 training script
   set it to 0.30 (`RL_MA_SHOOT_THRESHOLD`).
4. Otherwise each teammate in pass range (400–4000 mm) with a clear lane (no
   opponent in front closer than 800 mm) is rated `clamp(x / 4500, 0, 1)`. The
   best one is chosen only if its rating exceeds the own value by 0.15
   (`PASS_VALUE_MARGIN`).

**On the robot** (`computeStrikerPassArmed`), `pass` is armed when all of the
following hold:

- the robot is the striker and `finishArmed` (`shootArmed` or `dribbleArmed`);
- the shot opening with obstacles is below 0.55;
- a non-goalkeeper teammate is more than 300 mm ahead in x. The most advanced
  such teammate is the pass target.

## Role-conditioned mask

`buildMergedTeamMask` reads only the encoded observation: the role one-hot
(`obs[44:47]`), `pass_armed` (`obs[41]`), `observe_armed` (`obs[42]`), the ball
x in robot coordinates (`obs[3]`), and the distance to the nearest opponent in
front (`obs[21]`, both normalized by 4500 mm). `stand` and `walk` are always
legal.

| Skill | Striker | Open support | Off-ball support |
| --- | --- | --- | --- |
| `shoot` | `shootArmed` | — | — |
| `pass` | `passArmed` | — | — |
| `dribble` | `dribbleArmed` | — | `dribbleArmed` |
| `block` | — | `dribbleArmed` | ball ahead with 0 < x < 1800 mm (robot frame), or `dribbleArmed` |
| `mark` | — | — | opponent in front closer than 1500 mm |
| `observe` | — | `observeArmed` | `observeArmed` |

- **Block priority.** If the off-ball support selects `mark` while `block` is
  legal and the ball is nearer than the opponent in front, `block` is executed
  instead.
- **`observe`.** The skill is part of the action space; the current team
  context does not arm it (`observe_armed = 0`).
- **No role.** If no role bit is set (for example a zero-padded 26-value
  observation), the single-agent gate applies: `shoot` and `dribble` follow
  their armed bits, and both mask `stand` and `walk` once either is armed.

The mask is the C++ port of `gate_skill_mask_from_obs` (RL repository,
`rlfootball/core/gate.py`). Because it depends only on stored observation
columns, PPO recomputes exactly the same mask at update time.
