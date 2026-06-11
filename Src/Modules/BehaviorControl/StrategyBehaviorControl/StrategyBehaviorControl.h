/**
 * @file StrategyBehaviorControl.h
 *
 * This file declares a module that determines the strategy of the team.
 *
 * @author Arne Hasselbring
 */

#pragma once

#include "Behavior.h"
#include "Libs/RL/PPOActionDecoder.h"
#include "Libs/RL/PPOObservationEncoder.h"
#include "Libs/RL/PPOPolicyModel.h"
#include "Libs/RL/PPOSkillGate.h"
#include "Representations/BehaviorControl/ExpectedGoals.h"
#include "Representations/BehaviorControl/FieldBall.h"
#include "Representations/BehaviorControl/SkillRequest.h"
#include "Representations/BehaviorControl/StrategyStatus.h"
#include "Representations/Communication/ReceivedTeamMessages.h"
#include "Representations/Communication/SentTeamMessage.h"
#include "Representations/Communication/TeamData.h"
#include "Representations/Configuration/BallSpecification.h"
#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Configuration/SetupPoses.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Modeling/BallDropInModel.h"
#include "Representations/Modeling/BallModel.h"
#include "Representations/Modeling/ObstacleModel.h"
#include "Representations/Modeling/RobotPose.h"
#include "Representations/Modeling/TeammatesBallModel.h"
#include "Representations/MotionControl/MotionInfo.h"
#include "Representations/Perception/BallPercepts/BallPercept.h"
#include "Representations/Sensing/FallDownState.h"
#include "Representations/Sensing/GroundContactState.h"
#include "Tools/BehaviorControl/Strategy/Agent.h"
#include "Framework/Module.h"
#include <string>
#include <vector>

MODULE(StrategyBehaviorControl,
{,
  REQUIRES(BallDropInModel),
  REQUIRES(BallModel),
  REQUIRES(BallPercept),
  REQUIRES(BallSpecification),
  REQUIRES(ExpectedGoals),
  REQUIRES(ExtendedGameState),
  REQUIRES(FallDownState),
  REQUIRES(FieldBall),
  REQUIRES(FieldDimensions),
  REQUIRES(FrameInfo),
  REQUIRES(GameState),
  REQUIRES(GroundContactState),
  REQUIRES(MotionInfo),
  REQUIRES(ObstacleModel),
  REQUIRES(ReceivedTeamMessages),
  REQUIRES(RobotPose),
  REQUIRES(SentTeamMessage),
  REQUIRES(SetupPoses),
  REQUIRES(TeamData),
  REQUIRES(TeammatesBallModel),
  PROVIDES(SkillRequest),
  REQUIRES(SkillRequest),
  PROVIDES(StrategyStatus),
  LOADS_PARAMETERS(
  {,
    (Strategy::Type) strategy, /**< The strategy to play. */
    (bool)(false) enableEmbeddedPPO, /**< Enable the embedded PPO policy override. */
    (std::string)("Config/NeuralNets/RLPolicy/ppo_striker_hsl2026.onnx") embeddedPPOModelPath, /**< PPO model file, relative to the repo root unless absolute. */
    (int)(-1) embeddedPPOTeamNumber, /**< Team filter for PPO. -1 means own team. */
    (bool)(true) embeddedPPODynamicPlayBall, /**< If true, PPO follows the dynamically assigned playBall robot. */
    (std::vector<int>) embeddedPPOPlayers, /**< If non-empty, only these player numbers use PPO. */
    (int)(0) embeddedPPOStandWatchdogMs, /**< Disable PPO or force walk if PPO stand dominates this time window. 0 disables. */
    (int)(3000) embeddedPPOStandWatchdogCooldownMs, /**< Time to keep PPO disabled after the stand watchdog fires. */
    (bool)(false) embeddedPPOStandWatchdogForceWalk, /**< If true, force walk instead of falling back to B-Human when the watchdog fires. */
  }),
});

class StrategyBehaviorControl : public StrategyBehaviorControlBase
{
public:
  /** Constructor. */
  StrategyBehaviorControl();

  /**
   * Creates extended module info (union of this module's info and requirements of other behavior parts).
   * @return The extended module info.
   */
  static std::vector<ModuleBase::Info> getExtModuleInfo();

private:
  enum class RLRuntimeMode
  {
    bhuman,
    externalOverride,
    embeddedWaiting,
    embeddedFallback,
    embeddedActive,
  };

  /**
   * Updates the skill request.
   * @param skillRequest The provided skill request.
   */
  void update(SkillRequest& skillRequest) override;

  /**
   * Updates the strategy status.
   * @param strategyStatus The provided strategy status.
   */
  void update(StrategyStatus& strategyStatus) override;

  /**
   * Updates the list of agents to represent the most recent data.
   * @return A pointer to the agent that represents this player.
   */
  Agent* updateAgents();

  /**
   * Updates an agent using local representations.
   * @param agent The agent to update.
   */
  void updateAgentBySelf(Agent& agent);

  /**
   * Updates an agent using a team message.
   * @param agent The agent to update.
   * @param teamMessage The team message to incorporate.
   */
  void updateAgentByTeamMessage(Agent& agent, const ReceivedTeamMessage& teamMessage);

  /**
   * Updates the estimated position of the agent.
   * @param agent The agent to update.
   */
  void updateCurrentPosition(Agent& agent);

  bool usesEmbeddedPPO(const GameState& gameState) const;
  std::string embeddedPPOStatusReason(const GameState& gameState) const;
  std::string configuredEmbeddedPPOModelPath() const;
  bool updateEmbeddedPPO(SkillRequest& skillRequest);
  bool ensureEmbeddedPPOLoaded();
  void logRLModeIfChanged(RLRuntimeMode mode, const std::string& reason);
  void logEmbeddedPPODecisionIfChanged(
    int skillIndex,
    const RL::PPOGateDecision& gateDecision,
    const RL::PPOGateObservation& rawObservation,
    const std::array<float, RL::ppoSkillCount>& maskedLogits,
    const std::array<float, RL::ppoParamCount>& paramMean,
    const SkillRequest& skillRequest);
  void resetEmbeddedPPO();

  Behavior theBehavior; /**< The instance of the behavior. */
  std::vector<Agent> agents; /**< The list of active agents. */
  StrategyStatus theStrategyStatus; /**< The strategy status which is provided later. */
  RL::PPOSkillGate ppoSkillGate;
  RL::PPOObservationEncoder ppoObservationEncoder;
  RL::PPOPolicyModel ppoPolicyModel;
  RL::PPOActionDecoder ppoActionDecoder;
  bool ppoLoadAttempted = false;
  bool ppoLoadErrorReported = false;
  bool ppoInferErrorReported = false;
  std::string ppoRequestedModelPath;
  unsigned ppoStandWatchdogWindowStarted = 0;
  int ppoStandWatchdogStandFrames = 0;
  int ppoStandWatchdogTotalFrames = 0;
  unsigned ppoStandWatchdogCooldownStarted = 0;
  bool ppoStandWatchdogCooldownActive = false;
  bool hasLoggedRLRuntimeMode = false;
  RLRuntimeMode lastLoggedRLRuntimeMode = RLRuntimeMode::bhuman;
  std::string lastLoggedRLRuntimeReason;
  int lastLoggedEmbeddedPPOSkillIndex = -1;
  bool lastLoggedEmbeddedPPOShootArmed = false;
  bool lastLoggedEmbeddedPPODribbleArmed = false;
  unsigned lastLoggedEmbeddedPPOTimestamp = 0;
};
