# Entorno de entrenamiento RL (`RL/`)

## Resumen

Este documento describe el **entorno de simulación** que usa `RL/` para entrenar
las políticas PPO/MAPPO que luego se exportan a ONNX y se integran en
`SabanaHerons2026` (ver
[DOCUMENTACION_FINAL_RL_SABANAHERONS.md](./DOCUMENTACION_FINAL_RL_SABANAHERONS.md)
para la mitad de producción). Aquí el foco es exclusivamente el lado
experimental: cómo se construye la observación, cómo se decodifica la acción,
cómo se calcula la recompensa y cómo se generan los mundos de cada episodio.

`RL/` y `SabanaHerons2026/` son repos hermanos en el mismo directorio padre.
El entorno RL no simula física propia: abre una instancia real de **SimRobot**
(el simulador de B-Human) mediante el binding C++ `pybh` y usa memoria
compartida para mandar acciones y leer observaciones. RL nunca controla
motores directamente; solo pide un `SkillRequest` de alto nivel (`walk`,
`shoot`, `dribble`, ...) que B-Human convierte en `MotionRequest` real.

```text
RealSimRobotEnv (PettingZoo ParallelEnv)
        |
        v
PyBHBackend.step(StructuredSkillAction)
        |
        v
pybh.rl_set_action()   <- frontera de memoria compartida
        |
        v
RLSkillProvider.cpp -> SkillRequest -> SkillBehaviorControl -> MotionEngine -> SimRobot
```

Las observaciones viajan el camino inverso: `pybh.rl_get_obs()` ->
`BackendObservation` -> `RealSimRobotEnv._encode_obs()` -> vector de floats
que consume la política.

## Backend: `PyBHBackend` (`pybh_backend.py`)

`PyBHBackend` encapsula todo el ciclo de vida de la escena de SimRobot:

- **Modos**: `simrobot` (SimRobot embebido dentro del mismo proceso Python,
  headless) y `simrobot_visible` (instancia externa con ventana, útil para
  depurar en vivo).
- **Perfiles**: `2d` y `3d`, cada uno apuntando a un `.ros2d`/`.ros2` distinto
  bajo `Config/Scenes/` de `SabanaHerons2026` (`SIMROBOT_SCENE_PRESETS` /
  `SIMROBOT_VISIBLE_SCENE_PRESETS` en `config.py`). El perfil 3D visible tiene
  variantes adicionales (`3d_visible`, `3d_visible_ci60`) que ajustan la tasa
  de fotogramas de cámara para no chocar con `obs_timeout_ms`.
- **`frames_per_step`**: cuántos frames de SimRobot avanza cada `step()` antes
  de devolver observación (control sobre cuántas veces reacciona B-Human por
  cada decisión RL).

El ciclo de vida público es:

| Método | Qué hace |
| --- | --- |
| `start()` | Carga o conecta la escena de SimRobot; en 3D aplica un "preflight" de variantes Qt/OpenGL antes de arrancar. |
| `reset(world_spec)` | Reposiciona pelota, robot propio, compañeros y rivales según un `WorldSpec`; espera a que la observación se estabilice. |
| `step(action)` | Traduce la acción estructurada al formato del bridge, llama `rl_set_action`, avanza `frames_per_step` frames y lee `rl_get_obs`. |
| `set_dynamic_world(...)` | Reposiciona obstáculos dinámicos a mitad de episodio (currículo con rivales en movimiento). |
| `read_obs()` | Lee el estado actual sin avanzar la simulación. |
| `close()` | Descarga la escena o cierra el proceso visible. |

### `WorldSpec` — contrato de reset

```python
@dataclass(frozen=True)
class WorldPlayerSpec:
    number: int
    x: float
    y: float
    theta: float = 0.0
    upright: bool = True

@dataclass(frozen=True)
class WorldSpec:
    ball_x: float
    ball_y: float
    robot_x: float
    robot_y: float
    robot_theta: float
    teammates: tuple[WorldPlayerSpec, ...] = ()
    opponents: tuple[WorldPlayerSpec, ...] = ()
```

Todo mundo de entrenamiento —currículo de un solo agente, escenarios de
producción o los mundos multiagente— se expresa como un `WorldSpec`. Esto es
lo único que necesita `PyBHBackend.reset()` para saber dónde poner cada pieza
del campo; el currículo (ver más abajo) solo decide *qué* `WorldSpec` samplear
para cada episodio, no cómo se aplica.

