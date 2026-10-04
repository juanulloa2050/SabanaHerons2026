/* SabanaHerons fork extension (B-Human 2023 base).
 * Represent process/system resource measurements emitted by ResourceMonitor.
 * Release overview and commit references: README.md.
 */

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