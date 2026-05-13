#include "RLSharedState.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace
{
constexpr const char* shmName = "/pybh_rl_shared_state_v3";
constexpr unsigned int magic = 0x50594248u; // PYBH
constexpr unsigned int version = 22u;
}

void RLPlayerIO::lock()
{
  const int result = pthread_mutex_lock(&mutex);
  if(result == EOWNERDEAD)
    pthread_mutex_consistent(&mutex);
  else if(result != 0)
    throw std::runtime_error("pthread_mutex_lock failed for RLPlayerIO");
}

void RLPlayerIO::unlock()
{
  pthread_mutex_unlock(&mutex);
}

bool RLPlayerIO::waitForObs(unsigned timeoutMs)
{
  if(timeoutMs == 0)
    return sem_wait(&obsSignal) == 0;

  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += static_cast<time_t>(timeoutMs / 1000u);
  ts.tv_nsec += static_cast<long>(timeoutMs % 1000u) * 1000000l;
  if(ts.tv_nsec >= 1000000000l)
  {
    ++ts.tv_sec;
    ts.tv_nsec -= 1000000000l;
  }

  while(sem_timedwait(&obsSignal, &ts) != 0)
  {
    if(errno == EINTR)
      continue;
    return false;
  }
  return true;
}

bool RLPlayerIO::tryWaitObs()
{
  return sem_trywait(&obsSignal) == 0;
}

void RLPlayerIO::postObs()
{
  sem_post(&obsSignal);
}

std::string RLPlayerIO::getSkill() const
{
  return skill;
}

void RLPlayerIO::setSkill(const std::string& value)
{
  std::strncpy(skill, value.c_str(), sizeof(skill) - 1);
  skill[sizeof(skill) - 1] = '\0';
}

RLSharedState& RLSharedState::instance()
{
  static RLSharedState state;
  return state;
}

RLSharedState::RLSharedState()
{
  fd = shm_open(shmName, O_CREAT | O_RDWR, 0600);
  if(fd < 0)
    throw std::runtime_error("shm_open failed for RLSharedState");

  if(ftruncate(fd, static_cast<off_t>(sizeof(SharedBlock))) != 0)
    throw std::runtime_error("ftruncate failed for RLSharedState");

  void* mapping = mmap(nullptr, sizeof(SharedBlock), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(mapping == MAP_FAILED)
    throw std::runtime_error("mmap failed for RLSharedState");

  block = static_cast<SharedBlock*>(mapping);
  if(block->magic != magic || block->version != version)
    initializeBlock();
}

RLSharedState::~RLSharedState()
{
  if(block)
    munmap(block, sizeof(SharedBlock));
  if(fd >= 0)
    close(fd);
}

void RLSharedState::initializeBlock()
{
  std::memset(block, 0, sizeof(SharedBlock));
  block->magic = magic;
  block->version = version;

  pthread_mutexattr_t mutexAttr;
  pthread_mutexattr_init(&mutexAttr);
  pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_setrobust(&mutexAttr, PTHREAD_MUTEX_ROBUST);

  for(RLPlayerIO& player : block->players)
  {
    pthread_mutex_init(&player.mutex, &mutexAttr);
    sem_init(&player.obsSignal, 1, 0);
    player.setSkill("stand");
  }

  pthread_mutexattr_destroy(&mutexAttr);
}

RLPlayerIO& RLSharedState::player(int number)
{
  const int idx = (number >= 1 && number <= MAX_PLAYERS) ? number - 1 : 0;
  return block->players[static_cast<std::size_t>(idx)];
}
