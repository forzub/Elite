#pragma once

#include <string>

#include "NavigationScenarioRuntime.h"

namespace elite::tools::navigation_runtime
{

// Tool-input boundary. JSON/filesystem access ends here; planning and
// execution receive only the returned immutable ScenarioDefinition value.
[[nodiscard]] ScenarioDefinition loadScenarioDefinition(
    const std::string& scenarioJsonPath
);

} // namespace elite::tools::navigation_runtime
