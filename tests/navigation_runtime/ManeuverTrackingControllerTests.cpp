#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/TrajectoryFollower.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Tracker = game::navigation::ManeuverTrackingController;
using Follower = game::navigation::TrajectoryFollower;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Program baseProgram()
{
    Program program;
    program.valid = true;
    program.revision = 1001;
    program.objectiveRevision = 77;
    program.family = Program::ManeuverFamily::LeadRotateMainBurn;
    program.acceptedAtUniverseTimeSeconds = 10.0;
    program.validUntilUniverseTimeSeconds = 12.0;
    program.sampleCount = 2;

    auto& a = program.samples[0];
    a.timeOffsetSeconds = 0.0;
    a.positionMapMeters = {0.0, 0.0, 0.0};
    a.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};
    a.linearAccelerationFeedForwardMapMps2 = {3.0, 1.0, -2.0};
    a.forwardMap = {1.0, 0.0, 0.0};
    a.rightMap = {0.0, 0.0, 1.0};
    a.upMap = {0.0, 1.0, 0.0};
    a.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.5};
    a.angularAccelerationFeedForwardMapRadPerSec2 = {0.2, 0.3, 0.4};

    auto& b = program.samples[1];
    b.timeOffsetSeconds = 1.0;
    b.positionMapMeters = {10.0, 0.0, 0.0};
    b.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};
    b.linearAccelerationFeedForwardMapMps2 = {1.0, 2.0, -1.0};
    b.forwardMap = {1.0, 0.0, 0.0};
    b.rightMap = {0.0, 0.0, 1.0};
    b.upMap = {0.0, 1.0, 0.0};
    b.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.5};
    b.angularAccelerationFeedForwardMapRadPerSec2 = {0.1, 0.2, 0.3};

    program.tracking.positionErrorMeters = 20.0;
    program.tracking.linearVelocityErrorMps = 10.0;
    program.tracking.forwardAngleErrorRad = 0.5;
    program.tracking.angularVelocityErrorRadPerSec = 2.0;
    program.tracking.linearFeedbackReserveMps2 = 2.0;
    program.tracking.angularFeedbackReserveRadPerSec2 = 0.75;

    program.terminalTolerance.positionMeters = 0.1;
    program.terminalTolerance.linearVelocityMps = 0.1;
    program.terminalTolerance.forwardAngleRad = 0.01;
    program.terminalTolerance.angularVelocityRadPerSec = 0.1;
    return program;
}

Tracker::AgentState exactAgentFor(
    const Program::ReferenceSample& reference
)
{
    Tracker::AgentState agent;
    agent.positionMapMeters = reference.positionMapMeters;
    agent.velocityMapMetersPerSecond =
        reference.velocityMapMetersPerSecond;
    agent.forwardMap = reference.forwardMap;
    agent.rightMap = reference.rightMap;
    agent.upMap = reference.upMap;

    // With basis forward=(+X), right=(+Z), up=(+Y), the current angular
    // velocity is right*pitch + up*yaw + forward*roll.
    agent.pitchRateRadPerSec =
        glm::dot(reference.angularVelocityMapRadPerSecond,
                 reference.rightMap);
    agent.yawRateRadPerSec =
        glm::dot(reference.angularVelocityMapRadPerSecond,
                 reference.upMap);
    agent.rollRateRadPerSec =
        glm::dot(reference.angularVelocityMapRadPerSecond,
                 reference.forwardMap);
    return agent;
}

Follower::AgentState followerAgentFor(
    const Program::ReferenceSample& reference
)
{
    Follower::AgentState agent;
    agent.positionMapMeters = reference.positionMapMeters;
    agent.velocityMapMetersPerSecond =
        reference.velocityMapMetersPerSecond;
    agent.forwardMap = reference.forwardMap;
    agent.rightMap = reference.rightMap;
    agent.upMap = reference.upMap;
    agent.pitchRateRadPerSec =
        glm::dot(reference.angularVelocityMapRadPerSecond,
                 reference.rightMap);
    agent.yawRateRadPerSec =
        glm::dot(reference.angularVelocityMapRadPerSecond,
                 reference.upMap);
    agent.rollRateRadPerSec =
        glm::dot(reference.angularVelocityMapRadPerSecond,
                 reference.forwardMap);
    return agent;
}

