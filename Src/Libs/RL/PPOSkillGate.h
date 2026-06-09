/**
 * @file PPOSkillGate.h
 *
 * Stateful skill gate for the PPO policy — porta gate.py exactamente.
 *
 * Decide si shoot y dribble están "armados" en cada paso, basándose en
 * geometría, percepción natural y debounce temporal. Sus salidas se bakean
 * en obs[23], obs[24] y obs[25] antes del forward pass del actor ONNX.
 *
 * CONTRATO (del documento 26-implementacion-ppo-a-transplantar.md):
 *   - El estado persiste durante tod el episodio.
 *   - Se llama reset() al inicio de cada episodio (kickoff, penalización, etc).
 *   - Se llama step() en cada frame de Cognition, ANTES de construir el obs.
 *   - Las salidas de step() van a obs[23], obs[24], obs[25].
 *
 * Parámetros de producción (GateConfig en gate.py, commit ed8ffdc):
 *   shoot_enter_ball_mm   = 380   shoot_exit_ball_mm  = 460
 *   shoot_enter_angle_deg = 25    shoot_exit_angle_deg = 35
 *   shoot_goal_enter_mm   = 3000  shoot_goal_exit_mm  = 3200
 *   shoot_open_enter      = 0.6   shoot_open_exit     = 0.45
 *   shoot_align_enter_deg = 30    shoot_align_exit_deg = 45
 *   shoot_arm_frames      = 3
 *   dribble_enter_ball_mm = 450   dribble_exit_ball_mm = 600
 *   dribble_enter_angle_deg = 35  dribble_exit_angle_deg = 55
 *   dribble_arm_frames    = 1
 *   dribble_grace_enter_age_ms = 1500
 *   dribble_grace_hold_age_ms  = 2500
 *   trusted_max_age_ms    = 600
 */

#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// Parámetros del gate (mirrors de GateConfig en gate.py)
// No modificar sin actualizar también gate.py y el manifiesto ONNX.
// ---------------------------------------------------------------------------
struct PPOGateConfig
{
  // --- shoot: robot "on the ball" ---
  float shootEnterBallMm    = 380.f;
  float shootExitBallMm     = 460.f;
  float shootEnterAngleRad  = 25.f * M_PI / 180.f;
  float shootExitAngleRad   = 35.f * M_PI / 180.f;

  // --- shoot: distancia balón-portería ---
  float shootGoalEnterMm    = 3000.f;
  float shootGoalExitMm     = 3200.f;

  // --- shoot: apertura de carril ---
  float shootOpenEnter      = 0.6f;
  float shootOpenExit       = 0.45f;

  // --- shoot: alineación robot-portería ---
  float shootAlignEnterRad  = 30.f * M_PI / 180.f;
  float shootAlignExitRad   = 45.f * M_PI / 180.f;

  // --- shoot: debounce temporal ---
  int   shootArmFrames      = 3;

  // --- dribble: envelope ---
  float dribbleEnterBallMm  = 450.f;
  float dribbleExitBallMm   = 600.f;
  float dribbleEnterAngleRad = 35.f * M_PI / 180.f;
  float dribbleExitAngleRad  = 55.f * M_PI / 180.f;
  int   dribbleArmFrames    = 1;

  // --- dribble: gracia de percepción (evita deadlock ver->acercarse->ver) ---
  float dribbleGraceEnterAgeMs = 1500.f;
  float dribbleGraceHoldAgeMs  = 2500.f;

  // --- percepción natural confiable (canal natural, no exportado) ---
  float trustedMaxAgeMs     = 600.f;

  // --- coordenadas de la portería contraria (mm) ---
  float goalX               = 4500.f;
};

// ---------------------------------------------------------------------------
// Resultado de un paso del gate
// ---------------------------------------------------------------------------
struct PPOGateResult
{
  bool  shootArmed       = false;
  bool  dribbleArmed     = false;
  float shootArmProgress = 0.f;   ///< [0,1]: progreso hacia armar shoot

