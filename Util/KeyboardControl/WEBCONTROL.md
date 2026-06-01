# SabanaHerons WebControl

Sistema de control remoto para robots Nao (framework B-Human) accesible desde cualquier celular en la red local. Compuesto por dos módulos: `keyboard_control.py` (protocolo B-Human) y `web_control.py` (servidor HTTP/WebSocket).

---

## Arquitectura general

```
Celular / navegador
       │  WebSocket (ws://laptop:8080/ws)
       ▼
web_control.py  ──── FastAPI + Uvicorn ────────────────────────────────────┐
       │                                                                    │
       │  instancia BHumanController por robot                             │
       ▼                                                                    │
keyboard_control.py                                                         │
  └── BHumanController                                                      │
         │  TCP port 9999 (protocolo debug B-Human)                        │
         ▼                                                                  │
    Robot Nao                                                               │
    (B-Human firmware)  ◄──────────────────────────────────────────────────┘
```

### Descripción de capas

| Capa | Archivo | Responsabilidad |
|------|---------|----------------|
| UI móvil | `web_control.py` (HTML inline) | Interfaz touch, selección de robot, botones de control |
| Servidor web | `web_control.py` | FastAPI, endpoints HTTP y WebSocket, pool de controladores |
| Protocolo B-Human | `keyboard_control.py` | Serialización binaria, TCP, MessageQueue |

---

## `keyboard_control.py` — Protocolo B-Human

### Protocolo TCP (puerto 9999)

El robot expone un servidor TCP en el puerto 9999 que habla el protocolo **MessageQueue** de B-Human. Cada paquete tiene el formato:

```
[ 4 bytes LE: tamaño del payload ] [ payload: QueueHeader(8B) + mensajes ]
```

Cada mensaje dentro de la cola:

```
[ 1 byte: msg_id ] [ 3 bytes LE: tamaño del body ] [ body ]
```

### Handshake

Al conectar se envía un paquete especial de 12 bytes que solicita `numOfDataMessageIDs`. El robot responde con el ID 0 (`idNumOfDataMessageIDs`), confirmando que la conexión está activa.

### IDs de mensaje relevantes

| ID | Nombre | Uso |
|----|--------|-----|
| 0 | `idNumOfDataMessageIDs` | Respuesta del robot al handshake |
| 80 | `idDebugDataChangeRequest` | Inyectar/modificar representaciones |
| 95 | `idThread` | Seleccionar el hilo destino (Cognition / Motion) |
| 113 | `idDebugRequest` | Activar/desactivar debug responses |
| 114 | `idDebugResponse` | Tabla de debug requests disponibles |
| 28 | `idCameraImage` | Imagen cruda de cámara |
| 33 | `idJPEGImage` | Imagen JPEG comprimida |

### Cómo se envían comandos

Para modificar una representación (ej. `MotionRequest`) se construye un paquete con dos mensajes:

1. **`idThread`** — selecciona el hilo destino (e.g. `"Cognition"`)
2. **`idDebugDataChangeRequest`** — nombre de la representación + datos binarios serializados

```python
# Ejemplo simplificado del helper interno:
_set_representation_packet("Cognition", "representation:MotionRequest", binary_data)
```

### Clase `BHumanController`

```
BHumanController(robot_ip: str)
├── connect(timeout=5.0)       # Establece TCP, handshake, envía flags de kick
├── disconnect()
├── walk(x, y, rot)            # WALK_AT_RELATIVE_SPEED, valores en [-1, 1]
├── stand()                    # STAND normal
├── emergency_stop(bursts=5)   # STAND repetido 5 veces para parada rápida
├── sit()                      # PLAY_DEAD
├── kick_left() / kick_right() # WALK_TO_BALL_AND_KICK + stand() en 2s
├── head_up/down/left/right/center()
└── heartbeat()                # Paquete vacío para mantener la conexión
```

Al conectar y antes de cada comando de cabeza se fija `OdometryDataPreview` y
`OdometryTranslationRequest` en cero dentro del hilo `Motion`. Esto mantiene los
comandos manuales de cabeza en el mismo marco que el `MotionRequest` enviado por
WebControl, cuyo `odometryData` también es cero. Sin este ajuste, después de que
el robot gira o camina, `HeadMotionEngine` puede sumar una compensación de
odometría al pan y convertir `head_center` (`pan = 0`) en una orden hacia un
límite lateral.

