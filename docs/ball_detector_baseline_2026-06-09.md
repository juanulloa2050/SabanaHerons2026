# Ball Detector Baseline 2026-06-09

## Summary

This baseline promotes the new ONNX ball detector and integrates a lightweight image-space Kalman tracker into `YoloBallDetector`.

The detector now supports:

- configurable ONNX model paths
- dynamic ONNX input shapes read from the model at runtime
- optional lightweight image-space tracking over YOLO detections
- robot-specific rollout through `Config/Robots/<head>/<body>/yoloBallDetector.cfg`

The restart-memory set-play search changes are included in the same baseline because they depend on the current shared-ball and behavior stack.

## Model Files

- Previous shared model: `Config/NeuralNets/BallDetector/yolo_ball.onnx`
- New baseline candidate: `Config/NeuralNets/BallDetector/yolo_ball_best1.onnx`

`yolo_ball_best1.onnx` is the asset copied from `Downloads/best 1.onnx`.

## Kalman Integration

The new tracker is implemented inside `Src/Modules/Perception/BallPerceptors/YoloBallDetector.*`.

It tracks one ball target in image space with state:

- `x`
- `y`
- `vx`
- `vy`
- `w`
- `h`

Design intent:

- keep `BallStateEstimator` unchanged
- stabilize raw YOLO detections before projection to field coordinates
- preserve existing downstream behavior interfaces

## Rollout Policy

Default team behavior remains unchanged unless a robot-specific override enables the new path.

Current rollout target:

- `ospinao`

Reason:

- field player
- active robot
- lower blast radius than team-wide rollout
- explicitly avoids the goalkeeper

Robot-specific override:

- `Config/Robots/ospinao/ospinao/yoloBallDetector.cfg`

This override enables:

- `modelName = "NeuralNets/BallDetector/yolo_ball_best1.onnx"`
- `enableKalman = true`

## Operational Notes

- The ONNX input shape is no longer hardcoded. The detector reads the model input tensor dimensions and letterboxes accordingly.
- A partial robot-specific `yoloBallDetector.cfg` is not valid here. The loader expects a complete config file for the module.
- `ospinao` was deployed successfully after fixing the config and dynamic input sizing.

## Known Risk

`ospinao` no longer crashes, but the new model increases runtime load and can trigger perception timing warnings. Treat this as a functional baseline, not yet a performance-validated team-wide default.
