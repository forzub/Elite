#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <glm/glm.hpp>

#include "src/game/ship/physics/ShipImpulseSystem.h"

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testImpactThroughExplicitComProducesNoSpin()
{
    ShipTransform ship;
    ship.setWorldPositionMeters({100.0, 200.0, 300.0});
    ship.orientation = glm::mat4(1.0f);

    ShipParams params {};
    params.massKg = 1000.0;
    params.centerOfMassLocalMeters = {2.0, -1.0, 4.0};
    params.pitchInertiaKgM2 = 100.0;
    params.yawInertiaKgM2 = 120.0;
    params.rollInertiaKgM2 = 140.0;

    const glm::dvec3 comWorld =
        ship.fullWorldMeters() +
        params.centerOfMassLocalMeters;

    const auto result =
        game::ship::physics::ShipImpulseSystem::applyImpulseAtWorldPoint(
            ship,
            params,
            {0.0, 1000.0, 0.0},
            comWorld
        );

    require(
        glm::length(result.deltaAngularVelocityBodyRadPerSec) < 1.0e-12,
        "impact through explicit CoM produced spin"
    );
}

void testOffsetComChangesAngularImpulse()
{
    ShipTransform centered;
    centered.setWorldPositionMeters({0.0, 0.0, 0.0});
    centered.orientation = glm::mat4(1.0f);

    ShipTransform shifted = centered;

    ShipParams base {};
    base.massKg = 1000.0;
    base.centerOfMassLocalMeters = {0.0, 0.0, 0.0};
    base.pitchInertiaKgM2 = 100.0;
    base.yawInertiaKgM2 = 100.0;
    base.rollInertiaKgM2 = 100.0;

    ShipParams offset = base;
    offset.centerOfMassLocalMeters = {2.0, 0.0, 0.0};

    const glm::dvec3 point {0.0, 0.0, 0.0};
    const glm::dvec3 impulse {0.0, 1000.0, 0.0};

    const auto throughOrigin =
        game::ship::physics::ShipImpulseSystem::applyImpulseAtWorldPoint(
            centered,
            base,
            impulse,
            point
        );
    const auto shiftedResult =
        game::ship::physics::ShipImpulseSystem::applyImpulseAtWorldPoint(
            shifted,
            offset,
            impulse,
            point
        );

    require(
        glm::length(throughOrigin.deltaAngularVelocityBodyRadPerSec) <
            1.0e-12,
        "origin-CoM specimen unexpectedly spun"
    );
    require(
        std::abs(shiftedResult.deltaAngularVelocityBodyRadPerSec.z) >
            1.0e-6,
        "offset CoM did not alter angular impulse"
    );
}

} // namespace

int main()
{
    try
    {
        testImpactThroughExplicitComProducesNoSpin();
        testOffsetComChangesAngularImpulse();

        std::cout
            << "SHIP MASS PROPERTIES: PASS\n"
            << " - explicit local CoM defines zero-torque impact line\n"
            << " - shifting CoM changes angular impulse response\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP MASS PROPERTIES: FAIL: "
            << error.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
