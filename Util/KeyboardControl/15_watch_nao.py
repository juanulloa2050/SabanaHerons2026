#!/usr/bin/env python3
"""
Script 15: Live stream de la cámara del NAO con overlay de detección y
           recolección manual de datos de entrenamiento.

Protocolo por frame:
    [4 bytes] magic  b'CAMF'
    [4 bytes] uint32 LE  tamaño JPEG
    [N bytes] datos JPEG (YCbCr estándar)
    [13 bytes] metadata detección:
        [1 byte]  status  0=notSeen  1=seen  2=guessed
        [4 bytes] float32 LE  ballX
        [4 bytes] float32 LE  ballY
        [4 bytes] float32 LE  ballRadius
    [1 byte]  num_spots (máx 50)
    [num_spots * 8 bytes] (int32_LE x, int32_LE y) por spot

Uso:
    python3 15_watch_nao.py                      # dual cámaras
    python3 15_watch_nao.py --camera lower
    python3 15_watch_nao.py --ip 10.0.49.7 --scale 1

Controles de visualización:
    q / ESC  → salir
    s        → screenshot
    b        → toggle overlay de BallSpots

Controles de recolección de datos:
    t        → guardar frame actual como TRIONDA (positivo)
    n        → guardar frame actual como SIN BALÓN (negativo)
    r / f    → subir / bajar radio de la bounding box (+/- 5 px)

En modo dual, t/n guarda el frame de AMBAS cámaras simultáneamente.
Los datos se guardan en data/sessions/ compatibles con 7_retrain.py.
"""

import argparse
import os
import socket
import struct
import subprocess
import threading
import time
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np

# ── Protocolo ─────────────────────────────────────────────────────────────────
MAGIC           = b'CAMF'
BALL_META_SIZE  = 13
DEFAULT_IP      = os.environ.get("NAO_WATCHER_IP", "192.168.49.2")
PORT_UPPER      = 7777   # directo B-Human; usar 7787 cuando corre 24_yolo_on_nao.py
PORT_LOWER      = 7778   # directo B-Human; usar 7788 cuando corre 24_yolo_on_nao.py
CONNECT_TIMEOUT = 15.0

NOT_SEEN = 0
SEEN     = 1
GUESSED  = 2

# ── Colores (BGR) ──────────────────────────────────────────────────────────────
CLR_SEEN    = (0, 220,   0)
CLR_GUESSED = (0, 200, 255)
CLR_SPOT    = (255, 120,  0)
CLR_HUD     = (200, 200, 200)
CLR_DIM     = (130, 130, 130)
CLR_SAVE_T  = ( 50, 220,  50)   # flash verde — guardado trionda
CLR_SAVE_N  = ( 50,  50, 220)   # flash rojo  — guardado negativo
CLR_BBOX    = (  0, 200, 255)   # bounding box preview

# ── Globals ────────────────────────────────────────────────────────────────────
_show_spots  = True

# ── NAO system monitor (SSH polling) ──────────────────────────────────────────

_nao_stats = {'cpu': 0.0, 'ram_used': 0, 'ram_total': 0, 'temp': 0.0}
_nao_stats_lock = threading.Lock()

_MONITOR_CMD = (
    "cat /proc/stat; "
    "echo '---MEM---'; cat /proc/meminfo; "
    "echo '---TEMP---'; cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null || echo 0"
)

