/**
 * @file StrategyBehaviorControl.cpp
 *
 * This file implements a module that determines the strategy of the team.
 *
 * @author Arne Hasselbring
 */

#include "StrategyBehaviorControl.h"
#include "Debugging/Annotation.h"
#include "Python/Controller/RLSharedState.h"
#include "Representations/Communication/TeamData.h"
#include "Tools/BehaviorControl/Strategy/BehaviorBase.h"
#include "Tools/Modeling/BallPhysics.h"
#include "Debugging/DebugDrawings.h"
#include "Streaming/Output.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

MAKE_MODULE(StrategyBehaviorControl, StrategyBehaviorControl::getExtModuleInfo);

namespace
{
constexpr const char* rlOverrideTeamEnv = "PYBH_RL_OVERRIDE_TEAM";
constexpr const char* rlOverridePlayersEnv = "PYBH_RL_ACTIVE_PLAYERS";
constexpr const char* ppoModelEnv = "PYBH_PPO_MODEL";
constexpr const char* ppoTeamEnv = "PYBH_PPO_TEAM";
constexpr const char* ppoPlayersEnv = "PYBH_PPO_ACTIVE_PLAYERS";
constexpr float disabledLogit = -1e9f;
constexpr float debugPi = 3.14159265358979323846f;
constexpr float standWatchdogRatioThreshold = 0.4f;

float radToDeg(const float radians)
{
  return radians * 180.f / debugPi;
}

int parseEnvInt(const char* envName, int fallback)
{
  const char* value = std::getenv(envName);
  if(!value || !*value)
    return fallback;

  char* end = nullptr;
  const long parsed = std::strtol(value, &end, 10);
  return end != value ? static_cast<int>(parsed) : fallback;
}

template<std::size_t count>
std::array<bool, count + 1> parsePlayerListEnv(const char* envName)
{
  std::array<bool, count + 1> enabled{};
  const char* raw = std::getenv(envName);
  if(!raw || !*raw)
    return enabled;

  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s", raw);
  for(char* token = std::strtok(buffer, ", "); token; token = std::strtok(nullptr, ", "))
  {
    char* end = nullptr;
    const long parsed = std::strtol(token, &end, 10);
    if(end != token && parsed >= 1 && parsed <= static_cast<long>(count))
      enabled[static_cast<std::size_t>(parsed)] = true;
  }
  return enabled;
}

const std::array<bool, RLSharedStateBridge::maxWorldPlayersPerTeam + 1>& rlOverridePlayers()
{
  static const std::array<bool, RLSharedStateBridge::maxWorldPlayersPerTeam + 1> players = parsePlayerListEnv<RLSharedStateBridge::maxWorldPlayersPerTeam>(rlOverridePlayersEnv);

  return players;
}

bool playerListHasAnyEnabled(const std::array<bool, RLSharedStateBridge::maxWorldPlayersPerTeam + 1>& players)
{
  return std::any_of(players.begin() + 1, players.end(), [](const bool enabled) { return enabled; });
}

bool usesExternalRLOverride(const GameState& gameState)
{
  const int teamNumber = parseEnvInt(rlOverrideTeamEnv, -1);
  if(teamNumber < 0 || gameState.ownTeam.number != teamNumber)
    return false;

  const int playerNumber = gameState.playerNumber;
  return playerNumber >= 1 &&
	         playerNumber <= RLSharedStateBridge::maxWorldPlayersPerTeam &&
	         rlOverridePlayers()[static_cast<std::size_t>(playerNumber)];
}

bool playerListHasAnyEnabled(const std::vector<int>& players)
{
  return !players.empty();
}

bool playerListContains(const std::vector<int>& players, const int playerNumber)
{
  return std::find(players.begin(), players.end(), playerNumber) != players.end();
}

bool isPlayBallRole(const Role::Type role)
{
  return role == ActiveRole::toRole(ActiveRole::playBall);
}

std::array<bool, RL::ppoSkillCount> buildStage4SkillMask()
{
  std::array<bool, RL::ppoSkillCount> mask{};
  mask[static_cast<std::size_t>(RL::SkillType::stand)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::walk)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::shoot)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::dribble)] = true;
  return mask;
}

std::array<bool, RL::ppoSkillCount> buildDefenderSkillMask()
{
  std::array<bool, RL::ppoSkillCount> mask{};
  mask[static_cast<std::size_t>(RL::SkillType::stand)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::walk)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::pass)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::block)] = true;
  return mask;
}

bool isDefenderPPO(const std::string& role)
{
  return role == "defender" || role == "defensa";
}

int argmax(const std::array<float, RL::ppoSkillCount>& logits)
{
  return static_cast<int>(std::distance(logits.begin(), std::max_element(logits.begin(), logits.end())));
}

void setProviderDebug(RLPlayerIO& io, const float tx, const float ty, const float tt, const int motionRequest)
{
  io.lock();
  ++io.debugProviderCallCount;
  io.debugProviderTargetX = tx;
  io.debugProviderTargetY = ty;
  io.debugProviderTargetTheta = tt;
  io.debugProviderMotionRequest = motionRequest;
  io.unlock();
}

const char* ppoSkillName(const int skillIndex)
{
  switch(static_cast<RL::SkillType>(skillIndex))
  {
    case RL::SkillType::stand:
      return "stand";
    case RL::SkillType::walk:
      return "walk";
    case RL::SkillType::shoot:
      return "shoot";
    case RL::SkillType::pass:
      return "pass";
    case RL::SkillType::dribble:
      return "dribble";
    case RL::SkillType::block:
      return "block";
    case RL::SkillType::mark:
      return "mark";
    case RL::SkillType::observe:
      return "observe";
    default:
      return "unknown";
  }
}

}

StrategyBehaviorControl::StrategyBehaviorControl() :
  theBehavior(theBallDropInModel, theExtendedGameState, theFieldBall, theFieldDimensions, theFrameInfo,
              theGameState, theRestartBallSearchContext, theTeammatesBallModel)
{
  // v4.2 team model was trained with shoot_open enter=0.30 / exit=0.20
  // (recorded in the ONNX manifest gate_config). Use a dedicated gate instance.
  ppoSkillGateV47.shootOpenEnter = 0.30f;
  ppoSkillGateV47.shootOpenExit = 0.20f;
}

std::vector<ModuleBase::Info> StrategyBehaviorControl::getExtModuleInfo()
{
  auto result = StrategyBehaviorControl::getModuleInfo();
  BehaviorBase::addToModuleInfo(result);
  return result;
}

StrategyBehaviorControl::EmbeddedPPORole StrategyBehaviorControl::selectedEmbeddedPPORole(const GameState& gameState) const
{
  if(!enableEmbeddedPPO)
    return EmbeddedPPORole::none;

  const int teamNumber = parseEnvInt(ppoTeamEnv, embeddedPPOTeamNumber >= 0 ? embeddedPPOTeamNumber : gameState.ownTeam.number);
  if(gameState.ownTeam.number != teamNumber)
    return EmbeddedPPORole::none;

  const int playerNumber = gameState.playerNumber;
  if(playerNumber < 1 || playerNumber > RLSharedStateBridge::maxWorldPlayersPerTeam)
    return EmbeddedPPORole::none;

  const auto configuredPlayers = std::getenv(ppoPlayersEnv) ? parsePlayerListEnv<RLSharedStateBridge::maxWorldPlayersPerTeam>(ppoPlayersEnv) : std::array<bool, RLSharedStateBridge::maxWorldPlayersPerTeam + 1>{};
  if(playerListHasAnyEnabled(configuredPlayers) && !configuredPlayers[static_cast<std::size_t>(playerNumber)])
    return EmbeddedPPORole::none;

  if(playerListContains(embeddedPPODefenderPlayers, playerNumber))
    return EmbeddedPPORole::defender;

  // Merged brain v5: all field players use the same policy with role coordinator.
  // GK is excluded in updateEmbeddedPPO. Takes highest priority when configured.
  if(!embeddedPPOMergedTeamModelPath.empty())
    return EmbeddedPPORole::mergedTeam;

  // Team striker v4.2: if the team model path is configured and this robot is playBall,
  // use the 47-dim model.  Takes priority over the legacy 26-dim striker path.
  if(!embeddedPPOTeamStrikerModelPath.empty() && embeddedPPODynamicPlayBall && isPlayBallRole(theStrategyStatus.role))
    return EmbeddedPPORole::teamStriker;

  if(embeddedPPODynamicPlayBall && isPlayBallRole(theStrategyStatus.role))
    return EmbeddedPPORole::striker;

  if(!playerListHasAnyEnabled(configuredPlayers) &&
     !embeddedPPODynamicPlayBall &&
     playerListHasAnyEnabled(embeddedPPOPlayers) &&
     playerListContains(embeddedPPOPlayers, playerNumber))
    return isDefenderPPO(embeddedPPORole) ? EmbeddedPPORole::defender : EmbeddedPPORole::striker;

  if(playerListHasAnyEnabled(configuredPlayers))
    return isDefenderPPO(embeddedPPORole) ? EmbeddedPPORole::defender : EmbeddedPPORole::striker;

  return EmbeddedPPORole::none;
}

std::string StrategyBehaviorControl::configuredEmbeddedPPOModelPath(const EmbeddedPPORole role) const
{
  const char* envModelPath = std::getenv(ppoModelEnv);
  if(envModelPath && *envModelPath)
    return envModelPath;

  switch(role)
  {
    case EmbeddedPPORole::striker:
      return embeddedPPOStrikerModelPath.empty() ? embeddedPPOModelPath : embeddedPPOStrikerModelPath;
    case EmbeddedPPORole::defender:
      return embeddedPPODefenderModelPath.empty() ? embeddedPPOModelPath : embeddedPPODefenderModelPath;
    case EmbeddedPPORole::teamStriker:
      return embeddedPPOTeamStrikerModelPath;
    case EmbeddedPPORole::mergedTeam:
      return embeddedPPOMergedTeamModelPath;
    case EmbeddedPPORole::none:
    default:
      return {};
  }
}

