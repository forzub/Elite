#pragma once

namespace core
{
// Background work (terrain bakes, route planning) must never take CPU time
// from the frame: such threads run below normal priority. No-op off Windows.
void lowerCurrentThreadPriority();
} // namespace core
