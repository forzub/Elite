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
        glm::length(
            state.programs.front().
                samples[0].velocityMapMetersPerSecond
        ) <= 1.0e-9,
        "trajectory compiler no longer preserves the physical stopped start"
    );
    require(
        output.targetLongitudinalAccelerationMps2 > 1.0e-6,
        "stopped spatial origin lost the acceleration required by the next point"
    );
    require(
        glm::dot(
            state.programs.front().
                samples[0].linearAccelerationFeedForwardMapMps2,
            glm::normalize(
                state.programs.front().
                    samples[1].positionMapMeters -
                state.programs.front().
                    samples[0].positionMapMeters
            )
        ) > 1.0e-6,
        "accepted first sample incorrectly encoded v=0 as a=0"
    );
    require(
        output.control.targetSpeedRate > 1.0e-6f,
        "Assisted pilot ignored positive trajectory acceleration at v=0"
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
    require(
        std::abs(output.control.strafeInput) < 1.0e-6 &&
        std::abs(output.control.liftInput) < 1.0e-6,
        "spatial turn used manoeuvre translation instead of hull attitude"
    );
}

void testCourseLagLeadsIntoUpcomingTurn()
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
        output.courseLeadDistanceMeters > 0.0,
        "course-lag follower produced no dynamic lead distance"
    );
    require(
        std::abs(output.control.pitchInput) > 1.0e-5 ||
        std::abs(output.control.yawInput) > 1.0e-5,
        "course-lag follower waited until arc entry instead of leading the turn"
    );
}

void testContinuousProgramCorrectsCrossTrackError()
{
    auto route = plan();
    for (auto& gate : route.executionGates)
        gate.speedMps = 20.0;
    route.executionGates.back().speedMps = 0.0;
    route.gates = route.executionGates;

    Autopilot::State state;
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {20.0, 0.0, 0.0};
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
            14,
            25.0
        ),
        "client autopilot rejected cross-track route"
    );

    Agent offset = initial;
    offset.positionMapMeters = {50.0, 0.0, 10.0};

    const auto output = Autopilot::update(
        state,
        offset,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "cross-track correction emitted invalid output");
    require(
        output.crossTrackErrorMeters > 9.0,
        "continuous follower did not measure cross-track displacement"
    );
    require(
        std::hypot(
            output.control.pitchInput,
            output.control.yawInput
        ) > 1.0e-4,
        "continuous follower did not steer nose to correct meter-scale cross-track error"
    );
    require(
        std::abs(output.control.strafeInput) < 1.0e-6 &&
        std::abs(output.control.liftInput) < 1.0e-6,
        "continuous follower incorrectly used manoeuvre thrusters for cross-track correction"
    );
}

void testCourseLagPredictionIgnoresCentimetresButCorrectsFutureMiss()
{
    const auto route = plan();
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {20.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State quietState;
    require(
        Autopilot::start(
            quietState,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            19,
            20.0
        ),
        "course-lag deadband route rejected"
    );

    Agent centimetreError = initial;
    centimetreError.positionMapMeters = {50.0, 0.0, 0.5};

    const auto quiet = Autopilot::update(
        quietState,
        centimetreError,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );
    require(quiet.valid, "course-lag deadband update invalid");
    require(
        quiet.centeringDeadbandMeters >= 1.9,
        "meter-scale centering deadband was not derived from corridor tolerance"
    );
    require(
        std::hypot(
            quiet.control.pitchInput,
            quiet.control.yawInput
        ) < 1.0e-3,
        "sub-deadband position error caused hull twitch"
    );

    Autopilot::State driftingState;
    require(
        Autopilot::start(
            driftingState,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            20,
            20.0
        ),
        "course-lag drift route rejected"
    );

    Agent futureMiss = centimetreError;
    futureMiss.velocityMapMetersPerSecond = {20.0, 0.0, 5.0};

    const auto correcting = Autopilot::update(
        driftingState,
        futureMiss,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );
    require(correcting.valid, "course-lag drift update invalid");
    require(
        correcting.predictedCrossTrackMeters >
            correcting.centeringDeadbandMeters,
        "velocity-lag prediction failed to see future cross-track miss"
    );
    require(
        std::hypot(
            correcting.control.pitchInput,
            correcting.control.yawInput
        ) > 1.0e-4,
        "predicted future miss did not produce anticipatory nose correction"
    );
    require(
        std::abs(correcting.control.strafeInput) < 1.0e-6 &&
        std::abs(correcting.control.liftInput) < 1.0e-6,
        "course-lag correction used manoeuvre thrusters"
    );
}

void testParallelOffsetActivelyCapturesCorridorCenter()
{
    const auto route = plan();
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {120.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            28,
            20.0
        ),
        "parallel-offset capture route rejected"
    );

    Agent offset = initial;
    offset.positionMapMeters = {200.0, 0.0, 80.0};
    offset.velocityMapMetersPerSecond = {120.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        offset,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "parallel-offset capture update invalid");
    require(
        output.crossTrackErrorMeters > 60.0,
        "parallel-offset test did not create a material corridor error"
    );
    require(
        std::abs(output.crossTrackCorrectionAngleRad) > 1.0e-3,
        "parallel-offset state did not request a course capture angle"
    );
    require(
        output.desiredCaptureAngularRateRadPerSec > 1.0e-3,
        "parallel-offset route pose did not detect a material capture angle"
    );
    require(
        std::hypot(
            output.control.pitchInput,
            output.control.yawInput
        ) > 1.0e-3,
        "parallel-offset state did not actively rotate back toward center"
    );
}

void testCruiseRollAlignsToDockBottomReference()
{
    const auto route = plan();
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {50.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    const glm::dvec3 dockUp(0.0, 0.0, 1.0);

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            29,
            20.0,
            dockUp,
            true
        ),
        "cruise roll-alignment route rejected"
    );

    const auto output = Autopilot::update(
        state,
        initial,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "cruise roll-alignment update invalid");
    require(
        std::abs(output.signedRollErrorRad) > 0.5,
        "dock-bottom roll test did not see the wrong hull roll"
    );
    require(
        std::abs(output.desiredRollRateRadPerSec) > 1.0e-3,
        "dock-bottom alignment did not publish a roll-rate target"
    );
    require(
        std::abs(output.control.rollInput) > 1.0e-4,
        "dock-bottom alignment did not command hull roll during cruise"
    );
}