bool StrategyBehaviorControl::usesEmbeddedPPO(const GameState& gameState) const
{
  const EmbeddedPPORole role = selectedEmbeddedPPORole(gameState);
  if(configuredEmbeddedPPOModelPath(role).empty())
    return false;
  return true;
}

std::string StrategyBehaviorControl::embeddedPPOStatusReason(const GameState& gameState) const
{
  if(!enableEmbeddedPPO)
    return "embedded PPO disabled";

  const int teamNumber = parseEnvInt(ppoTeamEnv, embeddedPPOTeamNumber >= 0 ? embeddedPPOTeamNumber : gameState.ownTeam.number);
  if(gameState.ownTeam.number != teamNumber)
    return "team filtered";

  const int playerNumber = gameState.playerNumber;
  if(playerNumber < 1 || playerNumber > RLSharedStateBridge::maxWorldPlayersPerTeam)
    return "player out of range";

  const auto configuredPlayers = std::getenv(ppoPlayersEnv) ? parsePlayerListEnv<RLSharedStateBridge::maxWorldPlayersPerTeam>(ppoPlayersEnv) : std::array<bool, RLSharedStateBridge::maxWorldPlayersPerTeam + 1>{};
  if(playerListHasAnyEnabled(configuredPlayers) && !configuredPlayers[static_cast<std::size_t>(playerNumber)])
    return "player not enabled";

  const EmbeddedPPORole role = selectedEmbeddedPPORole(gameState);
  if(role == EmbeddedPPORole::none)
    return embeddedPPODynamicPlayBall ? "role not playBall or defender PPO player" : "player not enabled";

  if(configuredEmbeddedPPOModelPath(role).empty())
  {
    if(role == EmbeddedPPORole::defender)
      return "no defender PPO model configured";
    if(role == EmbeddedPPORole::teamStriker)
      return "no team striker PPO model configured";
    if(role == EmbeddedPPORole::mergedTeam)
      return "no merged team PPO model configured";
    return "no striker PPO model configured";
  }

  if(gameState.playerState != GameState::active)
    return "player not active";

  if(gameState.state != GameState::playing)
    return "game not playing";

  if(gameState.ownTeam.isGoalkeeper(playerNumber))
    return "goalkeeper excluded";

  if(role == EmbeddedPPORole::defender)
    return "ready defender";
  if(role == EmbeddedPPORole::teamStriker)
    return "ready team striker (v4.2, 47-dim)";
  if(role == EmbeddedPPORole::mergedTeam)
    return "ready merged team (v5, 47-dim, role coordinator)";
  return "ready striker";
}

void StrategyBehaviorControl::update(SkillRequest& skillRequest)
{
  if(usesExternalRLOverride(theGameState))
  {
    logRLModeIfChanged(RLRuntimeMode::externalOverride, "external override env active");
    resetEmbeddedPPO();
    theStrategyStatus = StrategyStatus();

    if(theGameState.playerState != GameState::active ||
       theGameState.isPenaltyShootout() || theGameState.isInitial() || theGameState.isFinished())
    {
      skillRequest = SkillRequest::Builder::empty();
      return;
    }

    RLPlayerIO& io = RLSharedState::instance().player(theGameState.playerNumber);
    std::string skill;
    float tx = 0.f;
    float ty = 0.f;
    float tt = 0.f;
    int passTarget = -1;
    {
      io.lock();
      skill = io.getSkill();
      tx = io.targetX;
      ty = io.targetY;
      tt = io.targetTheta;
      passTarget = io.passTarget;
      io.unlock();
    }

    if(skill == "walkTo" || skill == "walk")
    {
      skillRequest = SkillRequest::Builder::walkTo(Pose2f(tt, tx, ty));
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::walkToPose));
    }
    else if(skill == "shoot")
    {
      skillRequest = SkillRequest::Builder::shoot();
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::walkToBallAndKick));
    }
    else if(skill == "pass")
    {
      skillRequest = SkillRequest::Builder::passTo(passTarget);
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::walkToBallAndKick));
    }
    else if(skill == "dribble")
    {
      skillRequest = SkillRequest::Builder::dribbleTo(tt);
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::dribble));
    }
    else if(skill == "block")
    {
      skillRequest = SkillRequest::Builder::block(Vector2f(tx, ty));
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::walkToPose));
    }
    else if(skill == "mark")
    {
      skillRequest = SkillRequest::Builder::mark(Vector2f(tx, ty));
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::walkToPose));
    }
    else if(skill == "observe")
    {
      skillRequest = SkillRequest::Builder::observe(Vector2f(tx, ty));
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::stand));
    }
    else
    {
      skillRequest = SkillRequest::Builder::stand();
      setProviderDebug(io, tx, ty, tt, static_cast<int>(MotionRequest::stand));
    }
    return;
  }

  auto* self = updateAgents();

  theBehavior.preProcess();

  if(theGameState.playerState != GameState::active ||
     theGameState.isPenaltyShootout() || theGameState.isStopped())
  {
    // Reset provided representations.
    theStrategyStatus.proposedTactic = Tactic::none;
    theStrategyStatus.acceptedTactic = Tactic::none;
    theStrategyStatus.proposedMirror = false;
    theStrategyStatus.acceptedMirror = false;
    theStrategyStatus.proposedSetPlay = SetPlay::none;
    theStrategyStatus.acceptedSetPlay = SetPlay::none;
    theStrategyStatus.setPlayStep = -1;
    theStrategyStatus.position = Tactic::Position::none;
    theStrategyStatus.role = Role::none;
    skillRequest = SkillRequest::Builder::empty();
  }
  else
  {
    ASSERT(self);

    skillRequest = theBehavior.update(strategy, *self, agents);

    theStrategyStatus.proposedTactic = self->proposedTactic;
    theStrategyStatus.acceptedTactic = self->acceptedTactic;
    theStrategyStatus.proposedMirror = self->proposedMirror;
    theStrategyStatus.acceptedMirror = self->acceptedMirror;
    theStrategyStatus.proposedSetPlay = self->proposedSetPlay;
    theStrategyStatus.acceptedSetPlay = self->acceptedSetPlay;
    theStrategyStatus.setPlayStep = self->setPlayStep;
    theStrategyStatus.position = self->position;
    theStrategyStatus.role = self->role;
  }

  if(!updateEmbeddedPPO(skillRequest))
  {
    resetEmbeddedPPO();
    if(usesEmbeddedPPO(theGameState))
    {
      const bool embeddedReady = theGameState.playerState == GameState::active &&
                                 theGameState.state == GameState::playing &&
                                 !theGameState.ownTeam.isGoalkeeper(theGameState.playerNumber);
      logRLModeIfChanged(embeddedReady ? RLRuntimeMode::embeddedFallback : RLRuntimeMode::embeddedWaiting,
                         embeddedReady ? "embedded PPO configured but fallback path used" : embeddedPPOStatusReason(theGameState));
    }
    else
      logRLModeIfChanged(RLRuntimeMode::bhuman, embeddedPPOStatusReason(theGameState));
  }

  theBehavior.postProcess();
}

void StrategyBehaviorControl::update(StrategyStatus& strategyStatus)
{
  strategyStatus = usesExternalRLOverride(theGameState) ? StrategyStatus() : theStrategyStatus;
}

