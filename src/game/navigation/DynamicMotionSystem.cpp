#include <cmath>
#include <algorithm>
#include <fstream>

#include "DynamicMotionSystem.h"

namespace game::navigation
{

void DynamicMotionSystem::updateHubTactical(
    DynamicMotionState& motion,
    world::coordinates::WorldPosition& worldPosition,
    const HubNavigationFrame& frame,
    double dt
)
{










    motion.referenceVelocityMps =
        frame.velocityMetersPerSecond;

    












    // Глобальная динамика.
    // Истина движения — worldVelocityMps.
    // Гравитация меняет глобальный вектор скорости.
    // Поворот корпуса сам по себе worldVelocity не трогает.
    motion.worldVelocityMps +=
        motion.gravityAccelerationMps2 * dt;

    // Двигатели меняют глобальную скорость только через ускорение.
    motion.worldVelocityMps +=
        motion.engineAccelerationMps2 * dt;

    glm::dvec3 worldMeters =
        world::coordinates::fullMeters(worldPosition);

    worldMeters +=
        motion.worldVelocityMps * dt;


























    worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            worldMeters
        );

    motion.localPositionMeters =
        frame.worldToLocalPosition(worldMeters);

    motion.localVelocityMps =
        frame.worldToLocalVelocity(
            motion.worldVelocityMps
        );
}




void DynamicMotionSystem::applyHubTacticalInput(
    DynamicMotionState& motion,
    const HubNavigationFrame& frame,
    float dt,
    float targetSpeedRate,
    bool cruiseActive,
    float forwardInput,
    float liftInput,
    float strafeInput,
    const glm::vec3& shipForward,
    const glm::vec3& shipRight,
    const glm::vec3& shipUp
)
{
    const double dtD =
        static_cast<double>(dt);

    const double maxCombatSpeed =
        500.0;

    const double maxStrafeSpeed =
        80.0;

    const double targetSpeedAccel =
        200.0;

    const double throttleAccel =
        5.0;

    const double strafeAccel =
        80.0;

    const double strafeDamping =
        4.0;

 

    const glm::dvec3 f =
        glm::normalize(glm::dvec3(shipForward));

    const glm::dvec3 r =
        glm::normalize(glm::dvec3(shipRight));

    const glm::dvec3 u =
        glm::normalize(glm::dvec3(shipUp));

    glm::dvec3 relativeWorldVelocity {0.0};

    if (cruiseActive)
    {
        motion.targetForwardSpeedMps =
            motion.forwardSpeedMps;

        motion.engineAccelerationMps2 =
            glm::dvec3(0.0);

        motion.desiredTacticalVelocityMps =
            glm::dvec3(0.0);


        return;

        
    }
    else
    {
        // +/- меняет ЖЕЛАЕМУЮ скорость, как в старой физике.
        motion.targetForwardSpeedMps +=
            static_cast<double>(targetSpeedRate) *
            targetSpeedAccel *
            dtD;

        motion.targetForwardSpeedMps =
            std::clamp(
                motion.targetForwardSpeedMps,
                0.0,
                maxCombatSpeed
            );

        // forwardSpeed догоняет targetForwardSpeed.
        // Это имитация системы управления двигателями.
        const double speedError =
            motion.targetForwardSpeedMps -
            motion.forwardSpeedMps;

        const double engineAccel =
            speedError * throttleAccel;

        motion.forwardSpeedMps +=
            engineAccel * dtD;

        motion.forwardSpeedMps =
            std::max(
                motion.forwardSpeedMps,
                0.0
            );

        // KP8/KP2 — манёвровый вперёд/назад, не основной throttle.
        // Если он тебе не нужен — потом уберём.
        motion.forwardSpeedMps +=
            static_cast<double>(forwardInput) *
            strafeAccel *
            dtD;

        motion.forwardSpeedMps =
            std::clamp(
                motion.forwardSpeedMps,
                0.0,
                maxCombatSpeed
            );

        // Боковые скорости — как старая localVelocity.x/y.
        motion.strafeSpeedMps +=
            static_cast<double>(strafeInput) *
            strafeAccel *
            dtD;

        motion.liftSpeedMps +=
            static_cast<double>(liftInput) *
            strafeAccel *
            dtD;

        motion.strafeSpeedMps =
            std::clamp(
                motion.strafeSpeedMps,
                -maxStrafeSpeed,
                maxStrafeSpeed
            );

        motion.liftSpeedMps =
            std::clamp(
                motion.liftSpeedMps,
                -maxStrafeSpeed,
                maxStrafeSpeed
            );

        const double damp =
            std::exp(-strafeDamping * dtD);

        motion.strafeSpeedMps *= damp;
        motion.liftSpeedMps   *= damp;

        // Главное:
        // каждый кадр пересобираем скорость из ТЕКУЩИХ осей корабля.
        relativeWorldVelocity =
            f * motion.forwardSpeedMps +
            r * motion.strafeSpeedMps +
            u * motion.liftSpeedMps;
    }

    // ВАЖНО:
    // applyHubTacticalInput НЕ имеет права перезаписывать worldVelocityMps.
    // Он только формирует желаемую локальную скорость относительно ориентира.
    // Это НЕ глобальная скорость.
    // Это только желаемая тактическая скорость относительно текущего frame.
    motion.desiredTacticalVelocityMps =
        relativeWorldVelocity;

    // Ошибка между желаемой тактической скоростью и текущей
    // скоростью корабля относительно reference frame.
    const glm::dvec3 currentRelativeVelocity =
        motion.worldVelocityMps -
        frame.velocityMetersPerSecond;

    const glm::dvec3 velocityError =
        motion.desiredTacticalVelocityMps -
        currentRelativeVelocity;


    const bool haveInput =
        std::abs(targetSpeedRate) > 0.001f ||
        std::abs(forwardInput) > 0.001f ||
        std::abs(strafeInput) > 0.001f ||
        std::abs(liftInput) > 0.001f;

    if (!haveInput)
    {
        motion.engineAccelerationMps2 =
            glm::dvec3(0.0);

        return;
    }

    // Flight assist: двигатели пытаются догнать желаемую локальную скорость,
    // но меняют worldVelocity только через ускорение.
    const double tacticalResponse = 1.0;

    glm::dvec3 desiredAcceleration =
        velocityError * tacticalResponse;

    // Ограничение перегрузки.
    // 5G примерно 49 м/с².
    const double maxTacticalAccel =
        49.0;

    const double accelLen =
        glm::length(desiredAcceleration);

    if (accelLen > maxTacticalAccel)
    {
        desiredAcceleration =
            desiredAcceleration / accelLen * maxTacticalAccel;
    }

    motion.engineAccelerationMps2 =
        desiredAcceleration;




}






} // namespace game::navigation