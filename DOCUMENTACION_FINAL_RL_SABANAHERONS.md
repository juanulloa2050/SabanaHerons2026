# Integración final de aprendizaje por refuerzo en SabanaHerons

## Resumen

El proyecto incorporó políticas de aprendizaje por refuerzo (RL) al stack de fútbol de `SabanaHerons2026` sin reemplazar la locomoción ni las habilidades de B-Human. RL decide **qué habilidad de alto nivel solicitar** —por ejemplo caminar, disparar, pasar, driblar o bloquear— y SabanaHerons conserva la ejecución física completa: convierte esa decisión en `SkillRequest`, luego en `MotionRequest` y finalmente en movimientos articulares del NAO.

El repositorio `RL/` es la base experimental: allí se construyen los entornos de SimRobot, se generan observaciones, se entrena y evalúa PPO/MAPPO y se exportan los modelos. `SabanaHerons2026/` es el producto operativo: contiene el runtime C++, los modelos ONNX, las reglas de seguridad, los escenarios y las opciones de despliegue. En competencia el robot no necesita Python ni entrena; realiza inferencia local con ONNX Runtime.

## Qué se desarrolló

En `RL/` se creó un entorno conectado a B-Human mediante `pybh`, compatible con SimRobot embebido y visible. El entorno permite reiniciar y actualizar pelota, robot, compañeros y rivales; leer percepción y estado interno de B-Human; aplicar recompensas y currículos; y entrenar políticas con acciones híbridas:

```text
acción = habilidad discreta + cuatro parámetros continuos
```

Las ocho habilidades de los jugadores de campo son `stand`, `walk`, `shoot`, `pass`, `dribble`, `block`, `mark` y `observe`. El proceso evolucionó desde un atacante individual con 26 observaciones hasta políticas de equipo con 47 observaciones, roles y contexto de compañeros. También se añadieron profesores heurísticos, preentrenamiento por imitación, reparaciones de estabilidad, evaluación por escenarios y exportadores ONNX compatibles con el runtime del NAO. El arquero usa una política independiente de 64 observaciones y 12 habilidades específicas.

En `SabanaHerons2026/` se implementaron las dos formas de consumir ese trabajo:

1. **Control externo para entrenamiento y pruebas.** Python escribe acciones y recibe observaciones mediante `RLSharedState`. Los escenarios `4v4_RL2D` y `4v4_RL3D` usan `RLSkillProvider` para introducir esas acciones en el flujo normal de B-Human.
2. **PPO embebido para partido y robot real.** `StrategyBehaviorControl` carga el modelo ONNX y decide cada frame sin depender de Python. Esta es la ruta utilizada por los escenarios de juego y por el despliegue a los robots.

## Cómo funciona dentro de SabanaHerons

El flujo de producción es:

```text
Representaciones de B-Human
  (pose, pelota, obstáculos, equipo y estado de juego)
              |
              v
PPOObservationEncoder / GKObservationEncoder
              |
              v
Modelo ONNX -> logits de habilidad + parámetros
              |
              v
SkillGate + máscara de acciones legales
              |
              v
PPOActionDecoder / GKActionDecoder
              |
              v
SkillRequest -> SkillBehaviorControl -> MotionRequest
              |
              v
WalkingEngine / MotionEngine -> articulaciones del NAO
```

La integración principal vive en `Src/Modules/BehaviorControl/StrategyBehaviorControl`. En cada ciclo SabanaHerons calcula primero su estrategia B-Human habitual. Después, si RL está habilitado, el jugador está activo, el partido está en `playing` y el jugador pertenece al conjunto permitido, intenta reemplazar el `SkillRequest` por la salida de PPO. El PPO de campo excluye explícitamente al arquero; la política de arquero se evalúa aparte y únicamente en el jugador marcado como goalkeeper.

La red no controla motores directamente. Los encoders reproducen la normalización usada durante el entrenamiento; el modelo produce ocho logits y cuatro parámetros; las *gates* eliminan acciones inseguras o incoherentes antes del `argmax`; y el decoder crea una solicitud válida de B-Human. Por ejemplo, un `walk` se ancla a una pose táctica, un `pass` recibe un compañero válido y un `shoot` solo queda disponible cuando la pelota, el ángulo, la percepción y la apertura de tiro cumplen los umbrales. Esto evita que una salida numérica de la red se convierta sin validación en una orden física.

Para los jugadores de campo existen cuatro variantes:

| Modo | Modelo principal | Uso dentro de SabanaHerons |
| --- | --- | --- |
| `striker_base` | `ppo_striker_hsl2026.onnx` | Atacante original, observación de 26 valores y cadena aproximación–drible–tiro. |
| `baseline_attack` | modelo defender | Defensa/apoyo con caminar, pasar y bloquear, según gates de posesión y amenaza. |
| `mixed_attack` | `ppo_team_hsl2026_v4_2.onnx` | Atacante con observación de equipo de 47 valores. |
| `complete` | `ppo_team_hsl2026_v5_merged.onnx` | Política unificada de 47 valores para todos los jugadores de campo. |

