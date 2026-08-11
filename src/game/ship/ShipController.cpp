#include "ShipController.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr double MinAlignmentSpeedMps = 0.05;
constexpr float AlignmentSnapAngleRad = 0.25f * 3.14159265358979323846f / 180.0f;
constexpr float AlignmentAxisEpsilon = 1.0e-6f;
constexpr float StandardGravityMps2 = 9.80665f;

float angularAccelerationEnvelope(const ShipParams& params)
{
    const float configured = std::max(0.0f, params.angularAccel);
    if (params.maxGs <= 0.0f || params.turnRadius <= 0.0f)
        return configured;

    // Characteristic-radius safety envelope shared by crewed and uncrewed
    // craft. Crewed ships normally have the lower maxGs; drones may advertise
    // a higher structural/equipment envelope while using the same equations.
    const float byLinearLoad =
        params.maxGs * StandardGravityMps2 / params.turnRadius;
    return std::min(configured, std::max(0.0f, byLinearLoad));
}

float angularRateEnvelope(const ShipParams& params)
{
    if (params.maxGs <= 0.0f || params.turnRadius <= 0.0f)
        return std::numeric_limits<float>::infinity();

    return std::sqrt(
        params.maxGs * StandardGravityMps2 / params.turnRadius
    );
}

float limitAxisControlAcceleration(
    float currentRate,
    float requestedAcceleration,
    float controlledRateLimit,
    float dt
)
{
    if (dt <= 0.0f || controlledRateLimit <= 0.0f)
        return 0.0f;

    const float currentAbs = std::abs(currentRate);
    const float candidate = currentRate + requestedAcceleration * dt;

    // External angular impulse may put the craft above its normal RCS/crew
    // envelope. Never clamp that physical spin. Control torque may reduce it,
    // but may not drive the same axis farther out.
    if (currentAbs > controlledRateLimit)
    {
        if (std::abs(candidate) <= currentAbs + 1.0e-6f)
            return requestedAcceleration;
        return 0.0f;
    }

    if (std::abs(candidate) <= controlledRateLimit + 1.0e-6f)
        return requestedAcceleration;

    const float targetRate =
        std::copysign(controlledRateLimit, candidate);
    return (targetRate - currentRate) / dt;
}

glm::vec3 limitAngularDeltaToControlledMagnitude(
    const glm::vec3& currentRate,
    const glm::vec3& requestedAcceleration,
    float controlledRateLimit,
    float dt
)
{
    if (dt <= 0.0f || !std::isfinite(controlledRateLimit))
        return requestedAcceleration;
    if (controlledRateLimit <= 0.0f)
        return glm::vec3(0.0f);

    const glm::vec3 delta = requestedAcceleration * dt;
    const float currentMagnitude = glm::length(currentRate);
    const float allowedRadius = std::max(controlledRateLimit, currentMagnitude);
    const glm::vec3 candidate = currentRate + delta;
    if (glm::length(candidate) <= allowedRadius + 1.0e-6f)
        return requestedAcceleration;

    const float a = glm::dot(delta, delta);
    if (a <= 1.0e-12f)
        return glm::vec3(0.0f);

    const float b = 2.0f * glm::dot(currentRate, delta);
    const float c = glm::dot(currentRate, currentRate) -
                    allowedRadius * allowedRadius;
    const float discriminant = std::max(0.0f, b * b - 4.0f * a * c);
    const float root = (-b + std::sqrt(discriminant)) / (2.0f * a);
    const float fraction = std::clamp(root, 0.0f, 1.0f);
    return requestedAcceleration * fraction;
}

bool isNewtonianVelocityAlignment(
    const ShipTransform& ship
)
{
    using game::navigation::LocalFlightControlLaw;
    using game::navigation::VelocityAlignmentMode;

    if (ship.motion.localControlLaw != LocalFlightControlLaw::Newtonian)
        return false;

    return
        ship.motion.velocityAlignmentMode ==
            VelocityAlignmentMode::ForwardToVelocity ||
        ship.motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BackwardToVelocity ||
        ship.motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BrakeToStop;
}

float alignmentRateLimitAlongDirection(
    const glm::vec2& direction,
    float maxPitchRate,
    float maxYawRate,
    float safeAngularRate
)
{
    float limit = safeAngularRate;

    if (std::abs(direction.x) > AlignmentAxisEpsilon)
        limit = std::min(limit, maxPitchRate / std::abs(direction.x));

    if (std::abs(direction.y) > AlignmentAxisEpsilon)
        limit = std::min(limit, maxYawRate / std::abs(direction.y));

    if (!std::isfinite(limit))
        return std::numeric_limits<float>::infinity();

    return std::max(0.0f, limit);
}

