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
    // ---------------- rotation ----------------
    ship.pitchRate += ship.pitchInput * params.angularAccel * dt;
    ship.yawRate   += ship.yawInput   * params.angularAccel * dt;
    ship.rollRate  += ship.rollInput  * params.angularAccel * dt;

    ship.pitchRate = glm::clamp(ship.pitchRate, -params.maxPitchRate, params.maxPitchRate);
    ship.yawRate   = glm::clamp(ship.yawRate,   -params.maxYawRate,   params.maxYawRate);
    ship.rollRate  = glm::clamp(ship.rollRate,  -params.maxRollRate,  params.maxRollRate);

    // Локальные оси корабля берём только через единый transform API
    glm::vec3 right   = ship.right();
    glm::vec3 up      = ship.up();
    glm::vec3 forward = ship.forward();

    // Вращения вокруг локальных осей
    glm::mat4 yawRot   = glm::rotate(glm::mat4(1.0f), ship.yawRate   * dt, up);
    glm::mat4 pitchRot = glm::rotate(glm::mat4(1.0f), ship.pitchRate * dt, right);
    glm::mat4 rollRot  = glm::rotate(glm::mat4(1.0f), ship.rollRate  * dt, forward);

    ship.orientation = yawRot * pitchRot * rollRot * ship.orientation;

    // Угловое демпфирование
    const float angDamp = std::exp(-params.angularDamping * dt);

    if (std::abs(ship.pitchInput) < 0.001f)
        ship.pitchRate *= angDamp;

    if (std::abs(ship.yawInput) < 0.001f)
        ship.yawRate *= angDamp;

    if (std::abs(ship.rollInput) < 0.001f)
        ship.rollRate *= angDamp;

    // Обновляем локальные оси после поворота
    forward = ship.forward();
    right   = ship.right();
    up      = ship.up();

    // ---------------- manoeuvre thrusters ----------------
    ship.localVelocity.x += ship.strafeInput  * params.strafeAccel * dt;
    ship.localVelocity.y += ship.liftInput    * params.strafeAccel * dt;
    ship.localVelocity.z += ship.forwardInput * params.strafeAccel * dt;

    ship.localVelocity = glm::clamp(
        ship.localVelocity,
        glm::vec3(-params.maxStrafeSpeed),
        glm::vec3( params.maxStrafeSpeed)
    );

    const float strafeDamp = std::exp(-params.strafeDamping * dt);
    ship.localVelocity *= strafeDamp;

    // ---------------- linear speed ----------------
    if (!ship.cruiseActive)
    {
        const float targetSpeedAccel = 200.0f;

        ship.targetSpeed += ship.targetSpeedRate * targetSpeedAccel * dt;
        ship.targetSpeed = glm::clamp(ship.targetSpeed, 0.0f, params.maxCombatSpeed);

        const float speedError  = ship.targetSpeed - ship.forwardVelocity;
        const float engineAccel = speedError * params.throttleAccel;
        const float drag        = -ship.forwardVelocity * world.linearDrag;

        ship.forwardVelocity += (engineAccel + drag) * dt;
        ship.forwardVelocity  = glm::max(ship.forwardVelocity, 0.0f);
    }

    // ---------------- position ----------------
    if (ship.cruiseActive)
    {
        const glm::dvec3 delta =
            glm::dvec3(forward) *
            static_cast<double>(params.maxCruiseSpeed) *
            static_cast<double>(dt);

        ship.addWorldMeters(delta);
    }
    else
    {
        const glm::vec3 worldVel =
            forward * (ship.forwardVelocity + ship.forwardInput) +
            right   * ship.localVelocity.x +
            up      * ship.localVelocity.y;

        ship.addWorldMeters(
            glm::dvec3(worldVel) * static_cast<double>(dt)
        );
    }
}