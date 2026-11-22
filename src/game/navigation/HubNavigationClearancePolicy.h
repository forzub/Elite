#pragma once

namespace game::navigation
{

// Shared conservative clearance used by the current diagnostic Hub
// infrastructure adapter. Client advisory planning and authoritative
// server Automatic planning must consume the same value; neither endpoint may
// invent its own obstacle-inflation policy.
inline constexpr double DiagnosticHubInfrastructureClearanceMeters = 80.0;
// the buildings of a generated city (live 2026-11-21: 80 m walled its 26-40 m streets)
// 5 (20 tried 2026-11-22: planning 80 s again and still a crash under the city): free legs among the buildings are the
// risk - the pad routes are to end on a lane near the pad (no free leg through the city), the pad service the rest.
inline constexpr double CityBuildingClearanceMeters = 5.0;

// Small geometric reserve used when an Automatic approach terminates outside
// a still-solid docking target. Physical latch/contact transfer is a separate
// game-state/physics layer and must not be approximated by allowing navigation
// to enter collision geometry.
inline constexpr double AutomaticDockingPreCaptureReserveMeters = 2.0;

} // namespace game::navigation