void testTerminalFrameHoldsStoppedAndAligned()
{
    const auto route = plan();
    const auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    const glm::dvec3 dockUp(0.0, 0.0, 1.0);

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            21,
            25.0,
            dockUp,
            true
        ),
        "terminal-hold route rejected"
    );

    require(
        !state.continuousSamples.empty(),
        "terminal-hold route produced no samples"
    );

    const auto& final = state.continuousSamples.back();

    Agent atHold;
    atHold.positionMapMeters = final.positionMapMeters;
    atHold.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    atHold.forwardMap = final.forwardMap;

    // Deliberately arrive stopped with the wrong roll. HOLD must not release;
    // it must keep rotating the hull until ship up matches dock up ("bottom"
    // mark alignment).
    atHold.rightMap = {0.0, 0.0, 1.0};
    atHold.upMap = {0.0, 1.0, 0.0};

    const auto output = Autopilot::update(
        state,
        atHold,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "terminal hold emitted invalid output");
    require(output.terminalHold, "terminal frame did not enter HOLD");
    require(!output.complete, "terminal HOLD incorrectly released autopilot");
    require(state.active, "terminal HOLD deactivated autopilot");
    require(
        output.targetSpeedMps <= 1.0e-6,
        "terminal HOLD retained non-zero translational target"
    );
    require(
        std::abs(output.control.rollInput) > 1.0e-6,
        "terminal HOLD stopped translating but did not continue dock-bottom alignment"
    );

    require(
        std::abs(output.control.strafeInput) < 1.0e-6 &&
        std::abs(output.control.liftInput) < 1.0e-6,
        "terminal HOLD incorrectly used manoeuvre thrusters"
    );
}

void testCourseLeadPreservesCurvedTunnelInsteadOfChordCutting()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    constexpr double RadiusMeters = 1000.0;
    constexpr double SpeedMps = 200.0;
    constexpr int ArcSteps = 48;
    constexpr double HalfPi =
        1.5707963267948966192313216916398;

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
    route.executionGates.back().speedMps = 0.0;
    route.gates = route.executionGates;

    game::navigation::planner::RouteCurveSegment exactArc;
    exactArc.kind =
        game::navigation::planner::RouteCurveKind::CircularArc;
    exactArc.startProgressMeters = 0.0;
    exactArc.endProgressMeters = RadiusMeters * HalfPi;
    exactArc.maxSpeedMps = SpeedMps;
    exactArc.startMeters = {0.0, 0.0, 0.0};
    exactArc.endMeters = {RadiusMeters, 0.0, RadiusMeters};
    exactArc.startForward = {1.0, 0.0, 0.0};
    exactArc.endForward = {0.0, 0.0, 1.0};
    exactArc.arcCenterMeters = {0.0, 0.0, RadiusMeters};
    exactArc.arcNormal = {0.0, -1.0, 0.0};
    exactArc.arcRadiusMeters = RadiusMeters;
    exactArc.arcSweepRadians = HalfPi;
    route.routeCurves.push_back(exactArc);

    Agent initial;
    initial.positionMapMeters =
        route.executionGates.front().positionMeters;
    initial.velocityMapMetersPerSecond =
        route.executionGates.front().forward * SpeedMps;
    initial.forwardMap =
        route.executionGates.front().forward;
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            params(),
            1000.0,
            23,
            20.0
        ),
        "curved-tunnel course-lead route rejected"
    );

    Agent onCurve = initial;
    const auto& probeGate =
        route.executionGates[ArcSteps / 4];
    onCurve.positionMapMeters = probeGate.positionMeters;
    onCurve.velocityMapMetersPerSecond =
        probeGate.forward * SpeedMps;
    onCurve.forwardMap = probeGate.forward;
    onCurve.rightMap =
        glm::normalize(
            glm::cross(onCurve.forwardMap, onCurve.upMap)
        );

    const auto output = Autopilot::update(
        state,
        onCurve,
        game::navigation::LocalFlightControlLaw::Assisted,
        params(),
        1000.0,
        0.02
    );

    require(output.valid, "curved-tunnel update invalid");
    require(
        output.courseLeadDistanceMeters > 0.0,
        "curved-tunnel route-pose look-ahead was not active"
    );
    require(
        std::hypot(
            output.control.pitchInput,
            output.control.yawInput
        ) > 1.0e-5,
        "curved-tunnel route pose did not rotate the hull along the authored curve"
    );
    require(
        std::abs(output.routeRadiusMeters - RadiusMeters) < 1.0,
        "follower ignored authoritative circular radius and fell back to sampled chords"
    );
    require(
        std::abs(
            output.routeCurvaturePerMeter -
            1.0 / RadiusMeters
        ) < 1.0e-6,
        "follower did not execute Planner curvature"
    );
    require(
        output.predictedCrossTrackMeters < 80.0,
        "future-tangent prediction departed too far from curved centerline"
    );
    require(
        std::abs(output.control.strafeInput) < 1.0e-6 &&
        std::abs(output.control.liftInput) < 1.0e-6,
        "curved-tunnel tracking incorrectly used manoeuvre thrusters"
    );
}