void testDefaultAttitudeLoopIsNotUnderdamped()
{
    const Tracker::Policy policy;
    const double criticalDamping =
        2.0 * std::sqrt(policy.attitudeGainPerSecond2);

    require(
        policy.angularVelocityGainPerSecond >= criticalDamping,
        "default attitude loop is underdamped and can oscillate around reference"
    );
}

void testAntiparallelAttitudeStillProducesCorrection()
{
    Program program = baseProgram();
    auto agent = exactAgentFor(program.samples[0]);

    // Same up axis, but forward/right are exactly antiparallel: 180 deg yaw.
    // The retired cross-product small-angle error evaluates to zero here.
    agent.forwardMap = {-1.0, 0.0, 0.0};
    agent.rightMap = {0.0, 0.0, -1.0};
    agent.upMap = {0.0, 1.0, 0.0};
    agent.pitchRateRadPerSec = 0.0;
    agent.yawRateRadPerSec = 0.0;
    agent.rollRateRadPerSec = 0.0;

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    require(
        result.status == Tracker::Status::EnvelopeExceeded,
        "180-degree attitude error must remain observable"
    );
    requireNear(
        glm::length(result.angularFeedbackMapRadPerSec2),
        program.tracking.angularFeedbackReserveRadPerSec2,
        1.0e-12,
        "180-degree attitude error failed to command bounded correction"
    );
}

void testZeroErrorPreservesAcceptedFeedForwardExactly()
{
    const Program program = baseProgram();
    const auto& reference = program.samples[0];
    const auto agent = exactAgentFor(reference);

    const auto result =
        Tracker::track(program, reference, agent, Tracker::Policy {});

    require(result.status == Tracker::Status::Tracking,
            "exact reference state must be inside tracking envelope");
    requireNear(glm::length(result.linearFeedbackMapMps2), 0.0, 0.0,
                "zero tracking error created linear feedback");
    requireNear(glm::length(result.angularFeedbackMapRadPerSec2), 0.0, 0.0,
                "zero tracking error created angular feedback");

    requireNear(
        result.intent.idealLinearAccelerationLocalMps2.x,
        reference.linearAccelerationFeedForwardMapMps2.x,
        0.0,
        "B10 changed accepted linear feed-forward at zero error"
    );
    requireNear(
        result.intent.idealLinearAccelerationLocalMps2.y,
        reference.linearAccelerationFeedForwardMapMps2.y,
        0.0,
        "B10 changed accepted linear feed-forward at zero error"
    );
    requireNear(
        result.intent.idealAngularAccelerationLocalRadPerSec2.z,
        reference.angularAccelerationFeedForwardMapRadPerSec2.z,
        0.0,
        "B10 changed accepted angular feed-forward at zero error"
    );
}

void testFeedbackCannotExceedReservedAuthority()
{
    Program program = baseProgram();
    auto agent = exactAgentFor(program.samples[0]);

    agent.positionMapMeters = {-100.0, 50.0, 0.0};
    agent.velocityMapMetersPerSecond = {-50.0, 0.0, 0.0};
    agent.forwardMap = {0.0, 0.0, -1.0};
    agent.rightMap = {1.0, 0.0, 0.0};
    agent.upMap = {0.0, 1.0, 0.0};
    agent.pitchRateRadPerSec = 10.0;
    agent.yawRateRadPerSec = -10.0;
    agent.rollRateRadPerSec = 5.0;

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    require(result.status == Tracker::Status::EnvelopeExceeded,
            "large tracking error must be reported outside envelope");
    requireNear(
        glm::length(result.linearFeedbackMapMps2),
        program.tracking.linearFeedbackReserveMps2,
        1.0e-12,
        "linear feedback exceeded or failed to consume the reserved authority clamp"
    );
    requireNear(
        glm::length(result.angularFeedbackMapRadPerSec2),
        program.tracking.angularFeedbackReserveRadPerSec2,
        1.0e-12,
        "angular feedback exceeded or failed to consume the reserved authority clamp"
    );
}

void testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()
{
    Program program = baseProgram();
    program.family = Program::ManeuverFamily::FreeTransit;

    auto agent = exactAgentFor(program.samples[0]);

    // Force a cross-track envelope violation while the sampled moving
    // reference itself still carries both translational feed-forward and a
    // non-zero angular velocity. This reproduces the viewer failure where
    // reference time was held on a curved trajectory sample.
    agent.positionMapMeters = {0.0, 30.0, 0.0};
    agent.velocityMapMetersPerSecond =
        program.samples[0].velocityMapMetersPerSecond;

    // The held pose is already correct, but the physical hull is still
    // rotating. Reacquisition must damp that spin instead of chasing the
    // moving sample's frozen +0.5 rad/s angular-velocity derivative.
    agent.pitchRateRadPerSec = 1.0;
    agent.yawRateRadPerSec = 0.0;
    agent.rollRateRadPerSec = 0.0;

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    require(
        result.status == Tracker::Status::EnvelopeExceeded,
        "reacquisition fixture did not leave the tracking envelope"
    );
    require(!result.angularCorrectionOnly,
            "cross-track departure was incorrectly treated as an angular-only correction");

    requireNear(
        glm::length(
            result.intent.idealLinearAccelerationLocalMps2 -
            result.linearFeedbackMapMps2
        ),
        0.0,
        1.0e-12,
        "reacquisition replayed frozen linear feed-forward"
    );
    requireNear(
        glm::length(
            result.intent.idealAngularAccelerationLocalRadPerSec2 -
            result.angularFeedbackMapRadPerSec2
        ),
        0.0,
        1.0e-12,
        "reacquisition replayed frozen angular feed-forward"
    );

    require(
        result.angularFeedbackMapRadPerSec2.z < -0.70,
        "reacquisition chased frozen reference angular velocity instead of damping hull spin"
    );
}

void testSlowDockAngularErrorKeepsMovingReference()
{
    Program program = baseProgram();
    program.tracking.angularVelocityErrorRadPerSec = 0.25;
    const auto& reference = program.samples[0];
    auto agent = exactAgentFor(reference);
    agent.rollRateRadPerSec += 0.30;

    const auto result =
        Tracker::track(program, reference, agent, Tracker::Policy {});
    require(result.status == Tracker::Status::EnvelopeExceeded &&
                result.angularCorrectionOnly,
            "isolated angular drift did not enter bounded steering correction");
    requireNear(
        glm::length(result.intent.idealLinearAccelerationLocalMps2 -
                    reference.linearAccelerationFeedForwardMapMps2 -
                    result.linearFeedbackMapMps2),
        0.0, 1.0e-12,
        "angular drift wrongly erased the accepted translation command"
    );
    requireNear(
        glm::length(result.intent.idealAngularAccelerationLocalRadPerSec2 -
                    reference.angularAccelerationFeedForwardMapRadPerSec2 -
                    result.angularFeedbackMapRadPerSec2),
        0.0, 1.0e-12,
        "angular drift wrongly erased moving-dock angular feed-forward"
    );
    require(result.angularFeedbackMapRadPerSec2.x < 0.0,
            "correction failed to reduce the excess roll rate");

    auto followerAgent = followerAgentFor(reference);
    followerAgent.rollRateRadPerSec += 0.30;
    const auto follower = Follower::follow(
        program, program.acceptedAtUniverseTimeSeconds,
        followerAgent, Tracker::Policy {}
    );
    require(follower.trackingErrorExceeded && follower.angularCorrectionOnly,
            "Follower lost the isolated angular correction status");
}

