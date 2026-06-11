#!/usr/bin/env python3
"""Record and preview NAO camera streams from the CAMF protocol.

This module connects directly to the robot camera stream ports used by
BallDetectorTL (upper 7777, lower 7778). Frames arrive as standard JPEG,
so recording can stay lightweight and avoid OpenCV on the PC.
"""

from __future__ import annotations

import shutil
import socket
import struct
import subprocess
import threading
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Optional

MAGIC = b"CAMF"
BALL_META_SIZE = 13
PATCH_META_SIZE = 1
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


def connect_camf_stream(ip: str, port: int, timeout: float = 15.0) -> socket.socket:
    deadline = time.time() + timeout
    last_error: Exception | None = None
    while time.time() < deadline:
        sock: socket.socket | None = None
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            sock.connect((ip, port))
            sock.settimeout(2.0)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            return sock
        except OSError as exc:
            last_error = exc
            if sock is not None:
                try:
                    sock.close()
                except OSError:
                    pass
            time.sleep(0.5)
    raise ConnectionError(f"No se pudo conectar a {ip}:{port}: {last_error}")


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Robot desconectado")
        buf += chunk
    return buf


def read_camf_frame(sock: socket.socket):
    """Return `(jpeg, status, bx, by, br, spots)` from the CAMF stream."""
    buf = b""
    while True:
        chunk = sock.recv(1)
        if not chunk:
            raise ConnectionError("Desconectado durante sincronización")
        buf += chunk
        if buf[-4:] == MAGIC:
            break
        if len(buf) > 4096:
            buf = buf[-4:]

    size = struct.unpack("<I", _recv_exact(sock, 4))[0]
    if size <= 0 or size > 2_000_000:
        raise ValueError(f"Tamaño JPEG inválido: {size}")
    jpeg = _recv_exact(sock, size)

    meta = _recv_exact(sock, BALL_META_SIZE)
    status, bx, by, br = struct.unpack("<Bfff", meta)

    n_spots = struct.unpack("B", _recv_exact(sock, 1))[0]
    spots = []
    if n_spots:
        raw = _recv_exact(sock, n_spots * 8)
        for idx in range(n_spots):
            spots.append(struct.unpack_from("<ii", raw, idx * 8))

    patch_valid = struct.unpack("B", _recv_exact(sock, PATCH_META_SIZE))[0]
    if patch_valid:
        patch_size = struct.unpack("<H", _recv_exact(sock, 2))[0]
        _recv_exact(sock, patch_size * patch_size * 3 * 4)

    return jpeg, int(status), float(bx), float(by), float(br), spots


def _encode_avi_from_jpeg(frame_dir: Path, avi_path: Path, fps: int = TARGET_FPS):
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
        raise RuntimeError("encoder produjo un AVI vacío")


def _expand_frames_to_timeline(
    raw_frames: list[bytes],
    captured_at: list[float],
    start_at: float,
    stop_at: float,
    fps: int,
) -> list[bytes]:
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
    port: int
    path: Optional[Path] = None
    frames: int = 0
    error: Optional[str] = None
    first_frame: threading.Event = field(default_factory=threading.Event)
    lock: threading.Lock = field(default_factory=threading.Lock)
    stop_event: threading.Event = field(default_factory=threading.Event)
    thread: Optional[threading.Thread] = None
    started_at: Optional[float] = None
    stopped_at: Optional[float] = None
    captured_at: list[float] = field(default_factory=list)
    jpeg_frames: list[bytes] = field(default_factory=list)


