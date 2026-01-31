#pragma once

#include "render/Camera.h"
#include "game/ship/ShipTransform.h"

class ShipCameraController
{
public:
    void update(
        float dt,
        const ShipTransform& ship,
        Camera& camera
    );
};