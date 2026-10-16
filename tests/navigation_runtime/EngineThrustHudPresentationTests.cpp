#include "src/game/presentation/EngineThrustHudPresentation.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Model =
    game::ship::plant::PhysicalControlModelId;
using Direction =
    game::presentation::EngineThrustFillDirection;

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

ShipParams nominalPhysics()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;
    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 73.549875f;
    p.reverseMainEngineAccelerationMps2 = 73.549875f;
    p.strafeAccel = 73.549875f;
    p.strafeDamping = 6.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;
    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

ShipDescriptor descriptorFor(Model model)
{
    ShipDescriptor descriptor;
    descriptor.physicalControlModel = model;
    descriptor.localFlightControlLaw =
        game::ship::plant::nativeFlightLaw(model);
    descriptor.physics = nominalPhysics();
    return descriptor;
}

ClientShipState shipFor(const ShipDescriptor& descriptor)
{
    ClientShipState ship {};
    ship.descriptor = &descriptor;
    ship.physicalControlModel = descriptor.physicalControlModel;
    ship.transform.orientation = glm::mat4(1.0f);
    ship.transform.motion.localControlLaw =
        descriptor.localFlightControlLaw;
    return ship;
}

std::size_t totalBars(
    const game::presentation::EngineThrustHudPresentation& p
)
{
    std::size_t count = 0;
    for (const auto& group : p.groups)
        count += group.bars.size();
    return count;
}

void testAssistedLayoutGrouping()
{
    {
        const auto descriptor = descriptorFor(Model::AssistedI);
        auto ship = shipFor(descriptor);
        const auto p =
            game::presentation::buildEngineThrustHudPresentation(
                ship
            );

        require(p.visible, "Assisted-I engine HUD is hidden");
        require(p.groups.size() == 2,
                "Assisted-I must render two paired groups");
        require(totalBars(p) == 4,
                "Assisted-I must render four main-engine bars");

        require(
            p.groups[0].bars[0].fillDirection == Direction::BottomUp &&
            p.groups[0].bars[1].fillDirection == Direction::TopDown &&
            p.groups[1].bars[0].fillDirection == Direction::TopDown &&
            p.groups[1].bars[1].fillDirection == Direction::BottomUp,
            "Assisted-I must keep aft bars outside and fore bars inside"
        );
    }

    {
        const auto descriptor = descriptorFor(Model::AssistedY);
        auto ship = shipFor(descriptor);
        const auto p =
            game::presentation::buildEngineThrustHudPresentation(
                ship
            );

        require(p.groups.size() == 3,
                "Assisted-Y must render three paired groups");
        require(totalBars(p) == 6,
                "Assisted-Y must render six main-engine bars");
    }

    {
        const auto descriptor = descriptorFor(Model::AssistedX);
        auto ship = shipFor(descriptor);
        const auto p =
            game::presentation::buildEngineThrustHudPresentation(
                ship
            );

        require(p.groups.size() == 4,
                "Assisted-X must render four paired groups");
        require(totalBars(p) == 8,
                "Assisted-X must render eight main-engine bars");
    }
}

void testRuntimeModelOverridesDescriptorModel()
{
    const auto descriptor = descriptorFor(Model::AssistedI);
    auto ship = shipFor(descriptor);

    ship.physicalControlModel = Model::AssistedY;
    auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    require(p.modelLabel == "ASSISTED-Y",
            "HUD label ignored replicated runtime model");
    require(p.groups.size() == 3,
            "runtime Assisted-Y must replace descriptor Assisted-I geometry");
    require(totalBars(p) == 6,
            "runtime Assisted-Y must render six main-engine bars");

    ship.physicalControlModel = Model::NewtonianX;
    p = game::presentation::buildEngineThrustHudPresentation(ship);

    require(p.modelLabel == "NEWTONIAN-X",
            "HUD label ignored runtime Newtonian family");
    require(p.groups.size() == 4,
            "runtime Newtonian-X must render four physical main groups");
    require(totalBars(p) == 4,
            "runtime Newtonian-X must render four aft main engines only");

    for (const auto& group : p.groups)
    {
        for (const auto& bar : group.bars)
        {
            require(
                bar.fillDirection == Direction::BottomUp,
                "runtime Newtonian-X exposed a fore-main bar"
            );
        }
    }
}

void testNewtonianNeverRendersForeMainBars()
{
    constexpr Model models[] = {
        Model::NewtonianI,
        Model::NewtonianY,
        Model::NewtonianX
    };

    for (const auto model : models)
    {
        const auto descriptor = descriptorFor(model);
        auto ship = shipFor(descriptor);
        const auto p =
            game::presentation::buildEngineThrustHudPresentation(
                ship
            );

        require(p.visible, "Newtonian engine HUD is hidden");

        for (const auto& group : p.groups)
        {
            for (const auto& bar : group.bars)
            {
                require(
                    bar.fillDirection == Direction::BottomUp,
                    "Newtonian HUD exposed a fore-main/top-down bar"
                );
            }
        }
    }
}

void testAssistedForwardThrustUsesAftBars()
{
    const auto descriptor = descriptorFor(Model::AssistedI);
    auto ship = shipFor(descriptor);

    const glm::dvec3 forward =
        glm::normalize(glm::dvec3(ship.transform.forward()));

    ship.transform.motion.engineAccelerationMps2 =
        forward * (73.549875 * 0.5);

    const auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    for (const auto& group : p.groups)
    {
        for (const auto& bar : group.bars)
        {
            requireNear(
                bar.capability01,
                1.0,
                1.0e-6,
                "healthy slot lost yellow capability"
            );

            const double expected =
                bar.fillDirection == Direction::BottomUp
                    ? 0.5
                    : 0.0;

            requireNear(
                bar.thrust01,
                expected,
                1.0e-5,
                "forward Assisted thrust mapped to wrong engine bars"
            );
        }
    }
}

void testAssistedLateralVectoringUsesBothBanks()
{
    const auto descriptor = descriptorFor(Model::AssistedI);
    auto ship = shipFor(descriptor);

    const glm::dvec3 right =
        glm::normalize(glm::dvec3(ship.transform.right()));

    ship.transform.motion.engineAccelerationMps2 =
        right * (73.549875 * 0.5);

    const auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    for (const auto& group : p.groups)
    {
        for (const auto& bar : group.bars)
        {
            requireNear(
                bar.thrust01,
                0.5,
                1.0e-5,
                "lateral Assisted vectoring must load both main banks"
            );
        }
    }
}

} // namespace

int main()
{
    try
    {
        testAssistedLayoutGrouping();
        testRuntimeModelOverridesDescriptorModel();
        testNewtonianNeverRendersForeMainBars();
        testAssistedForwardThrustUsesAftBars();
        testAssistedLateralVectoringUsesBothBanks();

        std::cout
            << "ENGINE THRUST HUD PRESENTATION: PASS\n"
            << " - Assisted I/Y/X render 2/3/4 paired groups\n"
            << " - runtime model overrides descriptor spawn model\n"
            << " - Newtonian HUD never renders fore-main bars\n"
            << " - yellow bars are per-slot capability\n"
            << " - red bars follow axial and vectoring main thrust\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ENGINE THRUST HUD PRESENTATION: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
