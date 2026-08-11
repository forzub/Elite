#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/ShipController.h"
#include "src/game/ship/physics/ShipImpulseSystem.h"
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
    params.massKg = 10.0;
    params.pitchInertiaKgM2 = 20.0;
    params.yawInertiaKgM2 = 25.0;
    params.rollInertiaKgM2 = 30.0;
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

void testNewtonianMinusDoesNotCreateReverseMainThrust()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    motion.localVelocityMps = glm::dvec3(0.0, 0.0, -30.0);

    auto position =
        world::coordinates::makeWorldPositionFromMeters(frame.originMeters);

    const glm::dvec3 before = motion.localVelocityMps;
    step(motion, position, frame, params, 0.25f, -1.0f);

    require(
        glm::length(motion.localVelocityMps - before) < 1.0e-12,
        "Newtonian '-' still created reverse main-engine thrust"
    );
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


void testAssistedThrottleReleaseCapturesReachedSpeed()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    auto exerciseRelease = [&](float trim, const char* label)
    {
        game::navigation::DynamicMotionState motion;
        motion.localControlLaw =
            game::navigation::LocalFlightControlLaw::Assisted;
        motion.localVelocityMps = glm::dvec3(0.0, 0.0, -40.0);
        motion.targetForwardSpeedMps = 40.0;

        auto position =
            world::coordinates::makeWorldPositionFromMeters(frame.originMeters);

        // Hold +/- long enough to create a target that the bounded engine has
        // not reached yet. This is the exact state that used to keep changing
        // speed after the pilot released the key.
        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            motion, frame, params, 0.25f, trim, false,
            0.0f, 0.0f, 0.0f,
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            motion, position, frame, params, 0.25
        );

        const double reachedSpeed = glm::length(motion.localVelocityMps);
        require(
            std::abs(motion.targetForwardSpeedMps - reachedSpeed) > 0.5,
            std::string(label) + " setup did not leave a pending Assisted target"
        );

        // Releasing the trim must capture the reached speed immediately. With
        // the hull already aligned to VREL this also means zero longitudinal
        // acceleration on the release frame.
        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            motion, frame, params, 0.1f, 0.0f, false,
            0.0f, 0.0f, 0.0f,
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        requireNear(
            motion.targetForwardSpeedMps,
            reachedSpeed,
            1.0e-9,
            label
        );
        requireNear(
            glm::length(motion.engineAccelerationMps2),
            0.0,
            1.0e-9,
            "Assisted release retained stale longitudinal acceleration"
        );

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            motion, position, frame, params, 0.1
        );
        requireNear(
            glm::length(motion.localVelocityMps),
            reachedSpeed,
            1.0e-9,
            "Assisted speed kept changing after trim release"
        );
    };

    exerciseRelease(+1.0f, "Assisted '+' release did not capture reached speed");
    exerciseRelease(-1.0f, "Assisted '-' release did not capture reached speed");
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


float forwardAngleTo(
    const ShipTransform& ship,
    const glm::vec3& desiredForward
)
{
    return std::acos(std::clamp(
        glm::dot(ship.forward(), glm::normalize(desiredForward)),
        -1.0f,
        1.0f
    ));
}

