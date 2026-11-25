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
    // Duration of continuous manual input on each attitude axis. Navigation
    // uses a separate angular-demand path and does not read this state.
    glm::vec3 manualAngularHoldSeconds {0.0f};

    bool cruiseActive = false;
    bool jumpActive   = false;

    float targetSpeedRate = 0.0f;

    // Скорость системы отсчёта, в которой находится корабль.
    // Например: орбитальная скорость станции/хаба/планеты.
    // Управление кораблём НЕ должно её демпфировать.
    glm::dvec3 referenceVelocityMetersPerSecond {0.0};

    game::navigation::DynamicMotionState motion;

    // DERIVED from motion - the one owner of the ship's kinematics (2026-11-24 duplicated-truth pass; the dead mirrors
    // targetSpeed / forwardVelocity / localVelocity / relativeVelocityMetersPerSecond, written by no physics, were
    // removed 2026-11-25 with wire schema 75)
    glm::dvec3 relativeVelocityMps() const { return motion.worldVelocityMps - motion.referenceVelocityMps; }
    double forwardSpeedMps() const { return glm::dot(relativeVelocityMps(), glm::dvec3(forward())); }
    double commandedForwardSpeedMps() const { return motion.targetForwardSpeedMps; }

    float strafeInput  = 0.0f;
    float liftInput    = 0.0f;
    float forwardInput = 0.0f;

};