void testApproachBrakesBeforeDynamicTurnLimit()
{
    using game::navigation::planner::RouteCurveKind;
    using game::navigation::planner::RouteCurveSegment;
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{1100.0, 0.0, 5.0}, {0.995, 0.0, 0.1}, 120.0},
        RouteGate{{1200.0, 0.0, 20.0}, {0.98, 0.0, 0.2}, 120.0},
        RouteGate{{1300.0, 0.0, 45.0}, {0.95, 0.0, 0.31}, 0.0}
    };
    route.gates = route.executionGates;

    RouteCurveSegment line;
    line.kind = RouteCurveKind::Line;
    line.startProgressMeters = 0.0;
    line.endProgressMeters = 1000.0;
    line.maxSpeedMps = 200.0;
    line.startMeters = {0.0, 0.0, 0.0};
    line.endMeters = {1000.0, 0.0, 0.0};
    line.startForward = line.endForward = {1.0, 0.0, 0.0};
    route.routeCurves.push_back(line);

    RouteCurveSegment arc;
    arc.kind = RouteCurveKind::CircularArc;
    arc.startProgressMeters = 1000.0;
    arc.endProgressMeters = 1000.0 + 1000.0 * 0.5;
    arc.maxSpeedMps = 200.0;
    arc.startMeters = {1000.0, 0.0, 0.0};
    arc.arcCenterMeters = {1000.0, 0.0, 1000.0};
    arc.arcNormal = {0.0, -1.0, 0.0};
    arc.arcRadiusMeters = 1000.0;
    arc.arcSweepRadians = 0.5;
    arc.endMeters = arc.positionAtParameter(1.0);
    arc.startForward = {1.0, 0.0, 0.0};
    arc.endForward = arc.tangentAtProgress(arc.endProgressMeters);
    route.routeCurves.push_back(arc);

    auto vehicle = params();
    vehicle.maxCombatSpeed = 250.0f;
    vehicle.maxCruiseSpeed = 250.0f;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {200.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            27,
            20.0
        ),
        "dynamic-turn-speed route rejected"
    );

    Agent approaching = initial;
    approaching.positionMapMeters = {850.0, 0.0, 0.0};

    // The route preview must use the same learned Assisted speed-handle
    // response as PredictivePilot. A slow measured response must move the
    // slowdown horizon outward instead of silently falling back to the
    // configured nominal setpoint rate.
    state.pilotState.assistedSpeedResponseMps2 = 1.0;

    const auto output = Autopilot::update(
        state,
        approaching,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "dynamic-turn-speed update invalid");
    require(
        output.distanceToTurnMeters > 0.0,
        "approach did not identify upcoming authored turn"
    );
    require(
        output.turnSpeedCeilingMps > 0.0 &&
        output.turnSpeedCeilingMps < 200.0,
        "Assisted course dynamics did not reduce curve speed ceiling"
    );
    require(
        output.targetSpeedMps < 200.0,
        "autopilot waited until curve entry to reduce speed"
    );
    require(
        output.turnSpeedSetpointSlewSeconds > 0.0,
        "pre-turn model ignored Assisted speed-handle slew"
    );
    const double effectiveSetpointRate =
        game::navigation::autopilot::PredictivePilot::
            effectiveAssistedTargetSpeedChangeRateMps2(
                vehicle,
                state.pilotState
            );
    const double expectedSlewSeconds =
        std::max(0.0, 200.0 - output.turnSpeedCeilingMps) /
        effectiveSetpointRate;
    require(
        std::abs(
            output.turnSpeedSetpointSlewSeconds -
            expectedSlewSeconds
        ) < 1.0e-6,
        "turn preview and PredictivePilot disagree on learned Assisted setpoint response"
    );
    require(
        output.requiredTurnSlowdownDistanceMeters >
            output.distanceToTurnMeters,
        "pre-turn slowdown horizon still assumes instant speed setpoint"
    );

    Agent preTurn = approaching;
    preTurn.positionMapMeters = {950.0, 0.0, 0.0};
    preTurn.velocityMapMetersPerSecond = {150.0, 0.0, 0.0};

    const auto preTurnOutput = Autopilot::update(
        state,
        preTurn,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );
    require(preTurnOutput.valid, "pre-turn ramp update invalid");

    const double fullTurnRate =
        preTurnOutput.turnSpeedCeilingMps / 1000.0;
    require(
        preTurnOutput.desiredCourseAngularRateRadPerSec > 1.0e-6,
        "pre-turn preparation did not begin angular-rate ramp"
    );
    require(
        preTurnOutput.desiredCourseAngularRateRadPerSec <
            fullTurnRate - 1.0e-6,
        "pre-turn angular-rate command jumped directly to full curve rate"
    );
}


void testTurnPreviewCrossesIntermediateStraight()
{
    using game::navigation::planner::RouteCurveKind;
    using game::navigation::planner::RouteCurveSegment;
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{1100.0, 0.0, 5.0}, {0.995, 0.0, 0.1}, 120.0},
        RouteGate{{1200.0, 0.0, 20.0}, {0.98, 0.0, 0.2}, 120.0},
        RouteGate{{1300.0, 0.0, 45.0}, {0.95, 0.0, 0.31}, 0.0}
    };
    route.gates = route.executionGates;

    RouteCurveSegment firstStraight;
    firstStraight.kind = RouteCurveKind::Line;
    firstStraight.startProgressMeters = 0.0;
    firstStraight.endProgressMeters = 500.0;
    firstStraight.maxSpeedMps = 200.0;
    firstStraight.startMeters = {0.0, 0.0, 0.0};
    firstStraight.endMeters = {500.0, 0.0, 0.0};
    firstStraight.startForward =
        firstStraight.endForward = {1.0, 0.0, 0.0};
    route.routeCurves.push_back(firstStraight);

    RouteCurveSegment secondStraight = firstStraight;
    secondStraight.startProgressMeters = 500.0;
    secondStraight.endProgressMeters = 1000.0;
    secondStraight.startMeters = {500.0, 0.0, 0.0};
    secondStraight.endMeters = {1000.0, 0.0, 0.0};
    route.routeCurves.push_back(secondStraight);

    RouteCurveSegment arc;
    arc.kind = RouteCurveKind::CircularArc;
    arc.startProgressMeters = 1000.0;
    arc.endProgressMeters = 1500.0;
    arc.maxSpeedMps = 200.0;
    arc.startMeters = {1000.0, 0.0, 0.0};
    arc.arcCenterMeters = {1000.0, 0.0, 1000.0};
    arc.arcNormal = {0.0, -1.0, 0.0};
    arc.arcRadiusMeters = 1000.0;
    arc.arcSweepRadians = 0.5;
    arc.endMeters = arc.positionAtParameter(1.0);
    arc.startForward = {1.0, 0.0, 0.0};
    arc.endForward = arc.tangentAtProgress(arc.endProgressMeters);
    route.routeCurves.push_back(arc);

    auto vehicle = params();
    vehicle.maxCombatSpeed = 250.0f;
    vehicle.maxCruiseSpeed = 250.0f;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {200.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            31,
            20.0
        ),
        "multi-straight turn-preview route rejected"
    );

    Agent approaching = initial;
    approaching.positionMapMeters = {400.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        approaching,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "multi-straight turn-preview update invalid");
    require(
        output.distanceToTurnMeters > 500.0,
        "turn preview stopped at the adjacent straight instead of the authored bend"
    );
    require(
        output.turnSpeedCeilingMps > 0.0 &&
        output.turnSpeedCeilingMps < 200.0,
        "future bend beyond an intermediate straight produced no dynamic speed ceiling"
    );
}

