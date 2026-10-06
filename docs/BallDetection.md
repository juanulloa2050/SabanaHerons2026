# Ball Detection

## Overview

The HSL 2026 Foundation league plays with a FIFA-style mini ball. The ball we
use has the Adidas Trionda pattern: large curved patches of saturated colors
on a white base. The B-Human 2023 perception was built for the SPL
black-and-white ball. Its candidate search ([BallSpotsProvider](../Src/Modules/Perception/BallPerceptors/BallSpotsProvider.cpp),
[CNSBallSpotsProvider](../Src/Modules/Perception/BallPerceptors/CNSBallSpotsProvider.cpp),
[BOPPerceptor](../Src/Modules/Perception/MultiPerceptors/BOPPerceptor.cpp))
and its grayscale patch classifier ([BallPerceptor](../Src/Modules/Perception/BallPerceptors/BallPerceptor.cpp))
did not find this ball reliably.

First, we extended `BallPerceptor` with a color encoder and a geometric
fallback (both are described below and still available). After that, we
replaced the image-to-percept part of the pipeline with a single-class YOLO
detector. The detector runs on the NAO with ONNX Runtime, works on the
full camera image, and provides `BallPercept` directly. Downstream of
`BallPercept`, we use the B-Human modeling stack.

In [Locations/4v4_Full/ballSpecification.cfg](../Config/Locations/4v4_Full/ballSpecification.cfg),
the ball radius is 65 mm (B-Human: 50 mm). This value is used for every
image-to-field projection.

## Pipeline

Match configuration (scenarios `4v4_Full` / `4v4_Complete`, location
`4v4_Full`). The same chain runs in the `Upper` and `Lower` threads:

```text
CameraImage (YUYV; upper 640x480, lower 320x240)
   |
   +--> YoloBallDetector ............ camera thread: copy frame, read latest result
   |       |  (frame handoff)
   |       v
   |    background thread: YUYV -> RGB letterbox -> ONNX Runtime (YOLO)
   |       -> candidate boxes -> optional image-space Kalman tracker
   |       |
   |       v
   |    BallPercept (seen / notSeen), RawBallPatch (always empty)
   |
   +--> TriondaBallSpotsProvider --> BallSpots   (only for the camera stream)
   |
   +--> CameraStreamer (JPEG + BallPercept + BallSpots + RawBallPatch over TCP)

Cognition thread:
BallPercept --> BallPerceptFilter --> FilteredBallPercepts
            --> BallStateEstimator --> BallModel --> behavior, team communication
```

