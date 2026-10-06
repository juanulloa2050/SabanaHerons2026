# KeyboardControl / WebControl

Tools for driving NAO robots that run the B-Human framework from a terminal, a phone or any browser on the local network, and for viewing, recording and labeling the robot camera streams.

- `keyboard_control.py` speaks the B-Human debug protocol (TCP 9999) and injects `MotionRequest` / `HeadMotionRequest` into the running code. It also works standalone as a terminal teleoperation tool.
- `web_control.py` is a FastAPI/Uvicorn server (HTTP + WebSocket, port 8080) with a touch UI. It reuses `BHumanController` from `keyboard_control.py` and reads the camera streams served by the `CameraStreamer` module.
- `15_watch_nao.py` is an OpenCV viewer for the same camera streams with ball-detection overlay, robot CPU/RAM/temperature monitor and manual dataset capture.

## Architecture

```
Phone / browser
   |  HTTP GET /                      (embedded single-page UI)
   |  WS   /ws                        (robot selection, commands, recording)
   |  WS   /camera/<robot>/<upper|lower>   (base64 JPEG frames)
   v
web_control.py  (FastAPI + Uvicorn, 0.0.0.0:8080)
   |-- BHumanController (keyboard_control.py) --TCP 9999--> B-Human debug server
   |       Cognition: representation:MotionRequest, representation:HeadMotionRequest
   |       Motion:    representation:OdometryDataPreview, OdometryTranslationRequest,
   |                  module:WalkToBallAndKickEngine:ignoreBall{Timestamp,Odometry}
   |
   `-- BallDetectorStreamRecorder / live preview --TCP 7777 (upper)--> CameraStreamer
                                                 --TCP 7778 (lower)--> CameraStreamer
                                                                          (on the NAO)
15_watch_nao.py --TCP 7777/7778--> CameraStreamer
                --SSH nao@<ip>---> /proc/stat, /proc/meminfo, thermal zones
```

## Requirements

| Component | Needs |
|-----------|-------|
| `keyboard_control.py` | Python 3.10+, standard library only (uses `termios`, so Linux/macOS) |
| `web_control.py` | `fastapi`, `uvicorn[standard]` |
| Camera recording (web UI) | `gst-launch-1.0` with `jpegparse` and `avimux` (Ubuntu: `gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad`) |
| `15_watch_nao.py` | `opencv-python`, `numpy`, an `ssh` client and the robot key `Install/Keys/id_rsa_nao` |
| Robot side | B-Human code deployed with a scenario that runs `CameraStreamer` (see below) |

Install everything into `Util/KeyboardControl/.venv` (do this on an internet-connected network, then switch back to the robot network):

```bash
Util/KeyboardControl/setup_nao_watcher_env.sh
```

The script creates the venv with `--system-site-packages`, installs `requirements.txt` and checks that `cv2`, `numpy`, `fastapi` and `uvicorn` import. If pip is not available, it prints the equivalent `apt` packages. Alternatively: `pip install -r Util/KeyboardControl/requirements.txt`.

## Network setup (adapt to your team)

The robot list, subnet and defaults are written for our field network `192.168.49.x`. Before using the tools on another network, edit:

- `ROBOTS` in `web_control.py`: id, display name and IP of every robot shown in the UI.
- The address used to detect the laptop IP when printing the URL (`192.168.49.1` in the `__main__` block of `web_control.py`). If it is not routable, the server still starts and prints `localhost`.
- The default robot IP of the watcher: `NAO_WATCHER_IP` in `watcher.env` (fallback `192.168.49.2` in `watch_nao.sh` and `15_watch_nao.py`).

The web server listens on `0.0.0.0:8080` with no authentication; any device on the network can control the robots.

## Usage

### Terminal teleoperation

```bash
python3 Util/KeyboardControl/keyboard_control.py <robot_ip>
```

Movement keys are toggles: press once and the command is resent every 50 ms until `V`. Other keys are one-shot. When idle, an empty heartbeat packet is sent every 500 ms. `Ctrl+C` / `Ctrl+D` sends a stop burst and disconnects.

| Key | Action | Key | Action |
|-----|--------|-----|--------|
| `W` / `S` | walk forward / backward | `I` / `K` | head up / down |
| `A` / `D` | strafe left / right | `J` / `L` | head left / right |
| `Q` / `E` | turn left / right | `H` | head center |
| `F` / `G` | diagonal forward-left / forward-right | `X` | sit (play dead) |
| `V` | stop (stand, sent 5 times) | `Z` / `C` | kick left / right foot |

### Web control

```bash
Util/KeyboardControl/.venv/bin/python3 Util/KeyboardControl/web_control.py
```

The script takes no arguments and prints `http://<laptop_ip>:8080`. Open it on a phone in the same network, tap a robot (the server connects on demand) and use the control screen:

- **Movement**: hold-to-move pad (forward, backward, strafe, turn, diagonals). Releasing the button sends a stop burst; `STOP` is always available. Closing the page also stops the selected robot.
- **Actions**: kick left/right, sit, stand.
- **Recording**: records both cameras of the selected robot (see below).
- **Live camera**: collapsible upper/lower camera preview.
- **Head control**: collapsible pan/tilt pad.

Each browser tab controls one robot at a time; several tabs or phones can control different robots in parallel.

### Camera recording

The recording button opens the CAMF streams on ports 7777/7778, buffers the JPEG frames in memory and, on stop, writes one AVI per camera at a constant 15 fps (frames are resampled onto a wall-clock timeline). Files are named `<robot>_<upper|lower>_<YYYYmmdd_HHMMSS>.avi` and saved to `$NAO_WATCHER_SAVE_DIR` if set, otherwise `~/Desktop/sabanaherons_recordings`. If GStreamer fails, its output is written to `/tmp/nao_recording_debug.log`.