Los **comandos de movimiento** (walk) deben re-enviarse cada ~50 ms porque el robot los aplica por ciclo. Si dejan de llegar, el robot aplica su comportamiento por defecto. El servidor gestiona esto con el `resend_loop`.

### Serialización de representaciones

`MotionRequest` se serializa campo a campo en binario little-endian, espejando exactamente el layout de `Src/Representations/MotionControl/MotionRequest.h`. Cualquier desincronización en el layout producirá comportamiento erróneo sin error de red.

`HeadMotionRequest` se serializa análogamente desde `Src/Representations/MotionControl/HeadMotionRequest.h`.

---

## `web_control.py` — Servidor Web

### Inicio

```bash
pip install fastapi "uvicorn[standard]"
python3 web_control.py
# → Servidor en http://<ip_laptop>:8080
```

El script detecta automáticamente la IP local en la red `10.0.49.x` e imprime la URL.

### Endpoints

| Endpoint | Tipo | Descripción |
|----------|------|-------------|
| `GET /` | HTTP | Sirve la UI HTML completa (embedded) |
| `WS /ws` | WebSocket | Canal bidireccional de control |

### Protocolo WebSocket

**Cliente → Servidor:**

| Mensaje | Campos | Acción |
|---------|--------|--------|
| `{"type":"select","id":"ronaoldinho"}` | id del robot | Conecta al robot si no está conectado |
| `{"type":"down","cmd":"walk_forward"}` | cmd | Ejecuta comando; si es continuo, activa `active_cmd` |
| `{"type":"up","cmd":"walk_forward"}` | cmd | Detiene el comando continuo, envía `emergency_stop` |
| `{"type":"ping"}` | — | Keepalive |

**Servidor → Cliente:**

| Mensaje | Cuándo |
|---------|--------|
| `{"type":"status","robots":[...]}` | Al conectar el WebSocket |
| `{"type":"selected","id":"...","ok":bool,"robots":[...]}` | Tras `select` |
| `{"type":"pong"}` | Tras `ping` |
| `{"type":"error","msg":"...","robots":[...]}` | Error de conexión al robot |

### Pool de controladores y re-conexión

```
controllers: dict[str, BHumanController]   # pool global
_connect_locks: dict[str, asyncio.Lock]    # evita conexiones duplicadas simultáneas
```

`get_or_connect(robot_id)` revisa si ya hay un controlador activo. Si no, adquiere el lock y conecta en un executor (para no bloquear el event loop). Si el lock ya está tomado, retorna `None` inmediatamente (reconexión en curso).

### Comandos continuos vs. discretos

```python
CONTINUOUS_CMDS = {"walk_forward", "walk_backward", "strafe_left", "strafe_right",
                   "turn_left", "turn_right", "fwd_left", "fwd_right"}
```

Los comandos continuos se re-envían cada 50 ms vía `resend_loop` (tarea asyncio por cliente). Los discretos (kick, sit, head) se ejecutan una sola vez.

### Registro de robots

```python
ROBOTS = [
    {"id": "ronaoldinho", "name": "ronaoldinho", "ip": "10.0.49.2"},
    # ... hasta 10.0.49.10
]
```

Para agregar un robot nuevo, añadir una entrada a esta lista y flashear el robot con la IP correspondiente.

---

## UI móvil

La interfaz está embebida en la cadena `HTML` dentro de `web_control.py`. Es una SPA sin dependencias externas.

**Flujo de pantallas:**
1. **Selección de robot** — grid 3×3, punto verde = conectado
2. **Panel de control** — se activa tras seleccionar con éxito

**Secciones de control:**
- **Movimiento** — 9 botones (adelante, atrás, laterales, diagonales, STOP)
- **Acciones** — patada izquierda/derecha, stand, sit
- **Cabeza** — panel colapsable con D-pad para pan/tilt

Todos los botones usan `pointerdown`/`pointerup` (compatible con touch y mouse). Los botones de movimiento usan `setPointerCapture` para seguir recibiendo eventos si el dedo sale del área.

---

## Cómo ejecutar

