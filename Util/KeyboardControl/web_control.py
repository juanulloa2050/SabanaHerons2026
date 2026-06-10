#!/usr/bin/env python3
"""
SabanaHerons - Web Control Server
Abre http://<ip_laptop>:8080 en cualquier celular de la red para controlar robots.

Uso:
    python3 web_control.py
"""
import asyncio
import base64
import json
import socket
import threading
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from keyboard_control import BHumanController
from ball_detector_stream_recorder import (
    BallDetectorStreamRecorder,
    connect_camf_stream,
    read_camf_frame,
)

try:
    from fastapi import FastAPI, WebSocket, WebSocketDisconnect
    from fastapi.responses import HTMLResponse
    import uvicorn
except ImportError:
    print("Instala dependencias con:  pip install fastapi 'uvicorn[standard]'")
    sys.exit(1)

# == Registro de robots ========================================================

ROBOTS = [
    {"id": "ronaoldinho", "name": "ronaoldinho", "ip": "10.0.49.2"},
    {"id": "loki",        "name": "loki",         "ip": "10.0.49.3"},
    {"id": "falcanao",    "name": "falcanao",      "ip": "10.0.49.4"},
    {"id": "juan",        "name": "juan",          "ip": "10.0.49.5"},
    {"id": "iniesnao",    "name": "iniesnao",      "ip": "10.0.49.6"},
    {"id": "ospinao",     "name": "ospinao",       "ip": "10.0.49.7"},
    {"id": "Heron",       "name": "Heron",         "ip": "10.0.49.8"},
    {"id": "naonaldo",    "name": "naonaldo",      "ip": "10.0.49.9"},
    {"id": "naonel",      "name": "naonel",        "ip": "10.0.49.10"},
]
ROBOT_MAP = {r["id"]: r for r in ROBOTS}

# Pool global: robot_id -> BHumanController
controllers: dict[str, BHumanController] = {}
_connect_locks: dict[str, asyncio.Lock] = {}

# Grabadores de cámaras por robot
recorders: dict[str, BallDetectorStreamRecorder] = {}

# == FastAPI app ===============================================================

app = FastAPI()

# == Gestion de conexiones =====================================================

async def get_or_connect(robot_id: str) -> BHumanController | None:
    ctrl = controllers.get(robot_id)
    if ctrl and ctrl.connected:
        return ctrl

    lock = _connect_locks.setdefault(robot_id, asyncio.Lock())
    if lock.locked():
        return None  # reconnect already in progress

    async with lock:
        ctrl = controllers.get(robot_id)
        if ctrl and ctrl.connected:
            return ctrl
        info = ROBOT_MAP.get(robot_id)
        if not info:
            return None
        ctrl = BHumanController(info["ip"])
        try:
            loop = asyncio.get_event_loop()
            await loop.run_in_executor(None, ctrl.connect)
            controllers[robot_id] = ctrl
            print(f"[INFO] Conectado a {robot_id} ({info['ip']})")
            return ctrl
        except Exception as e:
            print(f"[ERROR] No se pudo conectar a {robot_id}: {e}")
            return None


def get_recorder(robot_id: str) -> BallDetectorStreamRecorder:
    recorder = recorders.get(robot_id)
    if recorder is None:
        recorder = BallDetectorStreamRecorder()
        recorders[robot_id] = recorder
    return recorder


def robots_status() -> list:
    return [
        {
            "id": r["id"],
            "name": r["name"],
            "ip": r["ip"],
            "connected": r["id"] in controllers and controllers[r["id"]].connected,
            "recording": get_recorder(r["id"]).recording,
        }
        for r in ROBOTS
    ]

# == Comandos disponibles ======================================================

COMMANDS: dict[str, object] = {
    "walk_forward":  lambda c: c.walk(x=1.0),
    "walk_backward": lambda c: c.walk(x=-1.0),
    "strafe_left":   lambda c: c.walk(y=1.0),
    "strafe_right":  lambda c: c.walk(y=-1.0),
    "turn_left":     lambda c: c.walk(rot=1.0),
    "turn_right":    lambda c: c.walk(rot=-1.0),
    "fwd_left":      lambda c: c.walk(x=1.0, y=1.0),
    "fwd_right":     lambda c: c.walk(x=1.0, y=-1.0),
    "stand":         lambda c: c.emergency_stop(),
    "sit":           lambda c: c.sit(),
    "kick_left":     lambda c: c.kick_left(),
    "kick_right":    lambda c: c.kick_right(),
    "head_up":       lambda c: c.head_up(),
    "head_down":     lambda c: c.head_down(),
    "head_left":     lambda c: c.head_left(),
    "head_right":    lambda c: c.head_right(),
    "head_center":   lambda c: c.head_center(),
}