bool StrategyBehaviorControl::updateEmbeddedPPO(SkillRequest& skillRequest)
{
  if(usesExternalRLOverride(theGameState) ||
     !usesEmbeddedPPO(theGameState) ||
     theGameState.playerState != GameState::active ||
     theGameState.state != GameState::playing ||
     theGameState.ownTeam.isGoalkeeper(theGameState.playerNumber))
    return false;

  if(ppoStandWatchdogCooldownActive)
  {
    if(theFrameInfo.getTimeSince(ppoStandWatchdogCooldownStarted) < std::max(0, embeddedPPOStandWatchdogCooldownMs))
      return false;
    ppoStandWatchdogCooldownActive = false;
  }

  const EmbeddedPPORole ppoRole = selectedEmbeddedPPORole(theGameState);
  if(ppoRole == EmbeddedPPORole::none)
    return false;

  if(!ensureEmbeddedPPOLoaded(ppoRole))
    return false;

  RL::PPOPolicyModel& policyModel = ppoRole == EmbeddedPPORole::defender ? defenderPPOPolicyModel : strikerPPOPolicyModel;
  // (teamStriker and mergedTeam handle their own models in dedicated branches below)

  const RL::PPOGateObservation rawObservation = ppoObservationEncoder.buildRawObservation(
    theFrameInfo,
    theRobotPose,
    theFieldBall,
    theBallModel,
    theBallPercept,
    theObstacleModel,
    theExpectedGoals,
    theTeamData,
    theFieldDimensions);
  const bool defenderPPO = ppoRole == EmbeddedPPORole::defender;
  const bool teamStrikerPPO = ppoRole == EmbeddedPPORole::teamStriker;

  // === TEAM STRIKER (47-dim, v4.2) PATH ===
  if(teamStrikerPPO)
  {
    const RL::PPOGateDecision gateDecision = ppoSkillGateV47.step(rawObservation);

    int passTarget = -1;
    const bool passArmed = gateDecision.finishArmed() && computeStrikerPassArmed(rawObservation, passTarget);

    RL::PPOTeamContext teamCtx = buildTeamContext(gateDecision, true);
    teamCtx.passArmed = passArmed;
    teamCtx.passArmProgress = passArmed ? 1.f : 0.f;

    const std::array<float, RL::ppoObsSize47> obs47 = ppoObservationEncoder.encode47(rawObservation, gateDecision, teamCtx);

    RL::PPOPolicyOutput output;
    std::string error;
    if(!teamStrikerPPOPolicyModel.infer(obs47, output, &error) || !output.valid)
    {
      if(!ppoInferErrorReported)
      {
        OUTPUT_WARNING("[RL] Team striker PPO inference failed"
                       << " player=" << theGameState.playerNumber
                       << " error=" << error);
        ppoInferErrorReported = true;
      }
      return false;
    }

    const std::array<bool, RL::ppoSkillCount> teamMask = buildTeamStrikerMask(gateDecision, passArmed);
    std::array<float, RL::ppoSkillCount> maskedLogits = output.skillLogits;
    for(std::size_t i = 0; i < maskedLogits.size(); ++i)
      if(!teamMask[i])
        maskedLogits[i] = disabledLogit;

    if(gateDecision.finishArmed())
    {
      maskedLogits[static_cast<std::size_t>(RL::SkillType::stand)] = disabledLogit;
      maskedLogits[static_cast<std::size_t>(RL::SkillType::walk)] = disabledLogit;
    }

    const bool anyValid = std::any_of(maskedLogits.begin(), maskedLogits.end(),
                                      [](const float l){ return l > disabledLogit * 0.5f; });
    if(!anyValid)
      return false;

    int selectedSkill = argmax(maskedLogits);

    if(embeddedPPOStandWatchdogMs > 0)
    {
      if(ppoStandWatchdogWindowStarted == 0)
        ppoStandWatchdogWindowStarted = theFrameInfo.time;
      ++ppoStandWatchdogTotalFrames;
      if(selectedSkill == static_cast<int>(RL::SkillType::stand))
        ++ppoStandWatchdogStandFrames;

      const int windowMs = theFrameInfo.getTimeSince(ppoStandWatchdogWindowStarted);
      if(windowMs >= embeddedPPOStandWatchdogMs)
      {
        const float standRatio = ppoStandWatchdogTotalFrames > 0 ?
                                 static_cast<float>(ppoStandWatchdogStandFrames) / static_cast<float>(ppoStandWatchdogTotalFrames) :
                                 0.f;
        const bool standDominates = standRatio >= standWatchdogRatioThreshold && ppoStandWatchdogStandFrames >= 3;
        ppoStandWatchdogWindowStarted = theFrameInfo.time;
        ppoStandWatchdogStandFrames = 0;
        ppoStandWatchdogTotalFrames = 0;
        if(standDominates)
        {
          OUTPUT_WARNING("[RL] Team striker PPO stand watchdog fired"
                         << " player=" << theGameState.playerNumber
                         << " windowMs=" << windowMs
                         << " standRatio=" << standRatio
                         << " action=" << (embeddedPPOStandWatchdogForceWalk ? "forceWalk" : "fallbackBHuman")
                         << " cooldownMs=" << embeddedPPOStandWatchdogCooldownMs);
          if(embeddedPPOStandWatchdogForceWalk)
            selectedSkill = static_cast<int>(RL::SkillType::walk);
          else
          {
            ppoStandWatchdogCooldownActive = true;
            ppoStandWatchdogCooldownStarted = theFrameInfo.time;
            return false;
          }
        }
      }
    }
    else
    {
      ppoStandWatchdogWindowStarted = 0;
      ppoStandWatchdogStandFrames = 0;
      ppoStandWatchdogTotalFrames = 0;
    }

    skillRequest = ppoActionDecoder.decodeTeam(rawObservation, selectedSkill, output.paramMean, passTarget);
    logRLModeIfChanged(RLRuntimeMode::embeddedActive, "embedded team striker PPO v4.2 controlling skill requests");
    logEmbeddedPPODecisionIfChanged(selectedSkill, gateDecision, rawObservation, maskedLogits, output.paramMean, skillRequest);
    ppoInferErrorReported = false;
    return true;
  }

  // === MERGED BRAIN (47-dim, v5) PATH: all field roles with role coordinator ===
  const bool mergedTeamPPO = ppoRole == EmbeddedPPORole::mergedTeam;
  if(mergedTeamPPO)
  {
    const RL::PPOGateDecision gateDecision = ppoSkillGateV47.step(rawObservation);

    // R1: assign roles via D1+TTRB hysteresis (mutates mergedTeamRoleAssignment)
    const int selfRole = assignTeamRoles(rawObservation);  // 0=striker, 1=open, 2=off_ball

    // Update sticky attack/defense phase from team ball
    {
      float tbx = rawObservation.ballX;
      if(theTeammatesBallModel.isValid)
        tbx = theTeammatesBallModel.position.x();
      if(tbx < -300.f)      mergedTeamPhase = 1;
      else if(tbx > 300.f)  mergedTeamPhase = 0;
    }

    // Gather positions of the other roles for coordination target computation.
    float strikerX = rawObservation.robotX, strikerY = rawObservation.robotY;
    float openX = rawObservation.robotX, openY = rawObservation.robotY;
    float ballX = rawObservation.ballX, ballY = rawObservation.ballY;
    if(theTeammatesBallModel.isValid) { ballX = theTeammatesBallModel.position.x(); ballY = theTeammatesBallModel.position.y(); }
    for(const auto& tm : theTeamData.teammates)
    {
      if(tm.number == theGameState.playerNumber || tm.isGoalkeeper) continue;
      const int tmRole = (tm.number >= 1 && tm.number <= 7) ? mergedTeamRoleAssignment[tm.number] : -1;
      const float tx = tm.theRobotPose.translation.x(), ty = tm.theRobotPose.translation.y();
      if(tmRole == 0) { strikerX = tx; strikerY = ty; }
      if(tmRole == 1) { openX = tx; openY = ty; }
    }
    if(selfRole == 0) { strikerX = rawObservation.robotX; strikerY = rawObservation.robotY; }
    if(selfRole == 1) { openX = rawObservation.robotX; openY = rawObservation.robotY; }

    // Compute EMA coordination target for this robot's role.
    float coordX = rawObservation.robotX, coordY = rawObservation.robotY;
    if(selfRole == 1)  // open_support
    {
      const auto raw = computeMergedOpenLaneTarget(ballX, ballY, strikerX, strikerY);
      const float alpha = 0.15f;
      if(!emaOpenLaneInited) { emaOpenLaneX = raw.first; emaOpenLaneY = raw.second; emaOpenLaneInited = true; }
      else { emaOpenLaneX += alpha * (raw.first - emaOpenLaneX); emaOpenLaneY += alpha * (raw.second - emaOpenLaneY); }
      coordX = emaOpenLaneX; coordY = emaOpenLaneY;
    }
    else if(selfRole == 2)  // off_ball_support
    {
      if(!triangleForcedSideSet) { triangleForcedSide = openY >= 0.f ? -1.f : 1.f; triangleForcedSideSet = true; }
      const auto raw = computeMergedTriangleTarget(ballX, ballY, strikerX, strikerY, openX, openY);
      const float alpha = 0.10f;
      if(!emaTriangleInited) { emaTriangleX = raw.first; emaTriangleY = raw.second; emaTriangleInited = true; }
      else { emaTriangleX += alpha * (raw.first - emaTriangleX); emaTriangleY += alpha * (raw.second - emaTriangleY); }
      coordX = emaTriangleX; coordY = emaTriangleY;
    }

    // Compute anchor theta (face coordination target from current robot position)
    const float anchorTheta = std::atan2(coordY - rawObservation.robotY, coordX - rawObservation.robotX);

    // R2: pass-arm for striker only
    int passTarget = -1;
    const bool passArmed = (selfRole == 0) && gateDecision.finishArmed() && computeStrikerPassArmed(rawObservation, passTarget);

    // Build PPOTeamContext with role one-hot and gate bits
    RL::PPOTeamContext teamCtx = buildTeamContext(gateDecision, selfRole == 0);
    teamCtx.isStriker = (selfRole == 0);
    teamCtx.isOpenSupport = (selfRole == 1);
    teamCtx.isOffBallSupport = (selfRole == 2);
    teamCtx.passArmed = passArmed;
    teamCtx.passArmProgress = passArmed ? 1.f : 0.f;
    teamCtx.observeArmed = false;

    const std::array<float, RL::ppoObsSize47> obs47 = ppoObservationEncoder.encode47(rawObservation, gateDecision, teamCtx);

    RL::PPOPolicyOutput output;
    std::string error;
    if(!mergedTeamPPOPolicyModel.infer(obs47, output, &error) || !output.valid)
    {
      if(!ppoInferErrorReported)
      {
        OUTPUT_WARNING("[RL] Merged team PPO inference failed"
                       << " player=" << theGameState.playerNumber
                       << " role=" << selfRole
                       << " error=" << error);
        ppoInferErrorReported = true;
      }
      return false;
    }

    // Role-conditioned gate mask (§4.4)
    const std::array<bool, RL::ppoSkillCount> mergedMask = buildMergedTeamMask(
      gateDecision, selfRole, passArmed, rawObservation.ballRelX, rawObservation.nearestOpponentFrontDist);
    std::array<float, RL::ppoSkillCount> maskedLogits = output.skillLogits;
    for(std::size_t i = 0; i < maskedLogits.size(); ++i)
      if(!mergedMask[i])
        maskedLogits[i] = disabledLogit;

    // Striker: mute stand/walk when finish_armed (same as v4.2 path)
    if(selfRole == 0 && gateDecision.finishArmed())
    {
      maskedLogits[static_cast<std::size_t>(RL::SkillType::stand)] = disabledLogit;
      maskedLogits[static_cast<std::size_t>(RL::SkillType::walk)] = disabledLogit;
    }

    const bool anyValid = std::any_of(maskedLogits.begin(), maskedLogits.end(),
                                      [](const float l){ return l > disabledLogit * 0.5f; });
    if(!anyValid)
      return false;

    int selectedSkill = argmax(maskedLogits);

    // §4.8 Block-priority wrapper: if off_ball and argmax==mark and block armed
    // and ball is nearer threat than opponent → override to block
    if(selfRole == 2 &&
       selectedSkill == static_cast<int>(RL::SkillType::mark) &&
       mergedMask[static_cast<std::size_t>(RL::SkillType::block)] &&
       rawObservation.ballRelX > 0.f &&
       rawObservation.ballRelX < rawObservation.nearestOpponentFrontDist)
    {
      selectedSkill = static_cast<int>(RL::SkillType::block);
    }

    // Stand watchdog (shared with other paths)
    if(embeddedPPOStandWatchdogMs > 0)
    {
      if(ppoStandWatchdogWindowStarted == 0)
        ppoStandWatchdogWindowStarted = theFrameInfo.time;
      ++ppoStandWatchdogTotalFrames;
      if(selectedSkill == static_cast<int>(RL::SkillType::stand))
        ++ppoStandWatchdogStandFrames;
      const int windowMs = theFrameInfo.getTimeSince(ppoStandWatchdogWindowStarted);
      if(windowMs >= embeddedPPOStandWatchdogMs)
      {
        const float standRatio = ppoStandWatchdogTotalFrames > 0 ?
          static_cast<float>(ppoStandWatchdogStandFrames) / static_cast<float>(ppoStandWatchdogTotalFrames) : 0.f;
        const bool standDominates = standRatio >= standWatchdogRatioThreshold && ppoStandWatchdogStandFrames >= 3;
        ppoStandWatchdogWindowStarted = theFrameInfo.time;
        ppoStandWatchdogStandFrames = 0;
        ppoStandWatchdogTotalFrames = 0;
        if(standDominates)
        {
          OUTPUT_WARNING("[RL] Merged team PPO stand watchdog fired"
                         << " player=" << theGameState.playerNumber
                         << " role=" << selfRole
                         << " standRatio=" << standRatio);
          if(embeddedPPOStandWatchdogForceWalk)
            selectedSkill = static_cast<int>(RL::SkillType::walk);
          else
          {
            ppoStandWatchdogCooldownActive = true;
            ppoStandWatchdogCooldownStarted = theFrameInfo.time;
            return false;
          }
        }
      }
    }
    else
    {
      ppoStandWatchdogWindowStarted = 0;
      ppoStandWatchdogStandFrames = 0;
      ppoStandWatchdogTotalFrames = 0;
    }

    // Decode action: striker uses ball-anchored decode; support/defender uses coord-target anchor.
    if(selfRole == 0)
      skillRequest = ppoActionDecoder.decodeTeam(rawObservation, selectedSkill, output.paramMean, passTarget);
    else
      skillRequest = ppoActionDecoder.decodeTeamWalkWithAnchor(rawObservation, selectedSkill, output.paramMean, coordX, coordY, anchorTheta, passTarget);

    logRLModeIfChanged(RLRuntimeMode::embeddedActive, "embedded merged team PPO v5 controlling skill requests");
    logEmbeddedPPODecisionIfChanged(selectedSkill, gateDecision, rawObservation, maskedLogits, output.paramMean, skillRequest);
    ppoInferErrorReported = false;
    return true;
  }

  // === LEGACY STRIKER / DEFENDER PATH (26-dim) ===
  const int defenderPassTarget = defenderPPO ? selectDefenderPPOPassTarget() : -1;
  const bool defenderEngageAllowed = defenderPPO && shouldDefenderPPOEngageBall(rawObservation);
  const RL::PPOGateDecision gateDecision = defenderPPO ? ppoSkillGate.stepDefender(rawObservation, defenderPassTarget > 0, defenderEngageAllowed) : ppoSkillGate.step(rawObservation);
  const RL::PPOObservation observation = defenderPPO ? ppoObservationEncoder.encodeDefender(rawObservation, gateDecision) :
                                                       ppoObservationEncoder.encode(rawObservation, gateDecision);

  RL::PPOPolicyOutput output;
  std::string error;
  if(!policyModel.infer(observation.values, output, &error) || !output.valid)
  {
    if(!ppoInferErrorReported)
    {
      OUTPUT_WARNING("[RL] Embedded PPO inference failed"
                     << " player=" << theGameState.playerNumber
                     << " error=" << error);
      ppoInferErrorReported = true;
    }
    return false;
  }

  std::array<float, RL::ppoSkillCount> maskedLogits = output.skillLogits;
  static const std::array<bool, RL::ppoSkillCount> stage4Mask = buildStage4SkillMask();
  static const std::array<bool, RL::ppoSkillCount> defenderMask = buildDefenderSkillMask();
  const auto& activeMask = defenderPPO ? defenderMask : stage4Mask;
  for(std::size_t i = 0; i < maskedLogits.size(); ++i)
    if(!activeMask[i])
      maskedLogits[i] = disabledLogit;

  if(!defenderPPO)
  {
    if(!gateDecision.shootArmed)
      maskedLogits[static_cast<std::size_t>(RL::SkillType::shoot)] = disabledLogit;
    if(!gateDecision.dribbleArmed)
      maskedLogits[static_cast<std::size_t>(RL::SkillType::dribble)] = disabledLogit;
    if(gateDecision.finishArmed())
    {
      maskedLogits[static_cast<std::size_t>(RL::SkillType::stand)] = disabledLogit;
      maskedLogits[static_cast<std::size_t>(RL::SkillType::walk)] = disabledLogit;
    }
  }
  else if(defenderPPO && !gateDecision.passArmed && !gateDecision.engageArmed)
  {
    for(std::size_t i = 0; i < maskedLogits.size(); ++i)
      if(i != static_cast<std::size_t>(RL::SkillType::stand) && i != static_cast<std::size_t>(RL::SkillType::walk))
        maskedLogits[i] = disabledLogit;
  }

  const bool anyValidSkill = std::any_of(maskedLogits.begin(), maskedLogits.end(), [](const float logit)
  {
    return logit > disabledLogit * 0.5f;
  });
  if(!anyValidSkill)
    return false;

  int selectedSkill = argmax(maskedLogits);
  if(embeddedPPOStandWatchdogMs > 0)
  {
    if(ppoStandWatchdogWindowStarted == 0)
      ppoStandWatchdogWindowStarted = theFrameInfo.time;

    ++ppoStandWatchdogTotalFrames;
    if(selectedSkill == static_cast<int>(RL::SkillType::stand))
      ++ppoStandWatchdogStandFrames;

    const int windowMs = theFrameInfo.getTimeSince(ppoStandWatchdogWindowStarted);
    if(windowMs >= embeddedPPOStandWatchdogMs)
    {
      const float standRatio = ppoStandWatchdogTotalFrames > 0 ?
                               static_cast<float>(ppoStandWatchdogStandFrames) / static_cast<float>(ppoStandWatchdogTotalFrames) :
                               0.f;
      const bool standDominates = standRatio >= standWatchdogRatioThreshold && ppoStandWatchdogStandFrames >= 3;
      ppoStandWatchdogWindowStarted = theFrameInfo.time;
      ppoStandWatchdogStandFrames = 0;
      ppoStandWatchdogTotalFrames = 0;

      if(standDominates)
      {
        OUTPUT_WARNING("[RL] Embedded PPO stand watchdog fired"
                       << " player=" << theGameState.playerNumber
                       << " windowMs=" << windowMs
                       << " standRatio=" << standRatio
                       << " action=" << (embeddedPPOStandWatchdogForceWalk ? "forceWalk" : "fallbackBHuman")
                       << " cooldownMs=" << embeddedPPOStandWatchdogCooldownMs);
        if(embeddedPPOStandWatchdogForceWalk)
          selectedSkill = static_cast<int>(RL::SkillType::walk);
        else
        {
          ppoStandWatchdogCooldownActive = true;
          ppoStandWatchdogCooldownStarted = theFrameInfo.time;
          return false;
        }
      }
    }
  }
  else
  {
    ppoStandWatchdogWindowStarted = 0;
    ppoStandWatchdogStandFrames = 0;
    ppoStandWatchdogTotalFrames = 0;
  }

  skillRequest = defenderPPO ?
                 ppoActionDecoder.decodeDefender(rawObservation, selectedSkill, defenderPassTarget) :
                 ppoActionDecoder.decode(rawObservation, selectedSkill, output.paramMean);
  logRLModeIfChanged(RLRuntimeMode::embeddedActive, defenderPPO ? "embedded defender PPO controlling skill requests" : "embedded striker PPO controlling skill requests");
  logEmbeddedPPODecisionIfChanged(selectedSkill, gateDecision, rawObservation, maskedLogits, output.paramMean, skillRequest);
  ppoInferErrorReported = false;
  return true;
}