### `BackendObservation`

Es el objeto que produce el bridge en cada `step`/`reset`: pose del robot,
percepción de pelota (posición relativa, velocidad, posición final predicha,
tiempo desde la última vez vista, flags de confianza natural vs.
exportada/corregida), distancias a obstáculos, y flags de estado del bridge
(`obs_ready`, `requested_skill`, `requested_pass_target`). El entorno nunca
expone este objeto crudo a la política: siempre pasa por `_encode_obs()`.

## `RealSimRobotEnv` (`environment.py`)

Es un `ParallelEnv` de PettingZoo con un solo agente lógico, `player_1`
(el entorno multiagente de equipo se construye *encima* de esto, ver más
abajo — no lo reemplaza).

```python
observation_spaces = {"player_1": Box(-inf, inf, shape=(OBS_SIZE,))}  # OBS_SIZE = 41 (47 con extensión de equipo)
action_spaces      = {"player_1": Box(low=[0, -1, -1, -1, 0], high=[7, 1, 1, 1, max_pass_target])}
```

### `reset(seed, options)`

1. Si hay escenarios de producción habilitados (`RL_PRODUCTION_SCENARIO_RESETS=1`)
   y no se pidió un `stage_id`/`world_spec` explícito, samplea un
   `ProductionScenario` ponderado (ver sección de currículo).
2. Resuelve el `stage_id` del episodio (`_sample_episode_stage_id`) — normalmente
   el stage configurado, salvo probabilidad de "interleave" hacia el stage de
   finalización (ver abajo).
3. Samplea un `WorldSpec` para ese stage (`_sample_episode_world_spec`):
   mezcla de reset genérico del stage, secuencia de finalización
   (`finish_sequence`), aproximación pre-tiro (`preshoot_dribble`) o remate
   puro (`finish`), según probabilidades por stage.
4. Llama `backend.reset(world_spec)`, resetea el `SkillGate` del episodio y
   codifica la primera observación.

### `step(actions)`

El flujo dentro de un step es, en orden:

1. Decodifica la acción cruda (`dict` o `np.ndarray` de 5 floats) a
   `StructuredSkillAction`.
2. `_decode_residual_policy_action`: los parámetros continuos que la política
   emite son **residuales** relativos a un ancla calculada por reglas
   (`build_fallback_action_targets`), no coordenadas absolutas de campo. Esto
   acota drásticamente el espacio de búsqueda (la política solo tiene que
   aprender un ajuste fino sobre una aproximación geométrica razonable).
3. *Handoff override*: si la política pide `walk` pero las condiciones
   geométricas de recepción de pelota ya se cumplen
   (`_should_force_dribble_handoff`), el entorno fuerza `dribble` en su lugar
   para no perder el momento de transición walk→dribble.
4. *Target repair* (`_repair_execution_targets`): corrige objetivos que
   llevarían al robot fuera de campo o a una pose degenerada.
5. `project_action`: proyecta la acción final a los rangos válidos del
   backend.
6. `backend.step(...)` avanza la simulación real y devuelve la nueva
   `BackendObservation`.
7. Si el stage tiene obstáculos dinámicos, actualiza sus posiciones
   (`get_dynamic_world_update` / `backend.set_dynamic_world`).
8. Calcula `done` (gol, timeout, robot caído/atascado) y arma el
   `StepContext` con toda la evidencia del step (skill ejecutada vs. pedida,
   historial de skills, si hubo colisión, si el "takeover" está comprometido
   con gracia post-toma, etc.).
9. `reward.evaluate_step(step_ctx, stage.reward_weights)` calcula la
   recompensa (ver sección Reward).
10. `SkillGate.step(obs)` recalcula qué skills están armadas para el *siguiente*
    step y las hornea en la observación codificada.
11. `_encode_obs(obs)` produce el vector final; se actualiza el tracker de
    métricas del episodio (usado por la evaluación y la promoción de stage).

## Espacio de observación (`observation.py`, `OBS_SIZE = 41`, extensión de equipo a 47)

`ObsIndex` es un `IntEnum` que fija el orden exacto de cada columna. Romper
este orden invalida cualquier checkpoint (`.pt`) u ONNX ya entrenado.