CONTINUOUS_CMDS = {
    "walk_forward", "walk_backward", "strafe_left", "strafe_right",
    "turn_left", "turn_right", "fwd_left", "fwd_right",
}

RESEND_INTERVAL = 0.05  # 50 ms


@app.websocket("/camera/{robot_id}/{camera}")
async def camera_ws(ws: WebSocket, robot_id: str, camera: str):
    await ws.accept()

    if robot_id not in ROBOT_MAP:
        await ws.send_text(json.dumps({"type": "camera_error", "msg": "Robot invalido"}))
        await ws.close()
        return

    camera_name = camera.capitalize()
    if camera_name not in ("Upper", "Lower"):
        await ws.send_text(json.dumps({"type": "camera_error", "msg": "Camara invalida"}))
        await ws.close()
        return

    loop = asyncio.get_event_loop()
    queue: asyncio.Queue[bytes] = asyncio.Queue(maxsize=2)
    stop_event = threading.Event()
    port = 7777 if camera_name == "Upper" else 7778
    robot_ip = ROBOT_MAP[robot_id]["ip"]

    def reader():
        sock: socket.socket | None = None
        while not stop_event.is_set():
            try:
                if sock is None:
                    sock = connect_camf_stream(robot_ip, port, timeout=10.0)
                jpeg, _status, _bx, _by, _br, _spots = read_camf_frame(sock)
            except Exception:
                if sock is not None:
                    try:
                        sock.close()
                    except OSError:
                        pass
                    sock = None
                time.sleep(0.5)
                continue

            def push(frame: bytes = jpeg):
                if queue.full():
                    try:
                        queue.get_nowait()
                    except asyncio.QueueEmpty:
                        pass
                try:
                    queue.put_nowait(frame)
                except asyncio.QueueFull:
                    pass

            loop.call_soon_threadsafe(push)

        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    worker = threading.Thread(target=reader, daemon=True, name=f"cam-preview-{camera_name.lower()}")
    worker.start()

    try:
        while True:
            jpeg = await asyncio.wait_for(queue.get(), timeout=10.0)
            await ws.send_text(base64.b64encode(jpeg).decode("ascii"))
    except (WebSocketDisconnect, asyncio.TimeoutError):
        pass
    finally:
        stop_event.set()
        worker.join(timeout=1.5)

