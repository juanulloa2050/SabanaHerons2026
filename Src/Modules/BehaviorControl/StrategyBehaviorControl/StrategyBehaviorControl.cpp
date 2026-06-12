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
              theGameState, theTeammatesBallModel)
{}

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
    return role == EmbeddedPPORole::defender ? "no defender PPO model configured" : "no striker PPO model configured";

  if(gameState.playerState != GameState::active)
    return "player not active";

  if(gameState.state != GameState::playing)
    return "game not playing";

  if(gameState.ownTeam.isGoalkeeper(playerNumber))
    return "goalkeeper excluded";

  return role == EmbeddedPPORole::defender ? "ready defender" : "ready striker";
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
  RL::PPOPolicyModel& model = role == EmbeddedPPORole::defender ? defenderPPOPolicyModel : strikerPPOPolicyModel;
  bool& loadAttempted = role == EmbeddedPPORole::defender ? defenderPPOLoadAttempted : strikerPPOLoadAttempted;
  bool& loadErrorReported = role == EmbeddedPPORole::defender ? defenderPPOLoadErrorReported : strikerPPOLoadErrorReported;
  std::string& requestedModelPath = role == EmbeddedPPORole::defender ? defenderPPORequestedModelPath : strikerPPORequestedModelPath;
  const char* roleName = role == EmbeddedPPORole::defender ? "defender" : "striker";

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
  ppoObservationEncoder.reset();
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