bool StrategyBehaviorControl::ensureEmbeddedPPOLoaded(const EmbeddedPPORole role)
{
  RL::PPOPolicyModel& model =
    role == EmbeddedPPORole::defender    ? defenderPPOPolicyModel :
    role == EmbeddedPPORole::teamStriker ? teamStrikerPPOPolicyModel :
    role == EmbeddedPPORole::mergedTeam  ? mergedTeamPPOPolicyModel :
                                           strikerPPOPolicyModel;
  bool& loadAttempted =
    role == EmbeddedPPORole::defender    ? defenderPPOLoadAttempted :
    role == EmbeddedPPORole::teamStriker ? teamStrikerPPOLoadAttempted :
    role == EmbeddedPPORole::mergedTeam  ? mergedTeamPPOLoadAttempted :
                                           strikerPPOLoadAttempted;
  bool& loadErrorReported =
    role == EmbeddedPPORole::defender    ? defenderPPOLoadErrorReported :
    role == EmbeddedPPORole::teamStriker ? teamStrikerPPOLoadErrorReported :
    role == EmbeddedPPORole::mergedTeam  ? mergedTeamPPOLoadErrorReported :
                                           strikerPPOLoadErrorReported;
  std::string& requestedModelPath =
    role == EmbeddedPPORole::defender    ? defenderPPORequestedModelPath :
    role == EmbeddedPPORole::teamStriker ? teamStrikerPPORequestedModelPath :
    role == EmbeddedPPORole::mergedTeam  ? mergedTeamPPORequestedModelPath :
                                           strikerPPORequestedModelPath;
  const char* roleName =
    role == EmbeddedPPORole::defender    ? "defender" :
    role == EmbeddedPPORole::teamStriker ? "team_striker_v4.2" :
    role == EmbeddedPPORole::mergedTeam  ? "merged_team_v5" :
                                           "striker";

  if(model.isLoaded())
    return true;
  if(loadAttempted)
    return false;

  const std::string modelPath = configuredEmbeddedPPOModelPath(role);
  if(modelPath.empty())
    return false;

  loadAttempted = true;
  requestedModelPath = modelPath;

  std::string error;
  if(!model.load(requestedModelPath, &error))
  {
    if(!loadErrorReported)
    {
      OUTPUT_WARNING("[RL] Embedded PPO model load failed"
                     << " role=" << roleName
                     << " player=" << theGameState.playerNumber
                     << " path=" << requestedModelPath
                     << " error=" << error);
      loadErrorReported = true;
    }
    return false;
  }

  loadErrorReported = false;
  ppoInferErrorReported = false;
  OUTPUT_WARNING("[RL] Embedded PPO model loaded"
                 << " role=" << roleName
                 << " player=" << theGameState.playerNumber
                 << " path=" << requestedModelPath);
  return true;
}