void testFollowerUsesB9ThenB10WithoutResolvingControl()
{
    const Program program = baseProgram();
    const double t = 10.5;

    Program::ReferenceSample midpoint;
    midpoint.timeOffsetSeconds = 0.5;
    midpoint.positionMapMeters = {5.0, 0.0, 0.0};
    midpoint.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};
    midpoint.linearAccelerationFeedForwardMapMps2 = {2.0, 1.5, -1.5};
    midpoint.forwardMap = {1.0, 0.0, 0.0};
    midpoint.rightMap = {0.0, 0.0, 1.0};
    midpoint.upMap = {0.0, 1.0, 0.0};
    midpoint.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.5};
    midpoint.angularAccelerationFeedForwardMapRadPerSec2 =
        {0.15, 0.25, 0.35};

    const auto agent = followerAgentFor(midpoint);

    const auto result = Follower::follow(program, t, agent, game::navigation::ManeuverTrackingController::Policy {});
    require(result.status == Follower::Status::Following,
            "mid-program follower must remain active");
    require(!result.trackingErrorExceeded,
            "exact sampled state must remain inside tracking envelope");
    require(result.intent.revision == program.objectiveRevision,
            "follower lost objective revision");
    require(result.intent.targetRevision == program.revision,
            "follower lost accepted program revision");

    requireNear(
        result.intent.idealLinearAccelerationLocalMps2.x,
        2.0,
        1.0e-12,
        "follower re-solved linear control instead of executing sampled A_ff"
    );
    requireNear(
        result.intent.idealLinearAccelerationLocalMps2.y,
        1.5,
        1.0e-12,
        "follower changed sampled linear feed-forward"
    );
    requireNear(
        result.intent.idealAngularAccelerationLocalRadPerSec2.z,
        0.35,
        1.0e-12,
        "follower re-solved angular control instead of executing sampled alpha_ff"
    );
}

void testFollowerCompletesOnlyAtTerminalState()
{
    const Program program = baseProgram();
    const auto agent = followerAgentFor(program.samples[1]);

    const auto atEnd = Follower::follow(program, 11.0, agent, game::navigation::ManeuverTrackingController::Policy {});
    require(atEnd.status == Follower::Status::Complete,
            "program must complete at its proved terminal state");

    auto offTerminal = agent;
    offTerminal.positionMapMeters.x -= 2.0;

    const auto notComplete =
        Follower::follow(program, 11.0, offTerminal, game::navigation::ManeuverTrackingController::Policy {});
    require(notComplete.status == Follower::Status::Following,
            "program completed despite missing terminal tolerance");
}

void testFreeTransitSpeedCorridorIgnoresTinyLongitudinalError()
{
    Program program = baseProgram();
    program.family = Program::ManeuverFamily::FreeTransit;
    program.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);
    program.tracking.alongTrackPositionDeadbandMeters = 2.0;
    program.tracking.alongTrackSpeedDeadbandMps = 0.5;

    auto agent = exactAgentFor(program.samples[0]);
    agent.velocityMapMetersPerSecond = {5.1, 0.0, 0.0};

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    require(result.status == Tracker::Status::Tracking,
            "tiny free-transit speed error left tracking state");
    requireNear(
        glm::length(result.linearFeedbackMapMps2),
        0.0,
        1.0e-12,
        "free-transit speed corridor still corrected 0.1 m/s overspeed"
    );
}

void testFreeTransitDeadbandDoesNotFalseTriggerEnvelope()
{
    Program program = baseProgram();
    program.family = Program::ManeuverFamily::FreeTransit;
    program.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);
    program.tracking.alongTrackPositionDeadbandMeters = 2.0;
    program.tracking.alongTrackSpeedDeadbandMps = 0.5;

    // Deliberately tighter than the raw along-track errors below. The
    // deadband means those errors are acceptable and must not report an
    // execution-envelope failure.
    program.tracking.positionErrorMeters = 0.5;
    program.tracking.linearVelocityErrorMps = 0.05;

    auto agent = exactAgentFor(program.samples[0]);
    agent.positionMapMeters = {1.0, 0.0, 0.0};
    agent.velocityMapMetersPerSecond = {5.1, 0.0, 0.0};

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    require(
        result.status == Tracker::Status::Tracking,
        "accepted free-transit longitudinal deadband falsely exceeded envelope"
    );
    requireNear(
        glm::length(result.linearFeedbackMapMps2),
        0.0,
        1.0e-12,
        "accepted free-transit deadband generated correction"
    );
}