| Rango | Bloque | Contenido |
| --- | --- | --- |
| `[0:3]` | Pose propia | `robot_x`, `robot_y`, `robot_theta` |
| `[3:13]` | Percepción de pelota | posición relativa, posición final predicha, velocidad, tiempo desde vista/desaparecida, % visto, consistencia con el estado de juego |
| `[13:17]` | Scoring | `can_score_now`, calidad de tiro sin obstáculos, apertura de tiro con obstáculos, cantidad de opciones de pase |
| `[17:23]` | Proximidad | distancia al compañero/rival/obstáculo incierto más cercano (global y frontal) |
| `[23:26]` | **Gate de skills (Diseño A)** | `shoot_armed`, `dribble_armed`, `shoot_arm_progress` |
| `[26:41]` | Contexto multiagente | 3 posiciones relativas de compañeros, estimación de pelota de equipo, flags de compañero enganchado a la pelota, vector al arco rival |
| `[41:44]` | Gate de equipo (extensión Diseño A) | `pass_armed`, `observe_armed`, `pass_arm_progress` |
| `[44:47]` | Rol one-hot | `is_striker`, `is_open_support`, `is_off_ball_support` (exactamente uno en 1.0 por agente RL; el arquero siempre lee 0 en los tres) |

Las columnas 26-46 se agregaron después de las 0-25 originales; por eso el
padding de checkpoints antiguos (26 dims) a la observación actual se hace con
ceros en la primera capa de la red, sin reentrenar desde cero (ver
`docs/RL_FLUJO_ENTRENAMIENTO.md`).

## Espacio y contrato de acción (`action.py`)

```python
class SkillType(IntEnum):
    stand, walk, shoot, pass_, dribble, block, mark, observe = range(8)
```

`SKILL_ORDER` fija el orden público (`pass_` se expone como `"pass"` porque es
palabra reservada en Python). La acción plana que ve `skrl`/PettingZoo es un
vector de 5 floats: `[skill_type, target_x, target_y, target_theta,
pass_target]`, donde `target_x/y/theta` son **residuales normalizados en
[-1, 1]** sobre el ancla geométrica, no coordenadas de campo.

No todos los parámetros continuos son relevantes para cada skill.
`ACTION_PARAM_MASKS` fija qué dimensiones están realmente "vivas" por skill
(por ejemplo `shoot` no usa ningún parámetro continuo, `pass` solo usa
`pass_target`, `walk`/`dribble` usan `y` y opcionalmente `theta`). Esta máscara
se comparte entre la pérdida de política, la pérdida de BC y la decodificación
del backend, para que una dimensión "muerta" esté realmente fijada en cero en
las tres rutas.

## `SkillGate` — enmascarado forbid-invalid con estado (`gate.py`)

El gate es la pieza que decide, cada step, qué skills son *seguras/posibles*
ahora mismo. Su semántica es estrictamente **forbid-invalid**: solo puede
*quitar* una skill insegura o imposible (disparar sin estar armado, driblar
sin tener la pelota controlada). `walk` y `stand` nunca se quitan, así la
política siempre tiene un fallback seguro y la máscara nunca puede
"encerrarla".

Mecanismos de robustez frente a ruido de percepción:

- **Histéresis**: umbrales de entrada (tight) y de salida (loose) distintos
  por skill, para no oscilar cerca del límite.
- **Debounce temporal**: la condición de entrada debe sostenerse `N` frames
  consecutivos antes de armar (`shoot_arm_frames`, `dribble_arm_frames`).
- **Gating por incertidumbre**: armar `shoot` exige percepción *natural*
  fresca de la pelota (no la estimada/exportada); `dribble` usa una ventana de
  gracia más ancha para tolerar que `walkTo` pierda la pelota de vista
  momentáneamente mientras se aproxima (evita el deadlock
  ver→enganchar→perder de vista).

El `GateDecision` resultante (`shoot_armed`, `dribble_armed`,
`shoot_arm_progress`, y en el entorno de equipo también `pass_armed`,
`observe_armed`, `is_striker`/`is_open_support`/`is_off_ball_support`) se
hornea directamente en la observación (`obs[23:26]` y `obs[41:47]`). Esto es
lo que en el código se llama **Diseño A**: la máscara de acciones válidas que
usa la política en `train.py` (`gate_skill_mask_from_obs`) es una función pura
del vector de observación guardado, así que el recálculo de log-probs de PPO
en la actualización reproduce exactamente la misma máscara que se usó al
samplear la acción, sin tener que guardar la máscara por separado en el buffer
de rollout. El mismo gate alimenta también al teacher heurístico, a las
etiquetas del dataset de imitación y al guardrail que corre embebido en el
robot — un solo dueño de la semántica de "qué es seguro ahora".

