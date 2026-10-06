#!/usr/bin/env python3
"""Dual camera recorder for B-Human Nao robots.

This recorder reuses the WebControl BHumanController connection, enables
`representation:JPEGImage` on Upper and Lower, stores JPEG frames temporarily,
and builds one AVI per camera on the PC.
"""

from __future__ import annotations

import shutil
import struct
import subprocess
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from keyboard_control import BHumanController

TARGET_FPS = 15
DEBUG_LOG = Path("/tmp/nao_recording_debug.log")


def _desktop_recordings_dir() -> Path:
    desktop = Path.home() / "Desktop"
    preferred = desktop / "sabanaherons_recordings"
    legacy = desktop / "SabanaHerons_Recordings"
    if preferred.exists():
        return preferred
    if legacy.exists():
        return legacy
    return preferred


def _extract_jpeg_image(payload: bytes) -> tuple[int, int, int, bytes]:
    if len(payload) < 16:
        raise ValueError("payload JPEGImage demasiado corto")
    width = struct.unpack_from("<i", payload, 0)[0]
    height = struct.unpack_from("<i", payload, 4)[0]
    timestamp = struct.unpack_from("<I", payload, 8)[0] & ~(1 << 31)
    size = struct.unpack_from("<I", payload, 12)[0]
    if width <= 0 or height <= 0 or size <= 0:
        raise ValueError("metadata JPEGImage invalida")
    jpeg = payload[16:]
    if len(jpeg) < size:
        raise ValueError("payload JPEGImage truncado")
    return width, height, timestamp, jpeg[:size]


