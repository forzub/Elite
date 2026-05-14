#pragma once

#include <string>
#include <glm/glm.hpp>

enum class ShipAttachmentKind
{
    CameraCockpit,
    CameraRear,
    CameraDrone,

    DroneDock,

    // Точка выхода дрона из дока перед началом миссии.
    // Это НЕ место крепления. Это первая безопасная внешняя точка.
    DroneLaunch,

    // Точка ожидания перед посадкой/стыковкой обратно в док.
    DroneRecovery,

    // Внешние навигационные точки вокруг корпуса корабля.
    // Через них дрон обходит корабль, а не летит сквозь него.
    RepairWorkPoint,

    // Точки монтажа/замены оборудования.
    // Пока не используем в repair runtime напрямую, но сразу резервируем тип.
    EquipmentMount,

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