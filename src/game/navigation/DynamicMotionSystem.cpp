#include <algorithm>
#include <cmath>

#include "DynamicMotionSystem.h"

namespace game::navigation
{
namespace
{
constexpr double StandardGravityMps2 = 9.80665;
constexpr double StopSpeedEpsilonMps = 0.05;
constexpr double BrakeAlignmentCos = 0.995;

inline double positiveOr(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

inline double localSpeedLimit(const ShipParams& params)
{
    return std::max(0.0, static_cast<double>(params.maxCombatSpeed));
}

inline double linearAccelerationLimit(const ShipParams& params)
{
    // A descriptor may opt into a linear-only envelope so longitudinal
    // response can be tuned independently from the angular/load envelope.
    // Zero preserves legacy descriptors and tests by falling back to maxGs.
    const double linearGs =
        params.maxLinearGs > 0.0f
            ? static_cast<double>(params.maxLinearGs)
            : static_cast<double>(params.maxGs);

    return std::max(0.0, linearGs * StandardGravityMps2);
}

inline double manoeuvreAccelerationLimit(const ShipParams& params)
{
    const double dedicated =
        static_cast<double>(params.manoeuvreThrusterAccel);
    if (dedicated > 0.0)
        return dedicated;

    // Legacy/test descriptors that predate the dedicated RCS field still get
    // their historical authority until every ship profile has migrated.
    return std::max(0.0, static_cast<double>(params.strafeAccel));
}

inline glm::dvec3 clampMagnitude(
    glm::dvec3 value,
    double maxLength
)
{
    if (maxLength <= 0.0)
        return glm::dvec3(0.0);

    const double length = glm::length(value);
    if (length > maxLength && length > 1.0e-12)
        value *= maxLength / length;

    return value;
}

inline glm::dvec3 limitPropulsionAccelerationToControlledSpeed(
    const glm::dvec3& currentVelocity,
    const glm::dvec3& requestedAcceleration,
    double controlledSpeedLimit,
    double dt
)
{
    if (dt <= 0.0 || controlledSpeedLimit <= 0.0)
        return glm::dvec3(0.0);

    const glm::dvec3 requestedDeltaV = requestedAcceleration * dt;
    const double currentSpeed = glm::length(currentVelocity);
    const double allowedRadius = std::max(controlledSpeedLimit, currentSpeed);
    const glm::dvec3 candidateVelocity =
        currentVelocity + requestedDeltaV;

    if (glm::length(candidateVelocity) <= allowedRadius + 1.0e-12)
        return requestedAcceleration;

    // Clip only the ENGINE-produced delta-V to the current admissible speed
    // sphere. The physical velocity itself is never clamped. If an external
    // impulse has already pushed the craft above the normal control envelope,
    // propulsion may reduce that overspeed but may not make its magnitude
    // larger. This keeps collision/explosion impulses physically observable.
    const double a = glm::dot(requestedDeltaV, requestedDeltaV);
    if (a <= 1.0e-24)
        return glm::dvec3(0.0);

    const double b = 2.0 * glm::dot(currentVelocity, requestedDeltaV);
    const double c =
        glm::dot(currentVelocity, currentVelocity) -
        allowedRadius * allowedRadius;
    const double discriminant = std::max(0.0, b * b - 4.0 * a * c);
    const double root = (-b + std::sqrt(discriminant)) / (2.0 * a);
    const double fraction = std::clamp(root, 0.0, 1.0);

    return requestedAcceleration * fraction;
}
}

void DynamicMotionSystem::updateLocalFrameMotion(
    DynamicMotionState& motion,
    world::coordinates::WorldPosition& worldPosition,
    const KinematicFrame& frame,
    const ShipParams& params,
    double dt
)
{
    if (dt <= 0.0)
        return;

    // The main propulsion request and the RCS request are deliberately kept
    // separate until this fixed kinematic step. The ordinary controlled-speed
    // envelope still applies to the main engine in both laws. In Newtonian,
    // manual RCS is a real small force and may accumulate delta-v beyond that
    // envelope; Assisted applies the envelope to the combined controlled motion.
    const glm::dvec3 requestedMainLocalAcceleration =
        frame.worldToLocalVector(motion.mainEngineAccelerationMps2);

    glm::dvec3 actualManoeuvreWorldAcceleration =
        motion.manoeuvreAccelerationMps2;

    const double manoeuvreAuthority = manoeuvreAccelerationLimit(params);
    const double gasUsePerSecond = std::max(
        0.0,
        static_cast<double>(params.manoeuvreGasUsePerSecond)
    );
    const double gasRechargePerSecond = std::max(
        0.0,
        static_cast<double>(params.manoeuvreGasRechargePerSecond)
    );
    const double restartFraction = std::clamp(
        static_cast<double>(params.manoeuvreGasRestartFraction),
        0.0,
        1.0
    );

    motion.manoeuvreGasPressure01 = std::clamp(
        motion.manoeuvreGasPressure01,
        0.0,
        1.0
    );

    const double requestedManoeuvreMagnitude =
        glm::length(actualManoeuvreWorldAcceleration);
    const double normalizedManoeuvreUse = manoeuvreAuthority > 1.0e-12
        ? requestedManoeuvreMagnitude / manoeuvreAuthority
        : 0.0;

    if (motion.manoeuvreGasDepleted)
    {
        actualManoeuvreWorldAcceleration = glm::dvec3(0.0);
        motion.manoeuvreGasPressure01 = std::min(
            1.0,
            motion.manoeuvreGasPressure01 + gasRechargePerSecond * dt
        );

        if (motion.manoeuvreGasPressure01 >= restartFraction - 1.0e-12)
            motion.manoeuvreGasDepleted = false;
    }
    else
    {
        const double pressureDelta =
            gasRechargePerSecond -
            gasUsePerSecond * normalizedManoeuvreUse;

        motion.manoeuvreGasPressure01 = std::clamp(
            motion.manoeuvreGasPressure01 + pressureDelta * dt,
            0.0,
            1.0
        );

        if (normalizedManoeuvreUse > 1.0e-12 &&
            motion.manoeuvreGasPressure01 <= 1.0e-12)
        {
            motion.manoeuvreGasPressure01 = 0.0;
            motion.manoeuvreGasDepleted = true;
            actualManoeuvreWorldAcceleration = glm::dvec3(0.0);
        }
    }

    const glm::dvec3 actualManoeuvreLocalAcceleration =
        frame.worldToLocalVector(actualManoeuvreWorldAcceleration);

    glm::dvec3 localAcceleration(0.0);
    if (motion.localControlLaw == LocalFlightControlLaw::Newtonian)
    {
        const glm::dvec3 controlledMainAcceleration =
            limitPropulsionAccelerationToControlledSpeed(
                motion.localVelocityMps,
                requestedMainLocalAcceleration,
                localSpeedLimit(params),
                dt
            );

        // Deliberate exception: RCS is not a second main engine, but neither is
        // it a velocity setpoint. With enough time and replenished gas it can
        // continue adding tiny Newtonian delta-v past maxCombatSpeed.
        localAcceleration =
            controlledMainAcceleration + actualManoeuvreLocalAcceleration;
    }
    else
    {
        localAcceleration = limitPropulsionAccelerationToControlledSpeed(
            motion.localVelocityMps,
            requestedMainLocalAcceleration + actualManoeuvreLocalAcceleration,
            localSpeedLimit(params),
            dt
        );
    }

    motion.engineAccelerationMps2 =
        frame.localToWorldVector(localAcceleration);

    motion.localVelocityMps += localAcceleration * dt;

    if (motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BrakeToStop &&
        glm::length(motion.localVelocityMps) <= StopSpeedEpsilonMps)
    {
        motion.localVelocityMps = glm::dvec3(0.0);
        motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.velocityAlignmentMode = VelocityAlignmentMode::None;
        motion.targetForwardSpeedMps = 0.0;
    }

    motion.localPositionMeters += motion.localVelocityMps * dt;

    const glm::dvec3 worldMeters =
        frame.localToWorldPosition(motion.localPositionMeters);

    motion.referenceVelocityMps =
        frame.localToWorldVelocity(
            motion.localPositionMeters,
            glm::dvec3(0.0)
        );

    motion.worldVelocityMps =
        frame.localToWorldVelocity(
            motion.localPositionMeters,
            motion.localVelocityMps
        );

    worldPosition =
        world::coordinates::makeWorldPositionFromMeters(worldMeters);
}

void DynamicMotionSystem::applyLocalFrameInput(
    DynamicMotionState& motion,
    const KinematicFrame& frame,
    const ShipParams& params,
    float dt,
    float targetSpeedRate,
    bool cruiseActive,
    float forwardInput,
    float liftInput,
    float strafeInput,
    const glm::vec3& shipForward,
    const glm::vec3& shipRight,
    const glm::vec3& shipUp
)
{
    const double dtD = std::max(0.0, static_cast<double>(dt));
    const double maxSpeed = localSpeedLimit(params);
    const double maxAccel = linearAccelerationLimit(params);
    const double manoeuvreAccel = manoeuvreAccelerationLimit(params);

    const glm::dvec3 f = glm::normalize(glm::dvec3(shipForward));
    const glm::dvec3 r = glm::normalize(glm::dvec3(shipRight));
    const glm::dvec3 u = glm::normalize(glm::dvec3(shipUp));

    motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
    motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);