void testBezierInteriorCurvatureParticipatesInPreview()
{
    using game::navigation::planner::RouteCurveArcLengthKnot;
    using game::navigation::planner::RouteCurveKind;
    using game::navigation::planner::RouteCurveSegment;
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 200.0},
        RouteGate{{1100.0, 0.0, 3.0}, {1.0, 0.0, 0.05}, 140.0},
        RouteGate{{1200.0, 0.0, 25.0}, {0.98, 0.0, 0.2}, 120.0},
        RouteGate{{1300.0, 0.0, 200.0}, {0.6, 0.0, 0.8}, 0.0}
    };
    route.gates = route.executionGates;

    RouteCurveSegment line;
    line.kind = RouteCurveKind::Line;
    line.startProgressMeters = 0.0;
    line.endProgressMeters = 1000.0;
    line.maxSpeedMps = 200.0;
    line.startMeters = {0.0, 0.0, 0.0};
    line.endMeters = {1000.0, 0.0, 0.0};
    line.startForward = line.endForward = {1.0, 0.0, 0.0};
    route.routeCurves.push_back(line);

    RouteCurveSegment bezier;
    bezier.kind = RouteCurveKind::CubicBezier;
    bezier.startProgressMeters = 1000.0;
    bezier.endProgressMeters = 1400.0;
    bezier.maxSpeedMps = 200.0;
    bezier.startMeters = {1000.0, 0.0, 0.0};
    bezier.bezierControl1Meters = {1100.0, 0.0, 0.0};
    bezier.bezierControl2Meters = {1200.0, 0.0, 0.0};
    bezier.endMeters = {1300.0, 0.0, 200.0};
    bezier.startForward = {1.0, 0.0, 0.0};
    bezier.endForward =
        bezier.tangentAtProgress(bezier.endProgressMeters);
    bezier.arcLengthKnots = {
        RouteCurveArcLengthKnot{0.00, 0.0},
        RouteCurveArcLengthKnot{0.25, 100.0},
        RouteCurveArcLengthKnot{0.50, 200.0},
        RouteCurveArcLengthKnot{0.75, 300.0},
        RouteCurveArcLengthKnot{1.00, 400.0}
    };
    route.routeCurves.push_back(bezier);

    auto vehicle = params();
    vehicle.maxCombatSpeed = 250.0f;
    vehicle.maxCruiseSpeed = 250.0f;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {200.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            32,
            20.0
        ),
        "Bezier turn-preview route rejected"
    );

    Agent approaching = initial;
    approaching.positionMapMeters = {850.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        approaching,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "Bezier turn-preview update invalid");
    require(
        output.distanceToTurnMeters > 150.0,
        "Bezier preview ignored interior curvature after a zero-curvature endpoint"
    );
    require(
        output.turnSpeedCeilingMps > 0.0 &&
        output.turnSpeedCeilingMps < 200.0,
        "Bezier interior curvature produced no dynamic speed ceiling"
    );
}

void testExactCurveBoundaryActivatesAtAuthoredEntry()
{
    using game::navigation::planner::RouteCurveKind;
    using game::navigation::planner::RouteCurveSegment;
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 120.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 120.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 120.0},
        RouteGate{{1100.0, 0.0, 5.0}, {0.995, 0.0, 0.1}, 80.0},
        RouteGate{{1200.0, 0.0, 20.0}, {0.98, 0.0, 0.2}, 80.0},
        RouteGate{{1300.0, 0.0, 45.0}, {0.95, 0.0, 0.31}, 0.0}
    };
    route.gates = route.executionGates;

    RouteCurveSegment line;
    line.kind = RouteCurveKind::Line;
    line.startProgressMeters = 0.0;
    line.endProgressMeters = 1000.0;
    line.maxSpeedMps = 120.0;
    line.startMeters = {0.0, 0.0, 0.0};
    line.endMeters = {1000.0, 0.0, 0.0};
    line.startForward = line.endForward = {1.0, 0.0, 0.0};
    route.routeCurves.push_back(line);

    RouteCurveSegment arc;
    arc.kind = RouteCurveKind::CircularArc;
    arc.startProgressMeters = 1000.0;
    arc.endProgressMeters =
        1000.0 + 500.0 * 0.6435011087932844;
    arc.maxSpeedMps = 80.0;
    arc.startMeters = {1000.0, 0.0, 0.0};
    arc.arcCenterMeters = {1000.0, 0.0, 500.0};
    arc.arcNormal = {0.0, -1.0, 0.0};
    arc.arcRadiusMeters = 500.0;
    arc.arcSweepRadians = 0.6435011087932844;
    arc.endMeters = arc.positionAtParameter(1.0);
    arc.startForward = {1.0, 0.0, 0.0};
    arc.endForward = arc.tangentAtProgress(arc.endProgressMeters);
    route.routeCurves.push_back(arc);

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {120.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            params(),
            1000.0,
            24,
            20.0
        ),
        "exact curve-boundary route rejected"
    );

    Agent atArcEntry = initial;
    atArcEntry.positionMapMeters = {1000.5, 0.0, 0.001};
    atArcEntry.velocityMapMetersPerSecond = {80.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        atArcEntry,
        game::navigation::LocalFlightControlLaw::Assisted,
        params(),
        1000.0,
        0.02
    );

    require(output.valid, "exact curve-boundary update invalid");
    require(
        output.routeCurveIndex == 1,
        "follower stayed on previous straight after authored arc entry"
    );
    require(
        output.routeCurvaturePerMeter > 1.0e-6,
        "authored arc entry did not activate curvature"
    );
}

