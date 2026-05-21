#pragma once

#include <cstdint>
#include <string>

#include "src/world/types/ObjectType.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/geometry/ObjectAssembly.h"

namespace game::visual
{

using VisualDroneId = uint32_t;

enum class VisualDroneKind
{
    Repair,
    Utility,
    Combat
};

struct VisualDrone
{
    VisualDroneId id = 0;

    VisualDroneKind kind = VisualDroneKind::Repair;
    ObjectType type = ObjectType::RepairDroneDebug;

    const game::ship::geometry::ObjectAssembly* assembly = nullptr;

    ShipTransform transform;
    ShipTransform renderTransform;

    bool visible = true;
    float visualScale = 1.0f;

    glm::vec3 velocity {0.0f};
};

} // namespace game::visual