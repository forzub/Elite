#pragma once

#include <cstdint>
#include <string>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/geometry/ObjectAssembly.h"

namespace game::visual
{

using VisualShipId = uint32_t;

struct VisualShip
{
    VisualShipId id = 0;

    std::string shipType;
    std::string shipName;

    const ShipDescriptor* descriptor = nullptr;
    const game::ship::geometry::ObjectAssembly* assembly = nullptr;

    ShipTransform transform;
    ShipTransform renderTransform;

    bool visible = true;

    float alpha = 1.0f;
    float visualScale = 1.0f;

    // Для промо/трафика. Не физика, просто полезная кинематическая инфа.
    glm::vec3 velocity {0.0f};
};

} // namespace game::visual