void testTerminalBrakingIncludesControllerResponseMargin()
{
    using game::navigation::planner::RouteCurveKind;
    using game::navigation::planner::RouteCurveSegment;
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;
    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 288.0},
        RouteGate{{2000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 288.0},
        RouteGate{{3000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    route.gates = route.executionGates;

    RouteCurveSegment line;
    line.kind = RouteCurveKind::Line;
    line.startProgressMeters = 0.0;
    line.endProgressMeters = 3000.0;
    line.maxSpeedMps = 288.0;
    line.startMeters = {0.0, 0.0, 0.0};
    line.endMeters = {3000.0, 0.0, 0.0};
    line.startForward = line.endForward = {1.0, 0.0, 0.0};
    route.routeCurves.push_back(line);

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {288.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    auto vehicle = params();
    vehicle.maxCombatSpeed = 400.0f;
    vehicle.maxCruiseSpeed = 400.0f;
    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            25,
            20.0,
            {0.0, 1.0, 0.0},
            true
        ),
        "terminal-margin route rejected"
    );

    Agent nearStop = initial;
    nearStop.positionMapMeters = {2200.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        nearStop,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );
    require(output.valid, "terminal-margin update invalid");

    const double braking =
        std::max(
            0.1,
            static_cast<double>(
                game::ship::reverseMainAccelerationLimitMps2(vehicle)
            )
        );
    const double idealBoundary =
        std::sqrt(2.0 * braking * 800.0);

    require(
        output.targetSpeedMps + 1.0 < idealBoundary,
        "terminal target still rides the ideal no-response braking boundary"
    );
    require(
        output.terminalBrakeActive,
        "terminal full brake did not engage at measured stopping envelope"
    );
    require(
        output.requiredTerminalStopDistanceMeters >= 800.0,
        "terminal stopping envelope did not include response reserve"
    );
    require(
        output.brakeAttitudeLockActive,
        "terminal braking did not lock hull to final corridor attitude"
    );
    require(
        !output.terminalAttitudeCaptureActive,
        "terminal attitude capture started before translational stop"
    );
    require(
        std::hypot(
            output.control.pitchInput,
            output.control.yawInput
        ) < 0.05,
        "terminal braking pitched/yawed an already aligned hull"
    );
}

void testTerminalHoldKeepsStrongAttitudeCaptureForLargeError()
{
    auto route = plan();
    auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            26,
            20.0,
            {0.0, 1.0, 0.0},
            true
        ),
        "large-error terminal hold route rejected"
    );

    const auto& final = state.continuousSamples.back();
    Agent stoppedWrong = initial;
    stoppedWrong.positionMapMeters = final.positionMapMeters;
    stoppedWrong.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};

    // Deliberately rotate about up by roughly 45 degrees from final forward.
    const glm::dvec3 f = glm::normalize(final.forwardMap);
    const glm::dvec3 u = glm::normalize(final.upMap);
    const glm::dvec3 r = glm::normalize(glm::cross(f, u));
    stoppedWrong.forwardMap =
        glm::normalize(f * 0.7071067811865476 + r * 0.7071067811865476);
    stoppedWrong.upMap = u;
    stoppedWrong.rightMap =
        glm::normalize(
            glm::cross(stoppedWrong.forwardMap, stoppedWrong.upMap)
        );

    const auto output = Autopilot::update(
        state,
        stoppedWrong,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );
    require(output.valid, "large-error terminal hold invalid");
    require(output.terminalHold, "large-error final pose did not stay in HOLD");

    const double commandMagnitude =
        std::sqrt(
            output.control.pitchInput * output.control.pitchInput +
            output.control.yawInput * output.control.yawInput +
            output.control.rollInput * output.control.rollInput
        );
    require(
        commandMagnitude > 0.20,
        "terminal HOLD nearly stopped rotating despite large attitude error"
    );
}

void testContinuousProgramCrossesStoragePages()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;

    constexpr double StraightEndMeters = 900.0;
    constexpr double RadiusMeters = 250.0;
    constexpr int ArcSteps = 24;
    constexpr double StraightSpeedMps = 100.0;
    constexpr double TurnSpeedMps = 25.0;
    constexpr double HalfPi =
        1.5707963267948966192313216916398;

    route.executionGates.push_back(
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, StraightSpeedMps}
    );
    route.executionGates.push_back(
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, StraightSpeedMps}
    );
    route.executionGates.push_back(
        RouteGate{{StraightEndMeters, 0.0, 0.0}, {1.0, 0.0, 0.0}, TurnSpeedMps}
    );

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
        gate.speedMps = TurnSpeedMps;
        route.executionGates.push_back(gate);
    }
    route.executionGates.back().speedMps = 0.0;
    route.gates = route.executionGates;

    auto vehicle = params();
    vehicle.maxCombatSpeed = 120.0f;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond =
        {StraightSpeedMps, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            13,
            25.0
        ),
        "client autopilot rejected multi-page route"
    );
    require(
        state.programs.size() > 1,
        "continuous-program test did not span storage pages"
    );
    require(
        state.continuousSamples.size() >
            game::navigation::AcceptedManeuverProgram::kMaxSamples,
        "client did not assemble pages into one continuous spatial program"
    );

    // Builder pages deliberately overlap one boundary sample. The execution
    // program must remove that storage duplication: every adjacent continuous
    // sample must make positive spatial progress.
    for (std::size_t i = 1;
         i < state.continuousProgressMeters.size();
         ++i)
    {
        require(
            state.continuousProgressMeters[i] >
                state.continuousProgressMeters[i - 1],
            "continuous program retained a duplicate page-boundary sample"
        );
    }

    // Move to a point beyond the first storage page. The command target must
    // come from the continuous program and progress monotonically rather than
    // resetting because a fixed-capacity page changed.
    const std::size_t probe =
        game::navigation::AcceptedManeuverProgram::kMaxSamples + 2;
    require(
        probe < state.continuousSamples.size(),
        "continuous-program route too short for boundary probe"
    );

    Agent beyondBoundary = initial;
    beyondBoundary.positionMapMeters =
        state.continuousSamples[probe].positionMapMeters;
    beyondBoundary.velocityMapMetersPerSecond =
        state.continuousSamples[probe].velocityMapMetersPerSecond;
    beyondBoundary.forwardMap =
        state.continuousSamples[probe].forwardMap;
    beyondBoundary.rightMap =
        state.continuousSamples[probe].rightMap;
    beyondBoundary.upMap =
        state.continuousSamples[probe].upMap;

    const auto output = Autopilot::update(
        state,
        beyondBoundary,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "continuous program emitted invalid output");
    require(
        state.currentContinuousSegment >=
            game::navigation::AcceptedManeuverProgram::kMaxSamples,
        "continuous spatial progress reset at storage page boundary"
    );

    const double expectedSpeed =
        glm::length(
            state.continuousSamples[
                state.currentContinuousSegment
            ].velocityMapMetersPerSecond
        );
    require(
        std::abs(output.targetSpeedMps - expectedSpeed) < 5.0,
        "storage page transition changed the authored speed target"
    );
}

