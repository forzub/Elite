#include "src/game/navigation/autopilot/AutopilotLawController.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{

using game::navigation::LocalFlightControlLaw;
using game::navigation::autopilot::AutopilotControlRequest;
using game::navigation::autopilot::AutopilotLawController;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipParams params()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;
    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 50.0f;
    p.reverseMainEngineAccelerationMps2 = 50.0f;
    p.strafeAccel = 50.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.assistedMinimumTargetSpeedChangeRateMps2 = 5.0f;
    p.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;
    return p;
}

AutopilotControlRequest baseRequest()
{
    AutopilotControlRequest r;
    r.forwardMap = {1.0, 0.0, 0.0};
    r.rightMap = {0.0, 0.0, 1.0};
    r.upMap = {0.0, 1.0, 0.0};
    r.desiredForwardMap = r.forwardMap;
    r.desiredUpMap = r.upMap;
    r.deltaSeconds = 0.02;
    return r;
}

void requireNoLegacyNavigationDemand(
    const ShipControlState& control,
    const std::string& owner
)
{
    require(
        !control.navigationAccelerationDemandValid &&
        !control.navigationVelocityTargetValid &&
        !control.navigationPrecisionTranslationOnly,
        owner + " crossed the forbidden direct-navigation actuator seam"
    );
}

