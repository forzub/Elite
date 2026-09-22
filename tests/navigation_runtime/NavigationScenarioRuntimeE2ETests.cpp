#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "tools/navigation_runtime/NavigationScenarioRuntime.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"

namespace
{

using namespace elite::tools::navigation_runtime;

const ScenarioVehicleParameters& fixtureVehicle()
{
    static const ScenarioVehicleParameters vehicle =
        makeScenarioVehicleParameters(
            EliteCobraMk1::EliteCobraMk1Descriptor()
        );
    return vehicle;
}

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

    const auto low = calculateScenario(scenario, lowStandard, fixtureVehicle());
    const auto fast = calculateScenario(scenario, highStandard, fixtureVehicle());
    const auto extreme = calculateScenario(scenario, highExtreme, fixtureVehicle());

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

void testHighSpeedRunReacquiresInsteadOfOutrunningReference()
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
    settings.startSpeedOverrideMps = 26.15;
    settings.finishSpeedOverrideMps = 11.75;

    const auto planned =
        calculateScenario(scenario, settings, fixtureVehicle());
    require(
        planned.success,
        "high-speed reacquisition fixture failed Stage-1 planning"
    );

    const auto executed =
        executeCalculatedRoute(scenario, settings, planned.trace, fixtureVehicle());

    printDiagnostics("[E2E-HIGH-SPEED] ", executed);

    require(
        hasDiagnostic(executed, "REFERENCE CLOCK: MONOTONIC"),
        "high-speed execution lost monotonic reference-clock diagnostics"
    );
    require(
        executed.success,
        "high-speed execution outran its physical ship instead of reacquiring"
    );

    const double finalError =
        glm::length(
            executed.trace.frames.back().shipPosition -
            executed.trace.sceneFinishMapMeters
        );
    require(
        finalError <= 5.0,
        "high-speed execution still declared completion far from finish"
    );
}

void testAssistedLowSpeedDoesNotRunAwayDuringReferenceHold()
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
    settings.controlMode = ControlMode::Assisted;
    settings.pilot = PilotLevel::Expert;
    settings.flightStyle = FlightStyle::Standard;
    settings.enableSuddenObstacle = false;
    settings.startSpeedOverrideMps = 10.0;
    settings.finishSpeedOverrideMps = 10.0;

    const auto planned =
        calculateScenario(scenario, settings, fixtureVehicle());
    require(
        planned.success,
        "Assisted 10->10 regression fixture failed Stage-1 planning"
    );

    const auto executed =
        executeCalculatedRoute(scenario, settings, planned.trace, fixtureVehicle());

    printDiagnostics("[E2E-ASSISTED-10] ", executed);

    require(
        hasDiagnostic(executed, "REFERENCE CLOCK HOLD: "),
        "Assisted execution lost monotonic reference-clock diagnostics"
    );

    double maximumPhysicalSpeed = 0.0;
    for (const auto& frame : executed.trace.frames)
    {
        maximumPhysicalSpeed =
            std::max(
                maximumPhysicalSpeed,
                glm::length(frame.shipVelocity)
            );
    }

    require(
        maximumPhysicalSpeed <= 25.0,
        "Assisted 10->10 execution reproduced unbounded physical speed growth"
    );
    require(
        executed.success,
        "Assisted 10->10 execution failed to reacquire and finish physically"
    );

    const auto& finalFrame = executed.trace.frames.back();
    const double finalError =
        glm::length(
            finalFrame.shipPosition -
            executed.trace.sceneFinishMapMeters
        );
    const double finalSpeed =
        glm::length(finalFrame.shipVelocity);

    require(
        finalError <= 5.0,
        "Assisted 10->10 execution ended outside terminal position tolerance"
    );
    require(
        std::abs(finalSpeed - 10.0) <= 1.5,
        "Assisted 10->10 execution ended outside terminal speed tolerance"
    );
}