void testClientTrajectoryPreservesPlannerTurnSpeedConstraint()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;
    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 80.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 8.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 80.0},
        RouteGate{{1500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    route.gates = route.executionGates;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            params(),
            1000.0,
            12,
            25.0
        ),
        "client autopilot rejected speed-profile route"
    );

    double nearestDistance = 1.0e100;
    double nearestSpeed = 1.0e100;
    for (const auto& program : state.programs)
    {
        for (std::size_t i = 0; i < program.sampleCount; ++i)
        {
            const auto& sample = program.samples[i];
            const double distance =
                std::abs(sample.positionMapMeters.x - 500.0);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestSpeed =
                    glm::length(sample.velocityMapMetersPerSecond);
            }
        }
    }

    require(
        nearestDistance < 5.0,
        "accepted trajectory did not sample planner turn-speed station"
    );
    require(
        nearestSpeed <= 8.5,
        "planner turn-speed constraint was lost before client follower"
    );
}

void testInitialOverspeedBrakesWithoutRejectingRoute()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;
    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 40.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 40.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 40.0},
        RouteGate{{1500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    route.gates = route.executionGates;

    auto vehicle = params();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {100.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            18,
            25.0
        ),
        "initial overspeed incorrectly rejected valid route"
    );

    const auto output = Autopilot::update(
        state,
        initial,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "initial overspeed produced invalid execution");
    require(
        output.control.targetSpeedRate < -1.0e-6f,
        "initial overspeed did not command braking"
    );
    require(
        state.currentContinuousSegment == 0,
        "initial overspeed changed route progress instead of speed control"
    );
}

void testCheckpointReanchorsFutureSpeedFromMeasuredState()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;
    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 80.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 80.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 80.0},
        RouteGate{{1500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    route.gates = route.executionGates;

    auto vehicle = params();
    vehicle.forwardMainEngineAccelerationMps2 = 4.0f;
    vehicle.reverseMainEngineAccelerationMps2 = 8.0f;
    vehicle.throttleAccel = 4.0f;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {80.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            15,
            25.0
        ),
        "client autopilot rejected checkpoint-reanchor route"
    );

    const auto nominalBefore = state.nominalSpeedProfileMps;
    require(
        state.checkpointProgressMeters.size() >= 3,
        "checkpoint route did not produce visual checkpoint progress"
    );

    Agent slowAtCheckpoint = initial;
    slowAtCheckpoint.positionMapMeters = {505.0, 0.0, 0.0};
    slowAtCheckpoint.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        slowAtCheckpoint,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "checkpoint re-anchor emitted invalid output");
    require(
        state.nextCheckpointIndex >= 2,
        "crossing visual checkpoint did not trigger suffix re-anchor"
    );

    bool loweredFutureSpeed = false;
    for (std::size_t i = state.currentContinuousSegment + 1;
         i < state.runtimeSpeedProfileMps.size();
         ++i)
    {
        if (state.runtimeSpeedProfileMps[i] + 1.0e-6 <
            nominalBefore[i])
        {
            loweredFutureSpeed = true;
            break;
        }
    }
    require(
        loweredFutureSpeed,
        "measured slow checkpoint state did not lower unreachable future speed"
    );
}

void testCheckpointReanchorPreservesFutureBrakingConstraint()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;
    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 100.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 100.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 20.0},
        RouteGate{{1500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    route.gates = route.executionGates;

    auto vehicle = params();
    vehicle.forwardMainEngineAccelerationMps2 = 40.0f;
    vehicle.reverseMainEngineAccelerationMps2 = 4.0f;
    vehicle.throttleAccel = 40.0f;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {100.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            16,
            25.0
        ),
        "client autopilot rejected braking-checkpoint route"
    );

    Agent fastAtCheckpoint = initial;
    fastAtCheckpoint.positionMapMeters = {505.0, 0.0, 0.0};
    fastAtCheckpoint.velocityMapMetersPerSecond = {100.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        fastAtCheckpoint,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );
    require(output.valid, "braking checkpoint emitted invalid output");
    require(
        output.control.targetSpeedRate < -1.0e-6f,
        "overspeed checkpoint did not command braking"
    );

    double nearestDistance = 1.0e100;
    double constrainedSpeed = 1.0e100;
    for (std::size_t i = 0;
         i < state.continuousProgressMeters.size();
         ++i)
    {
        const double d =
            std::abs(
                state.continuousSamples[i].positionMapMeters.x -
                1000.0
            );
        if (d < nearestDistance)
        {
            nearestDistance = d;
            constrainedSpeed = state.runtimeSpeedProfileMps[i];
        }
    }

    require(
        nearestDistance < 5.0,
        "runtime profile lost future braking station"
    );
    require(
        constrainedSpeed < 100.0,
        "checkpoint re-anchor failed to reduce overspeed toward future limit"
    );
}

