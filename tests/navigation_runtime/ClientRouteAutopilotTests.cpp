#include "src/game/navigation/autopilot/ClientRouteAutopilot.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{

using Autopilot =
    game::navigation::autopilot::ClientRouteAutopilot;
using Agent =
    game::navigation::autopilot::RouteFollowerAgentState;

void require(bool condition, const char* message)
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
    p.maxCombatSpeed = 120.0f;
    p.maxCruiseSpeed = 200.0f;
    p.throttleAccel = 5.0f;
    p.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    p.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 40.0f;
    p.reverseMainEngineAccelerationMps2 = 40.0f;
    p.strafeAccel = 10.0f;
    p.manoeuvreThrusterAccel = 4.0f;
    p.maxGs = 5.0f;
    p.maxLinearGs = 5.0f;
    return p;
}

game::navigation::planner::RoutePlan plan()
{
    using namespace game::navigation::planner;

    RoutePlan p;
    p.disposition = RoutePlanDisposition::Ready;
    p.failureCode = RoutePlanFailureCode::None;

    p.gates = {
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 20.0},
        {{100.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 20.0},
        {{200.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    p.executionGates = p.gates;
    return p;
}

void testClientAutopilotEmitsOrdinaryControls()
{
    Autopilot::State state;
    const auto vehicle = params();
    const auto route = plan();

    require(
        Autopilot::start(
            state,
            route,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            7,
            25.0
        ),
        "client autopilot rejected valid route"
    );
    require(state.active, "client autopilot did not become active");
    require(!state.programs.empty(), "client autopilot built no programs");
    require(
        state.programs.front().referenceMode ==
            game::navigation::AcceptedManeuverProgram::
                ReferenceMode::SpatialCorridor,
        "client autopilot did not build a spatial corridor"
    );

    Agent agent;
    agent.positionMapMeters = {0.0, 0.0, 0.0};
    agent.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.rightMap = {0.0, 0.0, 1.0};
    agent.upMap = {0.0, 1.0, 0.0};

    const auto output = Autopilot::update(
        state,
        agent,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "client autopilot emitted invalid output");
    require(!output.complete, "client autopilot completed at route start");
    require(
        !output.control.navigationAccelerationDemandValid &&
        !output.control.navigationVelocityTargetValid &&
        !output.control.navigationPrecisionTranslationOnly,
        "client autopilot escaped through a direct navigation demand"
    );
    require(
        std::abs(output.control.targetSpeedRate) > 1.0e-6 ||
        std::abs(output.control.pitchInput) > 1.0e-6 ||
        std::abs(output.control.yawInput) > 1.0e-6,
        "client autopilot emitted no ordinary control input"
    );
}

void testStoppedSpatialOriginLaunches()
{
    auto route = plan();
    route.gates.front().speedMps = 0.0;
    route.executionGates.front().speedMps = 0.0;

    Autopilot::State state;
    const auto vehicle = params();

    require(
        Autopilot::start(
            state,
            route,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            8,
            25.0
        ),
        "client autopilot rejected stopped-start route"
    );

    Agent agent;
    agent.positionMapMeters = {0.0, 0.0, 0.0};
    agent.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.rightMap = {0.0, 0.0, 1.0};
    agent.upMap = {0.0, 1.0, 0.0};

    const auto output = Autopilot::update(
        state,
        agent,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "stopped-start route emitted invalid output");
    require(
        std::abs(output.control.targetSpeedRate) > 1.0e-6 ||
        std::abs(output.control.pitchInput) > 1.0e-6 ||
        std::abs(output.control.yawInput) > 1.0e-6,
        "stopped spatial origin deadlocked at zero control"
    );
    require(
        glm::length(
            state.programs.front().
                samples[0].velocityMapMetersPerSecond
        ) > 0.0,
        "stopped route origin was not converted into a moving departure seed"
    );
}

void testClientStabilizerUsesOrdinaryControls()
{
    Autopilot::State routeState;
    (void)routeState;

    game::navigation::autopilot::PredictivePilot::State pilotState;
    Agent agent;
    agent.velocityMapMetersPerSecond = {12.0, 0.0, 0.0};
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.rightMap = {0.0, 0.0, 1.0};
    agent.upMap = {0.0, 1.0, 0.0};

    const auto control = Autopilot::stabilize(
        agent,
        game::navigation::LocalFlightControlLaw::Assisted,
        params(),
        pilotState,
        0.02
    );

    require(
        control.velocityAlignmentCommand ==
            game::navigation::VelocityAlignmentMode::BrakeToStop,
        "client stabilizer did not request ordinary BrakeToStop"
    );
    require(
        !control.navigationAccelerationDemandValid &&
        !control.navigationVelocityTargetValid,
        "client stabilizer used a direct navigation demand"
    );
}

} // namespace

int main()
{
    try
    {
        testClientAutopilotEmitsOrdinaryControls();
        testStoppedSpatialOriginLaunches();
        testClientStabilizerUsesOrdinaryControls();
        std::cout
            << "CLIENT ROUTE AUTOPILOT TESTS: PASS\n"
            << " - RoutePlan is adapted to SpatialCorridor on the client\n"
            << " - execution emits only ordinary ShipControlState inputs\n"
            << " - stopped spatial origin launches instead of deadlocking\n"
            << " - stabilization uses the same BrakeToStop control surface\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr
            << "CLIENT ROUTE AUTOPILOT TESTS: FAIL: "
            << ex.what() << "\n";
        return 1;
    }
}