class BallDetectorStreamRecorder:
    def __init__(self, output_dir: Optional[Path] = None, record_fps: float = TARGET_FPS):
        self.recording = False
        self.robot_id: Optional[str] = None
        self.robot_ip: Optional[str] = None
        self.output_dir = output_dir or _desktop_recordings_dir()
        self.record_fps = record_fps
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._states = {
            "Upper": _CameraState("Upper", 7777),
            "Lower": _CameraState("Lower", 7778),
        }

    def _reset_states(self):
        for state in self._states.values():
            state.path = None
            state.frames = 0
            state.error = None
            state.first_frame.clear()
            state.stop_event.clear()
            state.thread = None
            state.started_at = None
            state.stopped_at = None
            state.captured_at.clear()
            state.jpeg_frames.clear()

    def _worker(self, robot_ip: str, state: _CameraState):
        sock: Optional[socket.socket] = None
        while not self._stop_event.is_set() and not state.stop_event.is_set():
            try:
                if sock is None:
                    sock = connect_camf_stream(robot_ip, state.port)
                    state.error = None
                jpeg, _status, _bx, _by, _br, _spots = read_camf_frame(sock)
                with state.lock:
                    if state.path is None:
                        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                        state.path = self.output_dir / f"{self.robot_id}_{state.name.lower()}_{timestamp}.avi"
                    if state.started_at is None:
                        state.started_at = time.monotonic()
                    state.jpeg_frames.append(jpeg)
                    state.captured_at.append(time.monotonic())
                    state.frames += 1
                    state.first_frame.set()
            except (socket.timeout, TimeoutError):
                continue
            except Exception as exc:
                state.error = str(exc)
                if sock is not None:
                    try:
                        sock.close()
                    except OSError:
                        pass
                    sock = None
                time.sleep(0.5)

        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def start(self, robot_ip: str, robot_id: str, timeout: float = 12.0):
        with self._lock:
            if self.recording:
                return {"ok": False, "error": "Ya hay una grabación activa"}

            self.output_dir.mkdir(parents=True, exist_ok=True)
            self.robot_ip = robot_ip
            self.robot_id = robot_id
            self._stop_event.clear()
            self._reset_states()
            self.recording = True

            start_time = time.monotonic()
            for state in self._states.values():
                state.started_at = start_time
                state.thread = threading.Thread(
                    target=self._worker,
                    args=(robot_ip, state),
                    daemon=True,
                    name=f"camrec-{state.name.lower()}",
                )
                state.thread.start()

        deadline = time.time() + timeout
        while time.time() < deadline:
            errors = [state.error for state in self._states.values() if state.error]
            if errors:
                err = errors[0]
                self.stop()
                return {"ok": False, "error": err}
            if all(state.first_frame.is_set() for state in self._states.values()):
                return {
                    "ok": True,
                    "upper": str(self._states["Upper"].path),
                    "lower": str(self._states["Lower"].path),
                }
            time.sleep(0.05)

        self.stop()
        return {"ok": False, "error": "No llegaron frames del stream desde Juan"}

    def stop(self):
        stop_time = time.monotonic()
        with self._lock:
            if not self.recording and self.robot_ip is None:
                return {"upper_frames": 0, "lower_frames": 0}
            self._stop_event.set()
            for state in self._states.values():
                state.stop_event.set()
            self.recording = False
            robot_ip = self.robot_ip
            self.robot_ip = None

        for state in self._states.values():
            if state.thread and state.thread.is_alive():
                state.thread.join(timeout=3.0)
            with state.lock:
                if state.path is not None and state.jpeg_frames:
                    frame_dir = self.output_dir / f".{state.path.stem}_frames"
                    try:
                        frame_dir.mkdir(parents=True, exist_ok=True)
                        started_at = state.started_at if state.started_at is not None else (
                            state.captured_at[0] if state.captured_at else stop_time
                        )
                        state.stopped_at = stop_time
                        expanded_frames = _expand_frames_to_timeline(
                            state.jpeg_frames,
                            state.captured_at,
                            started_at,
                            state.stopped_at,
                            int(round(self.record_fps)),
                        )
                        for idx, jpeg in enumerate(expanded_frames):
                            (frame_dir / f"frame_{idx:06d}.jpg").write_bytes(jpeg)
                        _encode_avi_from_jpeg(frame_dir, state.path, fps=int(round(self.record_fps)))
                    except Exception as exc:
                        state.error = str(exc)
                    finally:
                        shutil.rmtree(frame_dir, ignore_errors=True)
                state.thread = None

        return {
            "upper_frames": self._states["Upper"].frames,
            "lower_frames": self._states["Lower"].frames,
            "upper_path": str(self._states["Upper"].path) if self._states["Upper"].path and self._states["Upper"].path.exists() else "",
            "lower_path": str(self._states["Lower"].path) if self._states["Lower"].path and self._states["Lower"].path.exists() else "",
            "robot_ip": robot_ip or "",
        }

    def status(self):
        return {
            "recording": self.recording,
            "robot_id": self.robot_id,
            "robot_ip": self.robot_ip,
            "output_dir": str(self.output_dir),
            "upper_frames": self._states["Upper"].frames,
            "lower_frames": self._states["Lower"].frames,
        }
