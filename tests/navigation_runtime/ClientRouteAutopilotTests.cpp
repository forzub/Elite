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
            Agent{
                {0.0, 0.0, 0.0},
                {0.0, 0.0, 0.0},
                {1.0, 0.0, 0.0},
                {0.0, 0.0, 1.0},
                {0.0, 1.0, 0.0},
                0.0, 0.0, 0.0
            },
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
            Agent{
                {0.0, 0.0, 0.0},
                {0.0, 0.0, 0.0},
                {1.0, 0.0, 0.0},
                {0.0, 0.0, 1.0},
                {0.0, 1.0, 0.0},
                0.0, 0.0, 0.0
            },
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
        ) <= 1.0e-9,
        "trajectory compiler no longer preserves the physical stopped start"
    );
}

void testSpatialTurnUsesSameVelocityAndNoseTarget()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    // Real planner output never contains an instantaneous 90-degree corner.
    // Exercise the client compiler/follower with a smooth quarter-circle whose
    // tangent rotates continuously from +X to +Z.
    constexpr double RadiusMeters = 200.0;
    constexpr int ArcSteps = 16;
    constexpr double SpeedMps = 12.0;
    constexpr double HalfPi =
        1.5707963267948966192313216916398;

    route.executionGates.reserve(ArcSteps + 1);
    for (int i = 0; i <= ArcSteps; ++i)
    {
        const double t =
            HalfPi * static_cast<double>(i) /
            static_cast<double>(ArcSteps);

        RouteGate gate;
        gate.positionMeters = {
            RadiusMeters * std::sin(t),
            0.0,
            RadiusMeters * (1.0 - std::cos(t))
        };
        gate.forward = glm::normalize(glm::dvec3(
            std::cos(t),
            0.0,
            std::sin(t)
        ));
        gate.speedMps = SpeedMps;
        route.executionGates.push_back(gate);
    }
    route.gates = route.executionGates;

    Autopilot::State state;
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters =
        route.executionGates.front().positionMeters;
    initial.velocityMapMetersPerSecond = {
        SpeedMps, 0.0, 0.0
    };
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            9,
            25.0
        ),
        "client autopilot rejected smooth turning route"
    );

    Agent nearTurn = initial;
    nearTurn.positionMapMeters =
        route.executionGates[ArcSteps / 2].positionMeters;
    nearTurn.velocityMapMetersPerSecond =
        glm::normalize(
            route.executionGates[ArcSteps / 2].forward
        ) * SpeedMps;

    const auto output = Autopilot::update(
        state,
        nearTurn,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "smooth turning route emitted invalid output");
    require(
        std::abs(output.control.pitchInput) > 1.0e-6 ||
        std::abs(output.control.yawInput) > 1.0e-6,
        "spatial turn changed velocity target without rotating the nose"
    );
}

void testSpatialTurnStartsBeforeArcEntry()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    constexpr double StraightEndMeters = 200.0;
    constexpr double RadiusMeters = 200.0;
    constexpr int ArcSteps = 16;
    constexpr double SpeedMps = 12.0;
    constexpr double HalfPi =
        1.5707963267948966192313216916398;

    RouteGate origin;
    origin.positionMeters = {0.0, 0.0, 0.0};
    origin.forward = {1.0, 0.0, 0.0};
    origin.speedMps = SpeedMps;
    route.executionGates.push_back(origin);

    RouteGate straight;
    straight.positionMeters = {StraightEndMeters, 0.0, 0.0};
    straight.forward = {1.0, 0.0, 0.0};
    straight.speedMps = SpeedMps;
    route.executionGates.push_back(straight);

    for (int i = 1; i <= ArcSteps; ++i)
    {
        const double t =
            HalfPi * static_cast<double>(i) /
            static_cast<double>(ArcSteps);

        RouteGate gate;
        gate.positionMeters = {
            StraightEndMeters + RadiusMeters * std::sin(t),
            0.0,
            RadiusMeters * (1.0 - std::cos(t))
        };
        gate.forward = glm::normalize(glm::dvec3(
            std::cos(t),
            0.0,
            std::sin(t)
        ));
        gate.speedMps = SpeedMps;
        route.executionGates.push_back(gate);
    }
    route.gates = route.executionGates;

    Autopilot::State state;
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {SpeedMps, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            10,
            25.0
        ),
        "client autopilot rejected straight-to-arc route"
    );

    Agent beforeTurn = initial;
    beforeTurn.positionMapMeters = {
        StraightEndMeters - 25.0, 0.0, 0.0
    };

    const auto output = Autopilot::update(
        state,
        beforeTurn,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid,
        "straight-to-arc route emitted invalid output");
    require(
        std::abs(output.control.pitchInput) > 1.0e-6 ||
        std::abs(output.control.yawInput) > 1.0e-6,
        "follower waited for arc entry instead of steering ahead into bend"
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
        testSpatialTurnUsesSameVelocityAndNoseTarget();
        testSpatialTurnStartsBeforeArcEntry();
        testClientStabilizerUsesOrdinaryControls();
        std::cout
            << "CLIENT ROUTE AUTOPILOT TESTS: PASS\n"
            << " - RoutePlan is adapted to SpatialCorridor on the client\n"
            << " - execution emits only ordinary ShipControlState inputs\n"
            << " - stopped spatial origin launches from next accepted control speed\n"
            << " - spatial turn drives velocity and nose from one centerline source\n"
            << " - follower starts steering before straight-to-arc entry\n"
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
