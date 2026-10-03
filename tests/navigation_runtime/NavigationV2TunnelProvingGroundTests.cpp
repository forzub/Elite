#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/autopilot/PredictivePilot.h"
#include "src/game/navigation/autopilot/RouteFollowerApi.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::autopilot::RouteFollower;
using FollowerAgent = game::navigation::autopilot::RouteFollowerAgentState;
using FollowerPolicy = game::navigation::autopilot::RouteFollowerPolicy;
using FollowerStatus = game::navigation::autopilot::RouteFollowerStatus;
using Pilot = game::navigation::autopilot::PredictivePilot;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kTunnelHalfWidthMeters = 12.0;
constexpr double kRouteSpeedMps = 8.0;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct Basis
{
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 right {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

Basis basisForForward(const glm::dvec3& requested)
{
    const glm::dvec3 forward = glm::normalize(requested);
    glm::dvec3 up(0.0, 1.0, 0.0);
    glm::dvec3 right = glm::cross(forward, up);
    if (glm::length(right) <= 1.0e-9)
    {
        up = {0.0, 0.0, 1.0};
        right = glm::cross(forward, up);
    }
    right = glm::normalize(right);
    up = glm::normalize(glm::cross(right, forward));
    return {forward, right, up};
}

void setBasis(ShipTransform& transform, const Basis& basis)
{
    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] =
        glm::vec4(glm::vec3(basis.right), 0.0f);
    transform.orientation[1] =
        glm::vec4(glm::vec3(basis.up), 0.0f);
    transform.orientation[2] =
        glm::vec4(glm::vec3(-basis.forward), 0.0f);
}

ShipParams cobraAssistedParams()
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
    p.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    p.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;

    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 73.549875f;
    p.reverseMainEngineAccelerationMps2 = 73.549875f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 73.549875f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 2.0f;

    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

glm::dvec3 centerlinePosition(double x)
{
    return {
        x,
        0.0,
        30.0 * std::sin(2.0 * kPi * x / 300.0)
    };
}

glm::dvec3 centerlineTangent(double x)
{
    const double dzdx =
        30.0 * (2.0 * kPi / 300.0) *
        std::cos(2.0 * kPi * x / 300.0);
    return glm::normalize(glm::dvec3(1.0, 0.0, dzdx));
}

Program tunnelProgram()
{
    Program p;
    p.valid = true;
    p.revision = 2001;
    p.objectiveRevision = 2000;
    p.family = Program::ManeuverFamily::FreeTransit;
    p.referenceMode = Program::ReferenceMode::SpatialCorridor;
    p.controlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    p.translationMode = Program::TranslationMode::AssistedVelocity;
    p.acceptedAtUniverseTimeSeconds = 0.0;
    p.validUntilUniverseTimeSeconds = 120.0;
    p.sampleCount = Program::kMaxSamples;
    p.completionTriggersReplan = false;

    p.tracking.positionErrorMeters = kTunnelHalfWidthMeters;
    p.tracking.linearVelocityErrorMps = 10.0;
    p.tracking.forwardAngleErrorRad = glm::radians(80.0);
    p.tracking.angularVelocityErrorRadPerSec = 2.0;
    p.tracking.linearFeedbackReserveMps2 = 6.0;
    p.tracking.angularFeedbackReserveRadPerSec2 = 1.5;

    p.terminalTolerance.positionMeters = 5.0;
    p.terminalTolerance.linearVelocityMps = 3.0;
    p.terminalTolerance.forwardAngleRad = glm::radians(15.0);
    p.terminalTolerance.angularVelocityRadPerSec = 0.4;

    p.capability.revision = 1;
    p.capability.maxForwardAccelerationMetersPerSec2 = 73.549875;
    p.capability.maxReverseAccelerationMetersPerSec2 = 73.549875;
    p.capability.maxLateralAccelerationMetersPerSec2 = 73.549875;
    p.capability.maxVerticalAccelerationMetersPerSec2 = 73.549875;
    p.capability.maxForwardMainAccelerationMetersPerSec2 = 73.549875;
    p.capability.maxReverseMainAccelerationMetersPerSec2 = 73.549875;
    p.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    p.capability.maxAngularSpeedRadPerSec = 2.5;

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double x =
            300.0 * static_cast<double>(i) /
            static_cast<double>(Program::kMaxSamples - 1);
        const glm::dvec3 tangent = centerlineTangent(x);
        const Basis basis = basisForForward(tangent);

        auto& s = p.samples[i];
        s.timeOffsetSeconds = x / kRouteSpeedMps;
        s.positionMapMeters = centerlinePosition(x);
        s.velocityMapMetersPerSecond = tangent * kRouteSpeedMps;
        s.linearAccelerationFeedForwardMapMps2 = glm::dvec3(0.0);
        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond = glm::dvec3(0.0);
        s.angularAccelerationFeedForwardMapRadPerSec2 = glm::dvec3(0.0);
    }

    return p;
}