# == WebSocket handler =========================================================

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()

    selected_id: str | None = None
    active_cmd:  str | None = None
    loop = asyncio.get_event_loop()

    async def resend_loop():
        nonlocal active_cmd, selected_id
        while True:
            await asyncio.sleep(RESEND_INTERVAL)
            if active_cmd and selected_id:
                ctrl = controllers.get(selected_id)
                if ctrl and ctrl.connected:
                    fn = COMMANDS.get(active_cmd)
                    if fn:
                        await loop.run_in_executor(None, fn, ctrl)

    resend_task = asyncio.create_task(resend_loop())
    await ws.send_text(json.dumps({"type": "status", "robots": robots_status()}))

    try:
        while True:
            msg = json.loads(await ws.receive_text())
            kind = msg.get("type")

            if kind == "select":
                selected_id = msg.get("id", "")
                ctrl = await get_or_connect(selected_id) if selected_id in ROBOT_MAP else None
                ok = ctrl is not None and ctrl.connected
                if not ok:
                    await ws.send_text(json.dumps({
                        "type": "error",
                        "msg": f"No se pudo conectar a {selected_id}" if selected_id in ROBOT_MAP else "Robot invalido",
                        "robots": robots_status(),
                    }))
                await ws.send_text(json.dumps({
                    "type": "selected",
                    "id": selected_id,
                    "ok": ok,
                    "robots": robots_status(),
                }))

            elif kind == "down":
                cmd = msg.get("cmd", "")
                if cmd in COMMANDS and selected_id:
                    ctrl = await get_or_connect(selected_id) if selected_id in ROBOT_MAP else None
                    if ctrl and ctrl.connected:
                        active_cmd = cmd if cmd in CONTINUOUS_CMDS else None
                        await loop.run_in_executor(None, COMMANDS[cmd], ctrl)
                    else:
                        await ws.send_text(json.dumps({
                            "type": "error",
                            "msg": f"Robot {selected_id} desconectado, reintentando...",
                            "robots": robots_status(),
                        }))

            elif kind == "up":
                active_cmd = None
                if selected_id:
                    ctrl = controllers.get(selected_id)
                    if ctrl and ctrl.connected:
                        await loop.run_in_executor(None, ctrl.emergency_stop)

            elif kind == "ping":
                await ws.send_text(json.dumps({"type": "pong"}))

            elif kind == "record_start":
                if not selected_id or selected_id not in ROBOT_MAP:
                    await ws.send_text(json.dumps({"type": "record_error", "msg": "Sin robot seleccionado"}))
                else:
                    ctrl = await get_or_connect(selected_id)
                    if not ctrl or not ctrl.connected:
                        await ws.send_text(json.dumps({
                            "type": "record_error",
                            "msg": f"{selected_id} no esta conectado",
                        }))
                        continue
                    recorder = get_recorder(selected_id)
                    try:
                        result = await loop.run_in_executor(None, recorder.start, ROBOT_MAP[selected_id]["ip"], selected_id)
                    except Exception as e:
                        result = {"ok": False, "error": str(e)}
                    if result["ok"]:
                        await asyncio.sleep(0.5)
                        await ws.send_text(json.dumps({
                            "type": "record_started",
                            "robot_id": selected_id,
                            "upper": Path(result["upper"]).name,
                            "lower": Path(result["lower"]).name,
                            "robots": robots_status(),
                        }))
                    else:
                        await ws.send_text(json.dumps({
                            "type": "record_error",
                            "robot_id": selected_id,
                            "msg": result.get("error", "Error desconocido"),
                            "robots": robots_status(),
                        }))

            elif kind == "record_stop":
                if not selected_id or selected_id not in ROBOT_MAP:
                    await ws.send_text(json.dumps({"type": "record_error", "msg": "Sin robot seleccionado"}))
                    continue
                recorder = get_recorder(selected_id)
                result = await loop.run_in_executor(None, recorder.stop)
                await ws.send_text(json.dumps({
                    "type": "record_stopped",
                    "robot_id": selected_id,
                    "upper_frames": result["upper_frames"],
                    "lower_frames": result["lower_frames"],
                    "upper_path": result.get("upper_path", ""),
                    "lower_path": result.get("lower_path", ""),
                    "robots": robots_status(),
                }))

    except WebSocketDisconnect:
        pass
    finally:
        resend_task.cancel()
        if selected_id:
            ctrl = controllers.get(selected_id)
            if ctrl and ctrl.connected:
                await loop.run_in_executor(None, ctrl.emergency_stop)


# == HTML embebido =============================================================

