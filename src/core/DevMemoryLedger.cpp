#include "src/core/DevMemoryLedger.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2 // K32GetProcessMemoryInfo from kernel32, no psapi.lib
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace core::dev
{
namespace
{
struct Entry
{
    std::size_t bytes = 0;
    std::size_t entries = 0;
};

std::mutex& ledgerMutex()
{
    static std::mutex m;
    return m;
}

std::map<std::string, Entry>& ledger()
{
    static std::map<std::string, Entry> l;
    return l;
}

double intervalSeconds()
{
    static const double s = [] {
        const char* v = std::getenv("ELITE_DEV_CELESTIAL_MEMORY");
        if (!v || !*v || *v == '0')
            return 0.0;
        const double x = std::atof(v);
        return x > 0.0 ? x : 10.0;
    }();
    return s;
}

void startThreadOnce()
{
    static std::once_flag once;
    std::call_once(once, [] {
        const double interval = intervalSeconds();
        // detached on purpose: DEV diagnostics until the process ends
        std::thread([interval] {
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::duration<double>(interval));
                memoryLedgerPrint();
            }
        }).detach();
    });
}
} // namespace

bool memoryLedgerEnabled()
{
    return intervalSeconds() > 0.0;
}

void memoryLedgerSet(const std::string& name, std::size_t bytes, std::size_t entries)
{
    if (!memoryLedgerEnabled())
        return;
    startThreadOnce();
    std::lock_guard<std::mutex> lock(ledgerMutex());
    ledger()[name] = {bytes, entries};
}

void memoryLedgerPrint()
{
    std::string out;
    char line[256];
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc {};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
    {
        std::snprintf(line, sizeof(line), "[CelestialMemory] process working set %.0f MB, private %.0f MB\n",
                      pmc.WorkingSetSize / 1048576.0, pmc.PrivateUsage / 1048576.0);
        out += line;
    }
#endif
    double cpu = 0.0, gpu = 0.0;
    {
        std::lock_guard<std::mutex> lock(ledgerMutex());
        for (const auto& [name, e] : ledger())
        {
            std::snprintf(line, sizeof(line), "[CelestialMemory]   %-34s %9.1f MB %6zu entries\n", name.c_str(),
                          e.bytes / 1048576.0, e.entries);
            out += line;
            (name.rfind("gpu ", 0) == 0 ? gpu : cpu) += e.bytes / 1048576.0;
        }
    }
    std::snprintf(line, sizeof(line), "[CelestialMemory]   total celestial cpu %.1f MB, gpu %.1f MB\n", cpu, gpu);
    out += line;
    std::fputs(out.c_str(), stderr);
    std::fflush(stderr);
}
} // namespace core::dev