### NAO watcher and dataset capture

```bash
Util/KeyboardControl/watch_nao.sh                                   # defaults from watcher.env
Util/KeyboardControl/watch_nao.sh --ip 192.168.49.5 --camera lower --no-monitor
```

`watch_nao.sh` loads `watcher.env` (copy it from `watcher.env.example`; it is git-ignored), then runs `15_watch_nao.py` from the venv with `--ip`, `--camera`, `--scale` and `--key`. Extra flags are passed through. Options of `15_watch_nao.py`:

| Flag | Default | Meaning |
|------|---------|---------|
| `--ip` | `$NAO_WATCHER_IP` or `192.168.49.2` | robot address |
| `--camera {upper,lower,dual}` | `$NAO_WATCHER_CAMERA` or `dual` | dual shows both cameras side by side |
| `--scale N` | `$NAO_WATCHER_SCALE` or `1` | integer display scale |
| `--port-upper` / `--port-lower` | `7777` / `7778` | CAMF ports |
| `--key PATH` | `$NAO_WATCHER_SSH_KEY` or `Install/Keys/id_rsa_nao` | SSH key for the monitor |
| `--no-monitor` | off | disable the SSH CPU/RAM/temperature poll (every 3 s) |
| `--no-spots` | off | start with the BallSpots overlay hidden |
| `--window-name`, `--window-x/-y/-w/-h`, `--fullscreen` | | OpenCV window placement |

The overlay shows the `BallPercept` (green = seen, yellow = guessed, with radius), the `BallSpots` and a square preview of the box that will be annotated.

| Key | Action |
|-----|--------|
| `q` / `Esc` | quit (prints a summary of saved frames) |
| `s` | screenshot of every active camera to the current directory (`nao_<cam>_NNNN.jpg`) |
| `b` | toggle BallSpots overlay |
| `t` | save current frame as positive; one Pascal VOC box labeled `trionda` per BallSpot |
| `n` | save current frame as negative (image only) |
| `r` / `f` | box half-size +5 / -5 px (default 40, range 8-150) |

In dual mode `t`/`n` save both cameras. Sessions are written to `manual_<timestamp>_<camera>/{images,annotations}` under `$NAO_WATCHER_SAVE_DIR`, or `Util/KeyboardControl/data/sessions` when the variable is unset. Positive frames without BallSpots are saved without an XML file.

## Robot side

**Debug protocol (port 9999).** `BHumanController` performs the `RemoteConsole` handshake and sends `idThread` + `idDebugDataChangeRequest` messages, the same mechanism SimRobot uses for `set representation:...`. Message IDs (`idFrameBegin` 1, `idCameraImage` 5, `idJPEGImage` 10, `idDebugDataChangeRequest` 80, `idDebugRequest` 85, `idThread` 95) and the binary layout of `MotionRequest` and `HeadMotionRequest` are hard-coded and mirror `Src/Libs/Streaming/MessageIDs.h`, `Src/Representations/MotionControl/MotionRequest.h` and `HeadMotionRequest.h`. Any change to those headers must be mirrored in `keyboard_control.py`; a layout mismatch fails silently.

- Walking uses `walkAtRelativeSpeed` with speeds in [-1, 1].
- Kicks use `walkToBallAndKick` with `forwardFastLeft/Right` and a virtual ball 157 mm ahead, ±50 mm to the side; the target direction cancels the ±3° `rotationOffset` from `kickInfo.cfg`. The robot is told to stand again 2 s later. For this, `WalkToBallAndKickEngine` exposes `MODIFY("module:WalkToBallAndKickEngine:ignoreBallTimestamp")` and `ignoreBallOdometry`, which the controller enables on connect and before every kick.
- Before head commands, `OdometryDataPreview` and `OdometryTranslationRequest` are zeroed in the Motion thread, so `HeadMotionEngine` does not add an odometry compensation to the pan after the robot has turned (otherwise "head center" ends up at a joint limit).

**CameraStreamer (ports 7777/7778).** `Src/Modules/Infrastructure/CameraStreamer` runs in the Upper and Lower threads, provides `CameraStream`, and serves a non-blocking TCP stream per camera. It is enabled in `threads.cfg` of `Default` and most scenarios under `Config/Scenarios/`; ports and the `enabled` switch are set in `Config/Scenarios/<scenario>/cameraStreamer.cfg`. Frame layout (little-endian):

```
"CAMF" | uint32 jpegSize | JPEG bytes
| uint8 ballStatus (0 notSeen, 1 seen, 2 guessed) | float32 x | float32 y | float32 radius
| uint8 numSpots (<= 50) | numSpots x (int32 x, int32 y)
| uint8 patchValid [ | uint16 patchSize | patchSize*patchSize*3 float32 ]
```

## Other files in this folder

| File | Purpose |
|------|---------|
| `ball_detector_stream_recorder.py` | CAMF client (`connect_camf_stream`, `read_camf_frame`) and the two-camera AVI recorder used by `web_control.py` |
| `dual_camera_recorder.py` | Alternative recorder that captures `representation:JPEGImage` through the debug connection (port 9999) instead of CAMF; not used by the web UI |
| `setup_nao_watcher_env.sh` | Creates `.venv` and installs `requirements.txt` |
| `watch_nao.sh` | Launcher for `15_watch_nao.py` with defaults from `watcher.env` |
| `watcher.env.example` | Template for `watcher.env` (`NAO_WATCHER_IP`, `_CAMERA`, `_SCALE`, `_SAVE_DIR`, `_SSH_KEY`, `_VENV`, `_PYTHON`) |
| `requirements.txt` | Python dependencies for the web server and the watcher |
