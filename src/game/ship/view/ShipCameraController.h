#pragma once

#include "render/Camera.h"
#include "game/ship/core/ShipTransform.h"
#include "src/game/ship/view/ShipCameraMode.h"

class ShipCameraController
{
public:
    void update(
        float dt,
        const ShipTransform& ship,
        Camera& camera
    );


    void updateCockpit(
        float dt,
        ShipTransform& transform,
        Camera& camera
    );

    void updateRear(
        float dt,
        ShipTransform& transform,
        Camera& camera
    );

    void updateDrone(
        float dt,
        ShipTransform& transform,
        Camera& camera
    );

    void updateMode(
        ShipCameraMode mode,
        float dt,
        const ShipTransform& transform,
        Camera& camera
    );
};