  /// obs[23], obs[24], obs[25] listos para bakearse en el vector de observación
  float obsShootArmed()       const { return shootArmed    ? 1.f : 0.f; }
  float obsDribbleArmed()     const { return dribbleArmed  ? 1.f : 0.f; }
  float obsShootArmProgress() const { return shootArmProgress; }

  /// finish_armed = shoot_armed OR dribble_armed
  bool finishArmed() const { return shootArmed || dribbleArmed; }
};

// ---------------------------------------------------------------------------
// PPOSkillGate — objeto stateful, uno por robot, vive durante el partido
// ---------------------------------------------------------------------------
class PPOSkillGate
{
public:
  explicit PPOSkillGate(const PPOGateConfig& cfg = PPOGateConfig{}) : cfg(cfg) {}

  /// Resetear al inicio de cada episodio (kickoff, penalización, get-up, etc.)
  void reset()
  {
    shootArmed    = false;
    dribbleArmed  = false;
    shootStreak   = 0;
    dribbleStreak = 0;
  }

  /**
   * Ejecutar un paso del gate.
   *
   * Todos los parámetros vienen de representaciones BHuman. Ver PPOObsEncoder
   * para el mapeo exacto de cada campo.
   *
   * @param dBallMm          Distancia robot-balón en mm (sqrt(rel_x²+rel_y²))
   * @param aBallRad         Ángulo robot-balón en rad (atan2(rel_y, rel_x)), valor absoluto
   * @param shotOpen         Apertura del carril con obstáculos [0,1] (shot_opening_with_obstacles)
   * @param scoreNow         Flag can_score_now
   * @param goalDistMm       Distancia balón-portería contraria en mm
   * @param robotX           Posición robot en campo (mm)
   * @param robotY           Posición robot en campo (mm)
   * @param robotTheta       Orientación robot en rad [-pi, pi], 0 = hacia portería contraria
   * @param natBallAgeMs     Tiempo desde último avistamiento NATURAL del balón (ms)
   *                         NO usar BallModel.timeWhenLastSeen si está propagado —
   *                         debe ser el timestamp del último frame con detección visual real.
   *
   * @return PPOGateResult con shoot_armed, dribble_armed y shoot_arm_progress
   */
  PPOGateResult step(float dBallMm,
                     float aBallRad,
                     float shotOpen,
                     bool  scoreNow,
                     float goalDistMm,
                     float robotX,
                     float robotY,
                     float robotTheta,
                     float natBallAgeMs)
  {
    // --- Freshness de percepción natural ---
    // SHOOT requiere visión natural fresca (<=600ms). No se arma sobre modelo propagado.
    const bool fresh = natBallAgeMs <= cfg.trustedMaxAgeMs;

    // DRIBBLE usa ventana de gracia más amplia para sobrevivir dropouts breves
    // mientras el robot se compromete con la pelota (rompe el deadlock ver->acercarse->ver)
    const bool engageEnterFresh = natBallAgeMs <= cfg.dribbleGraceEnterAgeMs;
    const bool engageHoldFresh  = natBallAgeMs <= cfg.dribbleGraceHoldAgeMs;

    // --- Alineación robot-portería (centro de la portería contraria, y=0) ---
    // goal_bearing = atan2(0 - robot_y, GOAL_X - robot_x)
    const float goalBearing = std::atan2(0.f - robotY, cfg.goalX - robotX);
    const float align = std::abs(wrapAngle(robotTheta - goalBearing));

    // =========================================================
    // SHOOT GATE — histeresis + debounce temporal
    // =========================================================
    const bool onBallEnter = dBallMm <= cfg.shootEnterBallMm && aBallRad <= cfg.shootEnterAngleRad;
    const bool onBallExit  = dBallMm <= cfg.shootExitBallMm  && aBallRad <= cfg.shootExitAngleRad;
    const bool laneEnter   = shotOpen >= cfg.shootOpenEnter   || scoreNow;
    const bool laneExit    = shotOpen >= cfg.shootOpenExit    || scoreNow;
    const bool rangeEnter  = goalDistMm <= cfg.shootGoalEnterMm;
    const bool rangeExit   = goalDistMm <= cfg.shootGoalExitMm;
    const bool alignEnter  = align <= cfg.shootAlignEnterRad;
    const bool alignExit   = align <= cfg.shootAlignExitRad;

    const bool enterCond = fresh && onBallEnter && laneEnter && rangeEnter && alignEnter;
    const bool holdCond  = fresh && onBallExit  && laneExit  && rangeExit  && alignExit;

    if(!shootArmed)
    {
      shootStreak = enterCond ? shootStreak + 1 : 0;
      if(shootStreak >= cfg.shootArmFrames)
        shootArmed = true;
    }
    else
    {
      if(!holdCond)
      {
        shootArmed  = false;
        shootStreak = 0;
      }
    }

    // =========================================================
    // DRIBBLE GATE — envelope amplio + gracia de percepción
    // =========================================================
    const bool dbEnter = engageEnterFresh && dBallMm <= cfg.dribbleEnterBallMm && aBallRad <= cfg.dribbleEnterAngleRad;
    const bool dbHold  = engageHoldFresh  && dBallMm <= cfg.dribbleExitBallMm  && aBallRad <= cfg.dribbleExitAngleRad;

    if(!dribbleArmed)
    {
      dribbleStreak = dbEnter ? dribbleStreak + 1 : 0;
      if(dribbleStreak >= cfg.dribbleArmFrames)
        dribbleArmed = true;
    }
    else
    {
      if(!dbHold)
      {
        dribbleArmed  = false;
        dribbleStreak = 0;
      }
    }

    // =========================================================
    // Shoot arm progress [0,1]
    // =========================================================
    const float progress = shootArmed
      ? 1.f
      : std::min(1.f, static_cast<float>(shootStreak) / static_cast<float>(std::max(1, cfg.shootArmFrames)));

    return PPOGateResult{shootArmed, dribbleArmed, progress};
  }

private:
  PPOGateConfig cfg;

