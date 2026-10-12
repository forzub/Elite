#include "src/game/ship/ShipPropulsionState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

ShipDescriptor makeDescriptor()
{
    ShipDescriptor descriptor;
    descriptor.physicalControlModel =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    descriptor.localFlightControlLaw =
        game::ship::plant::nativeFlightLaw(
            descriptor.physicalControlModel
        );

    descriptor.physics.maxPitchRate = 2.5f;
    descriptor.physics.maxYawRate = 2.5f;
    descriptor.physics.maxRollRate = 3.0f;
    descriptor.physics.angularAccel = 3.0f;
    descriptor.physics.maxGs = 5.0f;
    descriptor.physics.maxLinearGs = 7.5f;
    descriptor.physics.forwardMainEngineAvailable = true;
    descriptor.physics.reverseMainEngineAvailable = true;
    descriptor.physics.forwardMainEngineAccelerationMps2 = 73.549875f;
    descriptor.physics.reverseMainEngineAccelerationMps2 = 73.549875f;
    descriptor.physics.strafeAccel = 73.549875f;
    descriptor.physics.manoeuvreThrusterAccel = 2.0f;
    descriptor.physics.massKg = 1.0;
    descriptor.physics.pitchInertiaKgM2 = 1.0;
    descriptor.physics.yawInertiaKgM2 = 1.0;
    descriptor.physics.rollInertiaKgM2 = 1.0;

    descriptor.physicalControl.slotModuleIds = {
        "aft_l",
        "aft_r",
        "fore_l",
        "fore_r"
    };

    return descriptor;
}

game::simulation::ObjectModuleSnapshot module(
    const char* id,
    world::modules::ObjectModuleRuntimeState state,
    float health
)
{
    game::simulation::ObjectModuleSnapshot out;
    out.moduleId = id;
    out.state = static_cast<std::uint8_t>(state);
    out.health = health;
    return out;
}

std::vector<game::simulation::ObjectModuleSnapshot> healthyModules()
{
    using State = world::modules::ObjectModuleRuntimeState;
    return {
        module("aft_l", State::Attached, 100.0f),
        module("aft_r", State::Attached, 100.0f),
        module("fore_l", State::Attached, 100.0f),
        module("fore_r", State::Attached, 100.0f)
    };
}

void testHealthyTopologyKeepsNominalAuthority()
{
    const ShipDescriptor descriptor = makeDescriptor();
    const auto modules = healthyModules();
    const auto capability =
        game::ship::capability::buildShipCapability(
            descriptor,
            modules
        );

    require(capability.valid, "healthy capability is invalid");
    require(
        capability.controlState ==
            game::ship::capability::ControlHealthState::Normal,
        "healthy Assisted-I was not NORMAL"
    );

    requireNear(
        game::ship::forwardMainAccelerationLimitMps2(
            capability.effectivePhysics
        ),
        game::ship::forwardMainAccelerationLimitMps2(
            descriptor.physics
        ),
        1.0e-6,
        "healthy aft pair lost thrust"
    );
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(
            capability.effectivePhysics
        ),
        game::ship::reverseMainAccelerationLimitMps2(
            descriptor.physics
        ),
        1.0e-6,
        "healthy fore pair lost thrust"
    );
}

void testPartialHealthDeratesOnlyItsShare()
{
    const ShipDescriptor descriptor = makeDescriptor();
    auto modules = healthyModules();

    // One of two fore engines has 50% health. The fore bank therefore retains
    // (1.0 + 0.5) / 2 = 75% of nominal braking authority.
    modules[2].health = 50.0f;

    const auto capability =
        game::ship::capability::buildShipCapability(
            descriptor,
            modules
        );

    require(capability.valid, "degraded capability is invalid");
    require(
        capability.controlState ==
            game::ship::capability::ControlHealthState::Degraded,
        "partial engine health did not report DEGRADED control"
    );

    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(
            capability.effectivePhysics
        ),
        73.549875 * 0.75,
        1.0e-4,
        "partial fore-engine health did not proportionally derate bank"
    );

    requireNear(
        game::ship::forwardMainAccelerationLimitMps2(
            capability.effectivePhysics
        ),
        73.549875,
        1.0e-4,
        "fore degradation leaked into aft authority"
    );
}

void testSingleForeFailureLeavesHalfReverseAuthority()
{
    using State = world::modules::ObjectModuleRuntimeState;
    const ShipDescriptor descriptor = makeDescriptor();
    auto modules = healthyModules();

    modules[2] =
        module("fore_l", State::Destroyed, 0.0f);

    const ShipParams effective =
        game::ship::effectiveShipPhysics(
            descriptor,
            modules
        );

    require(
        effective.forwardMainEngineAvailable,
        "single fore failure incorrectly disabled aft propulsion"
    );
    require(
        effective.reverseMainEngineAvailable,
        "single fore failure incorrectly removed the surviving fore engine"
    );
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(effective),
        73.549875 * 0.5,
        1.0e-4,
        "single fore failure did not leave half reverse authority"
    );
}

void testCompleteForeLossFallsBackWithoutCrashing()
{
    using State = world::modules::ObjectModuleRuntimeState;
    const ShipDescriptor descriptor = makeDescriptor();
    auto modules = healthyModules();

    modules[2] =
        module("fore_l", State::Destroyed, 0.0f);
    modules[3] =
        module("fore_r", State::Destroyed, 0.0f);

    const auto capability =
        game::ship::capability::buildShipCapability(
            descriptor,
            modules
        );

    require(capability.valid, "fore-bank loss invalidated ship capability");
    require(
        capability.controlState ==
            game::ship::capability::ControlHealthState::Degraded,
        "fore-bank loss should be degraded thrust-axis fallback"
    );
    require(
        !capability.effectivePhysics.reverseMainEngineAvailable,
        "destroyed fore bank retained reverse main authority"
    );
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(
            capability.effectivePhysics
        ),
        0.0,
        1.0e-12,
        "destroyed fore bank retained nonzero reverse thrust"
    );
    require(
        capability.effectivePhysics.forwardMainEngineAvailable,
        "fore-bank loss incorrectly disabled surviving aft propulsion"
    );
}

}

int main()
{
    try
    {
        testHealthyTopologyKeepsNominalAuthority();
        testPartialHealthDeratesOnlyItsShare();
        testSingleForeFailureLeavesHalfReverseAuthority();
        testCompleteForeLossFallsBackWithoutCrashing();

        std::cout << "SHIP PROPULSION STATE TESTS: PASS\n";
        std::cout << " - topology + module health produce runtime capability\n";
        std::cout << " - partial damage proportionally derates its actuator share\n";
        std::cout << " - one failed engine no longer kills an entire bank\n";
        std::cout << " - complete fore loss becomes degraded fallback, not an error\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SHIP PROPULSION STATE TESTS: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
