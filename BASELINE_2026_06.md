# 2026 Match Baseline

This branch's current best match baseline is `newBallDetection`.

The recommended runtime configuration after field play is:
- Scenario: `4v4_Full`
- Ball detector: `YoloBallDetector`
- Model: `Config/NeuralNets/BallDetector/yolo_ball.onnx`
- Tracking stack: `BallPerceptFilter` + `BallStateEstimator`
- Streaming: `CameraStreamer` enabled on ports `7777` and `7778`

What this means in practice:
- Ball inference runs on the NAO.
- The resulting `BallPercept` is filtered before world-model integration.
- Ball tracking and motion consistency are handled by the existing Kalman-based estimator in `BallStateEstimator`.

Additional utility scenarios kept in this branch:
- `Config/Scenarios/4v4_Full_BaselineStream`
  Keeps the classic baseline perception stack with `CameraStreamer` enabled for watcher-based comparisons.
- `Config/Scenarios/4v4_Full_BaselineBridgeStream`
  Routes `BallPercept` through `YoloBallBridge` for external detector experiments while preserving the downstream tracking stack.

Operational note:
- Team numbers, colors, and player numbers should be set at deploy time, not committed as the branch default.