```bash
# Requisitos
pip install fastapi "uvicorn[standard]"

# Asegurarse de estar en la red del robot (10.0.49.x)
python3 Util/KeyboardControl/web_control.py

# O para control por teclado (una sola IP)
python3 Util/KeyboardControl/keyboard_control.py 10.0.49.2
```

---

## Limitaciones conocidas

- No hay autenticación. Cualquier dispositivo en la red puede controlar el robot.
- Si el robot se reinicia, el controlador queda en estado `connected=False` y el próximo comando desencadena reconexión.
- La serialización binaria de `MotionRequest` es frágil: si el firmware del robot cambia el layout del struct, los comandos fallarán silenciosamente.

---

---

# Handoff: Suscripción a cámara para recolección de dataset

## Objetivo

Extender el servidor WebControl para exponer frames de la cámara del robot Nao en tiempo real, de modo que puedan capturarse y almacenarse como dataset para entrenar un modelo de reconocimiento del nuevo balón.

---

## Cómo funciona el stream de cámara en B-Human

El robot B-Human tiene un mecanismo de **Debug Requests**: cualquier cliente conectado puede activar una respuesta periódica de datos enviando un mensaje `idDebugRequest` con el nombre de la representación deseada.

Para imágenes JPEG, la representación es `representation:JPEGImage`, definida en:
- `Src/Representations/Infrastructure/JPEGImage.h`
- Activada en: `Src/Modules/Infrastructure/LogDataProvider/LogDataProvider.cpp`:
  ```cpp
  DEBUG_RESPONSE("representation:JPEGImage")
      OUTPUT(idJPEGImage, bin, JPEGImage(cameraImage));
  ```

Cuando está activa, el robot envía un frame por ciclo de cognición (~30 fps) como mensaje `idJPEGImage` (ID=33) en el flujo TCP normal.

---

## Implementación paso a paso

### Paso 1: Enviar el Debug Request para activar el stream

Hay que construir un mensaje `idDebugRequest` (ID=113). El formato del body es una cadena B-Human seguida de un bool (enabled):

```python
ID_DEBUG_REQUEST = 113

def _enable_jpeg_stream(sock, thread: str = "Upper") -> None:
    """Activa el stream de JPEGImage en el hilo Upper o Lower."""
    request_name = "representation:JPEGImage"
    # Body: string B-Human + bool enable
    body = _str(request_name) + bytes([1])  # 1 = enable
    pkt = _tcp_packet(_make_queue([
        (ID_THREAD, _str(thread)),
        (ID_DEBUG_REQUEST, body),
    ]))
    sock.sendall(pkt)

def _disable_jpeg_stream(sock, thread: str = "Upper") -> None:
    request_name = "representation:JPEGImage"
    body = _str(request_name) + bytes([0])  # 0 = disable
    pkt = _tcp_packet(_make_queue([
        (ID_THREAD, _str(thread)),
        (ID_DEBUG_REQUEST, body),
    ]))
    sock.sendall(pkt)
```

> **Nota:** El hilo puede ser `"Upper"` o `"Lower"` dependiendo de qué cámara se quiere (cámara superior o inferior del Nao). Para dataset de balón conviene probar ambas.

---

### Paso 2: Parsear los frames entrantes en `_process_packet`

En `BHumanController._process_packet` solo se maneja actualmente `idNumOfDataMessageIDs`. Hay que agregar el case para `idJPEGImage`:

```python
ID_JPEG_IMAGE = 33

class BHumanController:
    def __init__(self, robot_ip: str):
        # ... código existente ...
        self._frame_callback = None   # callback(jpeg_bytes: bytes) -> None
        self._frame_thread: str = "Upper"

    def set_frame_callback(self, callback, thread: str = "Upper"):
        """Registra un callback que recibe cada frame JPEG crudo."""
        self._frame_callback = callback
        self._frame_thread = thread

    def start_camera_stream(self):
        _enable_jpeg_stream(self.sock, self._frame_thread)

    def stop_camera_stream(self):
        _disable_jpeg_stream(self.sock, self._frame_thread)

    def _process_packet(self, data: bytes):
        if len(data) < 8:
            return
        msg_body = data[8:]
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

            elif msg_id == ID_JPEG_IMAGE and self._frame_callback:
                # payload = JPEGImage serializada por B-Human Streamable.
                # Los primeros bytes son metadata (size:uint32, width:int32,
                # height:int32) y luego los bytes JPEG.
                if len(payload) > 12:
                    jpeg_bytes = payload[12:]   # saltar header de JPEGImage
                    self._frame_callback(jpeg_bytes)
```

