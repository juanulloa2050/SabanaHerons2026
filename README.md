# Sabana-Herons Code Release

This is the current Sabana-Herons baseline. The code is still heavily based on the B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/), but it now includes our current HSL field, GameController, strategy, and ball-detection baseline.

The purpose of this branch is to keep the actual team baseline in one place, so the default branch contains the same code that is deployed on the robots.

## Improvements

1. **HSL 2026 migration**: The project now includes the current HSL GameController protocol and the main HSL restart behavior updates, including stop play, direct and indirect free kicks, throw-ins, goal kicks, corner kicks, penalty kicks, and updated kick-off restrictions.
2. **3v3 and 4v4 full-field strategies**: The team now has dedicated `3v3_Full` and `4v4_Full` scenarios and locations for HSL-style play, with updated field dimensions and set-play behavior.
3. **Trionda ball detection baseline**: The project baseline includes the current Trionda detector stack and the accepted far-ball baseline model named **Trionda Final Model**.
4. **New whistle implementation**: The baseline includes the current whistle recognizer and tuning used in the latest deployed match baseline.
5. **Web control and operational tooling**: The codebase includes the current web control, camera streaming, and recording workflow used for robot operation and data collection.
6. **Behavior and match fixes**: The baseline also includes the current kick-off, set-play, and field-behavior fixes that were merged into the deployed branch.

## Ball Detection Baseline

The accepted final detector baseline is named:

`Trionda Final Model`

The external baseline note for that model is documented in:

`/home/limao/workspace/semillero/TRIONDA_FINAL_MODEL_BASELINE_2026-06-10.md`

## Run this code

As we said, this code is heavily based in B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/), therefore and so far, you can just follow their documentation to build and run the code.

The build and deploy flow still follows the B-Human-style workflow. In practice, the team currently uses the configured scenarios and locations inside `Config/Scenarios` and `Config/Locations`, then deploys with `Make/Common/deploy`.

## RL and common SimRobot scenes

This repository also contains the C++ side of the RL bridge used by the sibling
`RL` repo.

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