void testFreeTransitCorridorStillCorrectsCrossTrackMotion()
{
    Program program = baseProgram();
    program.family = Program::ManeuverFamily::FreeTransit;
    program.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);
    program.tracking.alongTrackPositionDeadbandMeters = 2.0;
    program.tracking.alongTrackSpeedDeadbandMps = 0.5;

    auto agent = exactAgentFor(program.samples[0]);
    agent.positionMapMeters = {1.0, 1.0, 0.0};
    agent.velocityMapMetersPerSecond = {5.1, 0.25, 0.0};

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    require(
        std::abs(result.linearFeedbackMapMps2.y) > 0.1,
        "longitudinal corridor suppressed cross-track correction"
    );
    requireNear(
        result.linearFeedbackMapMps2.x,
        0.0,
        1.0e-12,
        "inside-corridor along-track error still produced longitudinal feedback"
    );
}

void testFreeTransitCorridorCorrectsOnlyExcessOutsideBand()
{
    Program program = baseProgram();
    program.family = Program::ManeuverFamily::FreeTransit;
    program.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);
    program.tracking.alongTrackPositionDeadbandMeters = 2.0;
    program.tracking.alongTrackSpeedDeadbandMps = 0.5;

    auto agent = exactAgentFor(program.samples[0]);
    agent.velocityMapMetersPerSecond = {6.0, 0.0, 0.0};

    const auto result =
        Tracker::track(program, program.samples[0], agent, Tracker::Policy {});

    requireNear(
        result.linearFeedbackMapMps2.x,
        -0.5,
        1.0e-12,
        "speed corridor did not subtract the allowed 0.5 m/s before correcting"
    );
}

void testFollowerDistinguishesStopFromMovingStart()
{
    Program stop = baseProgram();
    stop.samples[0].velocityMapMetersPerSecond = glm::dvec3(0.0);
    stop.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);

    auto residual = followerAgentFor(stop.samples[0]);
    residual.velocityMapMetersPerSecond = {0.03, 0.0, 0.0};

    const auto stopping = Follower::follow(
        stop,
        stop.acceptedAtUniverseTimeSeconds,
        residual,
        Tracker::Policy {}
    );
    require(
        stopping.intent.precisionTranslationOnly,
        "authored v=0 point did not request precision RCS stop"
    );

    Program moving = stop;
    moving.samples[0].velocityMapMetersPerSecond = {0.08, 0.0, 0.0};
    moving.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);

    auto launch = followerAgentFor(moving.samples[0]);
    launch.velocityMapMetersPerSecond = glm::dvec3(0.0);

    const auto starting = Follower::follow(
        moving,
        moving.acceptedAtUniverseTimeSeconds,
        launch,
        Tracker::Policy {}
    );
    require(
        !starting.intent.precisionTranslationOnly,
        "moving START with v>0 was mistaken for a STOP point"
    );
}

void testFollowerSpatialCorridorTracksPathInsteadOfClock()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::FreeTransit;

    // Nominal clock is at the end, but the craft is physically near 25% of
    // the path and one metre off-axis.
    auto agent = followerAgentFor(program.samples[0]);
    agent.positionMapMeters = {2.5, 1.0, 0.0};
    agent.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};

    const auto result = Follower::follow(
        program,
        11.0,
        agent,
        Tracker::Policy {}
    );

    require(result.status == Follower::Status::Following,
            "spatial corridor incorrectly completed by nominal time");
    require(result.spatialReference,
            "Follower did not use spatial reference for corridor mode");
    requireNear(
        result.referenceInterpolation01,
        0.25,
        1.0e-12,
        "Follower reference ran ahead of physical path progress"
    );
    requireNear(
        result.referenceSpatialDistanceMeters,
        1.0,
        1.0e-12,
        "Follower did not measure real cross-track distance to corridor"
    );
    require(
        result.crossTrackErrorMeters < 1.01,
        "Follower tracked the time-scheduled point instead of corridor projection"
    );
}

