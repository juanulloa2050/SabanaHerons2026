#pragma once
#include "Streaming/AutoStreamable.h" // ruta según tu árbol

STREAMABLE(ResourceStats,
{,
  (float) cpuUsageProcess,
  (float) cpuUsageSystem,
  (unsigned int) ramUsedProcess,
  (unsigned int) ramTotalSystem,
  (unsigned int) ramUsedSystem,
  (unsigned) timestamp,
});