def _monitor_loop(ip: str, key: str, interval: float = 3.0):
    prev_stat = None
    while True:
        try:
            r = subprocess.run(
                ["ssh", "-i", key,
                 "-o", "StrictHostKeyChecking=no",
                 "-o", "BatchMode=yes",
                 "-o", "ConnectTimeout=4",
                 f"nao@{ip}", _MONITOR_CMD],
                capture_output=True, text=True, timeout=8
            )
            raw = r.stdout
        except Exception:
            time.sleep(interval)
            continue

        sections = raw.split('---')
        cpu_block  = sections[0] if sections else ''
        mem_block  = sections[1].replace('MEM', '', 1) if len(sections) > 1 else ''
        temp_block = sections[2].replace('TEMP', '', 1) if len(sections) > 2 else ''

        # CPU %
        cpu_pct = 0.0
        for line in cpu_block.splitlines():
            if line.startswith('cpu '):
                vals = list(map(int, line.split()[1:]))
                idle  = vals[3] + (vals[4] if len(vals) > 4 else 0)
                total = sum(vals)
                if prev_stat:
                    dt = total - prev_stat[0]
                    di = idle  - prev_stat[1]
                    if dt > 0:
                        cpu_pct = max(0.0, (1 - di / dt) * 100)
                prev_stat = (total, idle)
                break

        # RAM
        mem = {}
        for line in mem_block.splitlines():
            parts = line.split()
            if len(parts) >= 2:
                try: mem[parts[0].rstrip(':')] = int(parts[1])
                except ValueError: pass
        total_mb = mem.get('MemTotal', 0) // 1024
        avail_mb = mem.get('MemAvailable', mem.get('MemFree', 0)) // 1024
        used_mb  = total_mb - avail_mb

        # Temp
        temps = []
        for ln in temp_block.strip().splitlines():
            try:
                t = int(ln.strip())
                if t > 0: temps.append(t / 1000.0)
            except ValueError: pass
        temp_c = round(sum(temps) / len(temps), 1) if temps else 0.0

        with _nao_stats_lock:
            _nao_stats['cpu']       = round(cpu_pct, 1)
            _nao_stats['ram_used']  = used_mb
            _nao_stats['ram_total'] = total_mb
            _nao_stats['temp']      = temp_c

        time.sleep(interval)


def start_monitor(ip: str, key: str):
    t = threading.Thread(target=_monitor_loop, args=(ip, key), daemon=True)
    t.start()


# ── Recolección de datos ───────────────────────────────────────────────────────

class DataCollector:
    """
    Guarda frames JPEG + XMLs Pascal VOC en data/sessions/.
    Formato compatible con 2b_extract_patches_color.py y 7_retrain.py.
    """

    SESSIONS_DIR = Path(__file__).parent / "data" / "sessions"

    def __init__(self, camera_label: str):
        ts = datetime.now().strftime("%Y-%m-%d_%H%M%S")
        base = self.SESSIONS_DIR / f"manual_{ts}_{camera_label.lower()}"
        self.img_dir = base / "images"
        self.ann_dir = base / "annotations"
        self.img_dir.mkdir(parents=True, exist_ok=True)
        self.ann_dir.mkdir(parents=True, exist_ok=True)
        self.camera     = camera_label
        self.n_trionda  = 0
        self.n_negative = 0
        self._fidx      = 0
        self.name       = base.name
        print(f"  Sesión iniciada: data/sessions/{self.name}")

    def save_trionda(self, jpeg: bytes, spots: list[tuple], radius: int) -> bool:
        """Guarda frame como positivo trionda. Retorna True si tuvo spots para anotar."""
        fname = f"frame_{self._fidx:05d}"
        (self.img_dir / f"{fname}.jpg").write_bytes(jpeg)

        if spots:
            arr = np.frombuffer(jpeg, dtype=np.uint8)
            img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
            h, w = (img.shape[:2] if img is not None else (480, 640))
            self._write_xml(fname, w, h, spots, radius, "trionda")

        self._fidx     += 1
        self.n_trionda += 1
        return bool(spots)

    def save_negative(self, jpeg: bytes) -> None:
        """Guarda frame como negativo (sin balón). Sin anotación XML."""
        fname = f"frame_{self._fidx:05d}"
        (self.img_dir / f"{fname}.jpg").write_bytes(jpeg)
        self._fidx      += 1
        self.n_negative += 1

    def _write_xml(self, fname, w, h, spots, radius, label):
        lines = [
            "<annotation>",
            f"  <folder>images</folder>",
            f"  <filename>{fname}.jpg</filename>",
            f"  <size><width>{w}</width><height>{h}</height><depth>3</depth></size>",
        ]
        for sx, sy in spots:
            xmin, ymin = max(0, sx - radius), max(0, sy - radius)
            xmax, ymax = min(w, sx + radius), min(h, sy + radius)
            lines += [
                "  <object>",
                f"    <name>{label}</name>",
                "    <bndbox>",
                f"      <xmin>{xmin}</xmin><ymin>{ymin}</ymin>",
                f"      <xmax>{xmax}</xmax><ymax>{ymax}</ymax>",
                "    </bndbox>",
                "  </object>",
            ]
        lines.append("</annotation>")
        (self.ann_dir / f"{fname}.xml").write_text("\n".join(lines))

    def summary(self):
        total = self.n_trionda + self.n_negative
        print(f"  [{self.camera}] Guardados: {self.n_trionda} trionda + "
              f"{self.n_negative} negativos = {total} frames  "
              f"→ data/sessions/{self.name}")