int StrategyBehaviorControl::selectDefenderPPOPassTarget() const
{
  int fallbackTarget = -1;
  float fallbackX = -std::numeric_limits<float>::max();
  int forwardTarget = -1;
  float forwardX = -std::numeric_limits<float>::max();
  for(const Agent& agent : agents)
  {
    if(agent.number == theGameState.playerNumber || agent.isGoalkeeper)
      continue;
    const float x = agent.currentPosition.x();
    if(x > fallbackX)
    {
      fallbackX = x;
      fallbackTarget = agent.number;
    }
    if(x > theRobotPose.translation.x() + 500.f && x > forwardX)
    {
      forwardX = x;
      forwardTarget = agent.number;
    }
  }
  return forwardTarget > 0 ? forwardTarget : fallbackTarget;
}

bool StrategyBehaviorControl::shouldDefenderPPOEngageBall(const RL::PPOGateObservation& rawObservation) const
{
  if(rawObservation.timeSinceBallSeenMs > 1200.f || rawObservation.ballX > -500.f)
    return false;

  const Vector2f ball(rawObservation.ballX, rawObservation.ballY);
  const float selfBallDistance = (theRobotPose.translation - ball).norm();
  const bool criticalOwnGoalThreat = rawObservation.ballX < -3000.f && std::abs(rawObservation.ballY) < 1800.f;
  const bool localEmergency = rawObservation.ballX < -1200.f && selfBallDistance < 1100.f;
  float bestTeammateDistance = std::numeric_limits<float>::max();
  bool teammateClearlyOwnsBall = false;

  for(const Agent& agent : agents)
  {
    if(agent.number == theGameState.playerNumber || agent.isGoalkeeper)
      continue;

    const float teammateDistance = (agent.currentPosition - ball).norm();
    bestTeammateDistance = std::min(bestTeammateDistance, teammateDistance);
    if(teammateDistance + 350.f < selfBallDistance && agent.currentPosition.x() > rawObservation.ballX - 500.f)
      teammateClearlyOwnsBall = true;
  }

  if(criticalOwnGoalThreat)
    return true;
  if(teammateClearlyOwnsBall)
    return false;
  if(localEmergency)
    return true;

  const bool defensiveThreat = rawObservation.ballX < -1600.f &&
                               (rawObservation.nearestOpponentDist <= 0.f || rawObservation.nearestOpponentDist < 2200.f);
  const bool noBetterTeammate = bestTeammateDistance == std::numeric_limits<float>::max() ||
                                selfBallDistance <= bestTeammateDistance + 250.f;
  return defensiveThreat && noBetterTeammate;
}

void StrategyBehaviorControl::logRLModeIfChanged(const RLRuntimeMode mode, const std::string& reason)
{
  if(hasLoggedRLRuntimeMode && lastLoggedRLRuntimeMode == mode && lastLoggedRLRuntimeReason == reason)
    return;

  const char* modeName = "Unknown";
  switch(mode)
  {
    case RLRuntimeMode::bhuman:
      modeName = "BHuman";
      break;
    case RLRuntimeMode::externalOverride:
      modeName = "ExternalRL";
      break;
    case RLRuntimeMode::embeddedWaiting:
      modeName = "EmbeddedPPOWaiting";
      break;
    case RLRuntimeMode::embeddedFallback:
      modeName = "EmbeddedPPOFallback";
      break;
    case RLRuntimeMode::embeddedActive:
      modeName = "EmbeddedPPOActive";
      break;
  }

  OUTPUT_WARNING("[RL] mode=" << modeName
                 << " player=" << theGameState.playerNumber
                 << " gameState=" << static_cast<int>(theGameState.state)
                 << " playerState=" << static_cast<int>(theGameState.playerState)
                 << " reason=" << reason);

  hasLoggedRLRuntimeMode = true;
  lastLoggedRLRuntimeMode = mode;
  lastLoggedRLRuntimeReason = reason;
  if(mode != RLRuntimeMode::embeddedActive)
  {
    lastLoggedEmbeddedPPOSkillIndex = -1;
    lastLoggedEmbeddedPPOShootArmed = false;
    lastLoggedEmbeddedPPODribbleArmed = false;
    lastLoggedEmbeddedPPOTimestamp = 0;
  }
}

void StrategyBehaviorControl::logEmbeddedPPODecisionIfChanged(
  const int skillIndex,
  const RL::PPOGateDecision& gateDecision,
  const RL::PPOGateObservation& rawObservation,
  const std::array<float, RL::ppoSkillCount>& maskedLogits,
  const std::array<float, RL::ppoParamCount>& paramMean,
  const SkillRequest& skillRequest)
{
  const bool sameDecision = lastLoggedEmbeddedPPOSkillIndex == skillIndex &&
                            lastLoggedEmbeddedPPOShootArmed == gateDecision.shootArmed &&
                            lastLoggedEmbeddedPPODribbleArmed == gateDecision.dribbleArmed;
  const bool heartbeatDue = lastLoggedEmbeddedPPOTimestamp == 0 ||
                            theFrameInfo.getTimeSince(lastLoggedEmbeddedPPOTimestamp) >= 1000;
  if(sameDecision && !heartbeatDue)
    return;

  const float ballDistance = std::hypot(rawObservation.ballRelX, rawObservation.ballRelY);
  const float ballAngleDeg = radToDeg(std::abs(std::atan2(rawObservation.ballRelY, rawObservation.ballRelX)));
  const float goalDistance = std::hypot(4500.f - rawObservation.ballX, rawObservation.ballY);
  const float goalBearing = std::atan2(-rawObservation.robotY, 4500.f - rawObservation.robotX);
  const float alignDeg = radToDeg(std::abs(std::atan2(
    std::sin(rawObservation.robotTheta - goalBearing),
    std::cos(rawObservation.robotTheta - goalBearing))));
  const bool fresh = rawObservation.naturalTimeSinceBallSeenMs <= 600.f;
  const bool engageEnterFresh = rawObservation.naturalTimeSinceBallSeenMs <= 1500.f;
  const bool engageHoldFresh = rawObservation.naturalTimeSinceBallSeenMs <= 2500.f;
  const bool laneEnter = rawObservation.shotOpeningWithObstacles >= 0.6f || rawObservation.canScoreNow;
  const bool laneHold = rawObservation.shotOpeningWithObstacles >= 0.45f || rawObservation.canScoreNow;
  const bool shootEnter = fresh &&
                          ballDistance <= 380.f &&
                          ballAngleDeg <= 25.f &&
                          goalDistance <= 3000.f &&
                          laneEnter &&
                          alignDeg <= 30.f;
  const bool shootHold = fresh &&
                         ballDistance <= 460.f &&
                         ballAngleDeg <= 35.f &&
                         goalDistance <= 3200.f &&
                         laneHold &&
                         alignDeg <= 45.f;
  const bool dribbleEnter = engageEnterFresh &&
                            ballDistance <= 450.f &&
                            ballAngleDeg <= 35.f;
  const bool dribbleHold = engageHoldFresh &&
                           ballDistance <= 600.f &&
                           ballAngleDeg <= 55.f;
  const auto standLogit = maskedLogits[static_cast<std::size_t>(RL::SkillType::stand)];
  const auto walkLogit = maskedLogits[static_cast<std::size_t>(RL::SkillType::walk)];
  const auto shootLogit = maskedLogits[static_cast<std::size_t>(RL::SkillType::shoot)];
  const auto dribbleLogit = maskedLogits[static_cast<std::size_t>(RL::SkillType::dribble)];
  const Pose2f rawRobotPose(rawObservation.robotTheta, rawObservation.robotX, rawObservation.robotY);
  const Pose2f targetRelative = rawRobotPose.inverse() * skillRequest.target;

  OUTPUT_WARNING("[RL] Embedded PPO action"
                 << " player=" << theGameState.playerNumber
                 << " skill=" << ppoSkillName(skillIndex)
                 << " shootArmed=" << static_cast<int>(gateDecision.shootArmed)
                 << " dribbleArmed=" << static_cast<int>(gateDecision.dribbleArmed)
                 << " shootArmProgress=" << gateDecision.shootArmProgress
                 << " dBall=" << ballDistance
                 << " aBallDeg=" << ballAngleDeg
                 << " goalDist=" << goalDistance
                 << " alignDeg=" << alignDeg
                 << " naturalSeenMs=" << rawObservation.naturalTimeSinceBallSeenMs
                 << " seenMs=" << rawObservation.timeSinceBallSeenMs
                 << " shotOpen=" << rawObservation.shotOpeningWithObstacles
                 << " canScoreNow=" << static_cast<int>(rawObservation.canScoreNow)
                 << " drEnter=" << static_cast<int>(dribbleEnter)
                 << " drHold=" << static_cast<int>(dribbleHold)
                 << " shEnter=" << static_cast<int>(shootEnter)
                 << " shHold=" << static_cast<int>(shootHold)
                 << " logits[s,w,d,sh]="
                 << standLogit << "," << walkLogit << "," << dribbleLogit << "," << shootLogit
                 << " params[x,y,th,p]="
                 << paramMean[0] << "," << paramMean[1] << "," << paramMean[2] << "," << paramMean[3]
                 << " targetAbs[x,y,th]="
                 << skillRequest.target.translation.x() << "," << skillRequest.target.translation.y() << ","
                 << static_cast<float>(skillRequest.target.rotation)
                 << " targetRel[x,y,th]="
                 << targetRelative.translation.x() << "," << targetRelative.translation.y() << ","
                 << static_cast<float>(targetRelative.rotation));

  lastLoggedEmbeddedPPOSkillIndex = skillIndex;
  lastLoggedEmbeddedPPOShootArmed = gateDecision.shootArmed;
  lastLoggedEmbeddedPPODribbleArmed = gateDecision.dribbleArmed;
  lastLoggedEmbeddedPPOTimestamp = theFrameInfo.time;
}

