#pragma once

#include <string>
#include <glm/glm.hpp>
#include "src/game/navigation/NavigationPlan.h"

namespace game::navigation
{

enum class MotionMode
{
    Inertial,
    Orbital,
    HubTactical,
    Cruise,
    JumpTransit,
    Docked,
    Reentry,
    Destroyed
};

struct DynamicMotionState
{
    MotionMode mode = MotionMode::Inertial;

    std::string parentBodyId;
    std::string hubId;

    bool pendingReferenceVelocityMatch = false;
    // Скорость большой системы отсчёта:
    // орбита планеты, хаба, коридора и т.д.
    glm::dvec3 referenceVelocityMps {0.0};

    // Локальная позиция относительно текущего frame.
    glm::dvec3 localPositionMeters {0.0};

    // Локальная скорость относительно текущего frame.
    glm::dvec3 localVelocityMps {0.0};
    // Flight-assist слой.
    // Это не мировая инерционная скорость.
    // Это управляемые скорости корабля относительно текущего frame.
    double targetForwardSpeedMps = 0.0;
    double forwardSpeedMps = 0.0;
    double strafeSpeedMps = 0.0;
    double liftSpeedMps = 0.0;

    // Tactical engine intent.
    // Это ускорение от двигателей в мировых координатах.
    // Оно меняет worldVelocityMps, но не заменяет его.
    glm::dvec3 engineAccelerationMps2 {0.0};

    // Диагностика: желаемая локальная тактическая скорость,
    // не глобальная скорость корабля.
    glm::dvec3 desiredTacticalVelocityMps {0.0};

    glm::dvec3 worldVelocityMps {0.0};

    // Локальная желаемая скорость относительно навигационного ориентира.
    // Это НЕ вся глобальная скорость корабля.
    glm::dvec3 desiredRelativeVelocityMps {0.0};

    // Автовыравнивание по орбитальному коридору.
    // Автовыравнивание по орбитальному коридору.
    // Пока выключено. Это будет использоваться позже,
    // когда игрок промахнулся мимо орбитального туннеля.
    bool orbitalAssistEnabled = false;

    double orbitalAssistMaxAngleDeg = 30.0;
    double orbitalAssistStrength = 0.35;

    // Гравитационный слой.
    glm::dvec3 gravityAccelerationMps2 {0.0};

    std::string primaryGravityBodyId;
    double primaryGravityDistanceMeters = 0.0;
    double primaryGravityAltitudeMeters = 0.0;
    double primaryGravityAccelerationMps2 = 0.0;

    // Орбитальный коридор.
    std::string orbitalCorridorId;
    int orbitalCorridorState = 0;

    double orbitalAltitudeMeters = 0.0;
    double orbitalAltitudeErrorMeters = 0.0;
    double orbitalTargetSpeedMps = 0.0;
    double orbitalTangentialSpeedMps = 0.0;
    double orbitalRadialSpeedMps = 0.0;
    double orbitalSpeedErrorMps = 0.0;

    // Будущий Cruise/Jump слой.
    NavigationPlan navigationPlan;

    // Орбитальный слой.
    double altitudeMeters = 0.0;
    double orbitalPhaseRadians = 0.0;
    double planeOffsetMeters = 0.0;

    bool lockedToFramePosition = false;
};

} // namespace game::navigation