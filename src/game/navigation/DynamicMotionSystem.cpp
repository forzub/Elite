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
    // maxGs is the common safety envelope. For a crewed craft it normally
    // represents crew tolerance; for an unmanned craft the descriptor can set
    // a higher structural/equipment limit without changing the motion code.
    return std::max(
        0.0,
        static_cast<double>(params.maxGs) * StandardGravityMps2
    );
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
    const glm::dvec3 localAcceleration =
        frame.worldToLocalVector(
            motion.engineAccelerationMps2
        );

    motion.localVelocityMps +=
        localAcceleration * dt;

    // Both local control laws are deliberately bounded. The separate J layer
    // exists for high-energy travel; normal local flight must remain inside a
    // survivable/structurally permitted envelope.
    const double maxLocalSpeed = localSpeedLimit(params);
    if (maxLocalSpeed > 0.0)
    {
        motion.localVelocityMps =
            clampMagnitude(
                motion.localVelocityMps,
                maxLocalSpeed
            );
    }

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
                motion.engineAccelerationMps2 =
                    antiVelocity * brakeAccel;
            }
            else
            {
                motion.engineAccelerationMps2 = glm::dvec3(0.0);
            }

            motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
            return;
        }

        // +/- is actual longitudinal thrust. Releasing the key removes thrust;
        // the velocity vector persists. Turning the hull alone does not alter
        // that vector.
        glm::dvec3 desiredAcceleration =
            f * (static_cast<double>(targetSpeedRate) * maxAccel) +
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
    // +/- changes target local speed. The flight computer then uses the same
    // bounded engine acceleration to make VREL follow the current ship nose.
    const double targetSpeedChangeRate =
        std::max(50.0, maxSpeed * 0.6);

    motion.targetForwardSpeedMps +=
        static_cast<double>(targetSpeedRate) *
        targetSpeedChangeRate *
        dtD;

    motion.targetForwardSpeedMps =
        std::clamp(
            motion.targetForwardSpeedMps,
            0.0,
            maxSpeed
        );

    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop)
    {
        motion.targetForwardSpeedMps = 0.0;
        motion.strafeSpeedMps = 0.0;
        motion.liftSpeedMps = 0.0;
    }

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