void StrategyBehaviorControl::resetEmbeddedPPO()
{
  ppoSkillGate.reset();
  ppoSkillGateV47.reset();
  ppoObservationEncoder.reset();
  // Clear merged brain coordinator state
  for(int i = 0; i < 8; ++i) { mergedTeamRoleAssignment[i] = -1; mergedTeamCandidateStreak[i] = 0; }
  mergedTeamPhase = 0;
  emaOpenLaneInited = false;
  emaTriangleInited = false;
  triangleForcedSideSet = false;
}

RL::PPOTeamContext StrategyBehaviorControl::buildTeamContext(const RL::PPOGateDecision& /*gateDecision*/, const bool isStriker) const
{
  RL::PPOTeamContext ctx;
  ctx.isStriker = isStriker;
  ctx.isOpenSupport = false;
  ctx.isOffBallSupport = false;

  int count = 0;
  for(const auto& teammate : theTeamData.teammates)
  {
    if(count >= RL::PPOTeamContext::maxTeammates)
      break;
    if(teammate.number == theGameState.playerNumber || teammate.isGoalkeeper)
      continue;
    const float ageMs = static_cast<float>(theFrameInfo.getTimeSince(teammate.theFrameInfo.time));
    ctx.teammates[count].fieldX = teammate.theRobotPose.translation.x();
    ctx.teammates[count].fieldY = teammate.theRobotPose.translation.y();
    ctx.teammates[count].ageMs = ageMs;
    const float tmBallDist = teammate.theBallModel.estimate.position.norm();
    ctx.teammates[count].isEngaging =
      ageMs < RL::PPOTeamContext::teammateStaleMsThreshold &&
      tmBallDist < RL::PPOTeamContext::engagingDistanceMm;
    ++count;
  }
  ctx.teammateCount = count;

  if(theTeammatesBallModel.isValid &&
     !std::isnan(theTeammatesBallModel.position.x()) &&
     !std::isnan(theTeammatesBallModel.position.y()))
  {
    ctx.teamBallX = theTeammatesBallModel.position.x();
    ctx.teamBallY = theTeammatesBallModel.position.y();
    ctx.teamBallAgeMs = static_cast<float>(theFrameInfo.getTimeSince(theTeammatesBallModel.timeWhenLastSeen));
    ctx.teamBallValid = ctx.teamBallAgeMs < 5000.f;
  }
  else
  {
    ctx.teamBallX = 0.f;
    ctx.teamBallY = 0.f;
    ctx.teamBallAgeMs = 5000.f;
    ctx.teamBallValid = false;
  }

  ctx.goalX = theFieldDimensions.xPosOpponentGroundLine;
  ctx.goalY = 0.f;

  ctx.passArmed = false;
  ctx.observeArmed = false;
  ctx.passArmProgress = 0.f;

  return ctx;
}

bool StrategyBehaviorControl::computeStrikerPassArmed(const RL::PPOGateObservation& rawObs, int& outPassTarget) const
{
  outPassTarget = -1;
  if(rawObs.shotOpeningWithObstacles >= 0.55f)
    return false;

  int forwardTarget = -1;
  float forwardX = -std::numeric_limits<float>::max();
  for(const Agent& agent : agents)
  {
    if(agent.number == theGameState.playerNumber || agent.isGoalkeeper)
      continue;
    const float x = agent.currentPosition.x();
    if(x > rawObs.robotX + 300.f && x > forwardX)
    {
      forwardX = x;
      forwardTarget = agent.number;
    }
  }

  if(forwardTarget < 0)
    return false;

  outPassTarget = forwardTarget;
  return true;
}

std::array<bool, RL::ppoSkillCount> StrategyBehaviorControl::buildTeamStrikerMask(const RL::PPOGateDecision& gateDecision, const bool passArmed) const
{
  std::array<bool, RL::ppoSkillCount> mask{};
  mask[static_cast<std::size_t>(RL::SkillType::stand)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::walk)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::shoot)] = gateDecision.shootArmed;
  mask[static_cast<std::size_t>(RL::SkillType::pass)] = passArmed;
  mask[static_cast<std::size_t>(RL::SkillType::dribble)] = gateDecision.dribbleArmed;
  mask[static_cast<std::size_t>(RL::SkillType::block)] = false;
  mask[static_cast<std::size_t>(RL::SkillType::mark)] = false;
  mask[static_cast<std::size_t>(RL::SkillType::observe)] = false;
  return mask;
}

// ---------------------------------------------------------------------------
// Merged brain v5 helpers — ports of multiagent_teacher.py
// ---------------------------------------------------------------------------

namespace
{
  // Constants (single source of truth — mirrors multiagent_teacher.py §14.12.1)
  constexpr float MERGED_ROLE_HYSTERESIS_FRAMES = 7.f;
  constexpr float MERGED_ROLE_MIN_GAP_MM = 200.f;
  constexpr float MERGED_TEAMMATE_FRESH_MS = 1000.f;
  constexpr float MERGED_BALL_CONTROL_RADIUS_MM = 350.f;
  constexpr float MERGED_SIGMA_TEAM_MM = 800.f;
  constexpr float MERGED_MIN_PASS_MM = 400.f;
  constexpr float MERGED_MAX_PASS_MM = 4000.f;
  constexpr float MERGED_GOAL_X_MM = 4500.f;

  // forward_rating: port of Forward::rating (PositionRoles/Forward.cpp:24-38)
  float mergedForwardRatingXY(const float cx, const float cy, const float bx, const float by)
  {
    const float passDist = std::hypot(cx - bx, cy - by);
    if(passDist < MERGED_MIN_PASS_MM || passDist > MERGED_MAX_PASS_MM)
      return 0.f;
    return std::clamp(cx / MERGED_GOAL_X_MM, 0.f, 1.f);
  }

  // separation_penalty: port of Midfielder::rating Gaussian (Midfielder.cpp:60-72)
  float mergedSepPenalty(const float cx, const float cy, const float t1x, const float t1y, const float t2x, const float t2y)
  {
    const float d1sq = (cx - t1x) * (cx - t1x) + (cy - t1y) * (cy - t1y);
    const float d2sq = (cx - t2x) * (cx - t2x) + (cy - t2y) * (cy - t2y);
    const float minDsq = std::min(d1sq, d2sq);
    return 1.f - std::exp(-0.5f * minDsq / (MERGED_SIGMA_TEAM_MM * MERGED_SIGMA_TEAM_MM));
  }

  // SUPPORT_SLOT_CANDIDATE_GRID (field-frame xy; y mirrored by ball_y_sign)
  constexpr std::array<std::pair<float, float>, 6> mergedSupportSlots{{
    {800.f, 1200.f}, {1200.f, 800.f}, {400.f, 1400.f}, {-400.f, 1200.f}, {2000.f, 1300.f}, {2800.f, 1200.f}
  }};
}