def _encode_avi_from_jpeg(frame_dir: Path, avi_path: Path, fps: int = 15):
    cmd = [
        "gst-launch-1.0",
        "-q",
        "multifilesrc",
        f"location={frame_dir / 'frame_%06d.jpg'}",
        "index=0",
        f"caps=image/jpeg,framerate={fps}/1",
        "!",
        "jpegparse",
        "!",
        "avimux",
        "!",
        "filesink",
        f"location={avi_path}",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        DEBUG_LOG.write_text(
            "COMMAND:\n"
            + " ".join(cmd)
            + "\n\nSTDOUT:\n"
            + result.stdout
            + "\n\nSTDERR:\n"
            + result.stderr
        )
        if avi_path.exists():
            try:
                avi_path.unlink()
            except OSError:
                pass
        raise subprocess.CalledProcessError(
            result.returncode,
            cmd,
            output=result.stdout,
            stderr=result.stderr,
        )
    if not avi_path.exists() or avi_path.stat().st_size == 0:
        if avi_path.exists():
            try:
                avi_path.unlink()
            except OSError:
                pass
        raise RuntimeError("encoder produjo un AVI vacio")


def _expand_frames_to_timeline(raw_frames: list[bytes], captured_at: list[float], start_at: float, stop_at: float, fps: int) -> list[bytes]:
    if not raw_frames:
        return []
    if stop_at <= start_at:
        stop_at = start_at + (1.0 / fps)

    out: list[bytes] = []
    frame_index = 0
    total_frames = max(1, int(round((stop_at - start_at) * fps)))
    for idx in range(total_frames):
        sample_time = start_at + (idx / fps)
        while frame_index + 1 < len(captured_at) and captured_at[frame_index + 1] <= sample_time:
            frame_index += 1
        out.append(raw_frames[frame_index])
    return out


@dataclass
class _CameraState:
    name: str
    path: Optional[Path] = None
    frames: int = 0
    error: Optional[str] = None
    first_frame: threading.Event = field(default_factory=threading.Event)
    lock: threading.Lock = field(default_factory=threading.Lock)
    started_at: Optional[float] = None
    stopped_at: Optional[float] = None
    captured_at: list[float] = field(default_factory=list)
    jpeg_frames: list[bytes] = field(default_factory=list)


class DualCameraRecorder:
    def __init__(self):
        self.recording = False
        self.robot_id: Optional[str] = None
        self.output_dir = _desktop_recordings_dir()
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._controller: Optional[BHumanController] = None
        self._callback = None
        self._states = {
            "Upper": _CameraState("Upper"),
            "Lower": _CameraState("Lower"),
        }

    def _reset_states(self):
        for state in self._states.values():
            state.path = None
            state.frames = 0
            state.error = None
            state.started_at = None
            state.stopped_at = None
            state.first_frame.clear()
            state.captured_at.clear()
            state.jpeg_frames.clear()

    def _make_callback(self):
        def on_frame(thread_name: str, payload: bytes):
            if self._stop_event.is_set():
                return
            state = self._states.get(thread_name)
            if state is None:
                return
            try:
                _width, _height, _timestamp, jpeg = _extract_jpeg_image(payload)
                with state.lock:
                    if state.path is None:
                        timestamp = time.strftime("%Y%m%d_%H%M%S")
                        filename = f"{self.robot_id}_{state.name.lower()}_{timestamp}.avi"
                        state.path = self.output_dir / filename
                    if state.started_at is None:
                        state.started_at = time.monotonic()
                    state.jpeg_frames.append(jpeg)
                    state.captured_at.append(time.monotonic())
                    state.frames += 1
                    state.first_frame.set()
            except Exception as exc:
                state.error = str(exc)

        return on_frame

    def start(self, controller: BHumanController, robot_id: str, timeout: float = 8.0):
        with self._lock:
            if self.recording:
                return {"ok": False, "error": "Ya hay una grabacion activa"}
            if not controller or not controller.connected:
                return {"ok": False, "error": f"{robot_id} no esta conectado"}

            self.output_dir.mkdir(parents=True, exist_ok=True)
            self._controller = controller
            self.robot_id = robot_id
            self._stop_event.clear()
            self._reset_states()
            start_time = time.monotonic()
            for state in self._states.values():
                state.started_at = start_time
            self._callback = self._make_callback()
            self._controller.add_jpeg_callback(self._callback)
            self._controller.start_jpeg_stream()
            self.recording = True

        deadline = time.time() + timeout
        while time.time() < deadline:
            if any(state.error for state in self._states.values()):
                err = next(state.error for state in self._states.values() if state.error)
                self.stop()
                return {"ok": False, "error": err}
            if self._states["Upper"].first_frame.is_set() and self._states["Lower"].first_frame.is_set():
                return {
                    "ok": True,
                    "upper": str(self._states["Upper"].path),
                    "lower": str(self._states["Lower"].path),
                }
            time.sleep(0.05)

        self.stop()
        return {"ok": False, "error": "No llegaron frames JPEG desde Juan"}

    def stop(self):
        stop_time = time.monotonic()
        with self._lock:
            if not self.recording and self._controller is None:
                return {"upper_frames": 0, "lower_frames": 0}
            self._stop_event.set()
            controller = self._controller
            callback = self._callback
            self._controller = None
            self._callback = None
            self.recording = False

        if controller is not None:
            try:
                controller.stop_jpeg_stream()
            except Exception:
                pass
            if callback is not None:
                try:
                    controller.remove_jpeg_callback(callback)
                except Exception:
                    pass

        for state in self._states.values():
            with state.lock:
                if state.path is not None and state.jpeg_frames:
                    try:
                        frame_dir = self.output_dir / f".{state.path.stem}_frames"
                        frame_dir.mkdir(parents=True, exist_ok=True)
                        started_at = state.started_at if state.started_at is not None else (state.captured_at[0] if state.captured_at else stop_time)
                        state.stopped_at = stop_time
                        expanded_frames = _expand_frames_to_timeline(
                            state.jpeg_frames,
                            state.captured_at,
                            started_at,
                            state.stopped_at,
                            TARGET_FPS,
                        )
                        for idx, jpeg in enumerate(expanded_frames):
                            (frame_dir / f"frame_{idx:06d}.jpg").write_bytes(jpeg)
                        _encode_avi_from_jpeg(frame_dir, state.path, fps=TARGET_FPS)
                    except Exception as exc:
                        state.error = str(exc)
                    finally:
                        if state.path is not None:
                            shutil.rmtree(self.output_dir / f".{state.path.stem}_frames", ignore_errors=True)

        return {
            "upper_frames": self._states["Upper"].frames,
            "lower_frames": self._states["Lower"].frames,
            "upper_path": str(self._states["Upper"].path) if self._states["Upper"].path else "",
            "lower_path": str(self._states["Lower"].path) if self._states["Lower"].path else "",
        }

    def status(self):
        return {
            "recording": self.recording,
            "robot_id": self.robot_id,
            "output_dir": str(self.output_dir),
            "upper_frames": self._states["Upper"].frames,
            "lower_frames": self._states["Lower"].frames,
        }
