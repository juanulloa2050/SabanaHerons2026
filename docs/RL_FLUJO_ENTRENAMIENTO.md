# Flujo de entrenamiento RL

## Resumen

Este documento describe cómo se entrena, evalúa y exporta la política que
corre sobre el entorno descrito en
[RL_ENTORNO.md](./RL_ENTORNO.md). Todo el trabajo vive en `RL/`; el resultado
final de este flujo es un par `.onnx` + `.json` de manifiesto que
`SabanaHerons2026` consume en inferencia (ver
[DOCUMENTACION_FINAL_RL_SABANAHERONS.md](./DOCUMENTACION_FINAL_RL_SABANAHERONS.md)).

El pipeline completo, de punta a punta, es:

```text
teacher_policy.py (experto heurístico)
        |
        v
teacher_production_dataset.py  -> samples.npz (demostraciones)
        |
        v
train_bc.py  -> warm-start por imitación (checkpoint .pt)
        |
        v
train.py  -> PPO/MAPPO por currículo (stages -1..4, luego 6..8)
        |    con regularización BC-replay + auto-promoción de stage
        v
behavioral_evaluator.py -> pass/fail en 5 escenarios de producción
        |
        v
train_production.py  -> fine-tune dirigido de reparación (stage 4 fijo)
        |
        v
tools/export_ppo_policy*.py -> ONNX + manifiesto JSON
        |
        v
SabanaHerons2026/Config/NeuralNets/RLPolicy/  (consumido por StrategyBehaviorControl)
```

## Algoritmo: MAPPO sobre `skrl` (`train.py`)

### Redes

`PolicyNetwork` es una política híbrida: un tronco compartido de dos capas
lineales de 128 unidades con activación ELU, que alimenta dos cabezas:

- `skill_head`: logits categóricos sobre las 8 skills (`SKILL_ORDER`).
- `param_head`: media (con `tanh`) de 4 parámetros continuos residuales, con
  desviación estándar aprendida pero acotada (`log_std` clamped a
  `[-2.0, -0.7]`, es decir std entre ~0.14 y ~0.50) — evita que el bonus de
  entropía necesario para explorar la skill discreta infle también el ruido
  de los targets continuos, que necesitan precisión para no perder el control
  del drible.

`ValueNetwork` es un MLP simple (128-128-1, ELU) sobre el mismo vector de
observación (hoy `state == observation`; el espacio de estado existe para que
`skrl`/CTDE puedan diferenciarlos en el futuro sin cambiar el contrato).

### Enmascarado en tiempo de acción

Antes de samplear la skill, `PolicyNetwork._masked_skill_logits` combina dos
máscaras: la máscara **de stage** (`active_skill_names` del `StageSpec`
actual) y la máscara **de gate** (`gate_skill_mask_from_obs`, derivada de
`obs[23:26]`/`obs[41:44]`, ver `RL_ENTORNO.md`). El resultado siempre deja al
menos `walk`/`stand` disponibles. Los parámetros continuos también se
enmascaran por skill (`ACTION_PARAM_MASKS`) para que las dimensiones "muertas"
de cada skill no acumulen log-prob ni gradiente.

### Configuración de la corrida (`ScriptRunConfig`)

`train.py` **no usa flags de línea de comandos**: el punto de entrada
(`if __name__ == "__main__": main(_run_cfg_from_env())`) construye un
`ScriptRunConfig` a partir de valores por defecto (`IDE_RUN`, pensados para
correr desde el IDE) y los sobreescribe con variables de entorno `RL_*`
(`RL_RUN_MODE`, `RL_STAGE_ID`, `RL_TRAIN_STEPS`, `RL_ROLLOUT_STEPS`,
`RL_BC_WARM_START`, `RL_PARALLEL_PLAYER_NUMBERS`, etc. — ver
`_run_cfg_from_env` para la lista completa). Ejemplo real:

```bash
RL_RUN_MODE=train \
RL_STAGE_ID=0 \
RL_TRAIN_STEPS=8000 \
PYTHONPATH=/path/to/SabanaHerons2026/Make/Python \
.venv/bin/python train.py
```

Hay dos modos:

- **`smoke`** (`run_smoke`): unos pocos steps para validar que `act`,
  `record_transition` y `post_interaction` funcionan de punta a punta contra
  SimRobot real, sin intención de aprender nada.
- **`train`** (`run_training`): el loop de entrenamiento real.

### `run_training(...)`

1. Construye el entorno (`make_env_config` + `build_env`) y el agente MAPPO
   (`build_mappo`), que instancia `PolicyNetwork`/`ValueNetwork` y una
   memoria de rollout (`RandomMemory` de `skrl`).
