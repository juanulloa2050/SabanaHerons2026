#!/usr/bin/env python3
"""
Keyboard Control for Real Nao Robot (B-Human framework)
=======================================================
Connects to a Nao robot running B-Human software via the debug TCP protocol
(port 9999) and controls its motion using keyboard input.

Controls (same layout as KeyboardJoystick.con for SimRobot):
  Movement:          Head:
    Q W E              I
    A   D            J   L
      S                K

  V = Stand (stop)   X = Sit (play dead)
  Z = Kick left      C = Kick right
  F = Forward+Left   G = Forward+Right
  H = Head center

Usage:
  python3 keyboard_control.py <robot_ip>
  python3 keyboard_control.py 192.168.49.3

Requirements: No external dependencies (uses standard library only).
"""

import math
import socket
import struct
import sys
import termios
import threading
import tty
import time

# ── B-Human TCP port ──────────────────────────────────────────────────────────

BHUMAN_PORT = 9999

# ── Message IDs (from Src/Libs/Streaming/MessageIDs.h) ───────────────────────
# numOfDataMessageIDs = 79 in this B-Human version

ID_NUM_OF_DATA_MESSAGE_IDS = 0
ID_FRAME_BEGIN = 1
ID_FRAME_FINISHED = 2
ID_CAMERA_IMAGE = 5
ID_JPEG_IMAGE = 10
ID_THREAD                  = 95
ID_DEBUG_DATA_CHANGE_REQUEST = 80
ID_DEBUG_REQUEST = 85

# ── MotionRequest::Motion enum ────────────────────────────────────────────────

MOTION_PLAY_DEAD             = 0
MOTION_STAND                 = 1
MOTION_WALK_AT_RELATIVE_SPEED = 3
MOTION_WALK_TO_BALL_AND_KICK = 5

# ── KickInfo::KickType enum (Src/Representations/Configuration/KickInfo.h) ───
# forwardFast uses a simple position threshold check — kicks immediately in place.
# walkForwardsAlternative uses canStart() which requires a walking phase.

KICK_FORWARD_FAST_RIGHT = 0   # forwardFastRight — kick in place, right foot
KICK_FORWARD_FAST_LEFT  = 1   # forwardFastLeft  — kick in place, left foot

# rotationOffset from active kickInfo.cfg: forwardFastLeft=-3deg, forwardFastRight=+3deg
# directionThreshold=3deg (strict <). target_direction = -rotationOffset → abs = 0 < 3 ✓
KICK_LEFT_TARGET_DIR  =  3.0 * math.pi / 180.0   # +3deg cancels -3deg offset
KICK_RIGHT_TARGET_DIR = -3.0 * math.pi / 180.0   # -3deg cancels +3deg offset

# ── KickPrecision / Dive::Request / Special::Request enums ───────────────────

KICK_PRECISION_NOT_PRECISE = 0
DIVE_PREPARE               = 0
SPECIAL_DEMO_BANNER_WAVE   = 0

# ── HeadMotionRequest mode / cameraControlMode enums ─────────────────────────

HEAD_MODE_PAN_TILT   = 0   # panTiltMode
HEAD_CAMERA_AUTO     = 0   # autoCamera

# ═════════════════════════════════════════════════════════════════════════════
# Binary serialization helpers
# ═════════════════════════════════════════════════════════════════════════════

def _str(s: str) -> bytes:
    """B-Human binary string: 4-byte LE length + raw bytes (no null terminator)."""
    b = s.encode("ascii")
    return struct.pack("<I", len(b)) + b


def _v2f(x: float, y: float) -> bytes:
    return struct.pack("<ff", x, y)


def _pose2f(rotation: float, tx: float, ty: float) -> bytes:
    """Pose2f: rotation (float) + translation.x (float) + translation.y (float)."""
    return struct.pack("<fff", rotation, tx, ty)


def _mat2f_identity() -> bytes:
    """Matrix2f identity stored column-major: col0=(1,0), col1=(0,1)."""
    return struct.pack("<ffff", 1.0, 0.0, 0.0, 1.0)