glm::vec2 predictiveAlignmentInput(
    const glm::vec2& angularError,
    const glm::vec2& currentRate,
    float dt,
    float safeAngularAccel,
    float maxPitchRate,
    float maxYawRate,
    float safeAngularRate
)
{
    const float remainingAngle = glm::length(angularError);
    if (remainingAngle <= AlignmentAxisEpsilon ||
        dt <= 0.0f ||
        safeAngularAccel <= AlignmentAxisEpsilon)
    {
        return glm::vec2(0.0f);
    }

    const glm::vec2 direction = angularError / remainingAngle;
    const float rateTowardTarget = glm::dot(currentRate, direction);

    // Time-optimal bounded turn with one fixed-step of look-ahead.  The
    // continuous stopping relation is theta_stop = omega^2 / (2 * alpha),
    // equivalently omega_allowed = sqrt(2 * alpha * theta_remaining).
    // Subtracting one frame of already accumulated angular travel makes the
    // controller start braking before the nose reaches the target instead of
    // discovering the overshoot at the target itself.
    const float reactionTravel =
        std::max(0.0f, rateTowardTarget) * dt;
    const float brakingAngleBudget =
        std::max(0.0f, remainingAngle - reactionTravel);
    const float brakingLimitedRate =
        std::sqrt(2.0f * safeAngularAccel * brakingAngleBudget);

    const float directionalRateLimit = alignmentRateLimitAlongDirection(
        direction,
        maxPitchRate,
        maxYawRate,
        safeAngularRate
    );

    const float targetRateMagnitude = std::min(
        directionalRateLimit,
        brakingLimitedRate
    );
    const glm::vec2 targetRate = direction * targetRateMagnitude;

    glm::vec2 requestedInput =
        (targetRate - currentRate) / (safeAngularAccel * dt);

    const float inputLength = glm::length(requestedInput);
    if (inputLength > 1.0f)
        requestedInput /= inputLength;

    return requestedInput;
}

bool applyVelocityAlignmentAttitude(
    ShipTransform& ship,
    float dt,
    float safeAngularAccel,
    float maxPitchRate,
    float maxYawRate,
    float safeAngularRate
)
{
    using game::navigation::VelocityAlignmentMode;

    if (!isNewtonianVelocityAlignment(ship) ||
        !ship.motion.travelFrame.valid)
    {
        return false;
    }

    const double localSpeed =
        glm::length(ship.motion.localVelocityMps);

    if (localSpeed <= MinAlignmentSpeedMps)
    {
        if (ship.motion.velocityAlignmentMode !=
                VelocityAlignmentMode::BrakeToStop)
        {
            ship.motion.velocityAlignmentMode =
                VelocityAlignmentMode::None;
        }
        return false;
    }

    glm::dvec3 velocityWorld =
        ship.motion.travelFrame.localToWorldVector(
            ship.motion.localVelocityMps
        );

    const double velocityWorldLength = glm::length(velocityWorld);
    if (velocityWorldLength <= 1.0e-9)
        return false;

    velocityWorld /= velocityWorldLength;

    glm::vec3 desiredForward = glm::vec3(velocityWorld);
    if (ship.motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BackwardToVelocity ||
        ship.motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BrakeToStop)
    {
        desiredForward = -desiredForward;
    }

    desiredForward = glm::normalize(desiredForward);

    const glm::vec3 currentForward = ship.forward();
    const glm::vec3 right = ship.right();
    const glm::vec3 up = ship.up();

    const float cosAngle = std::clamp(
        glm::dot(currentForward, desiredForward),
        -1.0f,
        1.0f
    );
    const float angle = std::acos(cosAngle);

    glm::vec3 correctionAxis =
        glm::cross(currentForward, desiredForward);
    float axisLength = glm::length(correctionAxis);

    // cross(a, -a) is undefined as a turn direction.  Pick the current up
    // axis for an exact 180-degree HOME/INSERT command so the autopilot does
    // not stall at the one orientation where every projected error is zero.
    if (axisLength <= AlignmentAxisEpsilon && cosAngle < 0.0f)
    {
        correctionAxis = up;
        axisLength = 1.0f;
    }
    else if (axisLength > AlignmentAxisEpsilon)
    {
        correctionAxis /= axisLength;
    }

    // Once the bounded controller has already shed almost all angular rate,
    // snap only the tiny residual angle. This provides exact HOME/INSERT
    // alignment without an instantaneous large rotation.
    if (angle <= AlignmentSnapAngleRad &&
        std::abs(ship.pitchRate) < 0.03f &&
        std::abs(ship.yawRate) < 0.03f)
    {
        if (axisLength > AlignmentAxisEpsilon && angle > 1.0e-7f)
        {
            ship.orientation =
                glm::rotate(
                    glm::mat4(1.0f),
                    angle,
                    correctionAxis
                ) * ship.orientation;
        }

        ship.pitchRate = 0.0f;
        ship.yawRate = 0.0f;
        ship.pitchInput = 0.0f;
        ship.yawInput = 0.0f;
        ship.rollInput = 0.0f;

        if (ship.motion.velocityAlignmentMode !=
                VelocityAlignmentMode::BrakeToStop)
        {
            ship.motion.velocityAlignmentMode =
                VelocityAlignmentMode::None;
        }
        return true;
    }

    // Represent the shortest forward-vector correction as a local pitch/yaw
    // rotation vector.  Its magnitude is the remaining turn angle.
    const glm::vec3 correctionRotation = correctionAxis * angle;
    const glm::vec2 angularError(
        glm::dot(correctionRotation, right),
        glm::dot(correctionRotation, up)
    );
    const glm::vec2 currentRate(ship.pitchRate, ship.yawRate);

    // Unlike the old proportional-to-angle controller, this explicitly
    // reserves enough angle to stop the current angular velocity.  HOME,
    // INSERT and the alignment phase of END therefore brake before the target
    // orientation instead of beginning to brake after passing it.
    const glm::vec2 alignmentInput = predictiveAlignmentInput(
        angularError,
        currentRate,
        dt,
        safeAngularAccel,
        maxPitchRate,
        maxYawRate,
        safeAngularRate
    );

    ship.pitchInput = alignmentInput.x;
    ship.yawInput = alignmentInput.y;
    ship.rollInput = 0.0f;
    return true;
}
}

