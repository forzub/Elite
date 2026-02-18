#include "ShipController.h"
#include <cstdio>

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

    

    // локальные оси
    glm::vec3 right   = glm::vec3(ship.orientation * glm::vec4(1,0,0,0));
    glm::vec3 up      = glm::vec3(ship.orientation * glm::vec4(0,1,0,0));
    glm::vec3 forward = glm::vec3(ship.orientation * glm::vec4(0,0,-1,0));

    // вращения
    glm::mat4 yawRot   = glm::rotate(glm::mat4(1.0f), ship.yawRate   * dt, up);
    glm::mat4 pitchRot = glm::rotate(glm::mat4(1.0f), ship.pitchRate * dt, right);
    glm::mat4 rollRot  = glm::rotate(glm::mat4(1.0f), ship.rollRate  * dt, forward);

    ship.orientation = yawRot * pitchRot * rollRot * ship.orientation;

    // угловое демпфирование (инерционное затухание)
    float angDamp = std::exp(-params.angularDamping * dt);

    if (std::abs(ship.pitchInput) < 0.001f)
        ship.pitchRate *= angDamp;

    if (std::abs(ship.yawInput) < 0.001f)
        ship.yawRate *= angDamp;

    if (std::abs(ship.rollInput) < 0.001f)
        ship.rollRate *= angDamp;

    // демпфирование
    // float angDamp = std::exp(-params.angularDamping * dt);
    const float g = 9.81f;
    float maxAngularSpeed = std::sqrt((params.maxGs * g) / params.turnRadius);


    // обновляем локальные оси после поворота
    forward = glm::vec3(ship.orientation * glm::vec4(0,0,-1,0));
    right   = glm::vec3(ship.orientation * glm::vec4(1,0,0,0));
    up      = glm::vec3(ship.orientation * glm::vec4(0,1,0,0));




    // ---------------- manoeuvre thrusters ----------------
    ship.localVelocity.x += ship.strafeInput  * params.strafeAccel * dt;
    ship.localVelocity.y += ship.liftInput    * params.strafeAccel * dt;
    ship.localVelocity.z += ship.forwardInput * params.strafeAccel * dt;

    ship.localVelocity = glm::clamp(
        ship.localVelocity,
        glm::vec3(-params.maxStrafeSpeed),
        glm::vec3( params.maxStrafeSpeed)
    );

    float strafeDamp = std::exp(-params.strafeDamping * dt);
    ship.localVelocity *= strafeDamp;

    // ---------------- linear speed ----------------
    if (!ship.cruiseActive)
    {
        const float targetSpeedAccel = 200.0f;

        ship.targetSpeed += ship.targetSpeedRate * targetSpeedAccel * dt;
        ship.targetSpeed = glm::clamp(ship.targetSpeed, 0.0f, params.maxCombatSpeed);

        float speedError  = ship.targetSpeed - ship.forwardVelocity;
        float engineAccel = speedError * params.throttleAccel;
        float drag        = -ship.forwardVelocity * world.linearDrag;

        ship.forwardVelocity += (engineAccel + drag) * dt;
        ship.forwardVelocity  = glm::max(ship.forwardVelocity, 0.0f);
    }

    // ---------------- position ----------------
    if (ship.cruiseActive)
    {
        ship.position += forward * params.maxCruiseSpeed * dt;
    }
    else
    {
        glm::vec3 worldVel =
            forward * (ship.forwardVelocity + ship.forwardInput) +
            right   * ship.localVelocity.x +
            up      * ship.localVelocity.y;

        ship.position += worldVel * dt;
    }

   
}