def _ball_state(pos_x: float = 4000.0, pos_y: float = 0.0) -> bytes:
    """BallState: position(8) + velocity(8) + radius(4) + covariance(16) = 36 bytes."""
    return (
        _v2f(pos_x, pos_y)   # position
        + _v2f(0.0, 0.0)      # velocity
        + struct.pack("<f", 50.0)  # radius
        + _mat2f_identity()    # covariance (identity)
    )


def _obstacle_avoidance_empty() -> bytes:
    """ObstacleAvoidance with empty path: avoidance(8) + path_count(4) = 12 bytes."""
    return _v2f(0.0, 0.0) + struct.pack("<I", 0)


def _encode_motion_request(
    motion: int,
    stand_high: bool = False,
    walk_rot: float = 0.0,
    walk_x: float = 0.0,
    walk_y: float = 0.0,
    kick_type: int = KICK_FORWARD_FAST_LEFT,
    target_direction: float = 0.0,
    ball_x: float = 4000.0,
    ball_y: float = 0.0,
) -> bytes:
    """
    Serialize a MotionRequest struct to B-Human binary format.
    Field layout mirrors Src/Representations/MotionControl/MotionRequest.h.
    """
    return (
        struct.pack("B", motion)                      # motion          (enum 1B)
        + struct.pack("B", int(stand_high))           # standHigh       (bool 1B)
        + _pose2f(walk_rot, walk_x, walk_y)           # walkSpeed       (Pose2f 12B)
        + _pose2f(0.0, 0.0, 0.0)                      # walkTarget      (Pose2f 12B)
        + struct.pack("B", 0)                         # keepTargetRotation (bool 1B)
        + _obstacle_avoidance_empty()                 # obstacleAvoidance  (12B)
        + struct.pack("<I", 0)                        # targetOfInterest (std::optional<Vector2f>) empty: size=0 (4B unsigned)
        + struct.pack("B", 0)                         # forceSideWalking (bool 1B)
        + struct.pack("B", 0)                         # shouldInterceptBall (bool 1B)
        + struct.pack("<f", target_direction)          # targetDirection  (Angle/float 4B)
        + struct.pack("<ff", 0.0, 0.0)               # directionPrecision (Rangea 8B)
        + struct.pack("B", kick_type)                 # kickType         (enum 1B)
        + struct.pack("<f", 1.0)                      # kickLength       (float 4B)
        + struct.pack("B", KICK_PRECISION_NOT_PRECISE)# alignPrecisely   (enum 1B)
        + struct.pack("B", 1)                         # preStepAllowed   (bool 1B, default true)
        + struct.pack("B", 1)                         # turnKickAllowed  (bool 1B, default true)
        + struct.pack("B", 0)                         # shiftTurnKickPose (bool 1B)
        + struct.pack("B", DIVE_PREPARE)              # diveRequest      (enum 1B)
        + struct.pack("B", SPECIAL_DEMO_BANNER_WAVE)  # specialRequest   (enum 1B)
        + _pose2f(0.0, 0.0, 0.0)                      # odometryData     (Pose2f 12B)
        + _ball_state(ball_x, ball_y)                 # ballEstimate     (BallState 36B)
        + struct.pack("<I", 0)                        # ballEstimateTimestamp (uint32 4B)
        + struct.pack("<I", 0)                        # ballTimeWhenLastSeen  (uint32 4B)
    )


def _encode_head_motion_request(
    pan: float = 0.0, tilt: float = 0.1, speed: float = 0.5
) -> bytes:
    """
    Serialize a HeadMotionRequest (panTiltMode, autoCamera).
    Field layout mirrors Src/Representations/MotionControl/HeadMotionRequest.h.
    mode(1) + cameraControlMode(1) + pan(4) + tilt(4) + speed(4) +
    target Vector3f(12) + stopAndGoMode(1) = 27 bytes.
    """
    return (
        struct.pack("B", HEAD_MODE_PAN_TILT)    # mode
        + struct.pack("B", HEAD_CAMERA_AUTO)    # cameraControlMode
        + struct.pack("<f", pan)                # pan
        + struct.pack("<f", tilt)               # tilt
        + struct.pack("<f", speed)              # speed
        + struct.pack("<fff", 1.0, 0.0, 0.0)   # target (Vector3f)
        + struct.pack("B", 0)                   # stopAndGoMode (bool)
    )