void testSpatialCorridorLaunchesFromStoppedControlPoint()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::FreeTransit;

    program.samples[0].velocityMapMetersPerSecond = glm::dvec3(0.0);
    program.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);
    program.samples[1].velocityMapMetersPerSecond =
        glm::dvec3(5.0, 0.0, 0.0);
    program.samples[1].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);

    auto agent = followerAgentFor(program.samples[0]);
    agent.velocityMapMetersPerSecond = glm::dvec3(0.0);

    const auto result = Follower::follow(
        program,
        program.acceptedAtUniverseTimeSeconds + 0.75,
        agent,
        Tracker::Policy {}
    );

    require(result.status == Follower::Status::Following,
            "stopped spatial launch did not remain active");
    require(result.spatialReference &&
                result.referenceInterpolation01 <= 1.0e-12,
            "stopped spatial launch advanced position by clock");
    requireNear(
        glm::length(result.targetVelocityMapMps),
        5.0,
        1.0e-12,
        "stopped spatial launch kept the zero-speed lower checkpoint"
    );
    require(
        result.intent.idealLinearAccelerationLocalMps2.x > 0.0,
        "stopped spatial launch produced no forward control demand"
    );
}

void testSpatialCorridorAngularRateIsSteeringNotRouteLoss()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::FreeTransit;
    program.tracking.angularVelocityErrorRadPerSec = 0.25;

    const auto& reference = program.samples[0];
    auto agent = exactAgentFor(reference);
    agent.rollRateRadPerSec += 0.55;

    const auto result =
        Tracker::track(program, reference, agent, Tracker::Policy {});

    require(
        result.status == Tracker::Status::Tracking,
        "spatial corridor treated angular-rate error as route loss"
    );
    require(
        result.angularCorrectionOnly,
        "spatial corridor did not report bounded angular correction"
    );
    requireNear(
        glm::length(
            result.intent.idealLinearAccelerationLocalMps2 -
            reference.linearAccelerationFeedForwardMapMps2 -
            result.linearFeedbackMapMps2
        ),
        0.0,
        1.0e-12,
        "spatial angular correction erased accepted translation"
    );
    require(
        result.angularFeedbackMapRadPerSec2.x < 0.0,
        "spatial angular correction failed to oppose excess spin"
    );

    auto followerAgent = followerAgentFor(reference);
    followerAgent.rollRateRadPerSec += 0.55;
    const auto follower = Follower::follow(
        program,
        program.acceptedAtUniverseTimeSeconds,
        followerAgent,
        Tracker::Policy {}
    );
    require(
        !follower.trackingErrorExceeded &&
            follower.angularCorrectionOnly,
        "Follower still rejected a spatial angular-rate-only excursion"
    );
}

void testNewtonianSpatialDriftDoesNotCountAsRouteLoss()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::PrecisionTransit;
    program.translationMode =
        Program::TranslationMode::NewtonianMainEngine;

    const auto& reference = program.samples[0];
    auto agent = exactAgentFor(reference);

    // Newtonian flight may legitimately carry velocity down the accepted
    // corridor while the hull is pointed elsewhere for drift/braking setup.
    // Cross-track P/V remain exact, so hull-course slip alone is not route
    // loss. Attitude feedback may still work toward the authored basis.
    agent.forwardMap = -reference.forwardMap;
    agent.rightMap = -reference.rightMap;
    agent.upMap = reference.upMap;

    const auto result =
        Tracker::track(program, reference, agent, Tracker::Policy {});

    require(
        result.status == Tracker::Status::Tracking,
        "Newtonian spatial drift was treated as corridor loss"
    );
    require(
        result.forwardAngleErrorRad > glm::radians(170.0),
        "Newtonian drift fixture did not create material hull/course slip"
    );
    requireNear(
        result.envelopeForwardAngleErrorRad,
        0.0,
        1.0e-12,
        "Newtonian hull/course slip leaked into spatial route-loss envelope"
    );
    requireNear(
        result.envelopePositionErrorMeters,
        0.0,
        1.0e-12,
        "Newtonian drift fixture unexpectedly left the corridor"
    );
    requireNear(
        result.envelopeVelocityErrorMps,
        0.0,
        1.0e-12,
        "Newtonian drift fixture unexpectedly gained cross-track velocity"
    );
}

