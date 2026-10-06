#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/autopilot/ClientRouteAutopilot.h"
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

using Autopilot =
    game::navigation::autopilot::ClientRouteAutopilot;
using Agent =
    game::navigation::autopilot::AutopilotAgentState;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kTunnelHalfWidthMeters = 12.0;
constexpr double kRouteSpeedMps = 8.0;
constexpr double kRadiusMeters = 300.0;
constexpr double kSweepRadians = 0.5 * kPi;

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
    p.stopSpeedEpsilonMps = 0.05f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

game::navigation::planner::RoutePlan tunnelPlan()
{
    using namespace game::navigation::planner;

    RoutePlan plan;
    plan.disposition = RoutePlanDisposition::Ready;
    plan.failureCode = RoutePlanFailureCode::None;

    constexpr int GateCount = 65;
    plan.gates.reserve(GateCount);
    plan.executionGates.reserve(GateCount);

    for (int i = 0; i < GateCount; ++i)
    {
        const double t =
            kSweepRadians *
            static_cast<double>(i) /
            static_cast<double>(GateCount - 1);

        RouteGate gate;
        gate.positionMeters = {
            kRadiusMeters * std::sin(t),
            0.0,
            kRadiusMeters * (1.0 - std::cos(t))
        };
        gate.forward = glm::normalize(glm::dvec3(
            std::cos(t),
            0.0,
            std::sin(t)
        ));
        gate.speedMps = kRouteSpeedMps;
        plan.gates.push_back(gate);
        plan.executionGates.push_back(gate);
    }

    RouteCurveSegment arc;
    arc.kind = RouteCurveKind::CircularArc;
    arc.startProgressMeters = 0.0;
    arc.endProgressMeters = kRadiusMeters * kSweepRadians;
    arc.maxSpeedMps = kRouteSpeedMps;
    arc.startMeters = plan.executionGates.front().positionMeters;
    arc.endMeters = plan.executionGates.back().positionMeters;
    arc.startForward = plan.executionGates.front().forward;
    arc.endForward = plan.executionGates.back().forward;
    arc.arcCenterMeters = {0.0, 0.0, kRadiusMeters};
    arc.arcNormal = {0.0, -1.0, 0.0};
    arc.arcRadiusMeters = kRadiusMeters;
    arc.arcSweepRadians = kSweepRadians;
    plan.routeCurves.push_back(arc);

    return plan;
}

Agent makeAgent(const ShipTransform& transform)
{
    Agent out;
    out.positionMapMeters = transform.motion.localPositionMeters;
    out.velocityMapMetersPerSecond = transform.motion.localVelocityMps;
    out.forwardMap = glm::dvec3(transform.forward());
    out.rightMap = glm::dvec3(transform.right());
    out.upMap = glm::dvec3(transform.up());
    out.pitchRateRadPerSec = transform.pitchRate;
    out.yawRateRadPerSec = transform.yawRate;
    out.rollRateRadPerSec = transform.rollRate;
    return out;
}

double distanceToAuthoredArc(const glm::dvec3& position)
{
    const glm::dvec3 center(0.0, 0.0, kRadiusMeters);
    const glm::dvec3 radial = position - center;
    const double radialXZ =
        std::hypot(radial.x, radial.z);
    return std::hypot(
        radialXZ - kRadiusMeters,
        position.y
    );
}