> **Importante:** El offset exacto del header de `JPEGImage` debe verificarse contra `Src/Representations/Infrastructure/JPEGImage.h`. La clase tiene `size` (uint32) + `width` (int32) + `height` (int32) = 12 bytes antes del payload JPEG. Validar con un frame real.

---

### Paso 3: Nuevo endpoint en `web_control.py`

Agregar un endpoint WebSocket dedicado a la cámara, o extender el endpoint existente. La opción más limpia es un endpoint separado:

```python
import base64
import asyncio

# Conjunto de clientes suscritos a la cámara por robot_id
camera_subscribers: dict[str, set[WebSocket]] = {}

@app.websocket("/camera/{robot_id}")
async def camera_ws(ws: WebSocket, robot_id: str):
    """
    Conectar desde el cliente:
      ws = new WebSocket("ws://laptop:8080/camera/ronaoldinho")
      ws.onmessage = ({data}) => {
          const img = document.getElementById('frame');
          img.src = 'data:image/jpeg;base64,' + data;
      }
    """
    await ws.accept()

    ctrl = await get_or_connect(robot_id)
    if not ctrl:
        await ws.send_text(json.dumps({"error": "no_robot"}))
        await ws.close()
        return

    # Registrar suscriptor
    subs = camera_subscribers.setdefault(robot_id, set())
    subs.add(ws)

    loop = asyncio.get_event_loop()
    frame_queue: asyncio.Queue = asyncio.Queue(maxsize=5)

    def on_frame(jpeg_bytes: bytes):
        """Llamado desde el hilo recv del BHumanController."""
        try:
            frame_queue.put_nowait(jpeg_bytes)
        except asyncio.QueueFull:
            pass  # descartar si el cliente va lento

    ctrl.set_frame_callback(on_frame)
    ctrl.start_camera_stream()

    try:
        while True:
            jpeg = await asyncio.wait_for(frame_queue.get(), timeout=5.0)
            b64 = base64.b64encode(jpeg).decode("ascii")
            await ws.send_text(b64)
    except (WebSocketDisconnect, asyncio.TimeoutError):
        pass
    finally:
        subs.discard(ws)
        if not subs:
            ctrl.stop_camera_stream()
            ctrl.set_frame_callback(None)
```

> **Nota sobre thread-safety:** `on_frame` se llama desde el hilo `_recv_loop` del controlador. `asyncio.Queue.put_nowait` es seguro solo si se llama desde el mismo thread del event loop, de lo contrario usar `loop.call_soon_threadsafe(frame_queue.put_nowait, jpeg_bytes)`.

Corrección del callback para cross-thread:

```python
def on_frame(jpeg_bytes: bytes):
    loop.call_soon_threadsafe(
        lambda: frame_queue.put_nowait(jpeg_bytes)
        if not frame_queue.full() else None
    )
```

---

### Paso 4: Script de captura para dataset

```python
#!/usr/bin/env python3
"""
capture_dataset.py — Captura frames de la cámara del Nao para dataset.

Uso:
    python3 capture_dataset.py <robot_ip> [--camera upper|lower] [--out ./dataset]
"""
import sys
import time
import argparse
from pathlib import Path
from keyboard_control import BHumanController, _enable_jpeg_stream, _disable_jpeg_stream

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("robot_ip")
    parser.add_argument("--camera", choices=["upper", "lower"], default="upper")
    parser.add_argument("--out", default="./dataset")
    parser.add_argument("--fps", type=float, default=5.0,
                        help="Frames por segundo a guardar (throttle)")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    thread = args.camera.capitalize()  # "Upper" o "Lower"
    frame_count = 0
    last_save = 0.0
    interval = 1.0 / args.fps

    def on_frame(jpeg_bytes: bytes):
        nonlocal frame_count, last_save
        now = time.time()
        if now - last_save < interval:
            return
        last_save = now
        ts = int(now * 1000)
        path = out_dir / f"{ts}_{thread.lower()}_{frame_count:06d}.jpg"
        path.write_bytes(jpeg_bytes)
        frame_count += 1
        print(f"\r  Frames: {frame_count}  →  {path.name}", end="", flush=True)

    robot = BHumanController(args.robot_ip)
    robot.connect()
    robot.set_frame_callback(on_frame, thread=thread)
    robot.start_camera_stream()

    print(f"Capturando desde cámara {thread} a {args.fps} fps. Ctrl+C para detener.\n")
    try:
        while robot.connected:
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        robot.stop_camera_stream()
        robot.disconnect()
        print(f"\n\nTotal frames guardados: {frame_count} en {out_dir}")

if __name__ == "__main__":
    main()
```