In the match configuration nothing in perception requires `BallSpots`.
`TriondaBallSpotsProvider` runs only because `CameraStreamer` requires it.
The spots appear as overlays and labeling hints in our camera watcher (see
[Data collection](#data-collection-and-training)).

## Modules

All perception modules are in [Src/Modules/Perception/BallPerceptors](../Src/Modules/Perception/BallPerceptors).
One instance of each runs per camera thread.

### YoloBallDetector (match detector)

[YoloBallDetector.h](../Src/Modules/Perception/BallPerceptors/YoloBallDetector.h) /
[.cpp](../Src/Modules/Perception/BallPerceptors/YoloBallDetector.cpp).
Requires `BallSpecification`, `CameraImage`, `CameraInfo` and `CameraMatrix`.
Provides `BallPercept` and `RawBallPatch`.

- **Asynchronous inference.** The constructor loads the model and starts a
  background thread. Each camera frame, `update()` copies the raw YUYV
  buffer together with the `CameraInfo` and `CameraMatrix` of that frame,
  wakes the background thread, and reads the most recent result. The camera
  thread never waits for inference. After each inference the background
  thread sleeps `inferenceIntervalMs`. The model therefore runs at most once
  per (inference time + `inferenceIntervalMs`) per camera.
- **Preprocessing.** YUYV is converted to RGB with the full-swing BT.601
  formula that libjpeg uses when it decodes YCbCr JPEGs. As a result, the
  network sees the same pixel values as on the JPEG frames from
  `CameraStreamer` that we train on. The image is letterboxed
  (nearest-neighbor, gray padding 0.5) into the input size of the model,
  which is read from the ONNX file at load time.
- **Decoding.** The output is `[1, 5, N]` (`cx, cy, w, h, conf` in model
  pixels, no NMS in the graph). Boxes are mapped back to camera pixels. The
  ball radius in the image is `(w + h) / 4`.
- **Selection.** If `enableKalman` is false, the box with the highest
  confidence is used if it is above `upperConf` / `lowerConf`. If
  `enableKalman` is true, every box with a confidence of at least
  `min(conf threshold, kalmanInitConf, kalmanActiveConf)` goes to the tracker.
  In practice this means `kalmanActiveConf` = 0.18, so the per-camera
  `upperConf` / `lowerConf` thresholds have no effect in this mode.
- **Image-space tracker** (`LightweightBallKalman`). It follows one target
  with the state `(x, y, vx, vy, w, h)` in camera pixels and a
  constant-velocity model with one step per inference.
  - A track starts on one box with confidence ≥ `kalmanInstantInitConf`, or
    after `kalmanInitConfirmFrames` boxes ≥ `kalmanInitConf` that are within
    `kalmanInitGatePx` of each other.
  - While a track is active, each candidate ≥ `kalmanActiveConf` is scored
    as confidence minus a distance penalty. Candidates must lie inside the
    gate `max(kalmanFarGatePx, size · kalmanNearGateScale)`.
  - A jump outside the gate ("relock") needs a confidence of 0.45, or 0.55
    after `kalmanStrongPredictionMissed` misses.
  - On a miss, the velocity decays by `kalmanMissedVelocityDecay`. The track
    is dropped after `kalmanMaxMissed` misses. Speed is clamped to
    `kalmanMaxSpeedPx`.
  - **Egomotion compensation.** Before each prediction, the tracked position
    is projected to the field with the camera matrix of the previous frame
    and back into the image with the current one. The resulting shift is
    removed from the velocity, so head and body rotation do not look like
    ball motion. If the projection fails (for example, the ball left the
    image), the velocity is reset and the gate is widened so that the next
    real detection can re-lock the track.
  - Only measured states are published. Predicted-only states are published
    only if `publishPredictedPercepts` is true.
- **Projection.** The image position is projected to the horizontal plane at
  ball-center height, using the camera matrix of the frame on which the
  network ran. For the upper camera there are two fallbacks: projection
  onto the ground plane, and a pinhole distance estimate from the apparent
  radius (`focalLength · radius / r`).
- **Output.** A valid result that is not older than `timeoutMs` is published
  as `BallPercept::seen`. The covariance is fixed at `diag(10000, 10000)` mm².
  The detector never produces `guessed`. A per-camera minimum number of
  consecutive new detections (`upperMinConsecutive` / `lowerMinConsecutive`)
  can delay publishing. `RawBallPatch` is always invalid.

### TriondaBallSpotsProvider

[TriondaBallSpotsProvider.cpp](../Src/Modules/Perception/BallPerceptors/TriondaBallSpotsProvider.cpp)
provides `BallSpots` from color blobs. This module needs no network.

- **First pass.** A chroma gate on the YUYV image (Y 60–235, U 60–122,
  V 148–255) finds orange/red pixels.
- **Second pass.** A saturation gate on `ECImage` (saturation ≥ 60, with the
  grass hue band 80–170 excluded) adds the other colored patches.
- **Blobs.** Hits are merged greedily into blobs within `mergeRadius`
  (40 px). Blobs are sorted by size, and every blob with at least
  `minBlobPixels` hits becomes a spot.
- **Upper camera.** It scans more densely and uses lower thresholds
  (`upperScanStep`, `upperMinBlobPixels`, `upperMinSaturation`).

The parameters are compiled in (`DEFINES_PARAMETERS`); no `.cfg` file is
read.

### TriondaBallPerceptor

[TriondaBallPerceptor.cpp](../Src/Modules/Perception/BallPerceptors/TriondaBallPerceptor.cpp)
was our first, network-free Trionda detector. It accepts the largest blob
from `TriondaBallSpotsProvider` as the ball, with a fixed image radius of
30 px. No scenario wires it in. We keep it as a reference and for quick
tests.

### BallPerceptor (B-Human, extended)

No scenario in this release uses the B-Human encoder/classifier/corrector
perceptor, which runs with CompiledNN. To use it, provide `BallPercept` and
`RawBallPatch` with `BallPerceptor` and add a `BallSpots` provider. We
extended it as follows:

- **Color encoder.** If the encoder input has 3 channels, a YCrCb patch is
  taken directly from the YUYV image. Each channel is normalized separately
  (`normalizeBrightness`). The configuration uses
  [`encoder_color.h5`](../Config/NeuralNets/BallPerceptor), `classify_color.h5`
  and the stock `corrector.h5`.
- **Circle fallback.** A spot that the classifier rejects can still pass as
  `guessed` if it meets two conditions. Its gradient-circle score on
  `ECImage` must be above a threshold, and the color spread (Cr/Cb standard
  deviation) inside the circle must be large enough. After
  `circleStreakForSeen` consecutive frames at the same position, the spot
  is promoted to `seen`.
- **`RawBallPatch`.** The module provides the float patch of the best
  candidate. `CameraStreamer` appends it to the stream, so that offline
  tools receive exactly the network input.

### YoloBallBridge

[YoloBallBridge.cpp](../Src/Modules/Perception/BallPerceptors/YoloBallBridge.cpp)
provides `BallPercept` from detections that an external process sends over
TCP. The ports are 7779 (upper) and 7780 (lower). The messages have the form
`"DETS"`, `uint8 count`, then `count × {float x, y, radius, conf}` in
little-endian. Projection and output are the same as in `YoloBallDetector`.
We used it to evaluate detectors on a PC with the robot in the loop. No
scenario in this release uses it, and the PC-side sender is not part of this
repository.

### CameraStreamer

[CameraStreamer.cpp](../Src/Modules/Infrastructure/CameraStreamer/CameraStreamer.cpp)
runs a non-blocking TCP server in each camera thread. Its configuration is in
`cameraStreamer.cfg`; the upper camera uses port 7777 and the lower camera
port 7778. When at least one client is connected, it encodes the frame as a
standard YCbCr JPEG (quality 75) and sends:

```text
"CAMF" | uint32 jpegSize | JPEG
uint8 status | float x | float y | float radius        (BallPercept, image coords)
uint8 numSpots (<= 50) | numSpots x {int32 x, int32 y} (BallSpots)
uint8 patchValid [| uint16 patchSize | float32 x patchSize^2 x 3]  (RawBallPatch)
```

The send timeout is 100 ms. A stalled client therefore cannot block the
camera thread long enough to trigger the "blind" emergency sit-down.

### Modeling

[BallPerceptFilter](../Src/Modules/Modeling/BallStateEstimator/BallPerceptFilter.cpp)
and [BallStateEstimator](../Src/Modules/Modeling/BallStateEstimator/BallStateEstimator.cpp)
are B-Human's. We only added debug output to the filter (`DEBUG_RESPONSE`
`module:BallPerceptFilter:verbose` and annotations). In our
`ballPerceptFilter.cfg`, `requiredPerceptionCountNear` and
`neededSeenBallsForAcceptingGuessedOne` are 1 instead of 2. Because
`YoloBallDetector` only reports `seen`, the filter's paths for guessed balls
are not used in matches.

## Configuration

### Selecting the detector

The detector is chosen per scenario in `threads.cfg`, in both the `Upper` and
the `Lower` thread. `BallPercept` and `RawBallPatch` must come from the same
module:

| Scenario(s) | `BallPercept` / `RawBallPatch` | `BallSpots` |
| --- | --- | --- |
| `4v4_Full`, `4v4_Complete`, `4v4_NoRLBlock`, `Default`, `Tortuga`, `3v3_Full`, `3v3_Scaled`, `4v4_BaselineAttack`, `4v4_MixedAttack`, `4v4_StrikerBase`, `4v4_RL2D`, `4v4_RL3D` | `YoloBallDetector` | `TriondaBallSpotsProvider` |
| Scenarios without their own `threads.cfg` (e.g. `4v4_Scaled`, `CompetitionWalk`) | inherited from `Scenarios/Default` | |
| `AnyPlaceDemo` | `YoloBallDetector` | `CNSBallSpotsProvider` |
| `2D` | none (`BallModel` from `OracledWorldModelProvider`) | |

The scenario and location are set in [Config/settings.cfg](../Config/settings.cfg)
(`scenario = 4v4_Full; location = 4v4_Full;`) or at deploy time.

Parameter files are resolved in this order: `Robots/<head>/<body>/`, then
`Locations/<location>/`, `Scenarios/<scenario>/`, and finally the `Default`
directories. A robot-specific `yoloBallDetector.cfg` replaces the scenario
file completely, so it must contain every parameter. No robot has such an
override in this release.

### `yoloBallDetector.cfg` (match values)

The values are identical in all scenarios that run YOLO.

| Parameter | Value | Meaning |
| --- | --- | --- |
| `enabled` | true | If false, `BallPercept` is always `notSeen` (there is no fallback). |
| `modelName` | `NeuralNets/BallDetector/yolo_ball_320.onnx` | Relative to `Config/`. |
| `upperConf` / `lowerConf` | 0.40 / 0.20 | Per-camera confidence threshold (only without the tracker). |
| `upperMinConsecutive` / `lowerMinConsecutive` | 1 / 1 | New detections required before `seen` is published. |
| `timeoutMs` | 500 | Maximum age of a detection that is still published (the header default is 2000). |
| `inferenceIntervalMs` | 150 | Sleep after each inference. |
| `enableKalman` | true | Enables the image-space tracker. |
| `publishPredictedPercepts` | false | Publish track predictions without a measurement. |
| `kalmanInitConf` / `kalmanInstantInitConf` | 0.28 / 0.55 | Confirmed start / immediate start of a track. |
| `kalmanInitConfirmFrames` / `kalmanInitGatePx` | 2 / 65 | Confirmations needed to start a track, and the gate between them. |
| `kalmanActiveConf` | 0.18 | Minimum confidence to update an active track. |
| `kalmanFarGatePx` / `kalmanNearGateScale` | 55 / 2.5 | Association gate: `max(far, size · scale)`. |
| `kalmanMaxMissed` / `kalmanStrongPredictionMissed` | 15 / 3 | Misses until the track is dropped / until the gate is widened. |
| `kalmanMaxSpeedPx` / `kalmanMissedVelocityDecay` | 160 / 0.82 | Velocity clamp (px per inference) and decay per miss. |
| `kalmanProcessNoise` / `kalmanMeasurementNoise` | 12 / 18 | Kalman noise terms. |

Other files: `yoloBallBridge.cfg` (ports 7779/7780, `timeoutMs` 500),
`cameraStreamer.cfg` (ports 7777/7778, `enabled`) and `ballPerceptor.cfg`
(thresholds 0.3 / 0.4 / 0.9 for guessed / accept / ensure, the color
networks, and the circle fallback parameters).

## Model

| File | Used by | Input (N×C×H×W) | Output | Class | Exported |
| --- | --- | --- | --- | --- | --- |
| [`yolo_ball_320.onnx`](../Config/NeuralNets/BallDetector) | all YOLO scenarios, module default | 1×3×320×320 | 1×5×2100 | `trionda` | 2026-07-04 |

The model is a YOLOv8n trained with Ultralytics at 320×320 on about 22,000
images (6,779 positives labeled by five team members), exported to ONNX
(opset 12, static shape, no NMS). Its best mAP50 on our validation split
was 0.829. The 2100 candidates are the cells of the stride-8, 16, and 32
grids (40×40 + 20×20 + 10×10).

History: until June 2026 we used a 160×160 model. Small, distant balls
covered only 4–8 pixels at that resolution and were often missed, so we
moved to 320×320 input for the final games.

With the 320×320 input, the upper 640×480 image is downscaled by 0.5 and
the lower 320×240 image is used at full resolution; in both cases rows of
gray padding are added at the top and at the bottom.

ONNX Runtime 1.10.0 is in [Util/onnxruntime](../Util/onnxruntime).
`Make/Common/deploy` copies `libonnxruntime.so.1.10.0` to the robot. Each
detector instance creates its own session with one intra-op and one inter-op
thread and full graph optimization. The upper and lower cameras therefore
use two sessions and two background threads.

## Data collection and training

The tools are in [Util/KeyboardControl](../Util/KeyboardControl). To set them
up, run `setup_nao_watcher_env.sh`, which creates a virtual environment from
`requirements.txt`. Local settings go into `watcher.env` (template:
`watcher.env.example`).

The full usage is described in the folder's
[README](../Util/KeyboardControl/README.md).

- **`watch_nao.sh` / `15_watch_nao.py`** connect to the `CameraStreamer`
  ports and show both cameras. The overlay shows the `BallPercept` and the
  `BallSpots`, and a CPU/RAM/temperature monitor reads its data over SSH.
  For manual labeling:
  - `t` stores the current frame as a positive. Each `BallSpot` becomes a
    Pascal VOC box labeled `trionda`, with an adjustable half-size (`r` /
    `f`).
  - `n` stores a negative frame.
  - Sessions are written as `images/` + `annotations/` under
    `data/sessions/`, or under `NAO_WATCHER_SAVE_DIR` if it is set.
  - In match scenarios, the spots come from `TriondaBallSpotsProvider`, so
    the color-blob candidates serve as box proposals. Each frame should
    still be checked before it is saved.
- **`ball_detector_stream_recorder.py`** records the CAMF stream of both
  cameras and builds one AVI per camera. **`web_control.py`** (robot
  teleoperation in the browser) uses this recorder for its record button.
- **`dual_camera_recorder.py`** does the same through the B-Human debug
  connection (`representation:JPEGImage`).

- **`7_retrain.py`** converts the labeled sessions from `data/sessions/`
  (Pascal VOC) into a YOLO dataset, trains with Ultralytics (`pip install
  ultralytics`), and exports the ONNX model. Run it with `--help` for the
  options (sessions folder, input size, base model).

The resulting `.onnx` file is placed in `Config/NeuralNets/BallDetector/`
and selected with `modelName`. The dataset itself is not part of this
release.

## Known limitations

- **Effective rate.** Inference is throttled. Between two results, the last
  detection is published on every camera frame for up to `timeoutMs`. It is
  projected with the camera matrix of the frame it came from. When the robot
  or the ball moves, the same percept is therefore fed several times and is
  increasingly stale.
- **Fixed covariance.** The measurement covariance does not depend on
  distance or on the camera.
- **No `guessed` and no fallback.** If the model fails to load, the robot is
  blind to the ball. Watch for the `[YoloBallDetector] Cannot load model`
  message.
- **CPU load.** The 320×320 model needs roughly four times the computation
  of the 160×160 one on the NAO CPU. Inference runs in the background, so it
  lowers the detection rate rather than blocking the camera threads;
  `inferenceIntervalMs` is the main knob to trade rate for load.
- **Spot provider runs only for streaming.** `TriondaBallSpotsProvider` runs
  every frame only to feed `CameraStreamer`. Its color thresholds are fixed
  in code and were tuned for our ball and lab lighting.
- **Unused modules.** The extended `BallPerceptor` has two issues that
  matter only if it is wired in again:
  - `BallPerceptor` and `BallSpotsProvider` print unconditional per-frame
    `OUTPUT_TEXT`.
  - The color patch extraction and the color-diversity check clamp x
    coordinates to the YUYV width (half the image width), so candidates in
    the right half of the image are sampled incorrectly.