double distanceToCenterline(
    const Program& program,
    const glm::dvec3& position
)
{
    double best = 1.0e100;
    for (std::size_t i = 0; i + 1 < program.sampleCount; ++i)
    {
        const glm::dvec3 a = program.samples[i].positionMapMeters;
        const glm::dvec3 b = program.samples[i + 1].positionMapMeters;
        const glm::dvec3 ab = b - a;
        const double ab2 = glm::dot(ab, ab);
        if (!(ab2 > 1.0e-12))
            continue;
        const double u = std::clamp(
            glm::dot(position - a, ab) / ab2,
            0.0,
            1.0
        );
        best = std::min(best, glm::length(position - (a + ab * u)));
    }
    return best;
}

FollowerAgent followerAgent(const ShipTransform& transform)
{
    FollowerAgent a;
    a.positionMapMeters = transform.motion.localPositionMeters;
    a.velocityMapMetersPerSecond = transform.motion.localVelocityMps;
    a.forwardMap = glm::dvec3(transform.forward());
    a.rightMap = glm::dvec3(transform.right());
    a.upMap = glm::dvec3(transform.up());
    a.pitchRateRadPerSec = transform.pitchRate;
    a.yawRateRadPerSec = transform.yawRate;
    a.rollRateRadPerSec = transform.rollRate;
    return a;
}