2. Si `bc_warm_start=True`, carga un checkpoint de imitación
   (`load_bc_warm_start`) como punto de partida — con padding de ceros en la
   primera capa si el checkpoint fue entrenado con menos columnas de
   observación que las actuales (por ejemplo 26 → 41/47), para no perder el
   trabajo previo cada vez que se extiende el vector de observación.
3. Clona esa política como referencia congelada
   (`clone_frozen_policy_reference`) para la regularización BC-replay descrita
   abajo.
4. Corre un loop propio (no el `SequentialTrainer.train()` de `skrl`): según
   quedó documentado en `RL/README.md`, la ruta de un solo `MultiAgent` de
   `skrl` en esta versión cae por el camino de single-agent y no tolera el
   retorno tipo `dict` de PettingZoo, así que `train.py` mantiene su propio
   loop de recolección + actualización usando el agente `MAPPO` real de
   `skrl` (`agent.act`, `agent.record_transition`, `agent.post_interaction`).
5. Cada `eval_interval` steps corre `evaluate_agent` (episodios
   deterministas contra escenarios fijos), agrega métricas
   (`_aggregate_eval_metrics`) y evalúa `promote_stage(...)` para decidir si
   sube de stage — exige `RL_PROMOTION_CONFIRM_EVALS` (default 2)
   evaluaciones consecutivas en verde.
6. Cada `checkpoint_interval` steps (y al promover de stage) guarda un
   checkpoint con `save_checkpoint`.

Hiperparámetros por defecto (`TrainConfig` en `config.py`): `rollout_steps=256`,
`learning_epochs=4`, `mini_batches=4`, `learning_rate=1e-4`,
`discount_factor=0.99`, `gae_lambda=0.95`, `entropy_loss_scale=0.03`,
`ratio_clip=0.2`, `eval_interval=checkpoint_interval=5000`, `eval_episodes=8`.

### Checkpoints y telemetría

Los checkpoints quedan en `runs/<experiment_name>/checkpoint_stage<S>_step<N>.pt`
y contienen `policy_state_dict`, `value_state_dict`, `timestep`, `stage_id` y
el `eval_summary` de esa evaluación — suficiente para retomar entrenamiento o
evaluar sin volver a correr nada. Si `save_telemetry=True`, además se escriben
bajo el mismo `runs/<experiment_name>/`: `run_config.json` (configuración
completa de la corrida), `smoke_steps.csv`/`smoke_summary.csv`,
`eval_summary.csv` y `eval_episodes.csv`, más gráficos si `save_plots=True`.

## Teacher heurístico (`teacher_policy.py`)

`stage_aware_teacher_action(...)` es un experto basado en reglas geométricas
(no aprendido) que decide la skill "correcta" dado el estado actual: aproximar
caminando, driblar cuando la pelota está controlada, tirar cuando el ángulo y
la distancia al arco son buenos, esquivar lateralmente un obstáculo frontal
(a partir de stage 2), y re-adquirir activamente la pelota si lleva demasiado
tiempo sin verla en el canal *natural* (`NATURAL_BLIND_MAX_AGE_MS`). Usa el
mismo `SkillGate` que la política, así que sus decisiones son consistentes con
lo que la política puede realmente ejecutar (no etiqueta "tira" cuando el gate
no lo permitiría). Este teacher tiene dos usos:

- Generar datasets de demostraciones para Behavioral Cloning.
- Aportar un sesgo blando (`reward.heuristic_bias`) dentro de la recompensa,
  sin forzar la acción.

## Behavioral Cloning — warm start (`train_bc.py`)

Antes de cualquier PPO, `train_bc.py` entrena `PolicyNetwork`/`ValueNetwork`
puramente por imitación sobre un dataset de demostraciones del teacher
(`samples.npz`). El entrenamiento está organizado en **fases por grupo de
stages** (`PHASES`, por ejemplo `basics_stage_minus1_0` cubre los stages -1 y
0 durante un número configurable de épocas), reflejando la misma progresión de
dificultad que el currículo RL.

La pérdida combina:

- **Cross-entropy de skill**, enmascarada a las skills activas del stage
  (`bc_ce_safe_logits`) para no penalizar a la red por no predecir una skill
  que ese stage ni siquiera permite.
- **MSE de parámetros continuos**, solo sobre las dimensiones vivas de la
  skill demostrada (`ACTION_PARAM_MASKS`).

Puede arrancar desde cero o desde un checkpoint existente
(`RL_BC_INIT_CHECKPOINT`) con el mismo padding de ceros en la primera capa que
usa el warm-start de PPO. El resultado queda en `runs/bc_warm_start/`.

