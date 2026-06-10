# Sabana-Herons Code Release

This is the current Sabana-Herons baseline. The code is still heavily based on the B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/), but it now includes our current HSL field, GameController, strategy, and ball-detection baseline.

The purpose of this branch is to keep the actual team baseline in one place, so the default branch contains the same code that is deployed on the robots.

## Improvements

1. **HSL 2026 migration**: The project now includes the current HSL GameController protocol and the main HSL restart behavior updates, including stop play, direct and indirect free kicks, throw-ins, goal kicks, corner kicks, penalty kicks, and updated kick-off restrictions.
2. **3v3 and 4v4 full-field strategies**: The team now has dedicated `3v3_Full` and `4v4_Full` scenarios and locations for HSL-style play, with updated field dimensions and set-play behavior.
3. **Trionda ball detection baseline**: The project baseline includes the current Trionda detector stack and the accepted far-ball baseline model named **Trionda Final Model**.
4. **Web control and operational tooling**: The codebase includes the current web control, camera streaming, and recording workflow used for robot operation and data collection.
5. **Behavior and match fixes**: The baseline also includes the current whistle, kick-off, set-play, and field-behavior fixes that were merged into the deployed branch.

## Ball Detection Baseline

The accepted final detector baseline is named:

`Trionda Final Model`

The external baseline note for that model is documented in:

`/home/limao/workspace/semillero/TRIONDA_FINAL_MODEL_BASELINE_2026-06-10.md`

## Run this code

The build and deploy flow still follows the B-Human-style workflow. In practice, the team currently uses the configured scenarios and locations inside `Config/Scenarios` and `Config/Locations`, then deploys with `Make/Common/deploy`.
