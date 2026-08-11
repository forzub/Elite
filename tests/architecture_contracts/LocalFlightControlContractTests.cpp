#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/ShipController.h"
#include "src/world/WorldParams.h"
#include "src/world/coordinates/WorldPosition.h"

namespace
{
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, double epsilon, const char* label)
{
    if (std::abs(actual - expected) > epsilon)
    {
        throw std::runtime_error(
            std::string(label) + ": expected " + std::to_string(expected) +
            ", got " + std::to_string(actual)
        );
    }
}

game::navigation::KinematicFrame makeFrame()
{
    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "ship_travel_test";
    frame.originMeters = glm::dvec3(1000.0, 2000.0, 3000.0);
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;
    return frame;
}

ShipParams makeParams()
{
    ShipParams params{};
    params.maxCombatSpeed = 100.0f;
    params.maxCruiseSpeed = 100000.0f;
    params.throttleAccel = 5.0f;
    params.strafeAccel = 20.0f;
    params.strafeDamping = 6.0f;
    params.maxStrafeSpeed = 40.0f;
    params.maxGs = 2.0f;
    params.angularAccel = 2.0f;
    params.maxPitchRate = 2.0f;
    params.maxYawRate = 2.0f;
    params.maxRollRate = 2.0f;
    params.angularDamping = 2.0f;
    return params;
}

void step(
    game::navigation::DynamicMotionState& motion,
    world::coordinates::WorldPosition& position,
    const game::navigation::KinematicFrame& frame,
    const ShipParams& params,
    float dt,
    float longitudinal,
    const glm::vec3& forward = glm::vec3(0.0f, 0.0f, -1.0f)
)
{
    game::navigation::DynamicMotionSystem::applyLocalFrameInput(
        motion,
        frame,
        params,
        dt,
        longitudinal,
        false,
        0.0f,
        0.0f,
        0.0f,
        forward,
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
        motion,
        position,
        frame,
        params,
        static_cast<double>(dt)
    );
}

void testNewtonianCoastsWithoutInput()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    motion.localVelocityMps = glm::dvec3(30.0, -4.0, 2.0);

    auto position = world::coordinates::makeWorldPositionFromMeters(
        frame.localToWorldPosition(glm::dvec3(0.0))
    );

    const glm::dvec3 before = motion.localVelocityMps;
    step(motion, position, frame, params, 0.5f, 0.0f);

    require(glm::length(motion.localVelocityMps - before) < 1.0e-12,
            "Newtonian coast changed VREL with no thrust");
}

void testNewtonianHullDirectionDoesNotRotateVelocity()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState a;
    a.localControlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    a.localVelocityMps = glm::dvec3(40.0, 5.0, -3.0);

    auto b = a;

    auto posA = world::coordinates::makeWorldPositionFromMeters(frame.originMeters);
    auto posB = posA;

    step(a, posA, frame, params, 0.25f, 0.0f, glm::vec3(0.0f, 0.0f, -1.0f));
    step(b, posB, frame, params, 0.25f, 0.0f, glm::vec3(1.0f, 0.0f, 0.0f));

    require(glm::length(a.localVelocityMps - b.localVelocityMps) < 1.0e-12,
            "Newtonian hull rotation changed velocity without thrust");
}

void testNewtonianThrustAndLocalSpeedCap()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Newtonian;

    auto position = world::coordinates::makeWorldPositionFromMeters(frame.originMeters);

    for (int i = 0; i < 200; ++i)
        step(motion, position, frame, params, 0.1f, 1.0f);

    const double speed = glm::length(motion.localVelocityMps);
    require(speed > 1.0, "Newtonian + did not create local acceleration");
    require(speed <= static_cast<double>(params.maxCombatSpeed) + 1.0e-9,
            "Newtonian local speed escaped ship maxCombatSpeed");
}

void testAssistedVelocityFollowsNoseWithBoundedAcceleration()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    motion.localVelocityMps = glm::dvec3(50.0, 0.0, 0.0);
    motion.targetForwardSpeedMps = 50.0;

    auto position = world::coordinates::makeWorldPositionFromMeters(frame.originMeters);

    game::navigation::DynamicMotionSystem::applyLocalFrameInput(
        motion,
        frame,
        params,
        0.1f,
        0.0f,
        false,
        0.0f,
        0.0f,
        0.0f,
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    const double accel = glm::length(motion.engineAccelerationMps2);
    const double maxAccel = static_cast<double>(params.maxGs) * 9.80665;

    require(accel > 0.1, "Assisted law stopped correcting velocity after hull turn");
    require(accel <= maxAccel + 1.0e-9,
            "Assisted controller exceeded ship maxGs envelope");

    game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
        motion,
        position,
        frame,
        params,
        0.1
    );

    require(motion.localVelocityMps.z < 0.0,
            "Assisted controller did not bend VREL toward nose");
}


void testAngularMotionUsesSharedLoadEnvelope()
{
    ShipParams params = makeParams();
    params.angularAccel = 100.0f;
    params.maxPitchRate = 100.0f;
    params.maxYawRate = 100.0f;
    params.maxRollRate = 100.0f;
    params.maxGs = 2.0f;
    params.turnRadius = 10.0f;
    params.angularDamping = 0.0f;

    ShipTransform ship;
    ship.pitchInput = 1.0f;
    ship.yawInput = 1.0f;
    ship.rollInput = 1.0f;

    WorldParams world;
    ShipController controller;
    controller.update(1.0f, params, ship, world);

    const float angularRate = glm::length(glm::vec3(
        ship.pitchRate, ship.yawRate, ship.rollRate
    ));
    const float safeRate = std::sqrt(
        params.maxGs * 9.80665f / params.turnRadius
    );

    require(angularRate <= safeRate + 1.0e-5f,
            "combined angular motion escaped the ship load envelope");
}

void testNewtonianEndBrakesOnlyAfterAntiVelocityAlignment()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    motion.velocityAlignmentMode =
        game::navigation::VelocityAlignmentMode::BrakeToStop;
    motion.localVelocityMps = glm::dvec3(0.0, 0.0, -20.0);

    auto position = world::coordinates::makeWorldPositionFromMeters(frame.originMeters);

    // Velocity points -Z. A nose pointing -Z is not tail-to-velocity yet.
    game::navigation::DynamicMotionSystem::applyLocalFrameInput(
        motion, frame, params, 0.1f, 0.0f, false,
        0.0f, 0.0f, 0.0f,
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    requireNear(glm::length(motion.engineAccelerationMps2), 0.0, 1.0e-12,
                "END applied braking before tail alignment");

    // Nose +Z means tail -Z, aligned with the velocity vector.
    for (int i = 0; i < 20 &&
         motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BrakeToStop;
         ++i)
    {
        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            motion, frame, params, 0.1f, 0.0f, false,
            0.0f, 0.0f, 0.0f,
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            motion, position, frame, params, 0.1
        );
    }

    require(glm::length(motion.localVelocityMps) < 20.0,
            "END did not reduce Newtonian local speed after alignment");
}
}

int main()
{
    try
    {
        testNewtonianCoastsWithoutInput();
        testNewtonianHullDirectionDoesNotRotateVelocity();
        testNewtonianThrustAndLocalSpeedCap();
        testAssistedVelocityFollowsNoseWithBoundedAcceleration();
        testAngularMotionUsesSharedLoadEnvelope();
        testNewtonianEndBrakesOnlyAfterAntiVelocityAlignment();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Local flight control contract failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Local flight control contract passed.\n";
    return EXIT_SUCCESS;
}
