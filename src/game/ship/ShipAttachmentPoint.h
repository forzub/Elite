#pragma once

#include <string>
#include <glm/glm.hpp>

enum class ShipAttachmentKind
{
    CameraCockpit,
    CameraRear,
    CameraDrone,

    DroneDock,
    MissileRack,
    ContainerMount,
    WeaponMuzzle,

    Generic
};

struct ShipAttachmentPoint
{
    std::string id;

    // К какому модулю attachment логически привязан.
    // Можно оставить пустым, если живёт в корневом пространстве корабля.
    std::string parentModuleId;

    ShipAttachmentKind kind = ShipAttachmentKind::Generic;

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};

    bool enabled = true;
};