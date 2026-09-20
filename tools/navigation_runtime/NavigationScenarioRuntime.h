#pragma once

#include <string>

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
};

[[nodiscard]] ScenarioRunResult calculateScenario(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings
);

} // namespace elite::tools::navigation_runtime
