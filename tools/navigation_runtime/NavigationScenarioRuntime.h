#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

#include "src/game/ship/core/ShipParams.h"

class ShipDescriptor;

#include "NavigationTrace.h"

namespace elite::tools::navigation_runtime
{

enum class ControlMode
{
    Assisted = 0,
    Newtonian
};

enum class PilotLevel
{
    Expert = 0,
    Average,
    Loser
};

enum class FlightStyle
{
    Standard = 0,
    Extreme
};

struct ScenarioRunSettings
{
    ControlMode controlMode = ControlMode::Newtonian;
    PilotLevel pilot = PilotLevel::Expert;
    FlightStyle flightStyle = FlightStyle::Standard;
    bool enableSuddenObstacle = false;

    // Negative means "use the authored scenario value". The runtime viewer
    // uses explicit overrides so start/finish speed can be swept without
    // rewriting scenario.json. Speed is a route-planning input because higher
    // inertia requires more geometric maneuver room.
    double startSpeedOverrideMps = -1.0;
    double finishSpeedOverrideMps = -1.0;
};

struct ScenarioVehicleParameters
{
    // One explicit immutable vehicle input crosses the scenario-runtime
    // boundary. Navigation runtime does not own or reconstruct ship data.
    ShipParams physics {};
    glm::dvec3 bodyHalfExtentsMeters {0.5};
    std::uint64_t capabilityRevision = 1;
};

[[nodiscard]] ScenarioVehicleParameters makeScenarioVehicleParameters(
    const ShipDescriptor& descriptor,
    std::uint64_t capabilityRevision = 1
);

struct ScenarioRunResult
{
    bool success = false;
    std::string message;
    TraceDocument trace;
    std::vector<std::string> diagnostics;

    // Authored defaults exposed to the viewer so its sliders initialize from
    // scenario data rather than hard-coded UI values.
    double authoredStartSpeedMps = 0.0;
    double authoredFinishSpeedMps = 0.0;
};

// Load only the authored input scene for pre-calculation visualization.
[[nodiscard]] ScenarioRunResult loadScenarioPreview(
    const std::string& scenarioJsonPath,
    const ScenarioVehicleParameters& vehicle
);

[[nodiscard]] ScenarioRunResult calculateScenario(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings,
    const ScenarioVehicleParameters& vehicle
);

[[nodiscard]] ScenarioRunResult executeCalculatedRoute(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings,
    const TraceDocument& calculatedRoute,
    const ScenarioVehicleParameters& vehicle
);

} // namespace elite::tools::navigation_runtime
