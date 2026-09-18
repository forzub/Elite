#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace game::diagnostics
{

// Stage-12 live NavigationWorld proving actor.
// Enabled only in the existing diagnostic hub scene.
inline constexpr bool NavigationRuntimeLabEnabled = true;
inline constexpr std::uint64_t NavigationRuntimeLabInstanceId = 9030;
inline constexpr const char* NavigationRuntimeLabLabel =
    "NAVIGATION V2 RUNTIME LAB";
inline constexpr const char* NavigationRuntimeLabHubId =
    "earth_orbital_hub";

// Hub ReferenceFrame uses tactical local axes:
//   X = prograde, Y = radial, Z = normal.
//
// The stress objects are authored in visual hub axes:
//   X = normal, Y = radial, Z = -prograde.
//
// This start/goal pair therefore corresponds to visual:
//   start = { 975, -1300, -8000 }
//   goal  = { 975, -1300,  1000 }
//
// The straight line crosses NAV STRESS CUBE 08 at
// visual { 975, -1300, -4900 }, forcing the live local planner to react.
inline const glm::dvec3 NavigationRuntimeLabStartTacticalLocalMeters {
    8000.0,
    -1300.0,
    975.0
};

inline const glm::dvec3 NavigationRuntimeLabGoalTacticalLocalMeters {
    -1000.0,
    -1300.0,
    975.0
};

inline constexpr double NavigationRuntimeLabMaximumSpeedMps = 60.0;
inline constexpr double NavigationRuntimeLabArrivalRadiusMeters = 20.0;
inline constexpr double NavigationRuntimeLabWorkspaceHalfExtentMeters = 12000.0;

} // namespace game::diagnostics
