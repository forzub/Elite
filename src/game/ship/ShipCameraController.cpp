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

    camera.setVisualLean(
        std::clamp(leanRoll,  -3.0f, 3.0f),
        std::clamp(leanPitch, -2.0f, 2.0f)
    );

    camera.setPosition(ship.position);
    camera.setOrientation(ship.pitch, ship.yaw, ship.roll);
}