def _encode_odometry_data_preview(
    rotation: float = 0.0, x: float = 0.0, y: float = 0.0,
    change_rotation: float = 0.0, change_x: float = 0.0, change_y: float = 0.0,
) -> bytes:
    """OdometryDataPreview: OdometryData(Pose2f) + odometryChange(Pose2f)."""
    return _pose2f(rotation, x, y) + _pose2f(change_rotation, change_x, change_y)


# ═════════════════════════════════════════════════════════════════════════════
# B-Human MessageQueue / TCP packet builders
# ═════════════════════════════════════════════════════════════════════════════

def _msg_header(msg_id: int, data_size: int) -> bytes:
    """4-byte MessageHeader: byte0=id, bytes1-3=size (24-bit LE)."""
    return bytes([msg_id]) + struct.pack("<I", data_size)[:3]


def _make_queue(messages: list) -> bytes:
    """
    Build a MessageQueue payload.
    messages: [(msg_id, data_bytes), ...]
    Returns: QueueHeader(8B) + messages
    """
    body = b"".join(_msg_header(mid, len(d)) + d for mid, d in messages)
    queue_header = struct.pack("<II", len(body), 0)  # sizeLow, upper=0
    return queue_header + body


def _tcp_packet(queue_bytes: bytes) -> bytes:
    """Wrap MessageQueue in a TCP packet with 4-byte LE size prefix."""
    return struct.pack("<I", len(queue_bytes)) + queue_bytes


def _handshake_packet() -> bytes:
    """
    Initial packet requesting the robot's numOfDataMessageIDs.
    From RemoteConsole::main(): 12 bytes = QueueHeader(8) + MessageHeader(4),
    first byte of QueueHeader = sizeof(MessageHeader) = 4, rest zero.
    """
    data = struct.pack("<I", 4) + bytes(4) + bytes(4)  # QH(sizeLow=4,upper=0) + MH(id=0,size=0)
    return struct.pack("<I", len(data)) + data


def _heartbeat_packet() -> bytes:
    """Empty TCP packet (size=0) used to keep the connection alive."""
    return struct.pack("<I", 0)


def _set_representation_packet(thread: str, repr_name: str, binary_data: bytes) -> bytes:
    """
    Build a packet that sets a representation value on the robot.
    Sends idThread + idDebugDataChangeRequest, mirroring RobotConsole::sendDebugData().
    """
    thread_msg  = _str(thread)
    change_msg  = _str(repr_name) + bytes([1]) + binary_data  # char(1) = has data
    return _tcp_packet(_make_queue([
        (ID_THREAD, thread_msg),
        (ID_DEBUG_DATA_CHANGE_REQUEST, change_msg),
    ]))


def _set_debug_request_packet(thread: str, request_name: str, enabled: bool) -> bytes:
    """Build a packet that enables/disables a debug request on the robot."""
    thread_msg = _str(thread)
    request_msg = _str(request_name) + bytes([1 if enabled else 0])
    return _tcp_packet(_make_queue([
        (ID_THREAD, thread_msg),
        (ID_DEBUG_REQUEST, request_msg),
    ]))


# ═════════════════════════════════════════════════════════════════════════════
# Robot controller
# ═════════════════════════════════════════════════════════════════════════════

