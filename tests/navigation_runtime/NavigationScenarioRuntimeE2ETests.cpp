#include <algorithm>
#include <cmath>
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

double maximumInteriorAbsY(
    const TraceDocument& trace
)
{
    double out = 0.0;
    if (trace.routePoints.size() <= 2)
        return out;

    for (std::size_t i = 1; i + 1 < trace.routePoints.size(); ++i)
        out = std::max(out, std::abs(trace.routePoints[i].y));
    return out;
}

void testSpeedAndStyleChangeStaticManeuverReserve()
{
#ifdef ELITE_SOURCE_ROOT
    const std::string scenario =
        std::string(ELITE_SOURCE_ROOT) +
        "/tools/navigation_runtime/scenario.json";
#else
    const std::string scenario =
        "tools/navigation_runtime/scenario.json";
#endif

    ScenarioRunSettings lowStandard;
    lowStandard.controlMode = ControlMode::Newtonian;
    lowStandard.pilot = PilotLevel::Expert;
    lowStandard.flightStyle = FlightStyle::Standard;
    lowStandard.startSpeedOverrideMps = 10.0;
    lowStandard.finishSpeedOverrideMps = 10.0;

    ScenarioRunSettings highStandard = lowStandard;
    highStandard.startSpeedOverrideMps = 40.0;
    highStandard.finishSpeedOverrideMps = 40.0;

    ScenarioRunSettings highExtreme = highStandard;
    highExtreme.flightStyle = FlightStyle::Extreme;

    const auto low = calculateScenario(scenario, lowStandard);
    const auto fast = calculateScenario(scenario, highStandard);
    const auto extreme = calculateScenario(scenario, highExtreme);

    require(low.success && fast.success && extreme.success,
            "speed/style route-reserve fixture failed to plan");

    const double lowOffset = maximumInteriorAbsY(low.trace);
    const double fastOffset = maximumInteriorAbsY(fast.trace);
    const double extremeOffset = maximumInteriorAbsY(extreme.trace);

    require(
        fastOffset > lowOffset + 1.0,
        "higher speed did not move static detour farther from the wall"
    );
    require(
        extremeOffset + 1.0 < fastOffset,
        "EXTREME did not cut closer than STANDARD at the same speed"
    );
    require(
        hasDiagnostic(fast, "ROUTE PLANNING SPEED: 40.00 M/S"),
        "Stage-1 diagnostics lost explicit route-planning speed"
    );
    require(
        hasDiagnostic(fast, "STYLE CLEARANCE: STANDARD"),
        "STANDARD route-clearance doctrine missing from diagnostics"
    );
    require(
        hasDiagnostic(extreme, "STYLE CLEARANCE: EXTREME"),
        "EXTREME route-clearance doctrine missing from diagnostics"
    );
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

    const double finalSpeed =
        glm::length(executed.trace.frames.back().shipVelocity);
    require(
        std::abs(finalSpeed - 10.0) <= 1.5,
        "default execution did not cross finish at the authored 10 m/s"
    );
}

} // namespace

int main()
{
    try
    {
        testSpeedAndStyleChangeStaticManeuverReserve();
        testDefaultScenarioRunsPlannerRouteThroughFollowerAndPhysics();
        std::cout
            << "NAVIGATION RETAINED-ROUTE E2E: PASS\n"
            << " - speed changes Stage-1 maneuver clearance and route points\n"
            << " - STANDARD/EXTREME change clearance, not nominal speed\n"
            << " - one Stage-1 route is retained unchanged during Stage-2\n"
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