HTML = """<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>SabanaHerons Control</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  :root {
    --bg: #0d0d0d; --surface: #1a1a1a; --border: #2a2a2a;
    --accent: #00b4d8; --green: #06d6a0; --red: #ef233c;
    --yellow: #ffd166; --text: #e0e0e0; --muted: #555;
    --disabled: #2a2a2a;
    --btn-h: 72px; --radius: 14px;
  }
  html, body {
    min-height: 100%; background: var(--bg); color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    -webkit-tap-highlight-color: transparent;
    touch-action: manipulation; user-select: none;
  }

  /* ── Header ── */
  header {
    display: flex; align-items: center; justify-content: space-between;
    padding: 14px 16px 10px; border-bottom: 1px solid var(--border);
  }
  .header-left { display: flex; align-items: center; gap: 10px; }
  header h1 { font-size: 1rem; font-weight: 700; }
  #ws-dot {
    width: 10px; height: 10px; border-radius: 50%;
    background: var(--red); flex-shrink: 0; transition: background .3s;
  }
  #ws-dot.ok { background: var(--green); }
  #back-btn {
    display: none; background: none; border: 1px solid var(--border);
    color: var(--muted); padding: 5px 12px; border-radius: 20px;
    font-size: .8rem; cursor: pointer;
  }
  #back-btn.visible { display: block; }

  /* ── Screens ── */
  .screen { display: none; }
  .screen.active { display: block; }

  /* ── Step 1: Robot selector ── */
  #step-title {
    text-align: center; padding: 28px 16px 16px;
    font-size: 1.1rem; font-weight: 600; color: var(--text);
  }
  #step-title small { display: block; font-size: .75rem; color: var(--muted); margin-top: 4px; font-weight: 400; }

  #robot-grid {
    display: grid; grid-template-columns: repeat(3, 1fr);
    gap: 10px; padding: 0 14px 20px;
  }
  .robot-btn {
    background: var(--surface); border: 2px solid var(--border);
    border-radius: var(--radius); padding: 16px 6px;
    display: flex; flex-direction: column; align-items: center; gap: 6px;
    cursor: pointer; transition: border-color .15s, background .15s;
    font-size: .75rem; color: var(--muted); font-weight: 500;
  }
  .robot-btn .dot { width: 10px; height: 10px; border-radius: 50%; background: var(--muted); }
  .robot-btn.connected { color: var(--text); }
  .robot-btn.connected .dot { background: var(--green); }
  .robot-btn.disabled { opacity: .45; }
  .robot-btn.disabled .dot { background: var(--disabled); }
  .robot-btn:active { background: #ffffff11; border-color: var(--accent); }

  /* ── Step 2: Controls ── */
  #robot-bar {
    display: flex; align-items: center; justify-content: space-between;
    padding: 10px 14px; background: var(--surface);
    border-bottom: 1px solid var(--border);
  }
  #robot-bar .name { font-weight: 700; font-size: .95rem; color: var(--accent); }
  #robot-bar .ip   { font-size: .7rem; color: var(--muted); }
  #robot-bar .status-dot {
    width: 10px; height: 10px; border-radius: 50%; background: var(--red);
  }
  #robot-bar .status-dot.ok { background: var(--green); }

  .section-label {
    font-size: .65rem; text-transform: uppercase; letter-spacing: 1px;
    color: var(--muted); padding: 10px 14px 5px;
  }
  .pad { padding: 0 10px 8px; }

  .grid3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; }
  .grid4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; }

  .btn {
    height: var(--btn-h); border: none; border-radius: var(--radius);
    font-size: 1.5rem; font-weight: 700; cursor: pointer;
    display: flex; flex-direction: column; align-items: center;
    justify-content: center; gap: 3px;
    background: var(--surface); color: var(--text);
    border: 1px solid var(--border);
    transition: background .08s, transform .08s;
    line-height: 1;
  }
  .btn .lbl { font-size: .55rem; text-transform: uppercase; letter-spacing: .5px; color: var(--muted); }
  .btn.pressed { background: #ffffff22; transform: scale(0.94); }

  .btn.danger  { background: #2a0a10; border-color: var(--red);    color: var(--red);    }
  .btn.warn    { background: #2a1f00; border-color: var(--yellow);  color: var(--yellow); }
  .btn.safe    { background: #002a1a; border-color: var(--green);   color: var(--green);  }
  .btn.accent  { background: #001f2a; border-color: var(--accent);  color: var(--accent); }
  .btn.stop-btn {
    height: var(--btn-h); background: #1a0a0a; border-color: var(--red);
    color: var(--red); font-size: .9rem; font-weight: 800; letter-spacing: 1px;
    border-width: 2px;
  }
  @keyframes blink-rec { 0%,100%{border-color:var(--red)} 50%{border-color:#ff000033} }
  .btn.recording {
    background: #2a0000; border-color: var(--red); color: var(--red);
    animation: blink-rec .9s ease-in-out infinite;
  }
  .rec-dot {
    display: inline-block; width: 10px; height: 10px; border-radius: 50%;
    background: currentColor; margin-right: 6px; vertical-align: middle;
  }

  /* ── Head section (collapsible) ── */
  #head-toggle {
    width: calc(100% - 28px); margin: 0 14px 8px;
    background: var(--surface); border: 1px solid var(--border);
    border-radius: var(--radius); color: var(--muted);
    font-size: .8rem; padding: 10px; cursor: pointer;
    display: flex; align-items: center; justify-content: center; gap: 6px;
  }
  #head-toggle .arrow { transition: transform .25s; }
  #head-toggle.open .arrow { transform: rotate(180deg); }

  #head-panel {
    display: none; padding: 0 10px 12px;
  }
  #head-panel.open { display: block; }

  #cam-toggle {
    width: calc(100% - 28px); margin: 0 14px 8px;
    background: var(--surface); border: 1px solid var(--border);
    border-radius: var(--radius); color: var(--muted);
    font-size: .8rem; padding: 10px; cursor: pointer;
    display: flex; align-items: center; justify-content: center; gap: 6px;
  }
  #cam-toggle .arrow { transition: transform .25s; }
  #cam-toggle.open .arrow { transform: rotate(180deg); }
  #cam-panel { display: none; padding: 0 10px 12px; }
  #cam-panel.open { display: block; }
  .camera-frame {
    width: 100%; min-height: 160px; object-fit: contain;
    background: #080808; border-radius: 12px; border: 1px solid var(--border);
  }
  .camera-row {
    display: grid; grid-template-columns: 1fr 1fr; gap: 6px; margin-top: 6px;
  }
  .camera-row .btn { height: 48px; font-size: .9rem; }
  .camera-row .btn.active { background: #ffffff18; }

  .head-pad {
    display: grid; grid-template-columns: 1fr 1fr 1fr;
    grid-template-rows: repeat(3, 62px); gap: 6px;
  }
  .hup    { grid-column: 2; grid-row: 1; }
  .hleft  { grid-column: 1; grid-row: 2; }
  .hctr   { grid-column: 2; grid-row: 2; }
  .hright { grid-column: 3; grid-row: 2; }
  .hdown  { grid-column: 2; grid-row: 3; }
  .head-pad .btn { height: 62px; font-size: 1.2rem; }

  /* ── Toast ── */
  #toast {
    position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
    background: #333; color: #fff; padding: 8px 20px; border-radius: 20px;
    font-size: .82rem; pointer-events: none; opacity: 0;
    transition: opacity .25s; white-space: nowrap; z-index: 99;
  }
  #toast.show { opacity: 1; }
</style>
</head>
<body>

<!-- Header -->
<header>
  <div class="header-left">
    <div id="ws-dot"></div>
    <h1>SabanaHerons</h1>
  </div>
  <button id="back-btn" onclick="goBack()">&#8592; Cambiar robot</button>
</header>

<!-- ═══ PASO 1: Seleccionar robot ═══ -->
<div id="screen-select" class="screen active">
  <div id="step-title">
    Selecciona un robot
    <small>Toca el robot que quieres controlar</small>
  </div>
  <div id="robot-grid"></div>
</div>

<!-- ═══ PASO 2: Controles ═══ -->
<div id="screen-control" class="screen">

  <!-- Barra del robot seleccionado -->
  <div id="robot-bar">
    <div>
      <div class="name" id="bar-name">-</div>
      <div class="ip"   id="bar-ip">-</div>
    </div>
    <div class="status-dot" id="bar-dot"></div>
  </div>

  <!-- Movimiento -->
  <div class="section-label">Movimiento</div>
  <div class="pad">
    <div class="grid3">
      <button class="btn warn"     data-cmd="turn_left"     data-hold="1">&#8634;<span class="lbl">Gira Izq</span></button>
      <button class="btn"          data-cmd="walk_forward"  data-hold="1">&#8593;<span class="lbl">Adelante</span></button>
      <button class="btn warn"     data-cmd="turn_right"    data-hold="1">&#8635;<span class="lbl">Gira Der</span></button>
      <button class="btn"          data-cmd="strafe_left"   data-hold="1">&#8592;<span class="lbl">Lateral</span></button>
      <button class="btn stop-btn" data-cmd="stand"         data-hold="0">STOP</button>
      <button class="btn"          data-cmd="strafe_right"  data-hold="1">&#8594;<span class="lbl">Lateral</span></button>
      <button class="btn"          data-cmd="fwd_left"      data-hold="1">&#8598;<span class="lbl">Diag Izq</span></button>
      <button class="btn"          data-cmd="walk_backward" data-hold="1">&#8595;<span class="lbl">Atras</span></button>
      <button class="btn"          data-cmd="fwd_right"     data-hold="1">&#8599;<span class="lbl">Diag Der</span></button>
    </div>
  </div>

  <!-- Acciones -->
  <div class="section-label">Acciones</div>
  <div class="pad">
    <div class="grid4">
      <button class="btn danger" data-cmd="kick_left"  data-hold="0">&#129466;<span class="lbl">Patada Izq</span></button>
      <button class="btn safe"   data-cmd="sit"        data-hold="0">&#129681;<span class="lbl">Sentar</span></button>
      <button class="btn accent" data-cmd="stand"      data-hold="0">&#129485;<span class="lbl">Stand</span></button>
      <button class="btn danger" data-cmd="kick_right" data-hold="0">&#129466;<span class="lbl">Patada Der</span></button>
    </div>
  </div>

  <!-- Grabación -->
  <div class="section-label">Grabación</div>
  <div class="pad">
    <button id="rec-btn" class="btn" style="width:100%;font-size:.9rem;font-weight:700;letter-spacing:.5px;" onclick="toggleRecord()">
      <span><span class="rec-dot" id="rec-dot"></span><span id="rec-lbl">Grabar cámaras</span></span>
      <span class="lbl" id="rec-sub"></span>
    </button>
  </div>

  <!-- Camara (colapsable) -->
  <button id="cam-toggle" onclick="toggleCamera()">
    Camara en vivo <span class="arrow">&#8964;</span>
  </button>
  <div id="cam-panel">
    <img id="cam-frame" class="camera-frame" alt="Sin senal">
    <div class="camera-row">
      <button id="cam-upper-btn" class="btn accent" onclick="setCameraView('upper')">&#9650;<span class="lbl">Superior</span></button>
      <button id="cam-lower-btn" class="btn accent active" onclick="setCameraView('lower')">&#9660;<span class="lbl">Inferior</span></button>
    </div>
    
    
  </div>

  <!-- Cabeza (colapsable) -->
  <button id="head-toggle" onclick="toggleHead()">
    Control de cabeza <span class="arrow">&#8964;</span>
  </button>
  <div id="head-panel">
    <div class="head-pad">
      <button class="btn accent hup"    data-cmd="head_up"     data-hold="0">&#8593;<span class="lbl">Arriba</span></button>
      <button class="btn accent hleft"  data-cmd="head_left"   data-hold="0">&#8592;<span class="lbl">Izq</span></button>
      <button class="btn        hctr"   data-cmd="head_center" data-hold="0">&#8857;<span class="lbl">Centro</span></button>
      <button class="btn accent hright" data-cmd="head_right"  data-hold="0">&#8594;<span class="lbl">Der</span></button>
      <button class="btn accent hdown"  data-cmd="head_down"   data-hold="0">&#8595;<span class="lbl">Abajo</span></button>
    </div>
  </div>

</div><!-- /screen-control -->

<div id="toast"></div>

<script>
// ── WebSocket ──────────────────────────────────────────────────────────────
const WS_URL = (location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + location.host + '/ws';
let ws = null;
let selectedId  = null;
let robotList   = [];
let recordingByRobot = {};
let camWs = null;
let camView = 'lower';
let camOpen = false;

function syncRecordingState() {
  recordingByRobot = Object.fromEntries(robotList.map(r => [r.id, !!r.recording]));
}

function selectedRobot() {
  return robotList.find(r => r.id === selectedId) || null;
}

function isSelectedRecording() {
  return !!(selectedId && recordingByRobot[selectedId]);
}

function connect() {
  ws = new WebSocket(WS_URL);
  ws.onopen  = () => { document.getElementById('ws-dot').classList.add('ok'); };
  ws.onclose = () => { document.getElementById('ws-dot').classList.remove('ok'); setTimeout(connect, 2000); };
  ws.onmessage = ({ data }) => {
    const msg = JSON.parse(data);
    if (msg.type === 'error') {
      toast(msg.msg);
      if (msg.robots) robotList = msg.robots;
    } else if (msg.type === 'status') {
      robotList = msg.robots;
      syncRecordingState();
      renderRobots();
    } else if (msg.type === 'selected') {
      robotList = msg.robots;
      syncRecordingState();
      renderRobots();
      if (msg.ok) {
        showControlScreen(msg.id, msg.robots);
      } else {
        toast('No se pudo conectar a ' + msg.id);
      }
    } else if (msg.type === 'record_started') {
      if (msg.robots) {
        robotList = msg.robots;
        syncRecordingState();
        renderRobots();
      } else if (msg.robot_id) {
        recordingByRobot[msg.robot_id] = true;
      }
      updateRecBtn();
      toast('Grabando → /home/limao/Desktop/SabanaHerons_Recordings');
      if (selectedId === msg.robot_id) {
        document.getElementById('rec-sub').textContent = msg.upper + ' · ' + msg.lower;
      }
    } else if (msg.type === 'record_stopped') {
      if (msg.robots) {
        robotList = msg.robots;
        syncRecordingState();
        renderRobots();
      } else if (msg.robot_id) {
        recordingByRobot[msg.robot_id] = false;
      }
      updateRecBtn();
      const frames = msg.upper_frames + msg.lower_frames;
      toast(
        'Grabación detenida · upper ' + msg.upper_frames +
        ' / lower ' + msg.lower_frames +
        ' · ' + frames + ' frames totales'
      );
      if (selectedId === msg.robot_id) {
        document.getElementById('rec-sub').textContent =
          (msg.upper_path ? msg.upper_path.split('/').pop() : '') +
          (msg.lower_path ? ' · ' + msg.lower_path.split('/').pop() : '');
      }
    } else if (msg.type === 'record_error') {
      if (msg.robots) {
        robotList = msg.robots;
        syncRecordingState();
        renderRobots();
      }
      updateRecBtn();
      toast('Error grabación: ' + msg.msg);
    }
  };
}
connect();

// ── Paso 1: seleccion ─────────────────────────────────────────────────────
function renderRobots() {
  const grid = document.getElementById('robot-grid');
  grid.innerHTML = '';
  robotList.forEach(r => {
  const btn = document.createElement('div');
  btn.className = 'robot-btn'
      + (r.connected ? ' connected' : '')
      + (r.recording ? ' recording' : '');
    btn.innerHTML = '<div class="dot"></div><div>' + r.name + (r.recording ? ' · REC' : '') + '</div>';
    btn.addEventListener('pointerdown', () => {
      if (!ws || ws.readyState !== WebSocket.OPEN) { toast('Sin conexion al servidor'); return; }
      selectedId = r.id;
      toast('Conectando a ' + r.name + '...');
      ws.send(JSON.stringify({ type: 'select', id: r.id }));
    });
    grid.appendChild(btn);
  });
}

// ── Paso 2: controles ─────────────────────────────────────────────────────
function showControlScreen(id, robots) {
  const info = robots.find(r => r.id === id) || { name: id, ip: '' };
  document.getElementById('bar-name').textContent = info.name;
  document.getElementById('bar-ip').textContent   = info.ip;
  document.getElementById('bar-dot').className    = 'status-dot ok';
  document.getElementById('screen-select').classList.remove('active');
  document.getElementById('screen-control').classList.add('active');
  document.getElementById('back-btn').classList.add('visible');
  const robot = selectedRobot();
  document.getElementById('rec-sub').textContent = robot && robot.recording ? 'Grabación activa' : '';
  updateRecBtn();
  document.getElementById('cam-frame').src = '';
  camView = 'lower';
  updateCamButtons();
}

function goBack() {
  stopCamera();
  camOpen = false;
  document.getElementById('cam-panel').classList.remove('open');
  document.getElementById('cam-toggle').classList.remove('open');
  document.getElementById('screen-control').classList.remove('active');
  document.getElementById('screen-select').classList.add('active');
  document.getElementById('back-btn').classList.remove('visible');
  if (ws && ws.readyState === WebSocket.OPEN && selectedId)
    ws.send(JSON.stringify({ type: 'up', cmd: 'stand' }));
}

// ── Grabación ─────────────────────────────────────────────────────────────
function toggleRecord() {
  if (!ws || ws.readyState !== WebSocket.OPEN) { toast('Sin conexión al servidor'); return; }
  if (!selectedId) { toast('Selecciona un robot primero'); return; }
  ws.send(JSON.stringify({ type: isSelectedRecording() ? 'record_stop' : 'record_start' }));
}

function updateRecBtn() {
  const btn = document.getElementById('rec-btn');
  const lbl = document.getElementById('rec-lbl');
  const dot = document.getElementById('rec-dot');
  const sub = document.getElementById('rec-sub');
  if (isSelectedRecording()) {
    btn.classList.add('recording');
    lbl.textContent = 'Detener grabación';
  } else {
    btn.classList.remove('recording');
    lbl.textContent = 'Grabar cámaras';
    if (!sub.textContent.startsWith('Grabación activa')) {
      sub.textContent = '';
    }
  }
}

// ── Control de cabeza (colapsable) ────────────────────────────────────────
function toggleHead() {
  const panel  = document.getElementById('head-panel');
  const toggle = document.getElementById('head-toggle');
  panel.classList.toggle('open');
  toggle.classList.toggle('open');
}

function updateCamButtons() {
  document.getElementById('cam-upper-btn').classList.toggle('active', camView === 'upper');
  document.getElementById('cam-lower-btn').classList.toggle('active', camView === 'lower');
}

function startCamera() {
  if (!selectedId) return;
  stopCamera();
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  camWs = new WebSocket(proto + '//' + location.host + '/camera/' + selectedId + '/' + camView);
  camWs.onmessage = ({ data }) => {
    document.getElementById('cam-frame').src = 'data:image/jpeg;base64,' + data;
  };
  camWs.onclose = () => {
    if (camOpen) {
      document.getElementById('cam-frame').removeAttribute('src');
    }
  };
}

function stopCamera() {
  if (camWs) {
    try { camWs.close(); } catch (_) {}
    camWs = null;
  }
}

function toggleCamera() {
  const panel = document.getElementById('cam-panel');
  const toggle = document.getElementById('cam-toggle');
  camOpen = !camOpen;
  panel.classList.toggle('open', camOpen);
  toggle.classList.toggle('open', camOpen);
  if (camOpen) startCamera();
  else stopCamera();
}

function setCameraView(view) {
  camView = view;
  updateCamButtons();
  if (camOpen) startCamera();
}

// ── Botones de control ────────────────────────────────────────────────────
document.querySelectorAll('[data-cmd]').forEach(btn => {
  const cmd  = btn.dataset.cmd;
  const hold = btn.dataset.hold === '1';

  btn.addEventListener('contextmenu', e => e.preventDefault());

  function send(type) {
    if (!ws || ws.readyState !== WebSocket.OPEN) { toast('Sin conexion'); return; }
    if (!selectedId) { toast('Sin robot seleccionado'); return; }
    ws.send(JSON.stringify({ type, cmd }));
  }

  btn.addEventListener('pointerdown', e => {
    e.preventDefault();
    btn.classList.add('pressed');
    btn.setPointerCapture(e.pointerId);
    send('down');
  });

  if (hold) {
    btn.addEventListener('pointerup',    () => { btn.classList.remove('pressed'); send('up'); });
    btn.addEventListener('pointerleave', () => {
      if (btn.classList.contains('pressed')) { btn.classList.remove('pressed'); send('up'); }
    });
  } else {
    btn.addEventListener('pointerup', () => btn.classList.remove('pressed'));
  }
});

// ── Toast ─────────────────────────────────────────────────────────────────
let toastTimer = null;
function toast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2500);
}

// ── Keepalive ─────────────────────────────────────────────────────────────
setInterval(() => {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ type: 'ping' }));
}, 10000);
</script>
</body>
</html>"""


@app.get("/", response_class=HTMLResponse)
async def index():
    return HTML


# == Entry point ===============================================================

if __name__ == "__main__":
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.0.49.1", 80))
        local_ip = s.getsockname()[0]
        s.close()
    except Exception:
        local_ip = "localhost"

    print("\n" + "=" * 52)
    print("  SabanaHerons Web Control")
    print("=" * 52)
    print(f"  Abre en tu celular:  http://{local_ip}:8080")
    print("  (El celular debe estar en la misma red WiFi)")
    print("=" * 52 + "\n")

    uvicorn.run(app, host="0.0.0.0", port=8080, log_level="warning")