class BHumanController:
    """Manages the TCP connection to the robot and sends motion commands."""

    def __init__(self, robot_ip: str):
        self.robot_ip = robot_ip
        self.sock: socket.socket | None = None
        self.connected = False
        self._recv_thread: threading.Thread | None = None
        self._robot_num_ids: int | None = None
        self._jpeg_callbacks: list = []
        self._camera_callbacks: list = []
        self._current_camera_thread: str | None = None
        self._jpeg_stream_enabled = False
        self._camera_stream_enabled = False

    # ── Connection ────────────────────────────────────────────────────────────

    def connect(self, timeout: float = 5.0, send_setup: bool = True):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeout)
        self.sock.connect((self.robot_ip, BHUMAN_PORT))
        self.sock.settimeout(None)
        self.connected = True

        self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._recv_thread.start()

        self._send_raw(_handshake_packet())

        # Wait up to 3 s for the robot to reply with its numOfDataMessageIDs
        deadline = time.time() + 3.0
        while self._robot_num_ids is None and time.time() < deadline:
            time.sleep(0.05)

        if self._robot_num_ids is None:
            print("[WARNING] Robot numOfDataMessageIDs not received, continuing anyway.")
        else:
            print(f"[INFO] Robot numOfDataMessageIDs = {self._robot_num_ids}  (local = 79)")

        # Send setup packets but don't kill the connection if they fail.
        # On real robots the handshake may not complete, but commands can still work.
        if send_setup:
            for repr_name, data in [
                ("module:WalkToBallAndKickEngine:ignoreBallTimestamp", bytes([1])),
                ("module:WalkToBallAndKickEngine:ignoreBallOdometry",  bytes([1])),
            ]:
                try:
                    self.sock.sendall(_set_representation_packet("Motion", repr_name, data))
                    print(f"[INFO] {repr_name} = true sent")
                except OSError as e:
                    print(f"[WARNING] Setup send failed ({repr_name}): {e}")

        self._reset_motion_odometry_preview()

    def disconnect(self):
        self.connected = False
        if self.sock:
            try:
                try:
                    self.sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                self.sock.close()
            except OSError:
                pass
        if self._recv_thread and self._recv_thread.is_alive():
            self._recv_thread.join(timeout=1.0)
        self.sock = None
        self._recv_thread = None

    # ── Internal send/receive ─────────────────────────────────────────────────

    def _send_raw(self, data: bytes):
        try:
            self.sock.sendall(data)
        except OSError as e:
            print(f"[ERROR] Send failed: {e}")
            self.connected = False

    def _reset_motion_odometry_preview(self):
        """
        Keep manual head pan commands in the same odometry frame as MotionRequest.

        HeadMotionEngine compensates pan by comparing MotionRequest.odometryData
        against OdometryDataPreview. WebControl sends MotionRequest.odometryData
        as zero, so OdometryDataPreview must also be zero during manual control;
        otherwise "head center" can be clipped to a side after the robot turned.
        """
        if not self.connected:
            return

        odometry_preview = _encode_odometry_data_preview()
        odometry_translation_request = _pose2f(0.0, 0.0, 0.0)

        for repr_name, data in [
            ("representation:OdometryDataPreview", odometry_preview),
            ("representation:OdometryTranslationRequest", odometry_translation_request),
        ]:
            self._send_raw(_set_representation_packet("Motion", repr_name, data))

    def _recv_loop(self):
        buf = b""
        while self.connected:
            try:
                chunk = self.sock.recv(8192)
                if not chunk:
                    self.connected = False
                    break
                buf += chunk
                while len(buf) >= 4:
                    pkt_size = struct.unpack_from("<I", buf)[0]
                    if pkt_size == 0:
                        buf = buf[4:]  # heartbeat
                        continue
                    if len(buf) < 4 + pkt_size:
                        break
                    self._process_packet(buf[4:4 + pkt_size])
                    buf = buf[4 + pkt_size:]
            except OSError:
                if self.connected:
                    self.connected = False
                break

    def _process_packet(self, data: bytes):
        """Parse an incoming MessageQueue packet."""
        if len(data) < 8:
            return
        msg_body = data[8:]  # skip 8-byte QueueHeader
        offset = 0
        while offset + 4 <= len(msg_body):
            msg_id   = msg_body[offset]
            msg_size = (msg_body[offset + 1]
                        | (msg_body[offset + 2] << 8)
                        | (msg_body[offset + 3] << 16))
            offset += 4
            if offset + msg_size > len(msg_body):
                break
            payload = msg_body[offset:offset + msg_size]
            offset += msg_size
            if msg_id == ID_NUM_OF_DATA_MESSAGE_IDS and payload:
                self._robot_num_ids = payload[0]
                continue

            if msg_id == ID_FRAME_BEGIN:
                thread_name = self._decode_thread_name(payload)
                if thread_name:
                    self._current_camera_thread = thread_name
                continue

            if msg_id == ID_CAMERA_IMAGE and self._camera_stream_enabled:
                thread_name = self._current_camera_thread or "Upper"
                for callback in list(self._camera_callbacks):
                    try:
                        callback(thread_name, payload)
                    except Exception as e:
                        print(f"[WARNING] Camera callback failed: {e}")
                continue

            if msg_id == ID_JPEG_IMAGE and self._jpeg_stream_enabled:
                thread_name = self._current_camera_thread or "Upper"
                for callback in list(self._jpeg_callbacks):
                    try:
                        callback(thread_name, payload)
                    except Exception as e:
                        print(f"[WARNING] JPEG callback failed: {e}")
                continue

            if msg_id == ID_FRAME_FINISHED:
                continue

    def _decode_thread_name(self, payload: bytes) -> str | None:
        if len(payload) < 4:
            return None
        size = struct.unpack_from("<I", payload)[0]
        if size <= 0 or len(payload) < 4 + size:
            return None
        try:
            return payload[4:4 + size].decode(errors="replace")
        except Exception:
            return None

    def add_jpeg_callback(self, callback):
        if callback not in self._jpeg_callbacks:
            self._jpeg_callbacks.append(callback)

    def remove_jpeg_callback(self, callback):
        try:
            self._jpeg_callbacks.remove(callback)
        except ValueError:
            pass

    def add_camera_callback(self, callback):
        if callback not in self._camera_callbacks:
            self._camera_callbacks.append(callback)

    def remove_camera_callback(self, callback):
        try:
            self._camera_callbacks.remove(callback)
        except ValueError:
            pass

    def start_camera_stream(self):
        if not self.connected:
            return
        self._camera_stream_enabled = True
        for thread in ("Upper", "Lower"):
            self._send_raw(_set_debug_request_packet(thread, "representation:CameraImage", True))

    def stop_camera_stream(self):
        if not self.connected:
            return
        self._camera_stream_enabled = False
        for thread in ("Upper", "Lower"):
            self._send_raw(_set_debug_request_packet(thread, "representation:CameraImage", False))

    def start_jpeg_stream(self):
        if not self.connected:
            return
        self._jpeg_stream_enabled = True
        for thread in ("Upper", "Lower"):
            self._send_raw(_set_debug_request_packet(thread, "representation:JPEGImage", True))

    def stop_jpeg_stream(self):
        if not self.connected:
            return
        self._jpeg_stream_enabled = False
        for thread in ("Upper", "Lower"):
            self._send_raw(_set_debug_request_packet(thread, "representation:JPEGImage", False))

    # ── High-level motion commands ────────────────────────────────────────────

    def _send_motion(self, motion, stand_high=False, walk_rot=0.0,
                     walk_x=0.0, walk_y=0.0,
                     kick_type=KICK_FORWARD_FAST_LEFT,
                     target_direction=0.0,
                     ball_x=157.0, ball_y=0.0):
        if not self.connected:
            return
        binary = _encode_motion_request(motion, stand_high, walk_rot,
                                        walk_x, walk_y, kick_type,
                                        target_direction, ball_x, ball_y)
        pkt = _set_representation_packet("Cognition", "representation:MotionRequest", binary)
        self._send_raw(pkt)

    def _send_head(self, pan: float = 0.0, tilt: float = 0.1, speed: float = 0.5):
        if not self.connected:
            return
        self._reset_motion_odometry_preview()
        binary = _encode_head_motion_request(pan, tilt, speed)
        pkt = _set_representation_packet("Cognition", "representation:HeadMotionRequest", binary)
        self._send_raw(pkt)

    def stand(self):
        self._send_motion(MOTION_STAND, stand_high=False)

    def emergency_stop(self, bursts: int = 5):
        """Send stand several times back-to-back for the fastest possible stop."""
        for _ in range(bursts):
            self._send_motion(MOTION_STAND, stand_high=False)

    def sit(self):
        self._send_motion(MOTION_PLAY_DEAD, stand_high=True)

    def walk(self, x: float = 0.0, y: float = 0.0, rot: float = 0.0):
        self._send_motion(MOTION_WALK_AT_RELATIVE_SPEED, stand_high=True,
                          walk_x=x, walk_y=y, walk_rot=rot)

    def _send_kick_setup(self):
        for repr_name, data in [
            ("module:WalkToBallAndKickEngine:ignoreBallTimestamp", bytes([1])),
            ("module:WalkToBallAndKickEngine:ignoreBallOdometry",  bytes([1])),
        ]:
            try:
                self.sock.sendall(_set_representation_packet("Motion", repr_name, data))
            except OSError:
                pass

    def kick_left(self):
        self._send_kick_setup()
        self._send_motion(MOTION_WALK_TO_BALL_AND_KICK, stand_high=False,
                          kick_type=KICK_FORWARD_FAST_LEFT,
                          target_direction=KICK_LEFT_TARGET_DIR,
                          ball_x=157.0, ball_y=50.0)
        threading.Timer(2.0, self.stand).start()

    def kick_right(self):
        self._send_kick_setup()
        self._send_motion(MOTION_WALK_TO_BALL_AND_KICK, stand_high=False,
                          kick_type=KICK_FORWARD_FAST_RIGHT,
                          target_direction=KICK_RIGHT_TARGET_DIR,
                          ball_x=157.0, ball_y=-50.0)
        threading.Timer(2.0, self.stand).start()

    def head_up(self):
        self._send_head(pan=0.0, tilt=-0.3)

    def head_down(self):
        self._send_head(pan=0.0, tilt=0.5)

    def head_left(self):
        self._send_head(pan=1.0, tilt=0.1)

    def head_right(self):
        self._send_head(pan=-1.0, tilt=0.1)

    def head_center(self):
        self._send_head(pan=0.0, tilt=0.1)

    def heartbeat(self):
        if self.connected:
            self._send_raw(_heartbeat_packet())


