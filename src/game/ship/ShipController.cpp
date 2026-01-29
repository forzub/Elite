#include "src/game/ship/ShipController.h"

#include "ShipController.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>




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

    ship.pitch += ship.pitchRate * dt;
    ship.yaw   += ship.yawRate   * dt;
    ship.roll  += ship.rollRate  * dt;

    float angDamp = std::exp(-params.angularDamping * dt);
    ship.pitchRate *= angDamp;
    ship.yawRate   *= angDamp;
    ship.rollRate  *= angDamp;

    // ---------------- orientation ----------------
    glm::mat4 rot(1.0f);
    rot = glm::rotate(rot, glm::radians(ship.yaw),   {0,1,0});
    rot = glm::rotate(rot, glm::radians(ship.pitch),{1,0,0});
    rot = glm::rotate(rot, glm::radians(ship.roll), {0,0,1});

    glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0,0,-1,0)));
    glm::vec3 right   = glm::normalize(glm::vec3(rot * glm::vec4(1,0,0,0)));
    glm::vec3 up      = glm::normalize(glm::vec3(rot * glm::vec4(0,1,0,0)));

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
            forward * ship.forwardVelocity +
            right   * ship.localVelocity.x +
            up      * ship.localVelocity.y;

        ship.position += worldVel * dt;
    }
}