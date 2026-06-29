#include "ShipController.h"
#include <cstdio>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>




void ShipController::update(
    float dt,
    const ShipParams& params,
    ShipTransform& ship,
    const WorldParams& world
)
{
    (void)world;

    // ---------------- attitude / rotation only ----------------
    ship.pitchRate += ship.pitchInput * params.angularAccel * dt;
    ship.yawRate   += ship.yawInput   * params.angularAccel * dt;
    ship.rollRate  += ship.rollInput  * params.angularAccel * dt;

    ship.pitchRate =
        glm::clamp(
            ship.pitchRate,
            -params.maxPitchRate,
            params.maxPitchRate
        );

    ship.yawRate =
        glm::clamp(
            ship.yawRate,
            -params.maxYawRate,
            params.maxYawRate
        );

    ship.rollRate =
        glm::clamp(
            ship.rollRate,
            -params.maxRollRate,
            params.maxRollRate
        );

    const glm::vec3 right =
        ship.right();

    const glm::vec3 up =
        ship.up();

    const glm::vec3 forward =
        ship.forward();

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

    // ВАЖНО:
    // Позицию, линейную скорость, cruise, tactical,
    // referenceVelocity здесь больше НЕ трогаем.
    //
    // Единственный владелец линейного движения:
    // game::navigation::DynamicMotionSystem.
}