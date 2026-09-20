#pragma once

#include <string>
#include <vector>

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
};

struct ScenarioRunResult
{
    bool success = false;
    std::string message;
    TraceDocument trace;
    std::vector<std::string> diagnostics;
};

// Load only the authored input scene for pre-calculation visualization.
[[nodiscard]] ScenarioRunResult loadScenarioPreview(
    const std::string& scenarioJsonPath
);

[[nodiscard]] ScenarioRunResult calculateScenario(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings
);

[[nodiscard]] ScenarioRunResult executeCalculatedRoute(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings,
    const TraceDocument& calculatedRoute
);

} // namespace elite::tools::navigation_runtime
