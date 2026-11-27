#include "src/core/ThreadPriority.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace core
{
void lowerCurrentThreadPriority()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void optOutOfPowerThrottling(const char* processName)
{
#ifdef _WIN32
    PROCESS_POWER_THROTTLING_STATE state {};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    state.StateMask = 0;   // controlled + not set = never throttled
    // resolved at run time: the toolchain's _WIN32_WINNT hides the declaration (Windows 8+ API)
    using SetProcessInformationFn = BOOL(WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    const auto setInfo = reinterpret_cast<SetProcessInformationFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation")));
    const BOOL ok = setInfo && setInfo(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));
    std::cerr << "[Power] " << processName << ": background power throttling "
              << (ok ? "disabled" : "NOT disabled (error " + std::to_string(GetLastError()) + ")") << std::endl;
#else
    (void)processName;
#endif
}
} // namespace core