## Recompensa (`reward.py`)

`RewardWeights` es un dataclass congelado con ~37 términos (progreso hacia la
pelota, alineación, entrar/sostener el "takeover", calidad de
walk/dribble/shoot, penalización por colisión, por "thrash" de skills, por
timeout, bonus de gol, términos de honestidad de percepción —
`stale_ball`/`collapsed_ball`/`rescued_ball`/`corrected_ball`/
`lost_trusted_ball`— que penalizan que el agente dependa de una pelota que en
el robot real no vería así, etc.). Cada `StageSpec` del currículo trae su
propio set de pesos, así que la forma de la recompensa cambia deliberadamente
etapa a etapa (ver `docs/RL_FLUJO_ENTRENAMIENTO.md`).

`evaluate_step(step_ctx, weights)` es la función central: clasifica el step en
zonas (`walk_zone`, `dribble_zone`, `shoot_zone`, envolvente de "takeover"),
calcula un sesgo heurístico blando hacia la skill preferida
(`heuristic_bias`), computa cada término, los pondera y devuelve un
`StepEvaluation` con el desglose completo (útil para depurar por qué un
episodio dio la recompensa que dio).

## Currículo y generación de mundos (`curriculum.py`, `production_curriculum.py`)

Cada `StageSpec` fija: rango de distancia inicial pelota-robot, si el campo es
completo o acotado, el modo de obstáculos (`none`/`static`/`dynamic`), qué
skills están activas y los pesos de recompensa de esa etapa.

| Stage | Nombre | Skills activas | Obstáculos | Descripción |
| --- | --- | --- | --- | --- |
| -1 | `stage_0a_walk_approach` | walk, stand | ninguno | Pre-stage: solo caminar hacia la pelota, sin cambiar de skill. |
| 0 | `stage_0_clean_handoff` | walk, dribble, stand | ninguno | Handoff limpio walk→dribble desde distancia real de aproximación, sin tiro. |
| 1 | `stage_1_finish_after_handoff` | walk, dribble, shoot, stand | ninguno | Cadena completa walk→dribble→shoot sin obstáculos. |
| 2 | `stage_2` | walk, dribble, shoot, stand | estáticos | Igual que 1, con obstáculos estáticos y penalización de colisión. |
| 3 | `stage_3` | walk, dribble, shoot, stand | dinámicos (scripted) | Robustez temporal, campo completo, obstáculos en movimiento. |
| 4 | `stage_4_production` | walk, dribble, shoot, stand | dinámicos | Objetivo de despliegue: remate real sin colapso de skill. |
| 6 | `multiagent_warmup_2v0` | + pass | estáticos | 1 arquero B-Human + 2 jugadores RL, mínima exposición a rivales; calienta el rol one-hot y la cabeza de pase. |
| 7 | `multiagent_adversarial_3v3` | + pass | dinámicos, campo completo | 1 arquero B-Human + 3 RL vs. equipo B-Human completo. |
| 8 | `multiagent_defender_3v3` | + pass, block | dinámicos, campo completo | Igual que 7 con `block` habilitado para el rol defensivo fuera de la pelota. |

Los stages 6-8 heredan los pesos de recompensa del stage 4; los términos
específicos de equipo (intento/completado de pase, sostener posición de apoyo
abierta, bonus de gol de equipo, penalización de "thrash" de rol,
separación entre compañeros) se aplican en tiempo de ejecución solo cuando el
paquete de info trae un `role_id`, para no romper la compatibilidad del
dataclass `RewardWeights`.

Sobre el `WorldSpec` de cada episodio, además del reset genérico del stage
existen generadores especializados que se activan con cierta probabilidad
(`stageN_finish_interleave_probability` en `config.py`, tuneables por
stage): `sample_finish_world_spec` (remate puro), `sample_finish_sequence_world_spec`
(secuencia completa desde más lejos) y `sample_preshoot_dribble_world_spec`
(aproximación justo antes del tiro). Esto evita que el agente solo vea el
reset "genérico" del stage y nunca practique la fase de cierre específica que
más le cuesta.