int StrategyBehaviorControl::assignTeamRoles(const RL::PPOGateObservation& rawObs)
{
  // Collect field agents: self + non-GK teammates from TeamData
  struct AgentInfo
  {
    int number;
    float x, y;
    float ballDist;
    float ballRelDist;   // direct ego ball dist (for D1 ball-control check)
    float ballTimeSinceMs;
  };
  std::vector<AgentInfo> fieldAgents;

  // Team ball (prefer teammates ball model if valid)
  float tbx = rawObs.ballX, tby = rawObs.ballY;
  bool tbValid = rawObs.timeSinceBallSeenMs < MERGED_TEAMMATE_FRESH_MS;
  if(theTeammatesBallModel.isValid)
  {
    tbx = theTeammatesBallModel.position.x();
    tby = theTeammatesBallModel.position.y();
    tbValid = true;
  }

  // Self
  const float selfBallRelDist = std::hypot(rawObs.ballRelX, rawObs.ballRelY);
  const float selfBallDist = std::hypot(rawObs.robotX - tbx, rawObs.robotY - tby);
  fieldAgents.push_back({theGameState.playerNumber, rawObs.robotX, rawObs.robotY,
                         selfBallDist, selfBallRelDist, rawObs.timeSinceBallSeenMs});

  // Teammates (non-GK only)
  for(const auto& tm : theTeamData.teammates)
  {
    if(tm.number == theGameState.playerNumber || tm.isGoalkeeper)
      continue;
    const float ageMs = static_cast<float>(theFrameInfo.getTimeSince(tm.theFrameInfo.time));
    const float tx = tm.theRobotPose.translation.x(), ty = tm.theRobotPose.translation.y();
    const float dist = std::hypot(tx - tbx, ty - tby);
    const float tmBallRel = tm.theBallModel.estimate.position.norm();
    fieldAgents.push_back({tm.number, tx, ty, dist, tmBallRel, ageMs});
  }

  if(fieldAgents.empty())
    return 0;  // only robot on field → striker

  // Sort by (ballDist, number) for tie-breaking consistency
  std::sort(fieldAgents.begin(), fieldAgents.end(), [](const AgentInfo& a, const AgentInfo& b)
  {
    if(a.ballDist != b.ballDist) return a.ballDist < b.ballDist;
    return a.number < b.number;
  });

  const int candidate = fieldAgents[0].number;

  // D1: ball-control override (physical possession bypasses TTRB hysteresis)
  const bool candidateHasBall = (fieldAgents[0].ballRelDist < MERGED_BALL_CONTROL_RADIUS_MM &&
                                  fieldAgents[0].ballTimeSinceMs <= MERGED_TEAMMATE_FRESH_MS);

  // Find previous striker
  int prevStriker = -1;
  for(int i = 1; i <= 7; ++i)
    if(mergedTeamRoleAssignment[i] == 0) { prevStriker = i; break; }

  // TTRB hysteresis
  int committedStriker = candidate;
  if(prevStriker >= 0 && prevStriker != candidate && !candidateHasBall)
  {
    float prevDist = 1e9f, candDist = 1e9f;
    for(const auto& a : fieldAgents)
    {
      if(a.number == prevStriker) prevDist = a.ballDist;
      if(a.number == candidate)  candDist = a.ballDist;
    }
    const float gap = prevDist - candDist;
    if(gap < MERGED_ROLE_MIN_GAP_MM)
    {
      committedStriker = prevStriker;
      for(int i = 0; i < 8; ++i) mergedTeamCandidateStreak[i] = 0;
    }
    else
    {
      int& streak = mergedTeamCandidateStreak[candidate < 8 ? candidate : 0];
      ++streak;
      if(streak < static_cast<int>(MERGED_ROLE_HYSTERESIS_FRAMES))
        committedStriker = prevStriker;
      else
      {
        committedStriker = candidate;
        for(int i = 0; i < 8; ++i) mergedTeamCandidateStreak[i] = 0;
      }
    }
  }
  else
  {
    for(int i = 0; i < 8; ++i) mergedTeamCandidateStreak[i] = 0;
  }

  // Clear and set roles
  for(int i = 0; i < 8; ++i) mergedTeamRoleAssignment[i] = -1;
  mergedTeamRoleAssignment[committedStriker < 8 ? committedStriker : 0] = 0;

  std::vector<int> nonStrikers;
  for(const auto& a : fieldAgents)
    if(a.number != committedStriker) nonStrikers.push_back(a.number);

  if(nonStrikers.size() == 1)
  {
    const int id = nonStrikers[0];
    if(id >= 0 && id < 8) mergedTeamRoleAssignment[id] = (mergedTeamPhase == 1) ? 2 : 1;
  }
  else if(!nonStrikers.empty())
  {
    if(mergedTeamPhase == 1)
    {
      // Defense: deepest (min x) = off_ball_support, rest = open_support
      int deepest = nonStrikers[0];
      float deepestX = std::numeric_limits<float>::max();
      for(int id : nonStrikers)
        for(const auto& a : fieldAgents)
          if(a.number == id && a.x < deepestX) { deepestX = a.x; deepest = id; }
      for(int id : nonStrikers)
        if(id >= 0 && id < 8) mergedTeamRoleAssignment[id] = (id == deepest) ? 2 : 1;
    }
    else
    {
      // Attack: open_support = highest forward rating
      int openSup = nonStrikers[0];
      float bestRating = -1.f;
      for(int id : nonStrikers)
        for(const auto& a : fieldAgents)
          if(a.number == id)
          {
            const float r = mergedForwardRatingXY(a.x, a.y, tbx, tby);
            if(r > bestRating || (r == bestRating && id < openSup)) { bestRating = r; openSup = id; }
          }
      for(int id : nonStrikers)
        if(id >= 0 && id < 8) mergedTeamRoleAssignment[id] = (id == openSup) ? 1 : 2;
    }
  }

  const int selfNum = theGameState.playerNumber;
  return (selfNum >= 0 && selfNum < 8 && mergedTeamRoleAssignment[selfNum] >= 0)
         ? mergedTeamRoleAssignment[selfNum] : 2;
}

std::pair<float, float> StrategyBehaviorControl::computeMergedOpenLaneTarget(
  const float ballX, const float ballY, const float strikerX, const float strikerY) const
{
  // Port of multiagent_teacher.compute_open_lane_target + support_target.
  // Score 6 candidates: forward_rating(cand, ball) × separation_penalty(cand, [striker, self]).
  // Mirror y by ball_y_sign.
  const float ySign = (ballY >= 0.f) ? 1.f : -1.f;
  float bestScore = -1.f;
  float bestX = 800.f, bestY = ySign * 1200.f;
  for(const auto& slot : mergedSupportSlots)
  {
    const float cx = slot.first, cy = slot.second * ySign;
    const float fwd = mergedForwardRatingXY(cx, cy, ballX, ballY);
    // Separate from striker only (compute_open_lane_target passes other_support=None in Python)
    const float sep = mergedSepPenalty(cx, cy, strikerX, strikerY, strikerX, strikerY);
    const float score = fwd * sep;
    if(score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
  }
  return {bestX, bestY};
}

std::pair<float, float> StrategyBehaviorControl::computeMergedTriangleTarget(
  const float ballX, const float ballY, const float strikerX, const float strikerY,
  const float openX, const float openY) const
{
  // Port of multiagent_teacher.compute_triangle_target.
  // Mirror y to OPPOSITE side from open_support, using triangleForcedSide.
  const float ySignFlipped = triangleForcedSideSet ? (triangleForcedSide >= 0.f ? 1.f : -1.f)
                                                   : (openY >= 0.f ? -1.f : 1.f);
  float bestScore = -1.f;
  float bestX = 800.f, bestY = ySignFlipped * 1200.f;
  for(const auto& slot : mergedSupportSlots)
  {
    const float cx = slot.first, cy = slot.second * ySignFlipped;
    const float fwd = mergedForwardRatingXY(cx, cy, ballX, ballY);
    const float sep = mergedSepPenalty(cx, cy, strikerX, strikerY, openX, openY);
    const float score = fwd * sep;
    if(score > bestScore) { bestScore = score; bestX = cx; bestY = cy; }
  }
  return {bestX, bestY};
}

std::array<bool, RL::ppoSkillCount> StrategyBehaviorControl::buildMergedTeamMask(
  const RL::PPOGateDecision& gateDecision,
  const int role,
  const bool passArmed,
  const float ballRelX,
  const float oppFront) const
{
  // Port of train.py::gate_skill_mask_from_obs — role-conditioned forbid-invalid.
  // role: 0=striker, 1=open_support, 2=off_ball_support
  // Thresholds: block 1800mm, mark 1500mm; normalized by X_RANGE=4500.
  constexpr float xRange = 4500.f;
  std::array<bool, RL::ppoSkillCount> mask{};
  mask[static_cast<std::size_t>(RL::SkillType::stand)] = true;
  mask[static_cast<std::size_t>(RL::SkillType::walk)] = true;

  if(role == 0)
  {
    // Striker: shoot/dribble follow arm flags; pass requires passArmed; block/mark/observe forbidden.
    mask[static_cast<std::size_t>(RL::SkillType::shoot)] = gateDecision.shootArmed;
    mask[static_cast<std::size_t>(RL::SkillType::pass)] = passArmed;
    mask[static_cast<std::size_t>(RL::SkillType::dribble)] = gateDecision.dribbleArmed;
    mask[static_cast<std::size_t>(RL::SkillType::block)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::mark)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::observe)] = false;
  }
  else if(role == 1)
  {
    // open_support: mostly walk; block iff dribble_armed; observe iff armed; no shoot/pass/mark.
    const bool blockAllowed = gateDecision.dribbleArmed;
    mask[static_cast<std::size_t>(RL::SkillType::shoot)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::pass)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::dribble)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::block)] = blockAllowed;
    mask[static_cast<std::size_t>(RL::SkillType::mark)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::observe)] = false;  // observe_armed not tracked yet
  }
  else
  {
    // off_ball_support (defender): block/mark/dribble(escape) armed from obs columns.
    const float ballRelXNorm = ballRelX / xRange;
    const float oppFrontNorm = oppFront / xRange;
    const bool blockThreat = (ballRelXNorm > 0.f) && (ballRelXNorm < (1800.f / xRange));
    const bool markThreat = (oppFrontNorm > 0.f) && (oppFrontNorm < (1500.f / xRange));
    mask[static_cast<std::size_t>(RL::SkillType::shoot)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::pass)] = false;
    mask[static_cast<std::size_t>(RL::SkillType::dribble)] = gateDecision.dribbleArmed;
    mask[static_cast<std::size_t>(RL::SkillType::block)] = blockThreat || gateDecision.dribbleArmed;
    mask[static_cast<std::size_t>(RL::SkillType::mark)] = markThreat;
    mask[static_cast<std::size_t>(RL::SkillType::observe)] = false;
  }
  return mask;
}

