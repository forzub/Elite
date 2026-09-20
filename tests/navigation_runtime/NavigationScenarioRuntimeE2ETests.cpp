#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "tools/navigation_runtime/NavigationScenarioRuntime.h"

namespace
{

using namespace elite::tools::navigation_runtime;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool hasDiagnostic(
    const ScenarioRunResult& result,
    const std::string& prefix
)
{
    for (const auto& line : result.diagnostics)
    {
        if (line.rfind(prefix, 0) == 0)
            return true;
    }
    return false;
}

void printDiagnostics(
    const char* prefix,
    const ScenarioRunResult& result
)
{
    for (const auto& line : result.diagnostics)
        std::cout << prefix << line << '\n';
}

void testDefaultScenarioRunsPlannerRouteThroughFollowerAndPhysics()
{
#ifdef ELITE_SOURCE_ROOT
    const std::string scenario =
        std::string(ELITE_SOURCE_ROOT) +
        "/tools/navigation_runtime/scenario.json";
#else
    const std::string scenario =
        "tools/navigation_runtime/scenario.json";
#endif

    ScenarioRunSettings settings;
    settings.controlMode = ControlMode::Newtonian;
    settings.pilot = PilotLevel::Expert;
    settings.flightStyle = FlightStyle::Standard;
    settings.enableSuddenObstacle = false;

    const auto planned =
        calculateScenario(scenario, settings);

    printDiagnostics("[E2E-STAGE1] ", planned);

    require(planned.success, "default Stage-1 route calculation failed");
    require(
        planned.trace.routePoints.size() >= 2,
        "default Stage-1 route contains fewer than two points"
    );

    const auto retainedRoute = planned.trace.routePoints;

    const auto executed =
        executeCalculatedRoute(
            scenario,
            settings,
            planned.trace
        );

    printDiagnostics("[E2E-STAGE2] ", executed);

    require(
        executed.trace.routePoints.size() == retainedRoute.size(),
        "Stage 2 changed retained-route point count"
    );
    for (std::size_t i = 0; i < retainedRoute.size(); ++i)
    {
        require(
            glm::length(
                executed.trace.routePoints[i] -
                retainedRoute[i]
            ) <= 1.0e-9,
            "Stage 2 mutated a retained Stage-1 route point"
        );
    }
    require(
        hasDiagnostic(executed, "TRAJECTORY: RUCKIG OK"),
        "Stage 2 did not produce a Ruckig trajectory"
    );
    require(
        hasDiagnostic(executed, "PROGRAM PHASES: "),
        "Stage 2 did not publish route-leg execution phases"
    );
    require(
        executed.trace.frames.size() > 100,
        "Stage 2 stopped before sustained Follower execution"
    );
    require(
        executed.success,
        "default retained route did not complete through Follower/Pilot/physics"
    );
    require(
        !executed.trace.frames.empty() &&
        executed.trace.frames.back().phase == "execution_complete",
        "default execution trace did not end in execution_complete"
    );

    const double finalError =
        glm::length(
            executed.trace.frames.back().shipPosition -
            executed.trace.sceneFinishMapMeters
        );
    require(
        finalError <= 5.0,
        "default execution finished outside terminal position tolerance"
    );
}

} // namespace

int main()
{
    try
    {
        testDefaultScenarioRunsPlannerRouteThroughFollowerAndPhysics();
        std::cout
            << "NAVIGATION RETAINED-ROUTE E2E: PASS\n"
            << " - one Stage-1 route is retained unchanged\n"
            << " - Ruckig parameterizes that retained route\n"
            << " - route-leg programs cross Follower -> PilotSkill -> physics\n"
            << " - Cobra reaches the authored finish\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "NAVIGATION RETAINED-ROUTE E2E: FAIL: "
            << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