void ShipController::update(
    float dt,
    const ShipParams& params,
    ShipTransform& ship,
    const WorldParams& world
)
{
    (void)world;

    // ---------------- attitude / rotation only ----------------
    // Resolve the common descriptor-driven envelope before either manual or
    // automatic attitude control. The predictive HOME/INSERT/END controller
    // uses these exact same limits to calculate its braking point.
    const float safeAngularAccel = angularAccelerationEnvelope(params);
    const float safeAngularRate = angularRateEnvelope(params);
    const float maxPitchRate = std::min(
        std::max(0.0f, params.maxPitchRate),
        safeAngularRate
    );
    const float maxYawRate = std::min(
        std::max(0.0f, params.maxYawRate),
        safeAngularRate
    );
    const float maxRollRate = std::min(
        std::max(0.0f, params.maxRollRate),
        safeAngularRate
    );

    (void)applyVelocityAlignmentAttitude(
        ship,
        dt,
        safeAngularAccel,
        maxPitchRate,
        maxYawRate,
        safeAngularRate
    );

    // The same envelope is used for player ships, NPCs and any future drones
    // that use ShipController. The limits describe CONTROL AUTHORITY, not a
    // hard clamp on physical spin: an off-centre collision may produce rates
    // above them, and the RCS then needs real time/torque to recover.
    glm::vec3 angularInput(
        ship.pitchInput,
        ship.yawInput,
        ship.rollInput
    );
    const float angularInputLength = glm::length(angularInput);
    if (angularInputLength > 1.0f)
        angularInput /= angularInputLength;

    glm::vec3 currentRate(
        ship.pitchRate,
        ship.yawRate,
        ship.rollRate
    );

    glm::vec3 requestedAngularAcceleration =
        angularInput * safeAngularAccel;

    // Stability damping is also RCS torque. It must obey the same available
    // angular acceleration instead of exponentially deleting collision spin.
    const float dampingGain = std::max(0.0f, params.angularDamping);
    if (std::abs(ship.pitchInput) < 0.001f)
        requestedAngularAcceleration.x += -ship.pitchRate * dampingGain;
    if (std::abs(ship.yawInput) < 0.001f)
        requestedAngularAcceleration.y += -ship.yawRate * dampingGain;
    if (std::abs(ship.rollInput) < 0.001f)
        requestedAngularAcceleration.z += -ship.rollRate * dampingGain;

    const float requestedAccelLength =
        glm::length(requestedAngularAcceleration);
    if (requestedAccelLength > safeAngularAccel &&
        requestedAccelLength > 1.0e-6f)
    {
        requestedAngularAcceleration *=
            safeAngularAccel / requestedAccelLength;
    }

    requestedAngularAcceleration.x = limitAxisControlAcceleration(
        ship.pitchRate, requestedAngularAcceleration.x, maxPitchRate, dt
    );
    requestedAngularAcceleration.y = limitAxisControlAcceleration(
        ship.yawRate, requestedAngularAcceleration.y, maxYawRate, dt
    );
    requestedAngularAcceleration.z = limitAxisControlAcceleration(
        ship.rollRate, requestedAngularAcceleration.z, maxRollRate, dt
    );

    requestedAngularAcceleration =
        limitAngularDeltaToControlledMagnitude(
            currentRate,
            requestedAngularAcceleration,
            safeAngularRate,
            dt
        );

    ship.pitchRate += requestedAngularAcceleration.x * dt;
    ship.yawRate   += requestedAngularAcceleration.y * dt;
    ship.rollRate  += requestedAngularAcceleration.z * dt;

    const glm::vec3 right = ship.right();
    const glm::vec3 up = ship.up();
    const glm::vec3 forward = ship.forward();

    const glm::mat4 yawRot =
        glm::rotate(
            glm::mat4(1.0f),
            ship.yawRate * dt,
            up
        );

    const glm::mat4 pitchRot =
        glm::rotate(
            glm::mat4(1.0f),
            ship.pitchRate * dt,
            right
        );

    const glm::mat4 rollRot =
        glm::rotate(
            glm::mat4(1.0f),
            ship.rollRate * dt,
            forward
        );

    ship.orientation =
        yawRot *
        pitchRot *
        rollRot *
        ship.orientation;

    // Linear translation is exclusively owned by DynamicMotionSystem.
}
