#pragma once
#include "Representations/Infrastructure/ResourceStats.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Framework/Module.h"
#include <deque>       // 👈 Necesario para guardar el historial
#include <numeric>     // 👈 Necesario para std::accumulate

MODULE(ResourceMonitor,
{,
  REQUIRES(FrameInfo),
  PROVIDES(ResourceStats),
  DEFINES_PARAMETERS(
  {,
    (int)(5000) sampleIntervalMs, // cada cuánto imprimir / plotear (ms)
  }),
});

class ResourceMonitor : public ResourceMonitorBase
{
public:
  ResourceMonitor();
  void update(ResourceStats& resourceStats) override;

private:
  unsigned lastUpdateTimeMs = 0;
  unsigned lastPrintedTimeMs = 0;

  // snapshots for CPU calculation
  uint64_t prevTotalJiffies = 0;
  uint64_t prevIdleJiffies  = 0;
  double   prevProcCpuSec   = 0.0;

  // 👇 NUEVO: Buffers de historial para suavizar las métricas
  std::deque<float> cpuSystemHistory;
  std::deque<float> cpuProcHistory;
  std::deque<uint64_t> ramUsedHistory;

  static constexpr size_t maxSamples = 30; // número de muestras para promedio (ajustable)
};