void testSpatialCorridorSteersBackWithoutReducingRouteSpeed()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::FreeTransit;
    program.tracking.positionErrorMeters = 20.0;

    auto agent = followerAgentFor(program.samples[0]);
    agent.positionMapMeters = {2.5, 15.0, 0.0};
    agent.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};

    const auto result = Follower::follow(
        program,
        10.25,
        agent,
        Tracker::Policy {}
    );

    require(
        result.status == Follower::Status::Following &&
        !result.trackingErrorExceeded,
        "spatial corridor treated recoverable cross-track error as route loss"
    );
    requireNear(
        result.spatialSpeedScale,
        1.0,
        1.0e-12,
        "spatial corridor still reduced route speed because of cross-track error"
    );
    requireNear(
        glm::length(result.targetVelocityMapMps),
        5.0,
        1.0e-12,
        "spatial corridor changed the authored route speed while correcting course"
    );
    require(
        result.targetVelocityMapMps.y < -1.0,
        "spatial corridor did not steer the velocity target back toward the path"
    );
    require(
        result.forwardAngleErrorRad >
            program.tracking.forwardAngleErrorRad,
        "test setup did not require a substantial inward steering angle"
    );
    requireNear(
        result.envelopeForwardAngleErrorRad,
        0.0,
        1.0e-12,
        "corrective steering was mistaken for geometric corridor course loss"
    );
    requireNear(
        result.envelopePositionErrorMeters,
        15.0,
        1.0e-12,
        "corridor envelope stopped measuring geometric cross-track position"
    );
}

void testSpatialCorridorHoldsTangentInsideCenterDeadband()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::FreeTransit;
    program.tracking.positionErrorMeters = 20.0;

    auto agent = followerAgentFor(program.samples[0]);
    agent.positionMapMeters = {2.5, 0.75, 0.0};
    agent.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};

    const auto result = Follower::follow(
        program,
        10.25,
        agent,
        Tracker::Policy {}
    );

    require(
        result.status == Follower::Status::Following &&
        !result.trackingErrorExceeded,
        "centered spatial corridor state was rejected"
    );
    requireNear(
        glm::length(result.targetVelocityMapMps),
        5.0,
        1.0e-12,
        "center deadband changed route speed"
    );
    requireNear(
        result.targetVelocityMapMps.y,
        0.0,
        1.0e-12,
        "center deadband kept hunting toward a moving look-ahead point"
    );
    requireNear(
        result.targetVelocityMapMps.z,
        0.0,
        1.0e-12,
        "center deadband introduced off-axis steering"
    );
}

void testSpatialZeroSpeedHoldDoesNotInventCourseLoss()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::PrecisionTransit;
    program.samples[0].velocityMapMetersPerSecond = glm::dvec3(0.0);
    program.samples[0].linearAccelerationFeedForwardMapMps2 =
        glm::dvec3(0.0);

    const auto& reference = program.samples[0];
    auto agent = exactAgentFor(reference);

    // 180-degree hull attitude at a stationary checkpoint is an attitude
    // correction, not a translational course error: course is undefined at
    // zero speed.
    agent.forwardMap = -reference.forwardMap;
    agent.rightMap = -reference.rightMap;
    agent.upMap = reference.upMap;

    const auto result =
        Tracker::track(program, reference, agent, Tracker::Policy {});

    require(
        result.status == Tracker::Status::Tracking,
        "zero-speed spatial hold treated hull attitude as route loss"
    );
    require(
        result.forwardAngleErrorRad > glm::radians(170.0),
        "zero-speed hold fixture did not create a large attitude error"
    );
    requireNear(
        result.envelopeForwardAngleErrorRad,
        0.0,
        1.0e-12,
        "zero-speed spatial hold invented a geometric course direction"
    );
}