# ── Socket helpers ─────────────────────────────────────────────────────────────

def recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Robot desconectado")
        buf += chunk
    return buf


def read_frame(sock: socket.socket):
    """Retorna (jpeg, status, bx, by, br, spots).
    Consume patch payload transparently to keep stream in sync."""
    buf = b''
    while True:
        buf += sock.recv(1)
        if buf[-4:] == MAGIC:
            break
        if len(buf) > 4096:
            buf = buf[-4:]

    size = struct.unpack('<I', recv_exact(sock, 4))[0]
    if size == 0 or size > 2_000_000:
        raise ValueError(f"Tamaño inválido: {size}")
    jpeg = recv_exact(sock, size)

    meta = recv_exact(sock, BALL_META_SIZE)
    status, bx, by, br = struct.unpack_from('<Bfff', meta)

    n = struct.unpack('B', recv_exact(sock, 1))[0]
    spots = []
    if n > 0:
        raw = recv_exact(sock, n * 8)
        for i in range(n):
            sx, sy = struct.unpack_from('<ii', raw, i * 8)
            spots.append((sx, sy))

    # Consume patch payload (uint8 valid; if valid: uint16_LE patchSize + floats)
    patch_valid = struct.unpack('B', recv_exact(sock, 1))[0]
    if patch_valid:
        ps = struct.unpack('<H', recv_exact(sock, 2))[0]
        recv_exact(sock, ps * ps * 3 * 4)  # discard float32 data

    return jpeg, int(status), float(bx), float(by), float(br), spots


def connect(ip: str, port: int) -> socket.socket:
    print(f"  Conectando a {ip}:{port}...", end='', flush=True)
    t0 = time.time()
    while time.time() - t0 < CONNECT_TIMEOUT:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(3.0)
            s.connect((ip, port))
            s.settimeout(8.0)
            print(" OK")
            return s
        except (ConnectionRefusedError, TimeoutError, OSError):
            print('.', end='', flush=True)
            time.sleep(0.5)
    raise ConnectionError(f"Sin conexión a {ip}:{port}")


# ── Drawing ────────────────────────────────────────────────────────────────────

