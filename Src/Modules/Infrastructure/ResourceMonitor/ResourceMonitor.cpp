#include "ResourceMonitor.h"
#include "Debugging/Plot.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <iostream>
#include <vector>
#include <iomanip>

MAKE_MODULE(ResourceMonitor);

// Helper: read /proc/stat first "cpu " line -> total and idle (idle + iowait)
static bool readProcStat(uint64_t &total, uint64_t &idleAll)
{
  std::ifstream f("/proc/stat");
  if(!f.good()) return false;
  std::string line;
  std::getline(f, line);
  std::istringstream ss(line);
  std::string cpu;
  ss >> cpu; // "cpu"
  uint64_t user=0, nice=0, system=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
  ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
  idleAll = idle + iowait;
  total = user + nice + system + idle + iowait + irq + softirq + steal;
  return true;
}

// Helper: get process cpu time in seconds (user+sys)
static double getProcCpuSeconds()
{
  struct rusage ru;
  if(getrusage(RUSAGE_SELF, &ru) != 0) return 0.0;
  double sec = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6
             + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec * 1e-6;
  return sec;
}

// Helper: process RSS from /proc/self/status (VmRSS: kB)
static uint64_t getProcRssBytes()
{
  std::ifstream f("/proc/self/status");
  if(!f.good()) return 0;
  std::string line;
  while(std::getline(f, line))
  {
    if(line.rfind("VmRSS:", 0) == 0)
    {
      std::istringstream ss(line);
      std::string key;
      uint64_t kb;
      ss >> key >> kb;
      return kb * 1024ULL;
    }
  }
  return 0;
}

ResourceMonitor::ResourceMonitor()
{
  // initialize snapshots so first update will set baseline
  prevTotalJiffies = 0;
  prevIdleJiffies = 0;
  prevProcCpuSec = getProcCpuSeconds();
}

void ResourceMonitor::update(ResourceStats& resourceStats)
{
  // run on every frame but do heavy calc only after parameter interval
  const unsigned nowMs = theFrameInfo.time;
  if(lastUpdateTimeMs == 0) lastUpdateTimeMs = nowMs;

  const unsigned elapsedSinceLastPrint = nowMs - lastPrintedTimeMs;
  // read system /proc/stat
  uint64_t totalJ, idleJ;
  if(!readProcStat(totalJ, idleJ))
    return; // can't read -> leave values 0

  // system CPU usage calculation
  float cpuSystemPct = 0.f;
  if(prevTotalJiffies != 0)
  {
    const uint64_t deltaTotal = totalJ - prevTotalJiffies;
    const uint64_t deltaIdle  = idleJ - prevIdleJiffies;
    if(deltaTotal > 0)
      cpuSystemPct = static_cast<float>(100.0 * (1.0 - (double)deltaIdle / (double)deltaTotal));
  }

  // process CPU usage: use getrusage to get proc cpu seconds
  double procCpuSec = getProcCpuSeconds();
  float cpuProcPct = 0.f;
  const unsigned elapsedMs = nowMs - lastUpdateTimeMs;
  const double elapsedSec = (elapsedMs > 0) ? (elapsedMs / 1000.0) : 0.001; // avoid div0
  if(prevProcCpuSec != 0.0)
  {
    const double deltaProc = procCpuSec - prevProcCpuSec;
    cpuProcPct = static_cast<float>(100.0 * (deltaProc / elapsedSec));
  }

  // RAM: system via sysinfo, process via /proc/self/status
  struct sysinfo si;
  sysinfo(&si);
  uint64_t totalRamBytes = static_cast<uint64_t>(si.totalram) * si.mem_unit;
  uint64_t usedRamBytes = static_cast<uint64_t>(si.totalram - si.freeram - si.bufferram) * si.mem_unit;
  uint64_t procRssBytes = getProcRssBytes();

  // ===========================
  // PROMEDIADO MÓVIL
  // ===========================
  cpuSystemHistory.push_back(cpuSystemPct);
  if(cpuSystemHistory.size() > maxSamples)
      cpuSystemHistory.pop_front();

  cpuProcHistory.push_back(cpuProcPct);
  if(cpuProcHistory.size() > maxSamples)
      cpuProcHistory.pop_front();

  ramUsedHistory.push_back(usedRamBytes);
  if(ramUsedHistory.size() > maxSamples)
      ramUsedHistory.pop_front();

  // calcular promedio de cada métrica
  float avgCpuSystem = std::accumulate(cpuSystemHistory.begin(), cpuSystemHistory.end(), 0.0f) / cpuSystemHistory.size();
  float avgCpuProc   = std::accumulate(cpuProcHistory.begin(), cpuProcHistory.end(), 0.0f) / cpuProcHistory.size();
  uint64_t avgRamUsed = std::accumulate(ramUsedHistory.begin(), ramUsedHistory.end(), 0ULL) / ramUsedHistory.size();

  // ===========================
  // Rellenar representation con valores suavizados
  // ===========================
  resourceStats.cpuUsageProcess = avgCpuProc;
  resourceStats.cpuUsageSystem  = avgCpuSystem;
  resourceStats.ramUsedProcess  = procRssBytes;
  resourceStats.ramTotalSystem  = totalRamBytes;
  resourceStats.ramUsedSystem   = avgRamUsed;
  resourceStats.timestamp       = nowMs;

  // plotting para SimRobot
  PLOT("ResourceCPUSystem", resourceStats.cpuUsageSystem);
  PLOT("ResourceCPUProcess", resourceStats.cpuUsageProcess);
  PLOT("ResourceRamUsedMB", static_cast<float>(resourceStats.ramUsedSystem) / (1024.0f * 1024.0f));

  // imprimir cada sampleIntervalMs
  if(elapsedSinceLastPrint >= (unsigned)sampleIntervalMs)
  {
    lastPrintedTimeMs = nowMs;
    std::ostringstream out;
    out << std::fixed << std::setprecision(1)
        << "[ResourceMonitor] t=" << nowMs << "ms, sysCPU=" << resourceStats.cpuUsageSystem << "%, "
        << "procCPU=" << resourceStats.cpuUsageProcess << "%, "
        << "sysRAM=" << (resourceStats.ramUsedSystem / (1024.0*1024.0)) << "MB/"
        << (resourceStats.ramTotalSystem / (1024.0*1024.0)) << "MB, procRSS="
        << (resourceStats.ramUsedProcess / 1024.0) << "KB";
    std::cout << out.str() << std::endl;
  }

  // update snapshots
  prevTotalJiffies = totalJ;
  prevIdleJiffies  = idleJ;
  prevProcCpuSec   = procCpuSec;
  lastUpdateTimeMs = nowMs;
}