void testCurrentAutopilotStaysInsideAcceptedTunnel()
{
    const auto plan = tunnelPlan();
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
        plan.executionGates.front().positionMeters;
    transform.motion.localVelocityMps =
        plan.executionGates.front().forward * kRouteSpeedMps;
    transform.motion.targetForwardSpeedMps = kRouteSpeedMps;
    transform.setWorldPositionMeters(
        transform.motion.localPositionMeters
    );
    setBasis(
        transform,
        basisForForward(plan.executionGates.front().forward)
    );

    Agent initial = makeAgent(transform);

    Autopilot::State autopilot;
    std::string failureReason;
    require(
        Autopilot::start(
            autopilot,
            plan,
            initial,
            game::navigation::LocalFlightControlLaw::Assisted,
            params,
            0.0,
            2001,
            kTunnelHalfWidthMeters,
            glm::dvec3(0.0, 1.0, 0.0),
            false,
            &failureReason
        ),
        "ClientRouteAutopilot rejected accepted tunnel: " + failureReason
    );

    double maxCrossTrack = 0.0;
    double maxContinuousSlipSeconds = 0.0;
    double currentSlipSeconds = 0.0;
    double minimumRemainingRouteMeters =
        plan.routeCurves.back().endProgressMeters;
    bool reachedEnd = false;

    for (int tick = 0; tick < 5000; ++tick)
    {
        const Agent agent = makeAgent(transform);
        const auto output =
            Autopilot::update(
                autopilot,
                agent,
                game::navigation::LocalFlightControlLaw::Assisted,
                params,
                static_cast<double>(tick) * kDt,
                kDt
            );

        require(
            output.valid,
            "ClientRouteAutopilot emitted invalid tunnel output"
        );
        require(
            !output.control.navigationAccelerationDemandValid &&
            !output.control.navigationVelocityTargetValid &&
            !output.control.navigationPrecisionTranslationOnly,
            "current autopilot escaped through a retired direct-demand seam"
        );

        SharedShipPhysics::integrate(
            transform,
            params,
            output.control,
            world,
            static_cast<float>(kDt)
        );

        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            transform.motion,
            frame,
            params,
            static_cast<float>(kDt),
            output.control.targetSpeedRate,
            output.control.cruiseActive,
            output.control.forwardInput,
            output.control.liftInput,
            output.control.strafeInput,
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
            distanceToAuthoredArc(
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
            const double slipDeg =
                std::acos(alignment) * 180.0 / kPi;
            if (slipDeg > 8.0)
            {
                currentSlipSeconds += kDt;
                maxContinuousSlipSeconds =
                    std::max(
                        maxContinuousSlipSeconds,
                        currentSlipSeconds
                    );
            }
            else
            {
                currentSlipSeconds = 0.0;
            }
        }

        minimumRemainingRouteMeters =
            std::min(
                minimumRemainingRouteMeters,
                output.exactRemainingRouteMeters
            );

        require(
            crossTrack <= kTunnelHalfWidthMeters + 1.0e-6,
            "ClientRouteAutopilot left the accepted tunnel"
        );

        if (output.complete ||
            output.exactRemainingRouteMeters <= 1.0)
        {
            reachedEnd = true;
            break;
        }
    }

    require(
        reachedEnd,
        "ClientRouteAutopilot failed to make physical progress through the tunnel"
    );
    require(
        maxContinuousSlipSeconds <= 3.0 + 1.0e-9,
        "Assisted velocity-to-nose lag exceeded three seconds"
    );
    require(
        autopilot.pilotState.effectivePitchAuthorityRadPerSec2 > 0.0 ||
        autopilot.pilotState.effectiveYawAuthorityRadPerSec2 > 0.0 ||
        autopilot.pilotState.effectiveRollAuthorityRadPerSec2 > 0.0,
        "PredictivePilot learned no angular authority while negotiating the tunnel"
    );

    std::cout
        << "[V2-TUNNEL] max_cross_track_m=" << maxCrossTrack
        << " max_continuous_slip_s=" << maxContinuousSlipSeconds
        << " min_remaining_m=" << minimumRemainingRouteMeters
        << " learned_pitch_alpha="
        << autopilot.pilotState.effectivePitchAuthorityRadPerSec2
        << " learned_yaw_alpha="
        << autopilot.pilotState.effectiveYawAuthorityRadPerSec2
        << " learned_roll_alpha="
        << autopilot.pilotState.effectiveRollAuthorityRadPerSec2
        << "\n";
}

} // namespace

int main()
{
    try
    {
        testCurrentAutopilotStaysInsideAcceptedTunnel();
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