def draw_overlay(vis: np.ndarray,
                 status: int, bx: float, by: float, br: float,
                 spots: list,
                 scale: int, label: str, fps: float, fidx: int,
                 collector: DataCollector | None,
                 ball_radius: int,
                 flash_color: tuple | None,
                 flash_until: float):
    global _show_spots
    H, W = vis.shape[:2]

    # Flash de confirmación al guardar (borde de color)
    if flash_color and time.time() < flash_until:
        cv2.rectangle(vis, (0, 0), (W - 1, H - 1), flash_color, 6)

    # BallSpots + preview de bounding box
    if _show_spots and spots:
        for sx, sy in spots:
            cx = int(round(sx * scale))
            cy = int(round(sy * scale))
            cv2.circle(vis, (cx, cy), 6, CLR_SPOT, 1, cv2.LINE_AA)
            cv2.drawMarker(vis, (cx, cy), CLR_SPOT,
                           cv2.MARKER_TILTED_CROSS, 8, 1, cv2.LINE_AA)
            # Preview del bbox que se guardará si se pulsa 't'
            # Ajusta con r/f hasta que el cuadro amarillo cubra el balón exactamente
            br_px = ball_radius * scale
            cv2.rectangle(vis,
                          (cx - br_px, cy - br_px),
                          (cx + br_px, cy + br_px),
                          (0, 220, 220), 2)
            cv2.putText(vis, f"r={ball_radius}", (cx - br_px, cy - br_px - 4),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.35, (0, 220, 220), 1)

    # Top bar (2 lines)
    cv2.rectangle(vis, (0, 0), (W, 38), (0, 0, 0), -1)
    spot_str  = f" | spots:{len(spots)}" if _show_spots else ""
    coll_str  = ""
    coll_col  = CLR_HUD
    if collector:
        coll_str = (f" | T:{collector.n_trionda}  N:{collector.n_negative}"
                    f"  r={ball_radius}px")
        coll_col = (50, 200, 50)
    hud = f"NAO {label} | FPS:{fps:.1f} | fr:{fidx}{spot_str}{coll_str}"
    cv2.putText(vis, hud, (6, 13), cv2.FONT_HERSHEY_SIMPLEX,
                0.42, coll_col, 1, cv2.LINE_AA)

    # Second HUD line: NAO system stats
    with _nao_stats_lock:
        cpu  = _nao_stats['cpu']
        ru   = _nao_stats['ram_used']
        rt   = _nao_stats['ram_total']
        temp = _nao_stats['temp']
    cpu_col = (0, 80, 255) if cpu > 80 else (0, 200, 255) if cpu > 50 else (120, 220, 120)
    temp_col = (0, 80, 255) if temp > 75 else (0, 200, 255) if temp > 65 else (120, 220, 120)
    stats_str = f"CPU:{cpu:.0f}%  RAM:{ru}/{rt}MB  T:{temp:.0f}C"
    cv2.putText(vis, stats_str, (6, 28), cv2.FONT_HERSHEY_SIMPLEX,
                0.40, cpu_col, 1, cv2.LINE_AA)

    # Detección BallPerceptor
    if status != NOT_SEEN:
        color = CLR_SEEN if status == SEEN else CLR_GUESSED
        cx = int(round(bx * scale))
        cy = int(round(by * scale))
        cr = max(3, int(round(br * scale)))
        cv2.circle(vis, (cx, cy), cr, color, 2, cv2.LINE_AA)
        cv2.drawMarker(vis, (cx, cy), color, cv2.MARKER_CROSS, 12, 1, cv2.LINE_AA)
        tag = "SEEN" if status == SEEN else "GUESSED"
        cv2.putText(vis, f"{tag} r={br:.0f}px",
                    (cx + cr + 4, cy + 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.42, color, 1, cv2.LINE_AA)
    else:
        cv2.putText(vis, "NOT SEEN", (W - 90, 15),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.42, (80, 80, 180), 1, cv2.LINE_AA)

    # Bottom hint
    hint = "q=salir  s=shot  b=spots  |  t=TRIONDA  n=NEGATIVO  r/f=radio+/-"
    cv2.putText(vis, hint, (6, H - 6),
                cv2.FONT_HERSHEY_SIMPLEX, 0.36, CLR_DIM, 1, cv2.LINE_AA)


def decode_frame(jpeg: bytes) -> np.ndarray | None:
    arr = np.frombuffer(jpeg, dtype=np.uint8)
    return cv2.imdecode(arr, cv2.IMREAD_COLOR)


# ── Camera thread ──────────────────────────────────────────────────────────────

class CameraThread(threading.Thread):
    """Lee frames en background. Expone el último en self.latest."""

    def __init__(self, ip: str, port: int, label: str):
        super().__init__(daemon=True)
        self.ip     = ip
        self.port   = port
        self.label  = label
        # (jpeg_bytes, frame_bgr, status, bx, by, br, spots, fps, fidx)
        self.latest = None
        self.lock   = threading.Lock()
        self.stop   = threading.Event()

    def run(self):
        sock   = connect(self.ip, self.port)
        fps    = 0.0
        t_prev = time.time()
        fidx   = 0
        while not self.stop.is_set():
            try:
                jpeg, status, bx, by, br, spots = read_frame(sock)
            except Exception as e:
                if self.stop.is_set():
                    break
                print(f"\n  [{self.label}] Reconectando: {e}")
                try: sock.close()
                except Exception: pass
                sock = connect(self.ip, self.port)
                continue

            frame = decode_frame(jpeg)
            if frame is None:
                continue

            t_now = time.time()
            fps    = 0.8 * fps + 0.2 / max(t_now - t_prev, 1e-6)
            t_prev = t_now
            fidx  += 1

            with self.lock:
                self.latest = (jpeg, frame, status, bx, by, br, spots, fps, fidx)

        try: sock.close()
        except Exception: pass


# ── Gestión de teclas de colección ─────────────────────────────────────────────

def _handle_key(key: int,
                collectors: dict,          # label → DataCollector | None
                latests: dict,             # label → latest tuple | None
                ball_radius: list,         # [int]  mutable via list
                flash: dict,               # {'color', 'until'}
                shot_idx: list):           # [int]
    """
    Gestiona las teclas de recolección y screenshot.
    Modifica collectors, ball_radius, flash y shot_idx in-place.
    Retorna True si hay que salir.
    """
    global _show_spots

    if key in (ord('q'), 27):
        return True

    elif key == ord('b'):
        _show_spots = not _show_spots
        print(f"  Spots: {'ON' if _show_spots else 'OFF'}")

    elif key == ord('r'):
        ball_radius[0] = min(150, ball_radius[0] + 5)
        print(f"  Radio bbox: {ball_radius[0]}px")

    elif key == ord('f'):
        ball_radius[0] = max(8, ball_radius[0] - 5)
        print(f"  Radio bbox: {ball_radius[0]}px")

    elif key == ord('s'):
        # Screenshot: guardar el frame actual de cada cámara disponible
        for lbl, latest in latests.items():
            if latest is not None:
                jpeg = latest[0]
                fname = f"nao_{lbl.lower()}_{shot_idx[0]:04d}.jpg"
                Path(fname).write_bytes(jpeg)
                print(f"  Screenshot: {fname}")
        shot_idx[0] += 1

    elif key == ord('t'):
        # Guardar frame actual como TRIONDA en todas las cámaras activas
        saved_any = False
        for lbl, latest in latests.items():
            if latest is None:
                continue
            jpeg, _, _, _, _, _, spots, _, _ = latest
            c = collectors.get(lbl)
            if c is None:
                c = DataCollector(lbl)
                collectors[lbl] = c
            had_spots = c.save_trionda(jpeg, spots, ball_radius[0])
            print(f"  [{lbl}] Trionda guardada"
                  + (f" con {len(spots)} spots" if spots else " (sin spots — solo imagen)"))
            saved_any = True
        if saved_any:
            flash['color'] = CLR_SAVE_T
            flash['until'] = time.time() + 0.18

    elif key == ord('n'):
        # Guardar frame actual como NEGATIVO en todas las cámaras activas
        saved_any = False
        for lbl, latest in latests.items():
            if latest is None:
                continue
            jpeg = latest[0]
            c = collectors.get(lbl)
            if c is None:
                c = DataCollector(lbl)
                collectors[lbl] = c
            c.save_negative(jpeg)
            print(f"  [{lbl}] Negativo guardado")
            saved_any = True
        if saved_any:
            flash['color'] = CLR_SAVE_N
            flash['until'] = time.time() + 0.18

    return False


def configure_window(win: str,
                     window_x: int | None,
                     window_y: int | None,
                     window_w: int | None,
                     window_h: int | None,
                     fullscreen: bool) -> None:
    cv2.namedWindow(win, cv2.WINDOW_NORMAL)
    if window_w and window_h:
        cv2.resizeWindow(win, window_w, window_h)
    if window_x is not None and window_y is not None:
        cv2.moveWindow(win, window_x, window_y)
    if fullscreen:
        cv2.setWindowProperty(win, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)


# ── Modo dual ──────────────────────────────────────────────────────────────────

def run_dual(ip: str, scale: int,
             port_upper: int = PORT_UPPER, port_lower: int = PORT_LOWER,
             window_name: str | None = None,
             window_x: int | None = None, window_y: int | None = None,
             window_w: int | None = None, window_h: int | None = None,
             fullscreen: bool = False):
    threads = {
        'UPPER': CameraThread(ip, port_upper, 'UPPER'),
        'LOWER': CameraThread(ip, port_lower, 'LOWER'),
    }
    for t in threads.values():
        t.start()

    win = window_name or f"NAO Live {ip}"
    configure_window(win, window_x, window_y, window_w, window_h, fullscreen)

    collectors  = {}           # label → DataCollector
    ball_radius = [40]         # mutable int  (trionda ~72mm → ~35-45px a dist típica)
    flash       = {'color': None, 'until': 0.0}
    shot_idx    = [0]

    try:
        while True:
            latests = {}
            panels  = []

            for label in ('UPPER', 'LOWER'):
                with threads[label].lock:
                    data = threads[label].latest
                latests[label] = data

                if data is None:
                    ph, pw = 240 * scale, 320 * scale
                    panel = np.zeros((ph, pw, 3), dtype=np.uint8)
                    cv2.putText(panel, f"{label}: conectando...",
                                (10, ph // 2), cv2.FONT_HERSHEY_SIMPLEX,
                                0.55, CLR_DIM, 1)
                else:
                    jpeg, frame, status, bx, by, br, spots, fps, fidx = data
                    h, w = frame.shape[:2]
                    panel = cv2.resize(frame, (w * scale, h * scale),
                                       interpolation=cv2.INTER_NEAREST)
                    draw_overlay(panel, status, bx, by, br, spots,
                                 scale, label, fps, fidx,
                                 collectors.get(label),
                                 ball_radius[0],
                                 flash['color'], flash['until'])
                panels.append(panel)

            # Igualar alturas
            h0, h1 = panels[0].shape[0], panels[1].shape[0]
            if h0 != h1:
                mh = max(h0, h1)
                for i, p in enumerate(panels):
                    if p.shape[0] < mh:
                        pad = np.zeros((mh - p.shape[0], p.shape[1], 3), np.uint8)
                        panels[i] = np.vstack([p, pad])

            sep = np.full((panels[0].shape[0], 4, 3), 60, dtype=np.uint8)
            combined = np.hstack([panels[0], sep, panels[1]])
            cv2.imshow(win, combined)

            key = cv2.waitKey(16) & 0xFF
            if _handle_key(key, collectors, latests, ball_radius, flash, shot_idx):
                break

    finally:
        for t in threads.values():
            t.stop.set()
        for c in collectors.values():
            c.summary()
        cv2.destroyAllWindows()
        print("Viewer cerrado.")


# ── Modo single ────────────────────────────────────────────────────────────────

def run_single(ip: str, port: int, label: str, scale: int,
               window_name: str | None = None,
               window_x: int | None = None, window_y: int | None = None,
               window_w: int | None = None, window_h: int | None = None,
               fullscreen: bool = False):
    sock = connect(ip, port)
    win = window_name or f"NAO Live {ip} - {label}"
    configure_window(win, window_x, window_y, window_w, window_h, fullscreen)

    fps         = 0.0
    t_prev      = time.time()
    fidx        = 0
    collectors  = {}
    ball_radius = [40]         # trionda ~72mm → ~35-45px a dist típica
    flash       = {'color': None, 'until': 0.0}
    shot_idx    = [0]
    latest_wrap = {label: None}

    try:
        while True:
            try:
                jpeg, status, bx, by, br, spots = read_frame(sock)
            except (ConnectionError, ValueError, OSError) as e:
                print(f"\n  Reconectando: {e}")
                try: sock.close()
                except Exception: pass
                sock = connect(ip, port)
                continue

            frame = decode_frame(jpeg)
            if frame is None:
                continue

            t_now  = time.time()
            fps    = 0.8 * fps + 0.2 / max(t_now - t_prev, 1e-6)
            t_prev = t_now
            fidx  += 1

            latest_wrap[label] = (jpeg, frame, status, bx, by, br, spots, fps, fidx)

            h, w = frame.shape[:2]
            vis = cv2.resize(frame, (w * scale, h * scale),
                             interpolation=cv2.INTER_NEAREST)
            draw_overlay(vis, status, bx, by, br, spots,
                         scale, label, fps, fidx,
                         collectors.get(label),
                         ball_radius[0],
                         flash['color'], flash['until'])

            cv2.imshow(win, vis)
            key = cv2.waitKey(1) & 0xFF
            if _handle_key(key, collectors, latest_wrap, ball_radius, flash, shot_idx):
                break

    finally:
        sock.close()
        for c in collectors.values():
            c.summary()
        cv2.destroyAllWindows()
        print("Viewer cerrado.")


# ── Entry point ────────────────────────────────────────────────────────────────

def _env_int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)))
    except ValueError:
        return default


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CAMERA = os.environ.get("NAO_WATCHER_CAMERA", "dual").lower()
DEFAULT_SCALE = _env_int("NAO_WATCHER_SCALE", 1)
DEFAULT_KEY = os.environ.get(
    "NAO_WATCHER_SSH_KEY",
    str(REPO_ROOT / "Install/Keys/id_rsa_nao"),
)

