#pragma once

namespace game::navigation
{

// Shared conservative clearance used by the current diagnostic Hub
// infrastructure adapter. Client advisory planning and authoritative
// server Automatic planning must consume the same value; neither endpoint may
// invent its own obstacle-inflation policy.
inline constexpr double DiagnosticHubInfrastructureClearanceMeters = 80.0;

// Small geometric reserve used when an Automatic approach terminates outside
// a still-solid docking target. Physical latch/contact transfer is a separate
// game-state/physics layer and must not be approximated by allowing navigation
// to enter collision geometry.
inline constexpr double AutomaticDockingPreCaptureReserveMeters = 2.0;

} // namespace game::navigation
