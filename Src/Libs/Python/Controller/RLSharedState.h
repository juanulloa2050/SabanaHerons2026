/**
 * @file RLSharedState.h
 *
 * Process-wide shared state between the Python RL loop and BHuman modules.
 * This state is backed by POSIX shared memory so it is shared across
 * independently loaded shared objects such as controller.so and libSimulatedNao.so.
 */

#pragma once

#include "RLSim2D.h"

#include <pthread.h>
#include <semaphore.h>

#include <array>
#include <cstddef>
#include <string>

namespace RLSharedStateBridge
{
  constexpr int teamNumber = 24;

  inline bool isEnabledForTeam(const int currentTeamNumber)
  {
    return currentTeamNumber == teamNumber;
  }
}

struct RLPlayerIO
{
  pthread_mutex_t mutex;

  char skill[16];
  float targetX = 0.f;
  float targetY = 0.f;
  float targetTheta = 0.f;
  int passTarget = -1;

  float ballX = 0.f;
  float ballY = 0.f;
  float robotX = 0.f;
  float robotY = 0.f;
  float robotTheta = 0.f;
  unsigned int frame = 0;
  bool obsReady = false;

  float ballRelX = 0.f;
  float ballRelY = 0.f;
  float ballEndRelX = 0.f;
  float ballEndRelY = 0.f;
  float ballVelX = 0.f;
  float ballVelY = 0.f;
  float timeSinceBallSeen = 0.f;
  float timeSinceBallDisappeared = 0.f;
  float ballSeenPercentage = 0.f;
  bool ballConsistentWithGameState = false;
  bool canScoreNow = false;
  float shotQualityNoObstacles = 0.f;
  float shotOpeningWithObstacles = 0.f;
  float passOptionsCount = 0.f;
  float nearestTeammateDist = 0.f;
  float nearestOpponentDist = 0.f;
  float nearestUncertainObstacleDist = 0.f;
  float nearestTeammateFrontDist = 0.f;
  float nearestOpponentFrontDist = 0.f;
  float nearestUncertainFrontDist = 0.f;
  int debugObstacleCount = 0;
  int debugObstacleTeammateCount = 0;
  int debugObstacleOpponentCount = 0;
  int debugObstacleUncertainCount = 0;
  int debugObstacleFrontCount = 0;
  float debugNearestObstacleDist = 0.f;
  float debugNearestObstacleAngle = 0.f;
  float debugNearestFrontObstacleDist = 0.f;
  float debugNearestOpponentAngle = 0.f;
  int debugObstacleFieldPerceptCount = 0;
  int debugObstacleFieldPerceptOpponentCount = 0;
  int debugObstacleFieldPerceptTeammateCount = 0;
  int debugObstacleFieldPerceptUnknownCount = 0;
  int debugObstacleAcceptedPerceptCount = 0;
  int debugObstacleHypothesisCount = 0;
  int debugObstaclePublishedCount = 0;
  int debugObstacleMaxSeenCount = 0;
  int debugObstacleMinPercepts = 0;
  int debugObstacleCamera = -1;
  unsigned int debugObstacleFrame = 0;
  bool debugObstacleArmContact = false;
  bool debugObstacleFootContact = false;

  float resetBallX = 0.f;
  float resetBallY = 0.f;
  float resetRobotX = 0.f;
  float resetRobotY = 0.f;
  float resetRobotTheta = 0.f;
  unsigned int resetRequestId = 0;
  unsigned int resetAppliedId = 0;
  bool resetPending = false;

  int debugMotionRequest = -1;
  int debugProviderMotionRequest = -1;
  int debugProviderCallCount = 0;
  float debugProviderTargetX = 0.f;
  float debugProviderTargetY = 0.f;
  float debugProviderTargetTheta = 0.f;
  int debugSkillBehaviorSkillRequest = -1;
  int debugSkillBehaviorMotionRequest = -1;
  int debugSkillBehaviorCallCount = 0;
  float debugSkillBehaviorWalkTargetX = 0.f;
  float debugSkillBehaviorWalkTargetY = 0.f;
  float debugSkillBehaviorWalkTargetTheta = 0.f;
  float debugMotionObstacleAvoidanceX = 0.f;
  float debugMotionObstacleAvoidanceY = 0.f;
  int debugMotionObstaclePathCount = 0;
  float debugMotionObstacleFirstX = 0.f;
  float debugMotionObstacleFirstY = 0.f;
  float debugMotionObstacleFirstRadius = 0.f;
  bool debugMotionObstacleFirstClockwise = false;
  int debugMotionEngineInputRequest = -1;
  int debugMotionEngineEffectiveRequest = -1;
  int debugMotionEnginePhase = -1;
  bool debugMotionEngineForceSitDown = false;
  bool debugMotionEngineGyroOffsetFinished = false;
  bool debugMotionEngineGyroBad = false;
  float debugMotionEngineInertialAngleX = 0.f;
  float debugMotionEngineInertialAngleY = 0.f;
  int debugMotionEngineFallState = -1;
  int debugWalkToPoseCallCount = 0;
  float debugWalkToPoseTargetX = 0.f;
  float debugWalkToPoseTargetY = 0.f;
  float debugWalkToPoseTargetTheta = 0.f;
  float debugWalkToPoseStepX = 0.f;
  float debugWalkToPoseStepY = 0.f;
  float debugWalkToPoseStepTheta = 0.f;
  float debugWalkToPoseAvoidanceX = 0.f;
  float debugWalkToPoseAvoidanceY = 0.f;
  int debugWalkToPosePathCount = 0;
  float debugWalkToPoseFirstObstacleX = 0.f;
  float debugWalkToPoseFirstObstacleY = 0.f;
  float debugWalkToPoseFirstObstacleRadius = 0.f;
  bool debugWalkToPoseFirstObstacleClockwise = false;
  bool debugMotionEngineGroundContact = false;
  int debugExecutedPhase = -1;
  float debugMotionSpeedX = 0.f;
  float debugMotionSpeedY = 0.f;
  float debugMotionSpeedRot = 0.f;
  bool motionJointRequestValid = false;
  std::array<float, 32> motionJointAngles{};
  float debugLHipPitch = 0.f;
  float debugLKneePitch = 0.f;
  float debugRHipPitch = 0.f;
  float debugRKneePitch = 0.f;

  RLSim2DState sim2D;

  sem_t obsSignal;

  void lock();
  void unlock();
  bool waitForObs(unsigned timeoutMs);
  bool tryWaitObs();
  void postObs();

  std::string getSkill() const;
  void setSkill(const std::string& value);
};

class RLSharedState
{
public:
  static constexpr int MAX_PLAYERS = 6;

  static RLSharedState& instance();

  RLPlayerIO& player(int number);

private:
  struct SharedBlock
  {
    unsigned int magic = 0;
    unsigned int version = 0;
    std::array<RLPlayerIO, MAX_PLAYERS> players;
  };

  RLSharedState();
  ~RLSharedState();

  RLSharedState(const RLSharedState&) = delete;
  RLSharedState& operator=(const RLSharedState&) = delete;

  void initializeBlock();

  int fd = -1;
  SharedBlock* block = nullptr;
};