**Escenarios de producción** (`production_curriculum.py`): un
`ProductionScenario` es un `WorldSpec` con nombre, `stage_id` y función de
mundo, pensado para reproducir exactamente las situaciones que usa
`behavioral_evaluator.py` y el dataset de imitación de producción. Se activan
con `RL_PRODUCTION_SCENARIO_RESETS=1`, se filtran por conjunto
(`RL_PRODUCTION_SCENARIO_SET=eval|topup|team|defense|all`) y se ponderan por
nombre vía `RL_PRODUCTION_SCENARIO_WEIGHTS` (ver
`docs/RL_FLUJO_ENTRENAMIENTO.md` para cómo se usa esto en el fine-tune final).

`promote_stage(stage_id, metrics, history)` es el gate de auto-currículo: cada
stage define umbrales propios (semántica de pelota sana, comportamiento
estable, tasa de éxito de "takeover", tasa de tiro, etc.) que las métricas
agregadas de evaluación deben superar antes de subir de stage. La promoción es
**monótona** (nunca degrada) y en `train.py` se exige confirmarla varias
evaluaciones seguidas (`RL_PROMOTION_CONFIRM_EVALS`, default 2) para no
promover por una evaluación afortunada.

## Entornos multiagente (`parallel_environment.py`, `team_parallel_environment.py`)

`ParallelSlotSimRobotEnv` es la capa que permite entrenar varios jugadores en
paralelo: crea un `RealSimRobotEnv` independiente ("slot") por número de
jugador, cada uno con su propia instancia de SimRobot visible, y expone la
unión como un solo `ParallelEnv` de PettingZoo. `reset`/`step` despachan a
cada slot en threads separados para no serializar el tiempo de simulación de
cada instancia.

`TeamSlotSimRobotEnv` agrega, encima de `ParallelSlotSimRobotEnv`, la capa de
coordinación de equipo:

- **Asignación de roles**: cada agente RL recibe `striker`, `open_support` u
  `off_ball_support` por step, con histéresis para evitar cambios continuos.
- **Inyección de observación cruzada**: cada slot ve, en `obs[26:41]`, la
  posición de sus compañeros y una estimación de pelota de equipo construida a
  partir de las observaciones de los otros slots (`_inject_for`).
- **Decisión de pase**: cuando el rol es `striker` y `shoot`/`dribble` están
  armadas, evalúa `smash_or_pass` con las posiciones de los compañeros para
  decidir si conviene tirar o pasar, y arma `pass_armed` en el gate de equipo.
- **Métricas de equipo**: intentos/completados de pase, steps sosteniendo
  cobertura defensiva, cambios de rol, colisiones arquero-defensor, etc.
  (`_team_metrics`), usadas por la evaluación y el currículo de los stages
  6-8.

El arquero es un modelo totalmente aparte (observación de 64 valores, 12
skills propias) y no pasa por este entorno de campo; se entrena y evalúa con
su propio stack (`goalkeeper_action.py`, `train_goalkeeper_ppo*.py`,
`run_goalkeeper_production.py`).

## Invariantes duros

- **`OBS_SIZE = 41`/47 y `SKILL_ORDER` no pueden cambiar** sin invalidar
  cualquier checkpoint (`.pt`) u ONNX ya entrenado y desplegado.
- **Diseño A es de único dueño**: la máscara forbid-invalid en tiempo de
  actualización de PPO se deriva únicamente de `obs[23:26]`/`obs[41:44]`
  (`gate_skill_mask_from_obs` en `train.py`); no debe recalcularse en ningún
  otro lugar del código.
- **Máximo 4 slots visibles de SimRobot en paralelo** — 6 satura Qt/OpenGL y
  hace crashear la corrida.
- Cualquier cambio en `RLSharedState`, `RLSkillProvider.cpp` o `Module.cpp`
  del lado `SabanaHerons2026/` requiere recompilar `pybh` **y**
  `SimulatedNao`; si quedan desalineados, `obs_ready` nunca vuelve a `True`
  aunque el resto del pipeline compile sin errores.

## Referencias

- `RL/docs/README.md` — índice histórico de la integración RL/B-Human/SimRobot.
- `RL/CLAUDE.md` — mapa de arquitectura y comandos operativos actualizados.
- [DOCUMENTACION_FINAL_RL_SABANAHERONS.md](./DOCUMENTACION_FINAL_RL_SABANAHERONS.md)
  — cómo el ONNX exportado de este entorno se consume en producción dentro de
  `SabanaHerons2026`.
- [RL_FLUJO_ENTRENAMIENTO.md](./RL_FLUJO_ENTRENAMIENTO.md) — cómo se entrena,
  evalúa y exporta la política que corre sobre este entorno.
