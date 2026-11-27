#pragma once

namespace core
{
// Background work (terrain bakes, route planning) must never take CPU time
// from the frame: such threads run below normal priority. No-op off Windows.
void lowerCurrentThreadPriority();

// Windows 11 power throttling (EcoQoS) slows a process whose window is not in the foreground: efficiency cores,
// lower clocks and coarse timers. Found live 2026-11-27: with another window focused the client's fixed simulation
// ran 2x slower and froze for 0.3-1.5 s (frame-gap hitches only while foreground_pid != the game). The game, its
// embedded server and the dedicated server must keep real time when unfocused: opt the process out. Logs the
// result once. No-op off Windows.
void optOutOfPowerThrottling(const char* processName);
} // namespace core