std::string readText(const std::string& path)
{
    std::ifstream input(path);
    require(input.good(), "cannot open architecture source: " + path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void testRouteLayerCannotSeeConcretePilots()
{
    const std::string route =
        readText("src/game/navigation/autopilot/ClientRouteAutopilot.h");

    require(
        route.find("AssistedAutopilot") == std::string::npos,
        "ClientRouteAutopilot leaked concrete Assisted pilot dependency"
    );
    require(
        route.find("NewtonianAutopilot") == std::string::npos,
        "ClientRouteAutopilot leaked concrete Newtonian pilot dependency"
    );
    require(
        route.find("PredictivePilot") == std::string::npos,
        "ClientRouteAutopilot still depends on retired mixed-law pilot"
    );
    require(
        route.find("assistedCourseResponseSeconds") == std::string::npos &&
        route.find("assistedBrakingResponseMps2") == std::string::npos,
        "ClientRouteAutopilot reads concrete Assisted controller state"
    );

    const std::string space =
        readText("src/game/SpaceState.cpp");
    require(
        space.find("PredictivePilot") == std::string::npos,
        "SpaceState bypassed the hard autopilot law API"
    );
    require(
        space.find("MainIgnitionAlignmentCosine") == std::string::npos,
        "SpaceState regained Newtonian main-engine ignition doctrine"
    );

    const std::string simulation =
        readText("src/game/simulation/GameSimulation.cpp");
    require(
        simulation.find("PredictivePilot") == std::string::npos,
        "GameSimulation bypassed the hard autopilot law API"
    );

    const std::string controller =
        readText("src/game/navigation/autopilot/AutopilotLawController.h");
    require(
        controller.find("AssistedAutopilot") != std::string::npos &&
        controller.find("NewtonianAutopilot") != std::string::npos,
        "AutopilotLawController is not the explicit two-law selection seam"
    );
}

void testLawStateIsIsolated()
{
    AutopilotLawController controller;
    const auto vehicle = params();

    auto assistedRequest = baseRequest();
    assistedRequest.desiredVelocityMapMps = {20.0, 0.0, 0.0};
    assistedRequest.desiredLinearAccelerationMapMps2 = {10.0, 0.0, 0.0};

    (void)controller.update(
        LocalFlightControlLaw::Assisted,
        assistedRequest,
        vehicle
    );

    const auto assisted =
        controller.telemetry(LocalFlightControlLaw::Assisted, vehicle);
    const auto newtonian =
        controller.telemetry(LocalFlightControlLaw::Newtonian, vehicle);

    require(assisted.initialized, "Assisted state did not initialize");
    require(
        !newtonian.initialized,
        "Assisted state leaked across the hard API into Newtonian"
    );
}

void testNewtonianCanAcquireLaunchAttitudeAtRest()
{
    AutopilotLawController controller;
    const auto vehicle = params();

    auto request = baseRequest();
    request.actualVelocityMapMps = {0.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {0.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {0.0, 0.0, 0.0};

    // Safe-course/startup orchestration may require hull attitude acquisition
    // before any translational command exists.
    request.desiredForwardMap = {0.0, 0.0, 1.0};

    const auto control = controller.update(
        LocalFlightControlLaw::Newtonian,
        request,
        vehicle
    );

    require(
        std::abs(control.pitchInput) + std::abs(control.yawInput) > 1.0e-6f,
        "Newtonian deadlocked at rest instead of acquiring launch attitude"
    );
    require(
        std::abs(control.targetSpeedRate) < 1.0e-6f,
        "Newtonian lit the main engine during attitude-only launch acquisition"
    );
    require(
        std::abs(control.forwardInput) < 1.0e-6f &&
        std::abs(control.strafeInput) < 1.0e-6f &&
        std::abs(control.liftInput) < 1.0e-6f,
        "Newtonian used translation RCS during attitude-only launch acquisition"
    );
}

void testNewtonianTurnsMainAxisBeforeBurn()
{
    AutopilotLawController controller;
    const auto vehicle = params();

    auto request = baseRequest();
    request.actualVelocityMapMps = {40.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {20.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {-20.0, 0.0, 0.0};

    const auto turning = controller.update(
        LocalFlightControlLaw::Newtonian,
        request,
        vehicle
    );

    requireNoLegacyNavigationDemand(turning, "Newtonian");
    require(
        std::abs(turning.targetSpeedRate) < 1.0e-6f,
        "Newtonian fired the main engine while its axis opposed braking demand"
    );
    require(
        std::abs(turning.forwardInput) < 1.0e-6f &&
        std::abs(turning.strafeInput) < 1.0e-6f &&
        std::abs(turning.liftInput) < 1.0e-6f,
        "Newtonian tried to replace main-engine braking with translation RCS"
    );
    require(
        std::abs(turning.pitchInput) + std::abs(turning.yawInput) > 1.0e-6f,
        "Newtonian did not begin the turn-and-burn attitude maneuver"
    );

    // Same acceleration demand after the hull has completed the flip.
    request.forwardMap = {-1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, -1.0};
    const auto burning = controller.update(
        LocalFlightControlLaw::Newtonian,
        request,
        vehicle
    );

    require(
        burning.targetSpeedRate > 0.0f,
        "Newtonian did not use the main engine after thrust-axis alignment"
    );
    require(
        std::abs(burning.forwardInput) < 1.0e-6f &&
        std::abs(burning.strafeInput) < 1.0e-6f &&
        std::abs(burning.liftInput) < 1.0e-6f,
        "Newtonian ordinary braking leaked into translation RCS"
    );
}

void testNewtonianArcAccelerationUsesMainAxis()
{
    AutopilotLawController controller;
    const auto vehicle = params();

    auto request = baseRequest();
    request.actualVelocityMapMps = {40.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {40.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {0.0, 0.0, 20.0};

    const auto control = controller.update(
        LocalFlightControlLaw::Newtonian,
        request,
        vehicle
    );

    require(
        std::abs(control.targetSpeedRate) < 1.0e-6f,
        "Newtonian burned before pointing main thrust toward curve acceleration"
    );
    require(
        std::abs(control.forwardInput) < 1.0e-6f &&
        std::abs(control.strafeInput) < 1.0e-6f &&
        std::abs(control.liftInput) < 1.0e-6f,
        "Newtonian used RCS as lateral main thrust on an arc"
    );
    require(
        std::abs(control.pitchInput) + std::abs(control.yawInput) > 1.0e-6f,
        "Newtonian did not turn its main axis toward curvature acceleration"
    );
}

void testPrecisionStopMayUseRcs()
{
    AutopilotLawController controller;
    const auto vehicle = params();

    auto request = baseRequest();
    request.actualVelocityMapMps = {0.10, 0.0, 0.0};
    request.desiredVelocityMapMps = {0.0, 0.0, 0.0};
    request.stopRequested = true;

    const auto control = controller.update(
        LocalFlightControlLaw::Newtonian,
        request,
        vehicle
    );

    require(
        std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput) > 1.0e-6f,
        "Newtonian precision stop did not allow ordinary RCS trim"
    );
}

void testAssistedAndNewtonianSharePublicCommandShape()
{
    AutopilotLawController controller;
    const auto vehicle = params();
    auto request = baseRequest();
    request.desiredVelocityMapMps = {20.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {10.0, 0.0, 0.0};

    const auto assisted = controller.update(
        LocalFlightControlLaw::Assisted,
        request,
        vehicle
    );
    const auto newtonian = controller.update(
        LocalFlightControlLaw::Newtonian,
        request,
        vehicle
    );

    requireNoLegacyNavigationDemand(assisted, "Assisted");
    requireNoLegacyNavigationDemand(newtonian, "Newtonian");
}

} // namespace

int main()
{
    try
    {
        testRouteLayerCannotSeeConcretePilots();
        testLawStateIsIsolated();
        testNewtonianCanAcquireLaunchAttitudeAtRest();
        testNewtonianTurnsMainAxisBeforeBurn();
        testNewtonianArcAccelerationUsesMainAxis();
        testPrecisionStopMayUseRcs();
        testAssistedAndNewtonianSharePublicCommandShape();

        std::cout
            << "AUTOPILOT LAW API: PASS\n"
            << " - route layer cannot see concrete pilot modules\n"
            << " - Assisted/Newtonian share one request/command API\n"
            << " - concrete controller state is isolated per law\n"
            << " - Newtonian can acquire launch attitude from rest without thrust\n"
            << " - Newtonian ordinary delta-v is main-engine owned\n"
            << " - Newtonian RCS translation is precision-stop only\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "AUTOPILOT LAW API: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
