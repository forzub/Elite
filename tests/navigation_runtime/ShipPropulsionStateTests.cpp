#include "src/game/ship/ShipPropulsionState.h"

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
    descriptor.physics.maxGs = 5.0f;
    descriptor.physics.maxLinearGs = 7.5f;
    descriptor.physics.forwardMainEngineAvailable = true;
    descriptor.physics.reverseMainEngineAvailable = true;
    descriptor.physics.forwardMainEngineAccelerationMps2 = 73.549875f;
    descriptor.physics.reverseMainEngineAccelerationMps2 = 73.549875f;
    descriptor.physics.manoeuvreThrusterAccel = 2.0f;
    descriptor.physics.massKg = 1.0;
    descriptor.physics.pitchInertiaKgM2 = 1.0;
    descriptor.physics.yawInertiaKgM2 = 1.0;
    descriptor.physics.rollInertiaKgM2 = 1.0;

    descriptor.mainPropulsion.aftEngineModuleIds = {
        "aft_l",
        "aft_r"
    };
    descriptor.mainPropulsion.foreEngineModuleIds = {
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

void testHealthyBanksKeepFullDescriptorAuthority()
{
    const ShipDescriptor descriptor = makeDescriptor();
    const auto modules = healthyModules();
    const ShipParams effective =
        game::ship::effectiveShipPhysics(descriptor, modules);

    require(effective.forwardMainEngineAvailable,
            "healthy aft bank was disabled");
    require(effective.reverseMainEngineAvailable,
            "healthy fore bank was disabled");

    requireNear(
        game::ship::forwardMainAccelerationLimitMps2(effective),
        game::ship::forwardMainAccelerationLimitMps2(descriptor.physics),
        1.0e-6,
        "healthy aft bank lost thrust"
    );
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(effective),
        game::ship::reverseMainAccelerationLimitMps2(descriptor.physics),
        1.0e-6,
        "healthy fore bank lost thrust"
    );
}

void testLowHealthDoesNotDerateOperationalMainEngine()
{
    const ShipDescriptor descriptor = makeDescriptor();
    auto modules = healthyModules();
    modules[2].health = 1.0f;

    const ShipParams effective =
        game::ship::effectiveShipPhysics(descriptor, modules);

    require(effective.reverseMainEngineAvailable,
            "nonzero-health fore engine was treated as failed");
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(effective),
        game::ship::reverseMainAccelerationLimitMps2(descriptor.physics),
        1.0e-6,
        "operational fore bank was proportionally derated"
    );
}

void testForeBankFailureIsBinary()
{
    using State = world::modules::ObjectModuleRuntimeState;
    const ShipDescriptor descriptor = makeDescriptor();
    auto modules = healthyModules();
    modules[2] = module("fore_l", State::Destroyed, 0.0f);

    const ShipParams effective =
        game::ship::effectiveShipPhysics(descriptor, modules);

    require(effective.forwardMainEngineAvailable,
            "fore failure incorrectly disabled aft main");
    require(!effective.reverseMainEngineAvailable,
            "failed fore bank retained reverse main authority");
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(effective),
        0.0,
        1.0e-12,
        "failed fore bank was partially derated instead of disabled"
    );
}

void testAftBankFailureLeavesFullForeAuthority()
{
    using State = world::modules::ObjectModuleRuntimeState;
    const ShipDescriptor descriptor = makeDescriptor();
    auto modules = healthyModules();
    modules[0] = module("aft_l", State::Disabled, 0.0f);

    const ShipParams effective =
        game::ship::effectiveShipPhysics(descriptor, modules);

    require(!effective.forwardMainEngineAvailable,
            "failed aft bank retained forward main authority");
    require(effective.reverseMainEngineAvailable,
            "aft failure incorrectly disabled fore main");
    requireNear(
        game::ship::reverseMainAccelerationLimitMps2(effective),
        game::ship::reverseMainAccelerationLimitMps2(descriptor.physics),
        1.0e-6,
        "surviving fore bank did not retain full thrust"
    );
}

} // namespace

int main()
{
    try
    {
        testHealthyBanksKeepFullDescriptorAuthority();
        testLowHealthDoesNotDerateOperationalMainEngine();
        testForeBankFailureIsBinary();
        testAftBankFailureLeavesFullForeAuthority();

        std::cout << "SHIP PROPULSION STATE TESTS: PASS\n";
        std::cout << " - operational main banks retain full descriptor thrust\n";
        std::cout << " - main-engine health is binary, never proportionally derated\n";
        std::cout << " - fore/aft bank failures remain direction-specific\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SHIP PROPULSION STATE TESTS: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