void testFollowerSpatialCorridorCanCompleteBeforeNominalTime()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.family = Program::ManeuverFamily::PrecisionTransit;

    auto agent = followerAgentFor(program.samples[1]);
    const auto result = Follower::follow(
        program,
        10.25,
        agent,
        Tracker::Policy {}
    );

    require(
        result.status == Follower::Status::Complete,
        "spatial corridor held a physically completed maneuver for nominal time"
    );
    require(result.spatialReference &&
                result.referenceInterpolation01 > 0.999999,
            "terminal spatial projection did not reach accepted endpoint"
    );
}

void testFollowerRejectsExecutionBeforeAcceptanceTime()
{
    const Program program = baseProgram();
    const auto agent = followerAgentFor(program.samples[0]);

    const auto result = Follower::follow(program, 9.9, agent, game::navigation::ManeuverTrackingController::Policy {});
    require(result.status == Follower::Status::InvalidInput,
            "follower executed a program before its acceptance time");
}

} // namespace

int main()
{
    try
    {
        testDefaultAttitudeLoopIsNotUnderdamped();
        testAntiparallelAttitudeStillProducesCorrection();
        testZeroErrorPreservesAcceptedFeedForwardExactly();
        testFeedbackCannotExceedReservedAuthority();
        testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives();
        testSlowDockAngularErrorKeepsMovingReference();
        testFollowerUsesB9ThenB10WithoutResolvingControl();
        testFollowerCompletesOnlyAtTerminalState();
        testFreeTransitSpeedCorridorIgnoresTinyLongitudinalError();
        testFreeTransitDeadbandDoesNotFalseTriggerEnvelope();
        testFreeTransitCorridorStillCorrectsCrossTrackMotion();
        testFreeTransitCorridorCorrectsOnlyExcessOutsideBand();
        testFollowerDistinguishesStopFromMovingStart();
        testFollowerSpatialCorridorTracksPathInsteadOfClock();
        testSpatialCorridorLaunchesFromStoppedControlPoint();
        testSpatialCorridorAngularRateIsSteeringNotRouteLoss();
        testNewtonianSpatialDriftDoesNotCountAsRouteLoss();
        testSpatialCorridorSteersBackWithoutReducingRouteSpeed();
        testSpatialCorridorHoldsTangentInsideCenterDeadband();
        testSpatialZeroSpeedHoldDoesNotInventCourseLoss();
        testFollowerSpatialCorridorCanCompleteBeforeNominalTime();
        testFollowerRejectsExecutionBeforeAcceptanceTime();

        std::cout << "MANEUVER TRACKING CONTROLLER TESTS: PASS\n";
        std::cout << " - default attitude loop is critically damped or stronger\n";
        std::cout << " - zero error preserves A_ff/alpha_ff exactly\n";
        std::cout << " - tracking feedback is bounded by proved reserve\n";
        std::cout << " - envelope recovery neutralizes frozen moving-reference derivatives\n";
        std::cout << " - follower composes B9 sampler -> B10 tracker\n";
        std::cout << " - terminal completion uses accepted tolerances\n";
        std::cout << " - free transit ignores harmless longitudinal speed drift\n";
        std::cout << " - free-transit deadbands do not falsely trip the execution envelope\n";
        std::cout << " - free transit keeps full cross-track correction\n";
        std::cout << " - spatial corridor launches from a stopped control point without clock progress\n";
        std::cout << " - spatial angular-rate drift is corrected without route loss\n";
        std::cout << " - cross-track error steers back into the corridor without reducing route speed\n";
        std::cout << " - centered spatial flight holds the segment tangent without look-ahead hunting\n";
        std::cout << " - zero-speed spatial hold has attitude error but no invented course loss\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER TRACKING CONTROLLER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
