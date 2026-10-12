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
    p.mainGimbalPitchLimitRad = 0.122173048f; // 7 deg
    p.mainGimbalYawLimitRad = 0.122173048f;   // 7 deg
    p.mainGimbalRateRadPerSec = 0.139626340f;// 8 deg/s
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
        route.find("NewtonianOptimalController") == std::string::npos,
        "ClientRouteAutopilot leaked concrete Newtonian controller dependency"
    );
    require(
        route.find("NewtonianAutopilot") == std::string::npos,
        "ClientRouteAutopilot still references retired NewtonianAutopilot"
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

    require(
        route.find("mainGimbal") == std::string::npos,
        "ClientRouteAutopilot leaked Newtonian TVC actuator details"
    );

    const std::string assistedPilot =
        readText("src/game/navigation/autopilot/AssistedAutopilot.h");
    require(
        assistedPilot.find("mainGimbal") == std::string::npos,
        "AssistedAutopilot leaked into the Newtonian TVC actuator channel"
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
        controller.find("NewtonianOptimalController") != std::string::npos,
        "AutopilotLawController is not the explicit Assisted/optimal-Newtonian selection seam"
    );
    require(
        controller.find("NewtonianAutopilot") == std::string::npos,
        "AutopilotLawController still exposes retired NewtonianAutopilot"
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

void testAssistedDoesNotUseNewtonianTvcChannel()
{
    AutopilotLawController controller;
    const auto vehicle = params();

    auto request = baseRequest();
    request.desiredVelocityMapMps = {20.0, 0.0, 5.0};
    request.desiredLinearAccelerationMapMps2 = {10.0, 0.0, 5.0};

    const auto control = controller.update(
        LocalFlightControlLaw::Assisted,
        request,
        vehicle
    );

    require(
        std::abs(control.mainGimbalRightInput) < 1.0e-7f &&
        std::abs(control.mainGimbalUpInput) < 1.0e-7f,
        "Assisted leaked into the Newtonian TVC actuator channel"
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
        testAssistedDoesNotUseNewtonianTvcChannel();
        testPrecisionStopMayUseRcs();
        testAssistedAndNewtonianSharePublicCommandShape();

        std::cout
            << "AUTOPILOT LAW API: PASS\n"
            << " - route layer cannot see concrete pilot modules\n"
            << " - Assisted/Newtonian share one request/command API\n"
            << " - concrete controller state is isolated per law\n"
            << " - Newtonian can acquire launch attitude from rest without thrust\n"
            << " - Newtonian optimal control is selected behind the same hard API\n"
            << " - Assisted never writes the Newtonian TVC channel\n"
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