    if (cruiseActive)
    {
        // J/cruise owns a different propulsion layer. Local RCS is suppressed
        // by the input mapper and explicitly cleared here as a second guard.
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
        return;
    }

    const glm::dvec3 relativeWorldVelocity =
        frame.localToWorldVector(motion.localVelocityMps);

    motion.forwardSpeedMps = glm::dot(relativeWorldVelocity, f);
    motion.strafeSpeedMps = glm::dot(relativeWorldVelocity, r);
    motion.liftSpeedMps = glm::dot(relativeWorldVelocity, u);

    // The grey-keypad controls one physical six-direction RCS system in both
    // flight laws. No axis is promoted to a throttle or a hidden velocity
    // setpoint. Assisted may stabilize the result afterwards; Newtonian keeps
    // every tiny impulse as inertial delta-v.
    motion.manoeuvreAccelerationMps2 =
        f * (static_cast<double>(forwardInput) * manoeuvreAccel) +
        r * (static_cast<double>(strafeInput) * manoeuvreAccel) +
        u * (static_cast<double>(liftInput) * manoeuvreAccel);

    if (motion.localControlLaw == LocalFlightControlLaw::Newtonian)
    {
        // END: rotate tail-to-velocity via ShipController, then use bounded
        // main-engine deceleration. RCS remains an independent body-axis force.
        if (motion.velocityAlignmentMode ==
                VelocityAlignmentMode::BrakeToStop)
        {
            const double speed = glm::length(relativeWorldVelocity);
            if (speed <= StopSpeedEpsilonMps || dtD <= 0.0)
            {
                motion.localVelocityMps = glm::dvec3(0.0);
                motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
                motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
                motion.engineAccelerationMps2 = glm::dvec3(0.0);
                motion.velocityAlignmentMode = VelocityAlignmentMode::None;
                motion.targetForwardSpeedMps = 0.0;
                return;
            }

            const glm::dvec3 antiVelocity = -relativeWorldVelocity / speed;

            if (glm::dot(f, antiVelocity) >= BrakeAlignmentCos)
            {
                const double brakeAccel = std::min(maxAccel, speed / dtD);
                motion.mainEngineAccelerationMps2 = f * brakeAccel;
            }

            motion.engineAccelerationMps2 =
                motion.mainEngineAccelerationMps2 +
                motion.manoeuvreAccelerationMps2;
            motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
            return;
        }

        // Newtonian main propulsion remains one-directional: '+' applies the
        // main engine and '-' is a no-op. This controlled propulsion is separate
        // from keypad RCS and retains its ordinary combat-speed envelope.
        const double mainThrustCommand = std::clamp(
            static_cast<double>(targetSpeedRate),
            0.0,
            1.0
        );

        motion.mainEngineAccelerationMps2 =
            f * (mainThrustCommand * maxAccel);
        motion.engineAccelerationMps2 =
            motion.mainEngineAccelerationMps2 +
            motion.manoeuvreAccelerationMps2;

        motion.desiredTacticalVelocityMps = relativeWorldVelocity;
        motion.targetForwardSpeedMps = glm::length(motion.localVelocityMps);
        return;
    }

