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
constexpr float AlignmentInputGain = 3.0f;
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

bool applyVelocityAlignmentAttitude(
    ShipTransform& ship
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

    // Once the normal bounded controller has brought the nose very close to
    // target, snap only the tiny residual angle. This provides the requested
    // exact HOME/INSERT alignment without an instantaneous large rotation.
    if (angle <= AlignmentSnapAngleRad &&
        std::abs(ship.pitchRate) < 0.03f &&
        std::abs(ship.yawRate) < 0.03f)
    {
        const glm::vec3 axis = glm::cross(currentForward, desiredForward);
        const float axisLength = glm::length(axis);
        if (axisLength > 1.0e-6f && angle > 1.0e-7f)
        {
            ship.orientation =
                glm::rotate(
                    glm::mat4(1.0f),
                    angle,
                    axis / axisLength
                ) * ship.orientation;
        }

        ship.pitchRate = 0.0f;
        ship.yawRate = 0.0f;

        if (ship.motion.velocityAlignmentMode !=
                VelocityAlignmentMode::BrakeToStop)
        {
            ship.motion.velocityAlignmentMode =
                VelocityAlignmentMode::None;
        }
        return true;
    }

    // Pitch/yaw inputs are fed through the exact same angular acceleration and
    // rate limits as manual controls. No pilot-only teleport/snap turn exists.
    ship.pitchInput = std::clamp(
        glm::dot(desiredForward, up) * AlignmentInputGain,
        -1.0f,
        1.0f
    );

    // Positive yaw around ship-up turns -Z toward -X in this basis, hence the
    // sign inversion for a target lying on +right.
    ship.yawInput = std::clamp(
        -glm::dot(desiredForward, right) * AlignmentInputGain,
        -1.0f,
        1.0f
    );

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

    (void)applyVelocityAlignmentAttitude(ship);

    // ---------------- attitude / rotation only ----------------
    // The same descriptor-driven envelope is used for player ships, NPCs and
    // any future drones that use ShipController. maxGs + turnRadius prevent a
    // craft from requesting angular dynamics that imply absurd local loads.
    glm::vec3 angularInput(
        ship.pitchInput,
        ship.yawInput,
        ship.rollInput
    );
    const float angularInputLength = glm::length(angularInput);
    if (angularInputLength > 1.0f)
        angularInput /= angularInputLength;

    const float safeAngularAccel = angularAccelerationEnvelope(params);
    ship.pitchRate += angularInput.x * safeAngularAccel * dt;
    ship.yawRate   += angularInput.y * safeAngularAccel * dt;
    ship.rollRate  += angularInput.z * safeAngularAccel * dt;

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

    ship.pitchRate = glm::clamp(
        ship.pitchRate, -maxPitchRate, maxPitchRate
    );
    ship.yawRate = glm::clamp(
        ship.yawRate, -maxYawRate, maxYawRate
    );
    ship.rollRate = glm::clamp(
        ship.rollRate, -maxRollRate, maxRollRate
    );

    // Combined-axis rotation is also bounded: holding pitch+yaw+roll at once
    // must not bypass the same characteristic-radius load envelope.
    glm::vec3 angularRateVector(
        ship.pitchRate,
        ship.yawRate,
        ship.rollRate
    );
    const float angularRateLength = glm::length(angularRateVector);
    if (std::isfinite(safeAngularRate) &&
        angularRateLength > safeAngularRate &&
        angularRateLength > 1.0e-6f)
    {
        angularRateVector *= safeAngularRate / angularRateLength;
        ship.pitchRate = angularRateVector.x;
        ship.yawRate = angularRateVector.y;
        ship.rollRate = angularRateVector.z;
    }

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

    const float angDamp =
        std::exp(-params.angularDamping * dt);

    if (std::abs(ship.pitchInput) < 0.001f)
        ship.pitchRate *= angDamp;

    if (std::abs(ship.yawInput) < 0.001f)
        ship.yawRate *= angDamp;

    if (std::abs(ship.rollInput) < 0.001f)
        ship.rollRate *= angDamp;

    // Linear translation is exclusively owned by DynamicMotionSystem.
}