El modo `complete` es el más integrado. Un coordinador C++ asigna dinámicamente los roles `striker`, `open_support` y `off_ball_support`; aplica histéresis para evitar cambios continuos; calcula posiciones de apoyo suavizadas; agrega el rol a la observación; y decodifica la salida de la misma red de forma distinta para atacante y apoyos. Así, la política aprendida participa en la decisión, mientras SabanaHerons aporta coordinación, geometría, comunicación de equipo y restricciones de seguridad.

El arquero es independiente del PPO de campo. Su modelo puede reposicionarse, observar, interceptar al centro o lateralmente, realizar bloqueos bajos y saltos, despejar, pasar y salir driblando. `GKSkillGate` enmascara las acciones no válidas antes de elegir, y SabanaHerons selecciona el compañero más adelantado cuando la red pide un pase.

## Configuración, modelos y despliegue

Los modelos y sus manifiestos están en `Config/NeuralNets/RLPolicy/`. Los manifiestos JSON documentan tamaño de observación, orden de habilidades, normalización, checkpoint y hash; deben mantenerse junto al ONNX para conservar el contrato entre entrenamiento y C++.

Los escenarios `4v4_StrikerBase`, `4v4_BaselineAttack`, `4v4_MixedAttack` y `4v4_Complete` seleccionan la variante. `4v4_Full` sirve como referencia configurable y `--rl-disable` permite comparar contra B-Human sin PPO. La localización de campo debe seguir siendo `4v4_Full`; escenario y modo RL son conceptos distintos.

El script `Make/Common/deploy` copia el binario, toda la configuración, los modelos y `libonnxruntime` al NAO. Un despliegue completo usa, conceptualmente:

```bash
./deploy Release \
  -r 1 <ip-arquero> -r 2 <ip-jugador> -r 3 <ip-jugador> -r 4 <ip-jugador> \
  -t 49 -s 4v4_Complete -l 4v4_Full \
  --rl-complete 2,3,4 --rl-gk on --goalkeeper-dive on
```

El preset actual de `Config/teams.cfg` combina `4v4_Complete`, modo `gk` para el arquero y `complete` para jugadores de campo. El selector gráfico de despliegue traduce los valores `rlModes` a las mismas opciones del script.

**Detalle operativo importante:** en la implementación actual, configurar `embeddedPPOMergedTeamModelPath` da prioridad absoluta al modo `complete`. El script recibe una lista en `--rl-complete`, pero no la copia a `embeddedPPOPlayers`; con la lista interna vacía, la política se aplica a todos los jugadores de campo activos y el arquero queda excluido. Por tanto, hoy esa opción selecciona el modo de equipo completo, no una restricción efectiva por jugador.

## Seguridad y comportamiento ante fallos

La política solo toma control durante juego activo. Estados iniciales, detenidos, penalizaciones y jugadores no habilitados permanecen en el flujo normal correspondiente. Si el modelo no carga, la inferencia falla, no existe una acción válida o se activa el *watchdog* de `stand`, `StrategyBehaviorControl` conserva o recupera el comportamiento B-Human. El watchdog puede forzar una caminata o aplicar un tiempo de enfriamiento antes de reintentar PPO.

Los logs permiten distinguir claramente los estados principales:

```text
[RL] Embedded PPO model loaded
[RL] mode=EmbeddedPPOActive
[RL] Embedded PPO action
[RL] Embedded PPO inference failed
[RL] mode=EmbeddedPPOFallback
[RL] Embedded GK model loaded
```

Una validación mínima consiste en compilar `SimRobot` y `Nao`, comprobar que el ONNX carga, confirmar `EmbeddedPPOActive` o el modo de arquero en los jugadores esperados y observar que las decisiones producen `SkillRequest` y movimiento real. Antes de reemplazar un modelo debe verificarse en `RL/` con los evaluadores de comportamiento y estabilidad, exportarse nuevamente a ONNX y conservar exactamente el orden de habilidades, la normalización, las gates y el decoder usados durante el entrenamiento.

## Resultado final

SabanaHerons quedó como un sistema híbrido: B-Human continúa gestionando reglas de juego, percepción, localización, comunicación, habilidades y locomoción; RL reemplaza de forma controlada una parte de la decisión estratégica. Esta separación permite entrenar y experimentar en `RL/`, trasladar únicamente políticas validadas como ONNX y mantener en el robot una ruta autónoma, rápida y con fallback al comportamiento clásico.