void testAssistedV2StaysInsideAcceptedTunnel()
{
    const Program program = tunnelProgram();
    const ShipParams params = cobraAssistedParams();
    WorldParams world {};

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "v2-tunnel-proving-ground";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;

    ShipTransform transform {};
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.travelFrame = frame;
    transform.motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    transform.motion.localPositionMeters =
        program.samples[0].positionMapMeters;
    transform.motion.localVelocityMps =
        program.samples[0].velocityMapMetersPerSecond;
    transform.setWorldPositionMeters(
        transform.motion.localPositionMeters
    );
    setBasis(transform, basisForForward(
        program.samples[0].velocityMapMetersPerSecond
    ));

    Pilot::State pilotState;
    FollowerPolicy followerPolicy;

    std::size_t spatialCursor = 0;
    std::size_t maximumSegment = 0;
    double maxCrossTrack = 0.0;
    double maxContinuousSlipSeconds = 0.0;
    double currentSlipSeconds = 0.0;
    bool reachedLastSegment = false;

    for (int tick = 0; tick < 4000; ++tick)
    {
        const double now = tick * kDt;
        const FollowerAgent agent = followerAgent(transform);
        const auto followed = Follower::follow(
            program,
            now,
            agent,
            followerPolicy,
            spatialCursor
        );

        require(
            followed.status != FollowerStatus::InvalidInput,
            "RouteFollower V2 rejected the accepted tunnel"
        );

        if (followed.spatialReference)
        {
            spatialCursor = std::max(
                spatialCursor,
                followed.referenceLowerSampleIndex
            );
            maximumSegment = std::max(
                maximumSegment,
                followed.referenceLowerSampleIndex
            );
        }

        const auto reference = Follower::sampleReference(
            program,
            now,
            agent.positionMapMeters,
            spatialCursor
        );
        require(reference.valid, "V2 reference sampling failed");

        Pilot::Request request;
        request.law = program.controlLaw;
        request.desiredVelocityMapMps = followed.targetVelocityMapMps;
        request.desiredLinearAccelerationMapMps2 =
            followed.intent.idealLinearAccelerationLocalMps2;

        const double targetSpeed =
            glm::length(followed.targetVelocityMapMps);
        request.desiredForwardMap =
            targetSpeed > 1.0e-9
                ? followed.targetVelocityMapMps / targetSpeed
                : reference.reference.forwardMap;
        request.desiredUpMap = reference.reference.upMap;

        request.actualVelocityMapMps =
            transform.motion.localVelocityMps;
        request.forwardMap = glm::dvec3(transform.forward());
        request.rightMap = glm::dvec3(transform.right());
        request.upMap = glm::dvec3(transform.up());
        request.pitchRateRadPerSec = transform.pitchRate;
        request.yawRateRadPerSec = transform.yawRate;
        request.rollRateRadPerSec = transform.rollRate;
        request.stopRequested = targetSpeed <= 1.0e-9;
        request.deltaSeconds = kDt;

        const ShipControlState control =
            Pilot::make(request, params, pilotState);

        require(
            !control.navigationAccelerationDemandValid &&
            !control.navigationVelocityTargetValid &&
            !control.navigationPrecisionTranslationOnly,
            "PredictivePilot V2 escaped through a legacy direct-demand seam"
        );

        SharedShipPhysics::integrate(
            transform,
            params,
            control,
            world,
            static_cast<float>(kDt)
        );

        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            transform.motion,
            frame,
            params,
            static_cast<float>(kDt),
            control.targetSpeedRate,
            control.cruiseActive,
            control.forwardInput,
            control.liftInput,
            control.strafeInput,
            transform.forward(),
            transform.right(),
            transform.up()
        );

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            transform.motion,
            transform.worldPosition,
            frame,
            params,
            kDt
        );
        transform.syncLegacyPositionFromWorld();

        const double crossTrack =
            distanceToCenterline(
                program,
                transform.motion.localPositionMeters
            );
        maxCrossTrack = std::max(maxCrossTrack, crossTrack);

        const double speed =
            glm::length(transform.motion.localVelocityMps);
        if (speed > 0.25)
        {
            const double alignment = std::clamp(
                glm::dot(
                    transform.motion.localVelocityMps / speed,
                    glm::normalize(glm::dvec3(transform.forward()))
                ),
                -1.0,
                1.0
            );
            const double slipDeg = std::acos(alignment) * 180.0 / kPi;
            if (slipDeg > 8.0)
            {
                currentSlipSeconds += kDt;
                maxContinuousSlipSeconds =
                    std::max(maxContinuousSlipSeconds, currentSlipSeconds);
            }
            else
            {
                currentSlipSeconds = 0.0;
            }
        }

        require(
            crossTrack <= kTunnelHalfWidthMeters + 1.0e-6,
            "PredictivePilot V2 left the accepted tunnel"
        );

        if (maximumSegment + 2 >= program.sampleCount)
        {
            reachedLastSegment = true;
            break;
        }
    }

    require(
        reachedLastSegment,
        "PredictivePilot V2 failed to make physical progress through the tunnel"
    );
    require(
        maxContinuousSlipSeconds <= 3.0 + 1.0e-9,
        "Assisted V2 velocity-to-nose lag exceeded three seconds"
    );
    require(
        pilotState.effectivePitchAuthorityRadPerSec2 > 0.0 ||
        pilotState.effectiveYawAuthorityRadPerSec2 > 0.0 ||
        pilotState.effectiveRollAuthorityRadPerSec2 > 0.0,
        "PredictivePilot V2 learned no angular authority while negotiating the tunnel"
    );

    std::cout
        << "[V2-TUNNEL] max_cross_track_m=" << maxCrossTrack
        << " max_continuous_slip_s=" << maxContinuousSlipSeconds
        << " max_segment=" << maximumSegment
        << " learned_pitch_alpha="
        << pilotState.effectivePitchAuthorityRadPerSec2
        << " learned_yaw_alpha="
        << pilotState.effectiveYawAuthorityRadPerSec2
        << " learned_roll_alpha="
        << pilotState.effectiveRollAuthorityRadPerSec2
        << "\n";
}

} // namespace

int main()
{
    try
    {
        testAssistedV2StaysInsideAcceptedTunnel();
        std::cout << "NAVIGATION V2 TUNNEL PROVING GROUND: PASS\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr
            << "NAVIGATION V2 TUNNEL PROVING GROUND: FAIL: "
            << ex.what() << "\n";
        return 1;
    }
}
