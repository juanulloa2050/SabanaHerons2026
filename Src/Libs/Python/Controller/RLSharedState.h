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