    // ---------------- Assisted / Elite-style law ----------------
    // +/- remains the persistent forward-speed controller. Keypad RCS is not
    // folded into that setpoint: while a body-axis RCS key is held, stabilization
    // yields on that axis and the small physical thruster moves the ship. On
    // release, the Assisted controller resumes correcting that axis.
    const double targetSpeedChangeRate = std::max(50.0, maxSpeed * 0.6);
    const bool throttleTrimActive =
        std::abs(static_cast<double>(targetSpeedRate)) > 1.0e-6;

    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop)
    {
        motion.targetForwardSpeedMps = 0.0;
        motion.assistedTargetSpeedHold = false;
        motion.assistedThrottleTrimWasActive = false;
    }
    else if (throttleTrimActive)
    {
        motion.assistedTargetSpeedHold = false;
        motion.assistedThrottleTrimWasActive = true;
        motion.targetForwardSpeedMps +=
            static_cast<double>(targetSpeedRate) *
            targetSpeedChangeRate * dtD;
    }
    else if (motion.assistedThrottleTrimWasActive)
    {
        // Capture exactly once on the +/- release edge. Keypad RCS is a
        // temporary body-axis translation command and must not become a new
        // longitudinal cruise setpoint merely because the trim is neutral.
        motion.targetForwardSpeedMps = std::max(
            0.0,
            glm::dot(relativeWorldVelocity, f)
        );
        motion.assistedThrottleTrimWasActive = false;
    }

