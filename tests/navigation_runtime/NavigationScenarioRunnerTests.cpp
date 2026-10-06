#include "src/game/navigation/diagnostics/NavigationScenarioRunner.h"

#include <algorithm>
#include <cmath>
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

Runner::Request assistedScenario()
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
    request.trackingToleranceMeters = 25.0;
    request.routeUpReference = {0.0, 1.0, 0.0};
    request.hasRouteUpReference = true;
    request.enforceTerminalStop = true;
    return request;
}

double angleBetween(
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    const double la = glm::length(a);
    const double lb = glm::length(b);
    if (la <= 1.0e-12 || lb <= 1.0e-12)
        return 3.14159265358979323846;
    return std::acos(std::clamp(
        glm::dot(a / la, b / lb),
        -1.0,
        1.0
    ));
}

void testPlannerFeedsFollowerWithoutSyntheticRoutePlan()
{
    auto request = assistedScenario();
    request.maximumRunSeconds = 0.20;

    const auto result = Runner::run(request);

    const std::string report = result.reportText();
    require(
        result.saveReport(
            "build/test-logs/navigation-scenario-smoke.log"
        ),
        "scenario report file could not be written"
    );

    if (!result.plannerAccepted ||
        !result.followerAccepted ||
        result.frames.empty())
    {
        std::cerr << report;
    }

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

    require(
        report.find("route_curves=") != std::string::npos &&
        report.find("transition[") != std::string::npos,
        "scenario report omitted planner geometry/transition diagnostics"
    );
}


void testPlannerLineArcTransitionFeedsSameFollower()
{
    auto request = assistedScenario();
    request.maximumRunSeconds = 20.0;

    const auto result = Runner::run(request);
    const std::string report = result.reportText();
    require(
        result.saveReport(
            "build/test-logs/navigation-line-arc.log"
        ),
        "line-arc report file could not be written"
    );

    require(
        result.plannerAccepted,
        "line-arc scenario planner rejected geometry"
    );
    require(
        result.followerAccepted,
        "line-arc scenario follower rejected planner output"
    );

    bool foundLineArc = false;
    std::size_t lineArcIndex = 0;
    for (std::size_t i = 0;
         i + 1 < result.plan.routeCurves.size();
         ++i)
    {
        const auto& a = result.plan.routeCurves[i];
        const auto& b = result.plan.routeCurves[i + 1];
        if (a.kind ==
                game::navigation::planner::RouteCurveKind::Line &&
            b.kind ==
                game::navigation::planner::RouteCurveKind::CircularArc)
        {
            foundLineArc = true;
            lineArcIndex = i;
            const double tangentError = angleBetween(
                a.tangentAtProgress(a.endProgressMeters),
                b.tangentAtProgress(b.startProgressMeters)
            );
            require(
                tangentError < 1.0e-6,
                "planner authored non-tangent line-to-arc transition"
            );
            break;
        }
    }
    require(
        foundLineArc,
        "planner scenario produced no line-to-arc transition"
    );

    bool reachedArc = false;
    double maxCrossTrackOnArc = 0.0;
    for (const auto& frame : result.frames)
    {
        if (frame.routeCurveIndex == lineArcIndex + 1)
        {
            reachedArc = true;
            maxCrossTrackOnArc = std::max(
                maxCrossTrackOnArc,
                std::abs(frame.crossTrackErrorMeters)
            );
        }
    }

    if (!reachedArc)
        std::cerr << report;

    require(
        reachedArc,
        "follower never reached planner-authored first arc"
    );
    require(
        maxCrossTrackOnArc <= request.trackingToleranceMeters,
        "follower exceeded corridor tolerance on first planner-authored arc"
    );
}

}

int main()
{
    try
    {
        testPlannerFeedsFollowerWithoutSyntheticRoutePlan();
        testPlannerLineArcTransitionFeedsSameFollower();
        std::cout
            << "NAVIGATION SCENARIO RUNNER TESTS: PASS\n"
            << " - scenario describes state/goal, not RoutePlan internals\n"
            << " - production RoutePlanner output feeds production follower\n"
            << " - headless execution emits shareable trace report\n"
            << " - production line->arc transition is tangent and reached by the same follower\n";
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
