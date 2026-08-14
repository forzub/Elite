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

    // Local translation is authoritative in the ship-owned travel frame. The
    // travel frame carries large-scale motion; ordinary engines only change
    // motion inside it. Future J propulsion owns travel-frame acceleration.
    const glm::dvec3 requestedLocalAcceleration =
        frame.worldToLocalVector(
            motion.engineAccelerationMps2
        );

    // maxCombatSpeed is a propulsion/control envelope, never a hard physical
    // velocity cap. Only this frame's engine-produced delta-V is limited. A
    // collision, explosion or other external impulse may leave VREL above the
    // envelope and that overspeed persists until real forces remove it.
    const glm::dvec3 localAcceleration =
        limitPropulsionAccelerationToControlledSpeed(
            motion.localVelocityMps,
            requestedLocalAcceleration,
            localSpeedLimit(params),
            dt
        );

    motion.engineAccelerationMps2 =
        frame.localToWorldVector(localAcceleration);

    motion.localVelocityMps +=
        localAcceleration * dt;

    if (motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BrakeToStop &&
        glm::length(motion.localVelocityMps) <= StopSpeedEpsilonMps)
    {
        motion.localVelocityMps = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.velocityAlignmentMode = VelocityAlignmentMode::None;
        motion.targetForwardSpeedMps = 0.0;
    }

    motion.localPositionMeters +=
        motion.localVelocityMps * dt;

    const glm::dvec3 worldMeters =
        frame.localToWorldPosition(
            motion.localPositionMeters
        );

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
        world::coordinates::makeWorldPositionFromMeters(
            worldMeters
        );
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
    const double manoeuvreAccel =
        positiveOr(static_cast<double>(params.strafeAccel), 0.0);
    const double maxStrafeSpeed =
        std::max(0.0, static_cast<double>(params.maxStrafeSpeed));
    const double strafeDamping =
        std::max(0.0, static_cast<double>(params.strafeDamping));

    const glm::dvec3 f = glm::normalize(glm::dvec3(shipForward));
    const glm::dvec3 r = glm::normalize(glm::dvec3(shipRight));
    const glm::dvec3 u = glm::normalize(glm::dvec3(shipUp));

    if (cruiseActive)
    {
        // Stage 3 deliberately leaves J/cruise semantics untouched. The next
        // propulsion stage will detach/accelerate the travel frame itself.
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
        return;
    }

    const glm::dvec3 relativeWorldVelocity =
        frame.localToWorldVector(motion.localVelocityMps);

    motion.forwardSpeedMps =
        glm::dot(relativeWorldVelocity, f);

    if (motion.localControlLaw == LocalFlightControlLaw::Newtonian)
    {
        // END: rotate tail-to-velocity via ShipController, then use bounded
        // main-engine deceleration. The translation vector itself is never
        // rotated merely because the hull turned.
        if (motion.velocityAlignmentMode ==
                VelocityAlignmentMode::BrakeToStop)
        {
            const double speed = glm::length(relativeWorldVelocity);
            if (speed <= StopSpeedEpsilonMps || dtD <= 0.0)
            {
                motion.localVelocityMps = glm::dvec3(0.0);
                motion.engineAccelerationMps2 = glm::dvec3(0.0);
                motion.velocityAlignmentMode = VelocityAlignmentMode::None;
                motion.targetForwardSpeedMps = 0.0;
                return;
            }

            const glm::dvec3 antiVelocity =
                -relativeWorldVelocity / speed;

            if (glm::dot(f, antiVelocity) >= BrakeAlignmentCos)
            {
                const double brakeAccel = std::min(
                    maxAccel,
                    speed / dtD
                );
                // The automatic brake uses the same main-engine direction as
                // manual '+'. Alignment guarantees that forward is already
                // almost anti-parallel to VREL; do not inject a magic force
                // directly along -velocity.
                motion.engineAccelerationMps2 =
                    f * brakeAccel;
            }
            else
            {
                motion.engineAccelerationMps2 = glm::dvec3(0.0);
            }

            motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
            return;
        }

        // Newtonian main propulsion is intentionally one-directional from the
        // pilot's +/- control: '+' applies the main engine, '-' is a no-op.
        // To reduce VREL the pilot turns the hull so its nose points opposite
        // the velocity vector and applies the same '+'. Releasing thrust leaves
        // the inertial velocity vector untouched.
        const double mainThrustCommand = std::clamp(
            static_cast<double>(targetSpeedRate),
            0.0,
            1.0
        );

        glm::dvec3 desiredAcceleration =
            f * (mainThrustCommand * maxAccel) +
            f * (static_cast<double>(forwardInput) * manoeuvreAccel) +
            r * (static_cast<double>(strafeInput) * manoeuvreAccel) +
            u * (static_cast<double>(liftInput) * manoeuvreAccel);

        motion.engineAccelerationMps2 =
            clampMagnitude(desiredAcceleration, maxAccel);

        motion.desiredTacticalVelocityMps = relativeWorldVelocity;
        motion.targetForwardSpeedMps = glm::length(motion.localVelocityMps);
        return;
    }

    // ---------------- Assisted / Elite-style law ----------------
    // +/- is a throttle/setpoint trim, not a request that keeps running after
    // the key is released. While the pilot holds a key we move the requested
    // VREL setpoint. As soon as the input returns to neutral, capture the
    // *actually reached* local speed as the new setpoint. This prevents the
    // ship from continuing to accelerate/decelerate toward an old setpoint
    // after the pilot has released the throttle trim. The direction controller
    // remains active, so VREL may still bend toward the nose at bounded G.
    const double targetSpeedChangeRate =
        std::max(50.0, maxSpeed * 0.6);
    const bool throttleTrimActive =
        std::abs(static_cast<double>(targetSpeedRate)) > 1.0e-6;

    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop)
    {
        motion.targetForwardSpeedMps = 0.0;
        motion.assistedTargetSpeedHold = false;
        motion.strafeSpeedMps = 0.0;
        motion.liftSpeedMps = 0.0;
    }
    else if (throttleTrimActive)
    {
        motion.assistedTargetSpeedHold = false;
        motion.targetForwardSpeedMps +=
            static_cast<double>(targetSpeedRate) *
            targetSpeedChangeRate *
            dtD;
    }
    else if (!motion.assistedTargetSpeedHold)
    {
        motion.targetForwardSpeedMps =
            glm::length(relativeWorldVelocity);
    }

    motion.targetForwardSpeedMps =
        std::clamp(
            motion.targetForwardSpeedMps,
            0.0,
            maxSpeed
        );

    motion.strafeSpeedMps +=
        static_cast<double>(strafeInput) *
        manoeuvreAccel * dtD;

    motion.liftSpeedMps +=
        static_cast<double>(liftInput) *
        manoeuvreAccel * dtD;

    motion.strafeSpeedMps =
        std::clamp(
            motion.strafeSpeedMps,
            -maxStrafeSpeed,
            maxStrafeSpeed
        );

    motion.liftSpeedMps =
        std::clamp(
            motion.liftSpeedMps,
            -maxStrafeSpeed,
            maxStrafeSpeed
        );

    if (strafeDamping > 0.0)
    {
        const double damp = std::exp(-strafeDamping * dtD);
        motion.strafeSpeedMps *= damp;
        motion.liftSpeedMps *= damp;
    }

    const glm::dvec3 desiredWorldVelocity =
        f * motion.targetForwardSpeedMps +
        r * motion.strafeSpeedMps +
        u * motion.liftSpeedMps;

    motion.desiredTacticalVelocityMps = desiredWorldVelocity;

    // Crucially this runs even with no fresh +/- input: the Assisted law is a
    // persistent velocity controller, so rotating the hull makes the velocity
    // vector follow at a bounded acceleration instead of snapping instantly.
    const glm::dvec3 velocityError =
        desiredWorldVelocity - relativeWorldVelocity;

    const double response =
        positiveOr(static_cast<double>(params.throttleAccel), 1.0);

    motion.engineAccelerationMps2 =
        clampMagnitude(
            velocityError * response +
                f * (static_cast<double>(forwardInput) * manoeuvreAccel),
            maxAccel
        );

    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop &&
        glm::length(motion.localVelocityMps) <= StopSpeedEpsilonMps)
    {
        motion.localVelocityMps = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.velocityAlignmentMode = VelocityAlignmentMode::None;
    }
}

} // namespace game::navigation