Agent* StrategyBehaviorControl::updateAgents()
{
  // Add agents that are active now but weren't before.
  for(unsigned int i = 0; i < theGameState.ownTeam.playerStates.size(); ++i)
  {
    const int number = Settings::lowestValidPlayerNumber + i;
    if((number == theGameState.playerNumber ? theGameState.playerState : theGameState.ownTeam.playerStates[i]) != GameState::active)
      continue;
    if(std::any_of(agents.begin(), agents.end(), [&](const Agent& agent){return agent.number == number;}))
      continue;
    agents.emplace_back();
    Agent& agent = agents.back();
    agent.number = number;
    agent.lastKnownTimestamp = theFrameInfo.time; // This is to avoid that "self" will write things into lastKnown* that were already sent a long time ago.
    agent.lastKnownPose = Vector2f(theFieldDimensions.xPosOwnPenaltyMark, number % 2 ? theFieldDimensions.yPosLeftSideline : theFieldDimensions.yPosRightSideline);
  }

  // Remove agents that are not active anymore.
  for(auto it = agents.begin(); it != agents.end();)
  {
    Agent& agent = *it;
    if((agent.number == theGameState.playerNumber ? theGameState.playerState : theGameState.ownTeam.playerStates[agent.number - Settings::lowestValidPlayerNumber]) != GameState::active)
      it = agents.erase(it);
    else
    {
      agent.isGoalkeeper = theGameState.ownTeam.isGoalkeeper(agent.number);
      ++it;
    }
  }

  // The list of agents is now final for this frame, so the self pointer can be set.
  Agent* self = nullptr;
  for(Agent& agent : agents)
    if(agent.number == theGameState.playerNumber)
    {
      self = &agent;
      break;
    }

  if(self)
    updateAgentBySelf(*self);

  if(theGameState.kickOffSetupFromSidelines)
  {
    for(Agent& agent : agents)
    {
      agent.lastKnownPose = theSetupPoses.getPoseOfRobot(agent.number).position;
    }
  }
  else if(theGameState.isSet() && theExtendedGameState.wasReady())
  {
    for(Agent& agent : agents)
    {
      agent.lastKnownPose = agent.basePose.translation;
      // Pretend that all other agents are where they are supposed to be and see the ball where they are supposed to see it - until the first message in playing arrives.
      if(&agent != self)
      {
        agent.pose = agent.basePose;
        agent.ballPosition = agent.pose.inverse() * (theGameState.isPenaltyKick() ? Vector2f(theGameState.isForOwnTeam() ? theFieldDimensions.xPosOpponentPenaltyMark : theFieldDimensions.xPosOwnPenaltyMark, 0.f) :  Vector2f::Zero());
        agent.ballVelocity = Vector2f::Zero();
        agent.timeWhenBallLastSeen = agent.timestamp;
        agent.timeWhenBallDisappeared = agent.timestamp;
        agent.disagreeOnBall = false;
        agent.isUpright = true;
        agent.timeWhenLastUpright = agent.timestamp;
        if(self)
        {
          agent.proposedTactic = self->proposedTactic;
          agent.proposedSetPlay = self->proposedSetPlay;
          agent.proposedMirror = self->proposedMirror;
        }
      }
    }
  }
  else
  {
    for(const ReceivedTeamMessage& teamMessage : theReceivedTeamMessages.messages)
    {
      auto it = std::find_if(agents.begin(), agents.end(), [&](const Agent& agent){return agent.number == teamMessage.number;});
      if(it != agents.end())
        updateAgentByTeamMessage(*it, teamMessage);
    }
  }

  for(Agent& agent : agents)
    updateCurrentPosition(agent);

  DEBUG_DRAWING("module:StrategyBehaviorControl:agents", "drawingOnField")
  {
    for(const Agent& agent : agents)
    {
      DRAW_TEXT("module:StrategyBehaviorControl:agents", agent.lastKnownPose.translation.x() - 50, agent.lastKnownPose.translation.y() - 50, 100, ColorRGBA::black, std::to_string(agent.number));
      CIRCLE("module:StrategyBehaviorControl:agents", agent.lastKnownPose.translation.x(), agent.lastKnownPose.translation.y(), 100, 10, Drawings::dashedPen, ColorRGBA::black, Drawings::noBrush, ColorRGBA::black);
      LINE("module:StrategyBehaviorControl:agents", agent.lastKnownPose.translation.x(), agent.lastKnownPose.translation.y(), agent.currentPosition.x(), agent.currentPosition.y(), 10, Drawings::dashedPen, ColorRGBA::black);
      CROSS("module:StrategyBehaviorControl:agents", agent.currentPosition.x(), agent.currentPosition.y(), 100, 10, Drawings::dashedPen, ColorRGBA::black);
    }
  }

  return self;
}

void StrategyBehaviorControl::updateAgentBySelf(Agent& agent)
{
  if(theSentTeamMessage.theFrameInfo.time > agent.lastKnownTimestamp)
  {
    agent.lastKnownTimestamp = theSentTeamMessage.theFrameInfo.time;
    agent.lastKnownPose = theSentTeamMessage.theRobotPose;
    agent.lastKnownTarget = theSentTeamMessage.theBehaviorStatus.walkingTo;
    agent.lastKnownSpeed = theSentTeamMessage.theBehaviorStatus.speed;
  }
  agent.timestamp = theFrameInfo.time;
  agent.pose = theRobotPose;
  agent.ballPosition = theBallModel.estimate.position;
  agent.ballVelocity = theBallModel.estimate.velocity;
  agent.timeWhenBallLastSeen = theBallModel.timeWhenLastSeen;
  agent.timeWhenBallDisappeared = theBallModel.timeWhenDisappeared;
  agent.disagreeOnBall = false;
  agent.isUpright = (theFallDownState.state == FallDownState::upright || theFallDownState.state == FallDownState::staggering || theFallDownState.state == FallDownState::squatting) &&
                    (theGroundContactState.contact && theMotionInfo.executedPhase != MotionPhase::getUp && theMotionInfo.executedPhase != MotionPhase::fall);
  if(agent.isUpright)
    agent.timeWhenLastUpright = theFrameInfo.time;
  agent.proposedTactic = theStrategyStatus.proposedTactic;
  agent.acceptedTactic = theStrategyStatus.acceptedTactic;
  agent.proposedMirror = theStrategyStatus.proposedMirror;
  agent.acceptedMirror = theStrategyStatus.acceptedMirror;
  agent.proposedSetPlay = theStrategyStatus.proposedSetPlay;
  agent.acceptedSetPlay = theStrategyStatus.acceptedSetPlay;
  agent.setPlayStep = theStrategyStatus.setPlayStep;
  agent.position = theStrategyStatus.position;
  agent.role = theStrategyStatus.role;
}

void StrategyBehaviorControl::updateAgentByTeamMessage(Agent& agent, const ReceivedTeamMessage& teamMessage)
{
  agent.lastKnownTimestamp = teamMessage.theFrameInfo.time;
  agent.lastKnownPose = teamMessage.theRobotPose;
  agent.lastKnownTarget = teamMessage.theBehaviorStatus.walkingTo;
  agent.lastKnownSpeed = teamMessage.theBehaviorStatus.speed;
  agent.timestamp = teamMessage.theFrameInfo.time;
  agent.pose = teamMessage.theRobotPose;
  agent.ballPosition = teamMessage.theBallModel.estimate.position;
  agent.ballVelocity = teamMessage.theBallModel.estimate.velocity;
  agent.timeWhenBallLastSeen = teamMessage.theBallModel.timeWhenLastSeen;
  agent.timeWhenBallDisappeared = teamMessage.theBallModel.timeWhenDisappeared;
  // Calculate disagreeOnBall.
  {
    // In theory, all those calculations should be made at the time teamMessage.theFrameInfo.time and therefore, there should be a buffer of the last few own RobotPoses and BallModels to compare with.
    // The following is okay as long as the network delay is sufficiently short.
    // Disagreement can come from either delocalized robots (where both would walk to the same physical ball, but at least one of them would play in the wrong direction) or from wrong balls (in which case both must go to their respective ball).
    // Agents who disagree on the ball with me will not be considered in my decision to play the ball.
    // Therefore, it is worse if both players wrongly assume no disagreement (while in fact they would go to two different balls).

    // Also, it could be nice if this decision was made somewhere in the modeling stage because similar calculations could happen in the TeammatesBallModel.
    if(teamMessage.theFrameInfo.getTimeSince(teamMessage.theBallModel.timeWhenLastSeen) > 1000 ||
       theFrameInfo.getTimeSince(theBallModel.timeWhenLastSeen) > 1000)
    {
      // If one of us hasn't seen the ball recently, this is not disagreement.
      // One could instead check abs(teamMessage.theBallModel.timeWhenLastSeen - theBallModel.timeWhenLastSeen).
      // This is upper-bounded by 1000 here, but if both balls are old, it doesn't really matter anyway since the ball can have moved long ago and neither robot would become striker.
      agent.disagreeOnBall = false;
    }
    else
    {
      // Compare "now", i.e. propagate the teammate's ball to the current time.
      const Vector2f itsBallOnField = teamMessage.theRobotPose * BallPhysics::propagateBallPosition(teamMessage.theBallModel.estimate.position, teamMessage.theBallModel.estimate.velocity, static_cast<float>(theFrameInfo.getTimeSince(teamMessage.theFrameInfo.time)) / 1000.f, theBallSpecification.friction);
      const Vector2f myBallOnField = theRobotPose * theBallModel.estimate.position;
      agent.disagreeOnBall = (itsBallOnField - myBallOnField).squaredNorm() > sqr(777.f + (agent.disagreeOnBall ? 0.f : 222.f));
    }
  }
  agent.isUpright = teamMessage.theRobotStatus.isUpright;
  agent.timeWhenLastUpright = teamMessage.theRobotStatus.timeWhenLastUpright;
  agent.proposedTactic = teamMessage.theStrategyStatus.proposedTactic;
  agent.acceptedTactic = teamMessage.theStrategyStatus.acceptedTactic;
  agent.proposedMirror = teamMessage.theStrategyStatus.proposedMirror;
  agent.acceptedMirror = teamMessage.theStrategyStatus.acceptedMirror;
  agent.proposedSetPlay = teamMessage.theStrategyStatus.proposedSetPlay;
  agent.acceptedSetPlay = teamMessage.theStrategyStatus.acceptedSetPlay;
  agent.setPlayStep = -1;
  agent.position = teamMessage.theStrategyStatus.position;
  agent.role = teamMessage.theStrategyStatus.role;
}

void StrategyBehaviorControl::updateCurrentPosition(Agent& agent)
{
  agent.currentPosition = Teammate::getEstimatedPosition(agent.lastKnownPose, agent.lastKnownTarget, agent.lastKnownSpeed, theFrameInfo.getTimeSince(agent.lastKnownTimestamp));
}