## Datasets de imitación de producción (`teacher_production_dataset.py`, `production_curriculum.py`)

Para el fine-tune de producción se genera un dataset dirigido: se corre el
teacher sobre los `ProductionScenario` (mundos hechos a mano que reproducen
las situaciones difíciles reales — banda de tiro 2.5-3 m limpia, la misma
banda con un rival estático en el carril, ángulo de pre-tiro límite, presión
dinámica de campo completo, reparación de corredor bloqueado — más variantes
"topup" y, para los stages de equipo, escenarios `team_*`/`defense_*`).
`teacher_production_dataset.py` filtra episodios malos (`_episode_kept`) y
verifica que la etiqueta de skill sea consistente con el bucket del gate en
ese frame (`_gate_bucket`/`_gate_label_consistent`) para que el dataset nunca
enseñe una skill que el gate real habría bloqueado. El resultado se fusiona en
un `samples.npz` + `summary.json`, que alimenta tanto a `train_bc.py` como al
regularizador BC-replay descrito a continuación.

## Regularización BC durante PPO (`BCReplayRegularizer` / `BCCompetenceGate`, `train.py`)

Mientras PPO entrena on-policy, una pérdida de destilación en segundo plano
tira periódicamente la política hacia la instantánea congelada de BC
(`clone_frozen_policy_reference`), reproduciendo batches del dataset de
demostraciones. Esto evita el colapso de skill que PPO puede inducir por su
cuenta (el caso documentado en `CLAUDE.md`: `policy_shoot_rate = 0.000`, un
óptimo local donde el agente deja de intentar tirar). El peso de esta pérdida
(`beta`) decae con la competencia medida en evaluaciones
(`BCCompetenceGate`: de `bc_beta_max` a `bc_beta_min` con una vida media en
número de evaluaciones), para que el ancla de imitación se relaje a medida que
la política demuestra que iguala o supera al teacher. `bc_skill_loss_scale` y
`bc_param_loss_scale` controlan por separado cuánto pesa la parte discreta vs.
la continua de esa destilación.

## Currículo multiagente — stages 6/7/8

Los stages 6 (`multiagent_warmup_2v0`), 7 (`multiagent_adversarial_3v3`) y 8
(`multiagent_defender_3v3`) se entrenan sobre `TeamSlotSimRobotEnv` (ver
`RL_ENTORNO.md`) con `train_multiagent.py` (exploratorio) y
`train_multiagent_production.py` (que fija
`RL_PRODUCTION_SCENARIO_SET=team`/`defense` para samplear los escenarios de
equipo de `production_curriculum.py`). Heredan los pesos de recompensa del
stage 4; los términos específicos de equipo (pases intentados/completados,
sostener una posición de apoyo abierta, bonus de gol de equipo, penalización
por cambiar de rol demasiado seguido, separación entre compañeros) se suman en
tiempo de ejecución cuando el paquete de info trae un `role_id`, sin tocar el
dataclass `RewardWeights` de un solo agente.

## Fine-tune de producción (`train_production.py`)

Es el punto de entrada para *reparar* un checkpoint casi listo, no para subir
por el currículo desde cero: fija `stage_id=4` y desactiva la auto-promoción
(`auto_promote=False`). Su CLI real:

```bash
python train_production.py \
  --checkpoint runs/production_finetune_v1/checkpoint_stage4_step35000.pt \
  --dataset runs/fine_tuning_production_dataset/.../samples.npz \
  [--output-dataset ...] [--prepare-only]
```

El resto de los parámetros de la corrida (pasos de entrenamiento, cadencia de
evaluación, agenda de `beta` de BC, timeouts de reset visible, número de
jugadores en paralelo) se controla con variables `RL_PROD_*`, con valores por
defecto ya ajustados a partir de corridas anteriores (por ejemplo
`bc_beta_max=0.15` en vez del `0.08` genérico, porque una corrida previa
apagó la señal de BC demasiado rápido y el agente nunca llegó a comprometerse
con el tiro). Antes de entrenar, prepara el dataset de replay
(`prepare_stage4_replay_dataset`) y habilita resets ponderados por escenario
de producción con una mezcla explícita ajustada a partir de los fallos de
`behavioral_evaluator.py` (por ejemplo
`dynamic_full_field_pressure` se pone en peso `0` porque resultó ser el
episodio más caro y el que menos aportaba). Corre con 4 instancias visibles de
SimRobot en paralelo (jugadores 1-4) — 6 satura Qt/OpenGL en esta máquina.