    motion.targetForwardSpeedMps = std::clamp(
        motion.targetForwardSpeedMps,
        0.0,
        maxSpeed
    );

    const glm::dvec3 desiredWorldVelocity =
        f * motion.targetForwardSpeedMps;
    motion.desiredTacticalVelocityMps = desiredWorldVelocity;

    glm::dvec3 velocityError =
        desiredWorldVelocity - relativeWorldVelocity;

    auto yieldAxisToManualRcs = [&](const glm::dvec3& axis, float input)
    {
        if (std::abs(static_cast<double>(input)) > 1.0e-6)
            velocityError -= axis * glm::dot(velocityError, axis);
    };

    yieldAxisToManualRcs(f, forwardInput);
    yieldAxisToManualRcs(r, strafeInput);
    yieldAxisToManualRcs(u, liftInput);

    const double response =
        positiveOr(static_cast<double>(params.throttleAccel), 1.0);

    motion.mainEngineAccelerationMps2 =
        clampMagnitude(velocityError * response, maxAccel);
    motion.engineAccelerationMps2 =
        motion.mainEngineAccelerationMps2 +
        motion.manoeuvreAccelerationMps2;

    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop &&
        glm::length(motion.localVelocityMps) <= StopSpeedEpsilonMps)
    {
        motion.localVelocityMps = glm::dvec3(0.0);
        motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.velocityAlignmentMode = VelocityAlignmentMode::None;
    }
}

} // namespace game::navigation
