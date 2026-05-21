#pragma once

#include <cstdint>

enum class ObjectType : uint16_t
{
    None,
    CobraMk1,
    Station,
    Asteroid,
    Planet,

    RepairDroneDebug
};