# ═════════════════════════════════════════════════════════════════════════════
# Keyboard input (raw terminal, no external dependencies)
# ═════════════════════════════════════════════════════════════════════════════

def _get_key(fd) -> str:
    """Read one keypress from the terminal (non-blocking, 10 ms timeout)."""
    import select
    if not select.select([fd], [], [], 0.01)[0]:
        return ""
    ch = sys.stdin.read(1)
    if ch == "\x1b":  # escape sequence — consume and discard
        if select.select([fd], [], [], 0.005)[0]:
            sys.stdin.read(1)
            if select.select([fd], [], [], 0.005)[0]:
                sys.stdin.read(1)
        return ""
    return ch.lower()


# ═════════════════════════════════════════════════════════════════════════════
# Main
# ═════════════════════════════════════════════════════════════════════════════

HELP = """
╔══════════════════════════════════════════════════════════╗
║         SabanaHerons – Nao Keyboard Control              ║
╠══════════════════════════════════════════════════════════╣
║  Modo TOGGLE: presiona la tecla para activar,            ║
║               presiona V para detener.                   ║
║                                                          ║
║  Movimiento:            Cabeza:                          ║
║    Q = Girar izq.         I = Mirar arriba              ║
║    W = Caminar fwd        K = Mirar abajo                ║
║    E = Girar der.         J = Mirar izq.                ║
║    A = Lateral izq.       L = Mirar der.                ║
║    S = Caminar atrás      H = Centrar cabeza            ║
║    D = Lateral der.                                      ║
║                                                          ║
║  F = Diagonal izq.     G = Diagonal der.                ║
║  V = Parar (stand)     X = Sentarse                     ║
║  Z = Patear pie izq.   C = Patear pie der.              ║
║                                                          ║
║  Ctrl+C = Salir                                         ║
╚══════════════════════════════════════════════════════════╝
"""