---

### Paso 5: Integrar el visor en la UI móvil

En el HTML de `web_control.py`, agregar una sección colapsable de cámara (similar al panel de cabeza):

```html
<!-- Dentro de #screen-control, después del head-panel -->
<button id="cam-toggle" onclick="toggleCamera()">
  Cámara en vivo <span class="arrow">&#8964;</span>
</button>
<div id="cam-panel" style="display:none; padding: 0 10px 12px;">
  <img id="cam-frame" style="width:100%; border-radius:10px; background:#111;"
       alt="Sin señal">
  <div style="display:flex; gap:6px; margin-top:6px;">
    <button class="btn accent" style="flex:1; height:44px; font-size:.8rem;"
            onclick="setCamThread('Upper')">Superior</button>
    <button class="btn accent" style="flex:1; height:44px; font-size:.8rem;"
            onclick="setCamThread('Lower')">Inferior</button>
  </div>
</div>
```

```javascript
// JavaScript adicional en el bloque <script>
let camWs = null;
let camThread = 'Upper';

function toggleCamera() {
  const panel = document.getElementById('cam-panel');
  const toggle = document.getElementById('cam-toggle');
  const open = panel.style.display === 'none';
  panel.style.display = open ? 'block' : 'none';
  toggle.querySelector('.arrow').style.transform = open ? 'rotate(180deg)' : '';
  if (open && selectedId) startCamera();
  else stopCamera();
}

function setCamThread(thread) {
  camThread = thread;
  stopCamera();
  startCamera();
}

function startCamera() {
  if (!selectedId) return;
  stopCamera();
  const url = (location.protocol === 'https:' ? 'wss:' : 'ws:')
              + '//' + location.host + '/camera/' + selectedId;
  camWs = new WebSocket(url);
  camWs.onmessage = ({ data }) => {
    document.getElementById('cam-frame').src = 'data:image/jpeg;base64,' + data;
  };
}

function stopCamera() {
  if (camWs) { camWs.close(); camWs = null; }
}
```

---

## Consideraciones para el dataset

| Aspecto | Recomendación |
|---------|--------------|
| **FPS de captura** | 3-5 fps es suficiente para dataset estático; 10+ para secuencias |
| **Cámara** | Inferior (Lower) para cuando el balón está cerca; Superior para campo |
| **Variedad** | Capturar con distintas iluminaciones, distancias y orientaciones del balón |
| **Anotación** | Usar CVAT, LabelImg o Roboflow para bounding boxes |
| **Formato** | YOLO v8 o COCO JSON son los más compatibles con pipelines modernos |
| **Cantidad mínima** | ~500-1000 imágenes por clase para fine-tuning de un modelo preentrenado |
| **Augmentation** | Flip horizontal, variación de brillo/saturación para compensar falta de datos |

---

## Archivos relevantes del framework

```
Src/Representations/Infrastructure/
  ├── CameraImage.h        # Imagen cruda YUV422
  └── JPEGImage.h          # Compresión JPEG sobre CameraImage

Src/Modules/Infrastructure/
  └── LogDataProvider/LogDataProvider.cpp   # DEBUG_RESPONSE("representation:JPEGImage")

Src/Libs/Streaming/MessageIDs.h            # IDs de todos los mensajes (idJPEGImage=33)

Util/KeyboardControl/
  ├── keyboard_control.py   # BHumanController + protocolo TCP
  └── web_control.py        # Servidor FastAPI + UI
```