void testVelocityAlignmentBrakesBeforeTarget()
{
    const auto frame = makeFrame();
    ShipParams params = makeParams();
    params.angularAccel = 1.5f;
    params.maxPitchRate = 1.2f;
    params.maxYawRate = 1.2f;
    params.maxRollRate = 1.2f;
    params.angularDamping = 0.0f;
    params.maxGs = 0.0f;
    params.turnRadius = 0.0f;

    ShipTransform ship;
    ship.motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    ship.motion.velocityAlignmentMode =
        game::navigation::VelocityAlignmentMode::ForwardToVelocity;
    ship.motion.travelFrame = frame;
    ship.motion.localVelocityMps = glm::dvec3(0.0, 0.0, -50.0);
    ship.orientation = glm::rotate(
        glm::mat4(1.0f),
        glm::radians(100.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    WorldParams world;
    ShipController controller;

    constexpr float dt = 1.0f / 60.0f;
    float previousRate = 0.0f;
    float previousAngle = forwardAngleTo(
        ship, glm::vec3(0.0f, 0.0f, -1.0f)
    );
    float peakRate = 0.0f;
    float brakeStartAngle = -1.0f;

    for (int i = 0; i < 600; ++i)
    {
        controller.update(dt, params, ship, world);

        const float angle = forwardAngleTo(
            ship, glm::vec3(0.0f, 0.0f, -1.0f)
        );
        const float rate = glm::length(glm::vec2(
            ship.pitchRate, ship.yawRate
        ));

        require(
            angle <= previousAngle + glm::radians(0.2f),
            "velocity-alignment controller visibly overshot its target"
        );

        if (brakeStartAngle < 0.0f &&
            peakRate > 0.2f &&
            rate + 1.0e-4f < previousRate)
        {
            brakeStartAngle = angle;
        }

        peakRate = std::max(peakRate, rate);
        previousRate = rate;
        previousAngle = angle;

        if (ship.motion.velocityAlignmentMode ==
                game::navigation::VelocityAlignmentMode::None)
        {
            break;
        }
    }

    require(brakeStartAngle > glm::radians(5.0f),
            "velocity alignment did not begin angular braking before the target");
    require(ship.motion.velocityAlignmentMode ==
                game::navigation::VelocityAlignmentMode::None,
            "velocity alignment did not converge");
    require(forwardAngleTo(ship, glm::vec3(0.0f, 0.0f, -1.0f)) <=
                glm::radians(0.3f),
            "velocity alignment did not finish on the velocity vector");
    require(std::abs(ship.pitchRate) < 1.0e-6f &&
                std::abs(ship.yawRate) < 1.0e-6f,
            "velocity alignment finished with residual angular rate");
}

void testVelocityAlignmentEscapesExactAntiparallelPose()
{
    const auto frame = makeFrame();
    ShipParams params = makeParams();
    params.angularDamping = 0.0f;
    params.maxGs = 0.0f;
    params.turnRadius = 0.0f;

    ShipTransform ship;
    ship.motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    ship.motion.velocityAlignmentMode =
        game::navigation::VelocityAlignmentMode::ForwardToVelocity;
    ship.motion.travelFrame = frame;
    ship.motion.localVelocityMps = glm::dvec3(0.0, 0.0, -25.0);
    ship.orientation = glm::rotate(
        glm::mat4(1.0f),
        glm::radians(180.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    const float initialAngle = forwardAngleTo(
        ship, glm::vec3(0.0f, 0.0f, -1.0f)
    );

    WorldParams world;
    ShipController controller;
    for (int i = 0; i < 20; ++i)
        controller.update(1.0f / 60.0f, params, ship, world);

    require(
        forwardAngleTo(ship, glm::vec3(0.0f, 0.0f, -1.0f)) <
            initialAngle - glm::radians(1.0f),
        "exact 180-degree velocity alignment remained stuck"
    );
}


void testExternalOverspeedIsPhysicalAndNotHardClamped()
{
    const auto frame = makeFrame();
    const auto params = makeParams();

    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    motion.localVelocityMps = glm::dvec3(0.0, 0.0, -150.0);

    auto position = world::coordinates::makeWorldPositionFromMeters(frame.originMeters);

    // No force: an externally acquired overspeed must persist exactly.
    step(motion, position, frame, params, 0.25f, 0.0f);
    requireNear(
        glm::length(motion.localVelocityMps),
        150.0,
        1.0e-9,
        "external overspeed was hard-clamped by normal flight envelope"
    );

    // Main engine pointing with VREL may not increase the external overspeed.
    step(
        motion, position, frame, params, 0.25f, 1.0f,
        glm::vec3(0.0f, 0.0f, -1.0f)
    );
    requireNear(
        glm::length(motion.localVelocityMps),
        150.0,
        1.0e-6,
        "normal propulsion increased speed above its control envelope"
    );

    // Turn around and use the same main engine: braking back toward the
    // controlled envelope is allowed and must happen through real acceleration.
    step(
        motion, position, frame, params, 0.25f, 1.0f,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );
    require(
        glm::length(motion.localVelocityMps) < 150.0,
        "normal propulsion could not brake external overspeed"
    );
}

void testImpulseAtCentreAddsLinearVelocityWithoutSpin()
{
    const auto frame = makeFrame();
    auto params = makeParams();

    ShipTransform ship;
    ship.motion.travelFrame = frame;
    ship.motion.localPositionMeters = glm::dvec3(0.0);
    ship.setWorldPositionMeters(frame.originMeters);

    const glm::dvec3 impulse(100.0, 0.0, 0.0);
    const auto result = game::ship::physics::ShipImpulseSystem::applyImpulseAtWorldPoint(
        ship, params, impulse, frame.originMeters
    );

    requireNear(result.deltaVelocityWorldMps.x, 10.0, 1.0e-9,
                "centre impulse linear delta-V");
    requireNear(glm::length(ship.motion.localVelocityMps), 10.0, 1.0e-9,
                "centre impulse did not reach physical VREL");
    requireNear(ship.pitchRate, 0.0, 1.0e-9, "centre impulse pitch spin");
    requireNear(ship.yawRate, 0.0, 1.0e-9, "centre impulse yaw spin");
    requireNear(ship.rollRate, 0.0, 1.0e-9, "centre impulse roll spin");
}

void testOffCentreImpulseAddsAngularVelocity()
{
    const auto frame = makeFrame();
    auto params = makeParams();

    ShipTransform ship;
    ship.motion.travelFrame = frame;
    ship.motion.localPositionMeters = glm::dvec3(0.0);
    ship.setWorldPositionMeters(frame.originMeters);

    // Hit 10 m out on the right wing with an impulse toward ship forward (-Z).
    // r x J = +Y angular impulse, so the identity-oriented hull receives yaw.
    const glm::dvec3 contact = frame.originMeters + glm::dvec3(10.0, 0.0, 0.0);
    const glm::dvec3 impulse(0.0, 0.0, -100.0);
    const auto result = game::ship::physics::ShipImpulseSystem::applyImpulseAtWorldPoint(
        ship, params, impulse, contact
    );

    requireNear(
        result.deltaAngularVelocityBodyRadPerSec.y,
        1000.0 / params.yawInertiaKgM2,
        1.0e-9,
        "off-centre impact yaw delta-omega"
    );
    require(std::abs(ship.yawRate) > 0.01f,
            "off-centre impact failed to spin the hull");
}

void testExternalSpinIsNotHardClampedAndRcsBrakesIt()
{
    auto params = makeParams();
    params.maxGs = 0.0f;
    params.turnRadius = 0.0f;
    params.maxPitchRate = 1.0f;
    params.maxYawRate = 1.0f;
    params.maxRollRate = 1.0f;
    params.angularAccel = 2.0f;

    WorldParams world;
    ShipController controller;

    ShipTransform freeSpin;
    freeSpin.yawRate = 5.0f;
    params.angularDamping = 0.0f;
    controller.update(0.5f, params, freeSpin, world);
    requireNear(
        freeSpin.yawRate,
        5.0,
        1.0e-6,
        "external spin was hard-clamped to controlled yaw rate"
    );

    ShipTransform stabilizedSpin;
    stabilizedSpin.yawRate = 5.0f;
    params.angularDamping = 2.5f;
    controller.update(0.5f, params, stabilizedSpin, world);

    require(stabilizedSpin.yawRate < 5.0f,
            "RCS stabilization did not brake external spin");
    require(stabilizedSpin.yawRate >= 4.0f - 1.0e-5f,
            "RCS deleted external spin faster than angular thrust allows");
    require(stabilizedSpin.yawRate > params.maxYawRate,
            "RCS magically snapped external spin back inside rate envelope");
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
        if (glm::length(motion.engineAccelerationMps2) > 1.0e-9)
        {
            require(
                glm::dot(
                    glm::normalize(motion.engineAccelerationMps2),
                    glm::dvec3(0.0, 0.0, 1.0)
                ) > 0.999,
                "END braking no longer uses the ship's forward main-engine direction"
            );
        }
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
        testExternalOverspeedIsPhysicalAndNotHardClamped();
        testImpulseAtCentreAddsLinearVelocityWithoutSpin();
        testOffCentreImpulseAddsAngularVelocity();
        testExternalSpinIsNotHardClampedAndRcsBrakesIt();
        testNewtonianMinusDoesNotCreateReverseMainThrust();
        testAssistedVelocityFollowsNoseWithBoundedAcceleration();
        testAssistedThrottleReleaseCapturesReachedSpeed();
        testAngularMotionUsesSharedLoadEnvelope();
        testVelocityAlignmentBrakesBeforeTarget();
        testVelocityAlignmentEscapesExactAntiparallelPose();
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