# (label, action, is_toggle)
# toggle=True  → el robot sigue ejecutando hasta que se presione V
# toggle=False → se ejecuta una vez (patadas, sentarse, cabeza)
KEY_ACTIONS = {
    "w": ("Walk forward",       lambda r: r.walk(x=1.0),         True),
    "s": ("Walk backward",      lambda r: r.walk(x=-1.0),        True),
    "a": ("Strafe left",        lambda r: r.walk(y=1.0),         True),
    "d": ("Strafe right",       lambda r: r.walk(y=-1.0),        True),
    "q": ("Turn left",          lambda r: r.walk(rot=1.0),       True),
    "e": ("Turn right",         lambda r: r.walk(rot=-1.0),      True),
    "f": ("Forward + Left",     lambda r: r.walk(x=1.0, y=1.0),  True),
    "g": ("Forward + Right",    lambda r: r.walk(x=1.0, y=-1.0), True),
    "v": ("Stop",               lambda r: r.stand(),             False),
    "x": ("Sit",                lambda r: r.sit(),               False),
    "z": ("Kick LEFT foot",     lambda r: r.kick_left(),         False),
    "c": ("Kick RIGHT foot",    lambda r: r.kick_right(),        False),
    "i": ("Head up",            lambda r: r.head_up(),           False),
    "k": ("Head down",          lambda r: r.head_down(),         False),
    "j": ("Head left",          lambda r: r.head_left(),         False),
    "l": ("Head right",         lambda r: r.head_right(),        False),
    "h": ("Head center",        lambda r: r.head_center(),       False),
}