def main():
    p = argparse.ArgumentParser(
        description="NAO Live Viewer con recolección manual de datos")
    p.add_argument("--ip",          default=DEFAULT_IP)
    p.add_argument("--camera",      default=DEFAULT_CAMERA,
                   choices=["upper", "lower", "dual"])
    p.add_argument("--scale",       default=DEFAULT_SCALE, type=int)
    p.add_argument("--no-spots",    action="store_true")
    p.add_argument("--port-upper",  default=PORT_UPPER, type=int,
                   help="Puerto cámara upper (7787 cuando corre 24_yolo_on_nao.py)")
    p.add_argument("--port-lower",  default=PORT_LOWER, type=int,
                   help="Puerto cámara lower (7788 cuando corre 24_yolo_on_nao.py)")
    p.add_argument("--key",         default=DEFAULT_KEY,
                   help="SSH key para monitoreo de CPU/RAM del NAO")
    p.add_argument("--no-monitor",  action="store_true",
                   help="Desactivar monitoreo SSH de CPU/RAM")
    p.add_argument("--window-name", default=None,
                   help="Nombre de la ventana OpenCV")
    p.add_argument("--window-x",    type=int,
                   help="Posicion X de la ventana")
    p.add_argument("--window-y",    type=int,
                   help="Posicion Y de la ventana")
    p.add_argument("--window-w",    type=int,
                   help="Ancho de la ventana")
    p.add_argument("--window-h",    type=int,
                   help="Alto de la ventana")
    p.add_argument("--fullscreen",  action="store_true",
                   help="Abrir la ventana en fullscreen")
    args = p.parse_args()

    global _show_spots
    if args.no_spots:
        _show_spots = False

    if not args.no_monitor:
        start_monitor(args.ip, args.key)

    print(f"\n=== NAO Camera Viewer + Data Collector ===")
    print(f"  Robot : {args.ip}  |  Modo: {args.camera.upper()}  |  Escala: {args.scale}x")
    print(f"  Spots : {'ON' if _show_spots else 'OFF'}")
    print()
    print("  Visualización : b=toggle spots   s=screenshot")
    print("  Recolección   : t=TRIONDA  n=NEGATIVO  r/f=radio bbox +/-")
    print("  El recuadro CIAN muestra el bbox que se anotará al pulsar 't'")
    print()
    print("  Overlay: verde=SEEN  amarillo=GUESSED  cian=BallSpot")
    print("  Al salir se imprime el resumen de frames guardados\n")

    if args.camera == "dual":
        run_dual(args.ip, args.scale, args.port_upper, args.port_lower,
                 args.window_name, args.window_x, args.window_y,
                 args.window_w, args.window_h, args.fullscreen)
    elif args.camera == "upper":
        run_single(args.ip, args.port_upper, "UPPER", args.scale,
                   args.window_name, args.window_x, args.window_y,
                   args.window_w, args.window_h, args.fullscreen)
    else:
        run_single(args.ip, args.port_lower, "LOWER", args.scale,
                   args.window_name, args.window_x, args.window_y,
                   args.window_w, args.window_h, args.fullscreen)


if __name__ == "__main__":
    main()