  // Estado persistente por episodio
  bool shootArmed    = false;
  bool dribbleArmed  = false;
  int  shootStreak   = 0;
  int  dribbleStreak = 0;

  /// wrap_angle: normaliza a [-pi, pi]
  static float wrapAngle(float angle)
  {
    constexpr float pi = static_cast<float>(M_PI);
    constexpr float twoPi = 2.f * pi;
    return std::fmod(angle + pi, twoPi) - pi;
  }
};

// ---------------------------------------------------------------------------
// gate_skill_mask — porta gate_skill_mask_from_obs() de train.py
//
// Aplica la máscara sobre los logits ANTES del argmax.
// Si finish_armed=True, walk y stand se llevan a -inf.
// Si shoot_armed=False, shoot se lleva a -inf.
// Si dribble_armed=False, dribble se lleva a -inf.
//
// SKILL_ORDER = {stand=0, walk=1, shoot=2, pass=3, dribble=4, block=5, mark=6, observe=7}
//
// @param logits     Array de 8 floats (skill_logits del ONNX), modificado in-place
// @param gateResult Resultado del gate del frame actual
// ---------------------------------------------------------------------------
inline void applyGateSkillMask(float logits[8], const PPOGateResult& gateResult)
{
  constexpr float NEG_INF = -1e9f;

  // shoot (índice 2): solo si shoot_armed
  if(!gateResult.shootArmed)
    logits[2] = NEG_INF;

  // dribble (índice 4): solo si dribble_armed
  if(!gateResult.dribbleArmed)
    logits[4] = NEG_INF;

  // stand (índice 0) y walk (índice 1): prohibidos cuando finish_armed
  if(gateResult.finishArmed())
  {
    logits[0] = NEG_INF;  // stand
    logits[1] = NEG_INF;  // walk
  }
}

/// argmax sobre los 8 logits ya enmascarados → skill_id
inline int argmaxSkill(const float logits[8])
{
  int best = 0;
  for(int i = 1; i < 8; ++i)
    if(logits[i] > logits[best])
      best = i;
  return best;
}