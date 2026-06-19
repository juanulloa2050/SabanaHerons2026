# Embedded Goalkeeper RL policy — integration notes (branch `RLGK`)

This branch adds an **operational RL goalkeeper** to `StrategyBehaviorControl`, mirroring the existing
embedded striker/merged PPO path but for the goalkeeper's separate contract. Full rationale, training
pipeline and the obs/skill/mask/action contract live in the RL repo: `RL/docs/39-goalkeeper-closedloop-and-naov6-transplant.md`.

## What was added

- **`Src/Libs/RL/GKCommon.h`** — contract: `gkObsSize=64`, `gkSkillCount=12`, `gkParamCount=4`,
  `GKSkillType` (logit index == enum), `GKPolicyOutput`, `GKRawObservation`, `GKObservation`.
- **`Src/Libs/RL/GKPolicyModel.{h,cpp}`** — CompiledNN2ONNX inference (verbatim pattern of `PPOPolicyModel`,
  retargeted to 64-in / 12+4-out; accepts two outputs or one combined `[16]`).
- **`Src/Libs/RL/GKObservationEncoder.{h,cpp}`** — builds the 64-dim obs from B-Human representations and
  replicates `goalkeeper_teacher.build_gk_cognition` / `_goalie_pose` / `_time_to_own_y_axis` so **no extra
  representation is required** and the feature distribution matches training. Indices 38–63 are zero.
- **`Src/Libs/RL/GKSkillGate.{h,cpp}`** — exact port of `goalkeeper_skill_mask_np` (Design-A). Caller masks
  illegal logits to `-inf` **before** argmax.
- **`Src/Libs/RL/GKActionDecoder.{h,cpp}`** — maps the chosen skill+params to keeper `SkillRequest`s:
  `stand → stand`, `walk_goalie_pose → walkTo(pose)`, `observe → observe(point)`,
  `intercept_center → interceptBall(stand|walk, noDive)`, `intercept_lateral → interceptBall(walk, noDive)`,
  `intercept_low_l/r → keeperDive(squatArmsBackLeft/Right)`, `intercept_jump_l/r → keeperDive(jumpLeft/Right)`,
  `clear → shoot`, `pass → passTo(forward teammate)` (clear fallback), `dribble_out → dribbleTo(heading)`.
- **`StrategyBehaviorControl.{h,cpp}`** — new params `enableEmbeddedGK`, `embeddedGKModelPath`; members for
  the GK model/encoder/gate/decoder; `usesEmbeddedGK` / `ensureEmbeddedGKLoaded` / `chooseGKPassTarget` /
  `updateEmbeddedGK`. `updateEmbeddedGK(skillRequest)` is called once per frame at the end of
  `update(SkillRequest&)`. It runs **only on the keeper** (`ownTeam.isGoalkeeper(playerNumber)`), which is
  exactly the complement of `updateEmbeddedPPO` (which excludes the keeper). On field players / when
  disabled / not playing it is a no-op and the existing behavior stands.
- **`Config/NeuralNets/RLPolicy/ppo_goalkeeper_hsl2026_gk_closedloop_elite.onnx`** — the deployable head-free
  GK policy (sha256 `6475c4528f76ac8fffde07fed404a57e5d8452e9ee197821d07262597408ff62`).
- **Config**: `enableEmbeddedGK = true` + model path added to the RL scenarios
  (`Default`, `4v4_RL3D`, `4v4_Full`, `3v3_Full`). Other scenarios keep the default (`false`).

CMake needs no change — `Make/CMake/B-Human.cmake` already globs `Libs/RL/*.cpp` / `*.h`.

## Build

```bash
# Linux SimRobot:
cmake --build Build/Linux/CMake/Develop --target SimulatedNao
# Real robot: build Nao target as usual. Windows: build SimulatedNao in Develop.
```

## Verify on the build machine (could not be compiled in the authoring env)

1. **Compiles** — confirm the four new `Libs/RL/GK*.cpp` compile and link. The only external APIs used are
   the same ones `PPO*` already use (`CompiledNN`, `SkillRequest::Builder`, `MotionRequest::Dive`,
   `Interception`), so this should be clean.
2. **Model loads** — on the keeper you should see `[RL] Embedded GK model loaded ... path=...gk_closedloop_elite.onnx`.
   If CompiledNN2ONNX rejects any op, re-export is trivial (`RL/tools/export_goalkeeper_policy.py --no-head`).
3. **Behavior** — in SimRobot keeper scenarios confirm it dives (`keeperDive jump*`), genuflects
   (`squatArmsBack*`), blocks central (`interceptBall`), repositions (`walkTo`), and clears/passes when it
   has the ball. Compare against the analytical results in `RL/docs/39` (overall 93 % save; dribble L/R/round 100 %).
4. **Parity (optional but recommended)** — feed identical obs to the C++ encoder and the Python reference;
   argmax must match (Python side already verifies torch↔onnx at 3e-5).

## Known caveats (read `RL/docs/39 §4`)

- **Head-free** — there is no scripted post-processing; do not add one.
- **Ball-velocity frame** — the encoder uses `BallModel.estimate.velocity` (robot frame). Training used a
  keeper facing ≈ +x (robot frame ≈ field frame). This holds while the keeper faces upfield; it is the
  same assumption as the obs encoding and is correct for the keeper's normal orientation.
- **Stability gate** — `obs[37]` is 0 when `FallDownState::upright`, else 1 (so intercepts are only armed
  when upright — the conservative, correct semantic).
- **Reach-window calibration** — the 93 % save rate is from the analytical benchmark. Confirm dive/genuflect
  reach on the real NAOv6 before treating it as an on-robot number.
- **Pass target** — `pass` uses the most-forward teammate (or clears if none); the policy does not pick it.