## Evaluación de comportamiento (`behavioral_evaluator.py`)

Carga un checkpoint y lo corre, determinista, sobre 5 situaciones
hechas a mano (las mismas `ProductionScenario` de `production_curriculum.py`):
`finish_band_2p5_3m_clean`, `finish_band_2p5_3m_static_lane`,
`preshoot_bad_angle_boundary`, `dynamic_full_field_pressure` y
`blocked_corridor_repair`. Cada una tiene su propio criterio de aprobación,
por ejemplo para el remate limpio:

```python
clean_takeover_rate  >= 0.50
clean_finish_rate    >= 0.60
shoot_finish_rate    >= 0.25
goal_rate >= 0.10  OR  ball_progress_post_takeover_mean >= 0.015
collision_rate       <= 0.12
policy_shoot_rate    >= 0.02
```

CLI:

```bash
python behavioral_evaluator.py \
  --checkpoint runs/production_readiness_v2/checkpoint_stage4_step30000.pt \
  --episodes 4 --seed 4200 [--capture-steps]
```

El objetivo actual antes de promover un checkpoint a exportación es
`behavioral_evaluator ≥ 3/5` escenarios en verde.

## Exportación a ONNX (`tools/export_ppo_policy*.py`)

El exportador reconstruye la red directamente desde los tensores del
`state_dict` guardado (`ActorExport`, con los mismos pesos de `trunk`,
`skill_head` y `param_head` que `PolicyNetwork`), sin pasar por la clase de
`skrl`, y la exporta con `torch.onnx.export` (entrada `obs`, salidas
`skill_logits` + `param_mean`, `opset_version=17`):

```bash
python tools/export_ppo_policy.py \
  --checkpoint runs/production_stability_repair_v1/checkpoint_stage4_step5000.pt \
  --output ppo_striker_hsl2026.onnx
```

Junto al `.onnx` escribe un manifiesto `.json` con el tamaño de observación de
entrada al modelo, el tamaño real que produce el entorno (para saber cuánto
padding hay), el orden de skills, las skills activas, el hash del checkpoint y
del propio ONNX, el commit de exportación y el contrato de normalización — el
mismo manifiesto que documenta `DOCUMENTACION_FINAL_RL_SABANAHERONS.md` como
requisito para mantener el contrato entre entrenamiento y el runtime C++. Hay
tres variantes de exportador: `export_ppo_policy.py` (striker de un solo
agente), `export_ppo_policy_team.py` (política de equipo de 47 observaciones)
y `export_goalkeeper_policy.py` (arquero, modelo independiente de 64
observaciones / 12 skills).

## Invariantes y puntos de fricción conocidos

- **`OBS_SIZE` y `SKILL_ORDER` congelados**: cambiarlos invalida cada
  checkpoint `.pt` y cada `.onnx` ya exportado. Cuando el vector de
  observación creció (26 → 41 → 47), la compatibilidad se resolvió con
  padding de ceros en la primera capa (`_load_state_dict_obs_padded`), nunca
  reentrenando desde cero.
- **La promoción de stage es monótona** (nunca degrada) y exige varias
  evaluaciones consecutivas en verde antes de subir, para que una evaluación
  con suerte de percepción no adelante al agente a un stage que todavía no
  puede sostener.
- **`train.py` se configura por variables de entorno, no por flags de CLI**;
  solo `train_production.py`, `behavioral_evaluator.py` y los scripts de
  `tools/` usan `argparse`.
- Cambios en `RLSharedState`, `RLSkillProvider.cpp` o `Module.cpp` del lado
  `SabanaHerons2026/` exigen recompilar `pybh` **y** `SimulatedNao` antes de
  volver a entrenar; si quedan desalineados, `obs_ready` nunca vuelve a
  `True` y el entrenamiento no arranca aunque no haya ningún error de Python.

## Referencias

- [RL_ENTORNO.md](./RL_ENTORNO.md) — contrato del entorno, observación, acción,
  gate y recompensa que consume este flujo.
- [DOCUMENTACION_FINAL_RL_SABANAHERONS.md](./DOCUMENTACION_FINAL_RL_SABANAHERONS.md)
  — cómo se consume el ONNX exportado dentro de `SabanaHerons2026`.
- `RL/CLAUDE.md` — mapa de archivos y comandos operativos.
- `RL/FOR_TRAIN_PRODUCTION.md` — guía rápida y checklist de triage del
  fine-tune de producción.
- `RL/.docs/09-production-selfplay-and-multiagent-handoff.md` y
  `RL/.docs/10-graph-coordination-post-production-roadmap.md` — handoffs
  detallados de la expansión multiagente.
