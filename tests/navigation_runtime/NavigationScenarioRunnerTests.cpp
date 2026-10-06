#include "src/game/navigation/diagnostics/NavigationScenarioRunner.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Runner =
    game::navigation::diagnostics::NavigationScenarioRunner;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipParams shipParams()
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
    p.forwardMainEngineAccelerationMps2 = 73.549875f;
    p.reverseMainEngineAccelerationMps2 = 73.549875f;
    p.strafeAccel = 20.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;
    return p;
}

void testPlannerFeedsFollowerWithoutSyntheticRoutePlan()
{
    Runner::Request request;
    request.controlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    request.ship = shipParams();

    request.initialAgent.positionMapMeters =
        {0.0, 0.0, 0.0};
    request.initialAgent.velocityMapMetersPerSecond =
        {20.0, 0.0, 0.0};
    request.initialAgent.forwardMap =
        {1.0, 0.0, 0.0};
    request.initialAgent.rightMap =
        {0.0, 0.0, 1.0};
    request.initialAgent.upMap =
        {0.0, 1.0, 0.0};

    // The scenario specifies world/vehicle/goal only. It deliberately does
    // not construct RoutePlan, RouteGate or RouteCurveSegment.
    request.route.goalMeters =
        {6000.0, 0.0, 6000.0};
    request.route.terminalOutward =
        {0.0, 0.0, 1.0};
    request.route.terminalReferenceDistanceMeters =
        500.0;
    request.route.agentRadiusMeters = 15.0;
    request.route.maxSpeedMps = 80.0;
    request.route.acceleratingMps2 = 20.0;
    request.route.brakingMps2 = 20.0;
    request.route.lateralMps2 = 20.0;
    request.route.maxAngularVelocityRadPerSecond = 2.5;
    request.route.maxAngularAccelerationRadPerSecond2 = 3.0;
    request.route.roundTurns = true;
    request.route.initialForwardLeadMeters = 1000.0;
    request.route.gateSpacingMeters = 500.0;
    request.route.terminalGateSpacingMeters = 250.0;
    request.route.terminalApproachLengthMeters = 2000.0;
    request.route.deriveTerminalTurnRadiusFromVehicle = true;

    request.planningLeadSeconds = 1.0;
    request.deltaSeconds = 0.02;
    request.maximumRunSeconds = 0.20;
    request.trackingToleranceMeters = 25.0;
    request.routeUpReference = {0.0, 1.0, 0.0};
    request.hasRouteUpReference = true;
    request.enforceTerminalStop = true;

    const auto result = Runner::run(request);

    require(
        result.plannerAccepted,
        "production RoutePlanner rejected scenario"
    );
    require(
        !result.plan.routeCurves.empty(),
        "production RoutePlanner produced no authored geometry"
    );
    require(
        result.followerAccepted,
        "production follower rejected production planner output"
    );
    require(
        !result.frames.empty(),
        "production navigation chain emitted no runtime frames"
    );

    const std::string report = result.reportText();
    require(
        report.find("route_curves=") != std::string::npos &&
        report.find("transition[") != std::string::npos,
        "scenario report omitted planner geometry/transition diagnostics"
    );

    require(
        result.saveReport(
            "build/test-logs/navigation-scenario-smoke.log"
        ),
        "scenario report file could not be written"
    );
}

}

int main()
{
    try
    {
        testPlannerFeedsFollowerWithoutSyntheticRoutePlan();
        std::cout
            << "NAVIGATION SCENARIO RUNNER TESTS: PASS\n"
            << " - scenario describes state/goal, not RoutePlan internals\n"
            << " - production RoutePlanner output feeds production follower\n"
            << " - headless execution emits shareable trace report\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NAVIGATION SCENARIO RUNNER TESTS: FAIL: "
            << error.what() << '\n';
        return 1;
    }
}