void testAssistedHigherSpeedUsesHullCoupledPhysicalBraking()
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
    settings.controlMode = ControlMode::Assisted;
    settings.pilot = PilotLevel::Expert;
    settings.flightStyle = FlightStyle::Standard;
    settings.enableSuddenObstacle = false;
    settings.startSpeedOverrideMps = 20.90;
    settings.finishSpeedOverrideMps = 20.00;

    const auto planned = calculateScenario(scenario, settings, fixtureVehicle());
    require(
        planned.success,
        "Assisted 20.9->20 physical-braking fixture failed Stage-1 planning"
    );

    const auto executed =
        executeCalculatedRoute(scenario, settings, planned.trace, fixtureVehicle());

    printDiagnostics("[E2E-ASSISTED-20] ", executed);

    double maximumPhysicalSpeed = 0.0;
    for (const auto& frame : executed.trace.frames)
    {
        maximumPhysicalSpeed =
            std::max(
                maximumPhysicalSpeed,
                glm::length(frame.shipVelocity)
            );

        const double mainMagnitude =
            glm::length(frame.mainEngineAccelerationMps2);
        if (mainMagnitude > 1.0e-6)
        {
            const glm::dvec3 forward =
                glm::normalize(frame.shipForward);
            require(
                glm::dot(
                    frame.mainEngineAccelerationMps2,
                    forward
                ) >= -1.0e-6,
                "Assisted runtime produced reverse/fore main thrust without rotating the hull"
            );
        }
    }

    require(
        maximumPhysicalSpeed <= 35.0,
        "Assisted 20.9->20 execution escaped its physical speed regime"
    );
    require(
        executed.success,
        "Assisted 20.9->20 did not complete through hull-coupled physical control"
    );

    const auto& finalFrame = executed.trace.frames.back();
    const double finalError =
        glm::length(
            finalFrame.shipPosition -
            executed.trace.sceneFinishMapMeters
        );
    const double finalSpeed =
        glm::length(finalFrame.shipVelocity);

    require(
        finalError <= 5.0,
        "Assisted 20.9->20 ended outside terminal position tolerance"
    );
    require(
        std::abs(finalSpeed - 20.0) <= 1.5,
        "Assisted 20.9->20 ended outside terminal speed tolerance"
    );
}

void testNewtonianHigherSpeedUsesMainEngineDominantManeuver()
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
    settings.startSpeedOverrideMps = 21.20;
    settings.finishSpeedOverrideMps = 21.20;

    const auto planned = calculateScenario(scenario, settings, fixtureVehicle());
    require(
        planned.success,
        "Newtonian 21.2->21.2 main-engine fixture failed Stage-1 planning"
    );

    const auto executed =
        executeCalculatedRoute(scenario, settings, planned.trace, fixtureVehicle());

    printDiagnostics("[E2E-NEWTONIAN-21] ", executed);

    bool mainParticipatedEarly = false;
    double maximumEarlyMainAcceleration = 0.0;

    for (const auto& frame : executed.trace.frames)
    {
        const double mainMagnitude =
            glm::length(frame.mainEngineAccelerationMps2);

        if (frame.timeSeconds <= 8.0)
        {
            maximumEarlyMainAcceleration =
                std::max(
                    maximumEarlyMainAcceleration,
                    mainMagnitude
                );
            if (mainMagnitude >= 0.25)
                mainParticipatedEarly = true;
        }

        if (mainMagnitude > 1.0e-6)
        {
            const glm::dvec3 forward =
                glm::normalize(frame.shipForward);
            require(
                glm::dot(
                    frame.mainEngineAccelerationMps2,
                    forward
                ) >= -1.0e-6,
                "Newtonian runtime produced impossible forward/nose main thrust"
            );
        }
    }

    require(
        mainParticipatedEarly,
        "Newtonian 21.2->21.2 still flew the material maneuver on RCS alone"
    );
    require(
        maximumEarlyMainAcceleration >= 0.25,
        "Newtonian 21.2->21.2 never acquired an early main-engine burn"
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
        calculateScenario(scenario, settings, fixtureVehicle());

    printDiagnostics("[E2E-STAGE1] ", planned);

    require(planned.success, "default Stage-1 route calculation failed");
    require(
        planned.trace.routePoints.size() >= 2,
        "default Stage-1 route contains fewer than two points"
    );

    const auto retainedRoute = planned.trace.routePoints;

    const auto executed =
        executeCalculatedRoute(scenario, settings, planned.trace, fixtureVehicle());

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
        hasDiagnostic(
            executed,
            "PLANNED ACTUATOR SOURCE COVERAGE: "
        ),
        "Stage 2 lost actuator/source coverage diagnostics"
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
        testHighSpeedRunReacquiresInsteadOfOutrunningReference();
        testAssistedLowSpeedDoesNotRunAwayDuringReferenceHold();
        testAssistedHigherSpeedUsesHullCoupledPhysicalBraking();
        testNewtonianHigherSpeedUsesMainEngineDominantManeuver();
        testDefaultScenarioRunsPlannerRouteThroughFollowerAndPhysics();
        std::cout
            << "NAVIGATION RETAINED-ROUTE E2E: PASS\n"
            << " - speed changes Stage-1 maneuver clearance and route points\n"
            << " - STANDARD/EXTREME change clearance, not nominal speed\n"
            << " - one Stage-1 route is retained unchanged during Stage-2\n"
            << " - Ruckig parameterizes that retained route\n"
            << " - high-speed execution uses one monotonic maneuver clock\n"
            << " - Assisted 10->10 cannot turn reference hold into a speed runaway\n"
            << " - Assisted 20.9->20 uses aft-only main thrust and physical hull coupling\n"
            << " - Newtonian 21.2->21.2 acquires main-engine thrust instead of flying on RCS alone\n"
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