void testMissedGateAdvancesToFutureRouteWithoutReturn()
{
    using game::navigation::planner::RouteGate;

    game::navigation::planner::RoutePlan route;
    route.disposition =
        game::navigation::planner::RoutePlanDisposition::Ready;
    route.failureCode =
        game::navigation::planner::RoutePlanFailureCode::None;
    route.executionGates = {
        RouteGate{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 60.0},
        RouteGate{{500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 60.0},
        RouteGate{{1000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 60.0},
        RouteGate{{1500.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 60.0},
        RouteGate{{2000.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.0}
    };
    route.gates = route.executionGates;

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {60.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    Autopilot::State state;
    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            params(),
            1000.0,
            17,
            25.0
        ),
        "client autopilot rejected missed-gate route"
    );

    Agent pastTwoGates = initial;
    pastTwoGates.positionMapMeters = {1250.0, 0.0, 0.0};

    const auto first = Autopilot::update(
        state,
        pastTwoGates,
        game::navigation::LocalFlightControlLaw::Assisted,
        params(),
        1000.0,
        0.02
    );
    require(first.valid, "missed-gate recovery emitted invalid output");

    const auto progressed = state.currentContinuousSegment;
    require(
        progressed > 0,
        "autopilot stayed attached to an already missed gate"
    );
    require(
        state.nextCheckpointIndex >= 3,
        "autopilot did not skip already crossed checkpoints"
    );

    Agent slightlyBack = pastTwoGates;
    slightlyBack.positionMapMeters = {1200.0, 0.0, 0.0};

    const auto second = Autopilot::update(
        state,
        slightlyBack,
        game::navigation::LocalFlightControlLaw::Assisted,
        params(),
        1000.0,
        0.02
    );
    require(second.valid, "post-miss monotonic update invalid");
    require(
        state.currentContinuousSegment >= progressed,
        "autopilot returned to an already missed gate"
    );
}

void testAssistedSpeedControlUsesTrueSpeedDuringTurn()
{
    using Pilot =
        game::navigation::autopilot::PredictivePilot;

    Pilot::State state;
    Pilot::Request request;
    request.law =
        game::navigation::LocalFlightControlLaw::Assisted;

    // Nose already turned toward +X, but velocity still lags 90 degrees along
    // +Z. The craft is physically moving at 40 m/s while the route wants 20.
    // A projection-based controller sees zero forward speed and accelerates;
    // correct scalar-speed control must brake.
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.desiredForwardMap = request.forwardMap;
    request.desiredUpMap = request.upMap;
    request.actualVelocityMapMps = {0.0, 0.0, 40.0};
    request.desiredVelocityMapMps = {20.0, 0.0, 0.0};
    request.deltaSeconds = 0.02;

    const auto control =
        Pilot::make(request, params(), state);

    require(
        control.targetSpeedRate < -1.0e-6f,
        "Assisted pilot accelerated because velocity lagged nose direction"
    );
}

void testPredictivePilotBrakesAngularRateBeforeTarget()
{
    using Pilot =
        game::navigation::autopilot::PredictivePilot;

    Pilot::State state;
    Pilot::Request request;
    request.law =
        game::navigation::LocalFlightControlLaw::Assisted;
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.desiredForwardMap =
        glm::normalize(glm::dvec3(1.0, 0.0, 0.08));
    request.desiredUpMap = {0.0, 1.0, 0.0};
    request.deltaSeconds = 0.02;

    const auto accelerate =
        Pilot::make(request, params(), state);
    require(
        std::abs(accelerate.yawInput) > 1.0e-6,
        "predictive pilot did not begin yaw toward target"
    );

    const double turnSign =
        accelerate.yawInput > 0.0f ? 1.0 : -1.0;

    // The hull is still on the same side of the target, but its angular rate
    // is already too high to stop inside the remaining angle. A correct
    // braking-envelope controller must counter-steer NOW, before overshoot.
    request.yawRateRadPerSec = turnSign * 1.5;
    const auto brake =
        Pilot::make(request, params(), state);

    require(
        static_cast<double>(brake.yawInput) * turnSign < -1.0e-6,
        "predictive pilot waited for angular overshoot before braking"
    );
}

void testPredictivePilotCapturesTurnsWithoutOvershoot()
{
    using Pilot =
        game::navigation::autopilot::PredictivePilot;

    const auto vehicle = params();
    constexpr double Dt = 0.02;
    constexpr double Pi =
        3.1415926535897932384626433832795;

    const auto runTurn =
        [&](double targetDegrees)
        {
            Pilot::State state;
            double angle = 0.0;
            double yawRate = 0.0;
            double holdSeconds = 0.0;
            double maximumAngle = 0.0;
            double minimumAngle = 0.0;

            for (int step = 0; step < 400; ++step)
            {
                const double c = std::cos(angle);
                const double sn = std::sin(angle);

                // Positive simulated yaw follows ShipController's +Y rotation:
                // +X turns toward -Z.
                const glm::dvec3 forward(c, 0.0, -sn);
                const glm::dvec3 right(sn, 0.0, c);
                const glm::dvec3 up(0.0, 1.0, 0.0);

                const double target =
                    targetDegrees * Pi / 180.0;
                const glm::dvec3 desiredForward(
                    std::cos(target),
                    0.0,
                    -std::sin(target)
                );

                Pilot::Request request;
                request.law =
                    game::navigation::LocalFlightControlLaw::Assisted;
                request.forwardMap = forward;
                request.rightMap = right;
                request.upMap = up;
                request.desiredForwardMap = desiredForward;
                request.desiredUpMap = up;
                request.yawRateRadPerSec = yawRate;
                request.deltaSeconds = Dt;

                const auto control =
                    Pilot::make(request, vehicle, state);

                // Reproduce the ordinary-keyboard angular path used by the
                // live client autopilot: held input ramps from 18% to full
                // authority over 0.25 s; neutral Assisted input damps rate.
                double acceleration = 0.0;
                if (std::abs(control.yawInput) < 0.001f)
                {
                    holdSeconds = 0.0;
                    acceleration =
                        -yawRate *
                        static_cast<double>(vehicle.angularDamping);
                }
                else
                {
                    const double authorityScale =
                        0.18 +
                        (1.0 - 0.18) *
                        std::clamp(holdSeconds / 0.25, 0.0, 1.0);
                    acceleration =
                        static_cast<double>(control.yawInput) *
                        authorityScale *
                        game::ship::
                            angularAccelerationLimitRadPerSec2(vehicle);
                    holdSeconds =
                        std::min(0.25, holdSeconds + Dt);
                }

                yawRate += acceleration * Dt;
                yawRate = std::clamp(
                    yawRate,
                    -static_cast<double>(vehicle.maxYawRate),
                    static_cast<double>(vehicle.maxYawRate)
                );
                angle += yawRate * Dt;

                maximumAngle = std::max(maximumAngle, angle);
                minimumAngle = std::min(minimumAngle, angle);
            }

            const double finalDegrees = angle * 180.0 / Pi;
            const double overshootDegrees =
                targetDegrees >= 0.0
                    ? maximumAngle * 180.0 / Pi - targetDegrees
                    : targetDegrees - minimumAngle * 180.0 / Pi;

            require(
                std::abs(finalDegrees - targetDegrees) < 0.25,
                "predictive pilot failed to settle on commanded turn angle"
            );
            require(
                overshootDegrees < 0.25,
                "predictive pilot overshot commanded turn angle"
            );
        };

    runTurn(5.0);
    runTurn(15.0);
    runTurn(45.0);
    runTurn(90.0);
    runTurn(-45.0);
    runTurn(-90.0);
}

void testClientAutopilotUsesDockUpReferenceForRoll()
{
    Autopilot::State state;
    const auto vehicle = params();
    const auto route = plan();

    Agent initial;
    initial.positionMapMeters = {0.0, 0.0, 0.0};
    initial.velocityMapMetersPerSecond = {20.0, 0.0, 0.0};
    initial.forwardMap = {1.0, 0.0, 0.0};
    initial.rightMap = {0.0, 0.0, 1.0};
    initial.upMap = {0.0, 1.0, 0.0};

    const glm::dvec3 dockUp(0.0, 0.0, 1.0);

    require(
        Autopilot::start(
            state,
            route,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            vehicle,
            1000.0,
            11,
            25.0,
            dockUp
        ),
        "client autopilot rejected explicit dock up reference"
    );

    require(
        !state.programs.empty(),
        "dock-up test produced no accepted program"
    );

    const auto& finalSample =
        state.programs.back().samples[
            state.programs.back().sampleCount - 1
        ];
    require(
        glm::dot(
            glm::normalize(finalSample.upMap),
            glm::normalize(dockUp)
        ) > 0.95,
        "accepted trajectory discarded dock up reference"
    );

    Agent mid = initial;
    mid.positionMapMeters = {100.0, 0.0, 0.0};

    const auto output = Autopilot::update(
        state,
        mid,
        game::navigation::LocalFlightControlLaw::Assisted,
        vehicle,
        1000.0,
        0.02
    );

    require(output.valid, "dock-up route emitted invalid output");
    require(
        std::abs(output.control.rollInput) > 1.0e-6,
        "client autopilot ignored dock bottom/up roll reference"
    );
}

void testHullAttitudeControlIsNotSlowedByCourseLag()
{
    using Pilot =
        game::navigation::autopilot::PredictivePilot;

    Pilot::State state;
    const auto vehicle = params();

    constexpr double FiveDegreesRad =
        5.0 * 3.1415926535897932384626433832795 / 180.0;

    Pilot::Request request;
    request.law =
        game::navigation::LocalFlightControlLaw::Assisted;
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.desiredForwardMap = {
        std::cos(FiveDegreesRad),
        0.0,
        -std::sin(FiveDegreesRad)
    };
    request.desiredUpMap = {0.0, 1.0, 0.0};

    // Deliberately huge course-response lag. This describes how long
    // velocity takes to follow the nose, NOT how slowly the hull may rotate.
    request.angularTrackingResponseSeconds = 5.0;
    request.deltaSeconds = 0.02;

    const auto control =
        Pilot::make(request, vehicle, state);

    require(
        std::abs(control.yawInput) > 0.5f,
        "Assisted course-response lag incorrectly throttled hull attitude control"
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
        testCourseLagLeadsIntoUpcomingTurn();
        testContinuousProgramCorrectsCrossTrackError();
        testCourseLagPredictionIgnoresCentimetresButCorrectsFutureMiss();
        testParallelOffsetActivelyCapturesCorridorCenter();
        testCruiseRollAlignsToDockBottomReference();
        testCourseLeadPreservesCurvedTunnelInsteadOfChordCutting();
        testApproachBrakesBeforeDynamicTurnLimit();
        testTurnPreviewCrossesIntermediateStraight();
        testBezierInteriorCurvatureParticipatesInPreview();
        testExactCurveBoundaryActivatesAtAuthoredEntry();
        testTerminalBrakingIncludesControllerResponseMargin();
        testTerminalHoldKeepsStrongAttitudeCaptureForLargeError();
        testTerminalFrameHoldsStoppedAndAligned();
        testContinuousProgramCrossesStoragePages();
        testClientTrajectoryPreservesPlannerTurnSpeedConstraint();
        testInitialOverspeedBrakesWithoutRejectingRoute();
        testCheckpointReanchorsFutureSpeedFromMeasuredState();
        testCheckpointReanchorPreservesFutureBrakingConstraint();
        testMissedGateAdvancesToFutureRouteWithoutReturn();
        testAssistedSpeedControlUsesTrueSpeedDuringTurn();
        testPredictivePilotBrakesAngularRateBeforeTarget();
        testPredictivePilotCapturesTurnsWithoutOvershoot();
        testClientAutopilotUsesDockUpReferenceForRoll();
        testHullAttitudeControlIsNotSlowedByCourseLag();
        testClientStabilizerUsesOrdinaryControls();
        std::cout
            << "CLIENT ROUTE AUTOPILOT TESTS: PASS\n"
            << " - RoutePlan is adapted to SpatialCorridor on the client\n"
            << " - execution emits only ordinary ShipControlState inputs\n"
            << " - stopped spatial origin accelerates from adjacent trajectory state\n"
            << " - spatial turn drives velocity and nose from one centerline source\n"
            << " - turn lead distance follows measured nose-to-course lag\n"
            << " - parallel offset actively captures corridor center\n"
            << " - cruise roll aligns to dock-bottom reference\n"
            << " - course lead preserves curved tunnel instead of cutting a chord\n"
            << " - approach brakes before the Assisted dynamic turn limit\n"
            << " - turn preview crosses intermediate straight primitives\n"
            << " - Bezier interior curvature participates in turn preview\n"
            << " - exact curve boundary activates at authored entry\n"
            << " - terminal braking includes controller response distance\n"
            << " - terminal HOLD keeps strong attitude capture for large errors\n"
            << " - meter-scale cross-track error is corrected by nose/course dynamics\n"
            << " - centimetres are ignored while predicted future misses are corrected\n"
            << " - final guidance frame is a stopped alignment HOLD\n"
            << " - accepted route executes as one continuous program across storage pages\n"
            << " - planner turn-speed constraints survive into accepted trajectory\n"
            << " - initial overspeed stays on route and commands braking\n"
            << " - checkpoint re-anchor lowers unreachable future speed\n"
            << " - checkpoint overspeed stays on route and brakes toward limits\n"
            << " - missed gates advance monotonically to future route\n"
            << " - Assisted scalar speed does not rise when velocity lags nose\n"
            << " - angular controller brakes before attitude overshoot\n"
            << " - 5/15/45/90 degree turns settle without overshoot\n"
            << " - docking up reference drives roll orientation\n"
            << " - hull attitude control is independent of Assisted course lag\n"
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