RESEND_INTERVAL = 0.05   # 50 ms — resend active toggle command to keep robot moving
HEARTBEAT_INTERVAL = 0.5  # 500 ms — keep TCP connection alive when idle


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 keyboard_control.py <robot_ip>")
        print("  e.g. python3 keyboard_control.py 192.168.49.3")
        sys.exit(1)

    robot_ip = sys.argv[1]
    print(f"\nConnecting to {robot_ip}:{BHUMAN_PORT} ...")

    robot = BHumanController(robot_ip)
    try:
        robot.connect()
    except OSError as e:
        print(f"[ERROR] Could not connect: {e}")
        sys.exit(1)

    if not robot.connected:
        print("[ERROR] Connection failed.")
        sys.exit(1)

    print(HELP)

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)

    active_key: str | None = None   # currently toggled movement key
    active_action = None
    last_send_time = 0.0
    last_heartbeat_time = 0.0

    try:
        tty.setraw(fd)
        while robot.connected:
            key = _get_key(fd)
            now = time.time()

            # ── Exit ─────────────────────────────────────────────────────────
            if key in ("\x03", "\x04"):
                break

            # ── Key pressed ──────────────────────────────────────────────────
            if key and key in KEY_ACTIONS:
                label, action, is_toggle = KEY_ACTIONS[key]
                sys.stdout.write(f"\r  [{key.upper()}] {label:<30}")
                sys.stdout.flush()

                if key == "v":
                    # Emergency stop: cancel active command first, then burst stand
                    active_key = None
                    active_action = None
                    robot.emergency_stop()
                elif is_toggle:
                    active_key = key
                    active_action = action
                    action(robot)
                else:
                    # One-shot: clear any running toggle
                    active_key = None
                    active_action = None
                    action(robot)

                last_send_time = now

            # ── Resend active toggle command ──────────────────────────────────
            elif active_action and (now - last_send_time) >= RESEND_INTERVAL:
                active_action(robot)
                last_send_time = now

            # ── Heartbeat when completely idle ────────────────────────────────
            elif not active_action and (now - last_heartbeat_time) >= HEARTBEAT_INTERVAL:
                robot.heartbeat()
                last_heartbeat_time = now

    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        print("\n\nSending emergency stop before exit...")
        robot.emergency_stop()
        time.sleep(0.3)
        robot.disconnect()
        print("Disconnected. Bye!")


if __name__ == "__main__":
    main()
