#include "ShipCameraController.h"
#include <glm/glm.hpp>
#include <algorithm>

void ShipCameraController::update(
    float /*dt*/,
    const ShipTransform& ship,
    Camera& camera
)
{
    float leanRoll  = 0.0f;
    float leanPitch = 0.0f;

    if (!ship.cruiseActive)
    {
        leanRoll  = -ship.localVelocity.x * 0.05f;
        leanPitch =  ship.localVelocity.z * 0.03f;
    }

    camera.setPosition(ship.position);
    camera.setOrientationMatrix(ship.orientation);
}


void ShipCameraController::updateCockpit(
    float dt,
    ShipTransform& transform,
    Camera& camera
)
{
    camera.setPosition(transform.position);
    camera.setOrientationMatrix(transform.orientation);
}



void ShipCameraController::updateRear(
    float,
    ShipTransform& transform,
    Camera& camera)
{
    glm::mat4 rearOrientation =
    transform.orientation *
    glm::rotate(
        glm::mat4(1.0f),
        glm::radians(180.0f),
        glm::vec3(0,1,0)
    );

    camera.setPosition(transform.position);
    camera.setOrientationMatrix(rearOrientation);
}



void ShipCameraController::updateDrone(
    float,
    ShipTransform& transform,
    Camera& camera)
{
    glm::vec3 pos = transform.position + glm::vec3(0.0f, 30.0f, 0.0f);

    camera.setPosition(pos);

    glm::mat4 look =
        glm::lookAt(
            pos,
            transform.position,
            glm::vec3(0,1,0)
        );

    // camera.setOrientationMatrix(glm::inverse(look));
    glm::mat4 world = glm::inverse(look);

    // берём только вращение
    glm::mat4 rotation = glm::mat4(glm::mat3(world));
    camera.setOrientationMatrix(rotation);
}










void ShipCameraController::updateMode(
    ShipCameraMode mode,
    float dt,
    const ShipTransform& transform,
    Camera& camera)
{
    switch (mode)
    {
        case ShipCameraMode::Cockpit:
            camera.setPosition(transform.position);
            camera.setOrientationMatrix(transform.orientation);
            break;

        case ShipCameraMode::Rear:
        {
            glm::mat4 rearOrientation =
                transform.orientation *
                glm::rotate(
                    glm::mat4(1.0f),
                    glm::radians(180.0f),
                    glm::vec3(0,1,0)
                );

            camera.setPosition(transform.position);
            camera.setOrientationMatrix(rearOrientation);
            break;
        }

        default:
            break;
    }
}