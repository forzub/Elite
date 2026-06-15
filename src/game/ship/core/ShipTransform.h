// // ShipState что с ним сейчас#pragma once
// // состояние корабля в настоящий момент
// // Это единственный mutable-блок.



#pragma once

#include "src/game/math/MathTransform.h"
#include "src/game/navigation/DynamicMotionState.h"

struct ShipTransform : public game::math::MathTransform
{
    float pitchRate = 0.0f;
    float yawRate   = 0.0f;
    float rollRate  = 0.0f;

    float pitchInput = 0.0f;
    float yawInput   = 0.0f;
    float rollInput  = 0.0f;

    bool cruiseActive = false;
    bool jumpActive   = false;

    float targetSpeed = 0.0f;
    float forwardVelocity = 0.0f;
    float targetSpeedRate = 0.0f;

    glm::vec3 localVelocity {0.0f, 0.0f, 0.0f};
    // Скорость системы отсчёта, в которой находится корабль.
    // Например: орбитальная скорость станции/хаба/планеты.
    // Управление кораблём НЕ должно её демпфировать.
    glm::dvec3 referenceVelocityMetersPerSecond {0.0};


    // Скорость корабля относительно системы отсчёта.
    glm::vec3 relativeVelocityMetersPerSecond {0.0f};
    game::navigation::DynamicMotionState motion;

    float strafeInput  = 0.0f;
    float liftInput    = 0.0f;
    float forwardInput = 0.0f;

};