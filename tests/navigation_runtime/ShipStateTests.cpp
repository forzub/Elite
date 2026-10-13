#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/game/ship/state/ShipState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"

namespace
{

using Model = game::ship::plant::PhysicalControlModelId;
using Family = game::ship::plant::PhysicalControlFamily;
using Source = game::ship::state::ShipActuatorStateSource;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipDescriptor descriptor()
{
    ShipDescriptor d {};
    d.physicalControlModel = Model::AssistedI;
    d.localFlightControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    d.physics.forwardMainEngineAvailable = true;
    d.physics.reverseMainEngineAvailable = true;
    d.physics.forwardMainEngineAccelerationMps2 = 60.0f;
    d.physics.reverseMainEngineAccelerationMps2 = 60.0f;
    d.physics.strafeAccel = 40.0f;
    d.physics.manoeuvreThrusterAccel = 4.0f;
    d.physics.angularAccel = 3.0f;
    d.physics.mainGimbalPitchLimitRad = 0.12f;
    d.physics.mainGimbalYawLimitRad = 0.12f;
    d.physics.mainGimbalRateRadPerSec = 0.14f;
    d.physics.maxGs = 20.0f;
    d.physics.maxLinearGs = 20.0f;
    d.physics.turnRadius = 20.0f;
    d.physics.massKg = 100000.0;
    d.physics.pitchInertiaKgM2 = 1000000.0;
    d.physics.yawInertiaKgM2 = 1000000.0;
    d.physics.rollInertiaKgM2 = 1000000.0;
    return d;
}

void testAllSixConstructionsAreStateTransitions()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);

    const std::array<Model, 6> models {
        Model::AssistedI,
        Model::AssistedY,
        Model::AssistedX,
        Model::NewtonianI,
        Model::NewtonianY,
        Model::NewtonianX
    };

    for (const Model model : models)
    {
        state.selectConstructionForDiagnostic(model);

        require(state.valid(), "diagnostic construction produced invalid state");
        require(
            state.construction.physicalControlModel == model,
            "construction model did not live in ShipState"
        );
        require(
            state.actuators.source == Source::DiagnosticOverride,
            "diagnostic construction did not own actuator state"
        );
        require(
            state.actuators.propulsionSlots.size() ==
                game::ship::plant::physicalControlSlots(model).size(),
            "actuator topology did not follow construction state"
        );

        const auto expectedLaw =
            game::ship::plant::familyOf(model) == Family::Assisted
                ? game::navigation::LocalFlightControlLaw::Assisted
                : game::navigation::LocalFlightControlLaw::Newtonian;
        require(
            state.construction.nativeFlightControlLaw == expectedLaw,
            "flight law did not follow construction state"
        );

        for (const auto& slot : state.actuators.propulsionSlots)
        {
            require(slot.operational, "healthy state created a dead actuator");
            require(slot.thrust01 == 1.0, "healthy state lost thrust");
            require(slot.vectoring01 == 1.0, "healthy state lost vectoring");
        }
    }
}

void testDiagnosticDamageChangesCapabilityWithoutChangingConstruction()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);
    state.selectConstructionForDiagnostic(Model::NewtonianI);

    const Model before = state.construction.physicalControlModel;
    require(!state.actuators.propulsionSlots.empty(), "Newtonian-I has no slots");

    state.actuators.propulsionSlots[0].thrust01 = 0.5;
    state.actuators.propulsionSlots[0].vectoring01 = 0.25;

    const auto capability =
        game::ship::capability::buildShipCapability(
            d,
            state.construction.physicalControlModel,
            state.actuators.propulsionSlots
        );

    require(capability.valid, "degraded state did not build capability");
    require(
        state.construction.physicalControlModel == before,
        "damage changed construction family/layout"
    );
    require(
        capability.controlState ==
            game::ship::capability::ControlHealthState::Degraded,
        "degraded actuator state was not classified as degraded"
    );
}

void testModuleProjectionCannotOverwriteDiagnosticOverride()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);
    state.selectConstructionForDiagnostic(Model::NewtonianX);

    auto replacement =
        game::ship::plant::healthySlotState(
            game::ship::plant::physicalControlSlots(Model::NewtonianX)
        );
    replacement[0].operational = false;

    state.actuators.synchronizeFromModules(std::move(replacement));

    require(
        state.actuators.propulsionSlots[0].operational,
        "module projection overwrote explicit diagnostic state"
    );
}

void testDifferentShipDescriptorsRemainIndependent()
{
    auto cobraLike = descriptor();
    cobraLike.physics.massKg = 100000.0;
    cobraLike.physics.forwardMainEngineAccelerationMps2 = 60.0f;
    cobraLike.physicalControlModel = Model::AssistedI;

    auto heavyLike = descriptor();
    heavyLike.physics.massKg = 900000.0;
    heavyLike.physics.forwardMainEngineAccelerationMps2 = 18.0f;
    heavyLike.physicalControlModel = Model::NewtonianY;

    game::ship::state::ShipState cobraState;
    game::ship::state::ShipState heavyState;
    cobraState.initialize(cobraLike);
    heavyState.initialize(heavyLike);

    require(
        cobraState.construction.definition == &cobraLike,
        "first ship lost its own descriptor binding"
    );
    require(
        heavyState.construction.definition == &heavyLike,
        "second ship lost its own descriptor binding"
    );
    require(
        cobraState.construction.physicalControlModel == Model::AssistedI,
        "first ship construction leaked from another mark"
    );
    require(
        heavyState.construction.physicalControlModel == Model::NewtonianY,
        "second ship construction leaked from another mark"
    );

    const auto cobraCapability =
        game::ship::capability::buildShipCapability(
            cobraLike,
            cobraState.construction.physicalControlModel,
            cobraState.actuators.propulsionSlots
        );
    const auto heavyCapability =
        game::ship::capability::buildShipCapability(
            heavyLike,
            heavyState.construction.physicalControlModel,
            heavyState.actuators.propulsionSlots
        );

    require(cobraCapability.valid, "first mark capability invalid");
    require(heavyCapability.valid, "second mark capability invalid");
    require(
        cobraCapability.effectivePhysics.massKg !=
            heavyCapability.effectivePhysics.massKg,
        "different marks collapsed onto one physics definition"
    );
    require(
        cobraCapability.effectivePhysics.forwardMainEngineAccelerationMps2 !=
            heavyCapability.effectivePhysics.forwardMainEngineAccelerationMps2,
        "different marks collapsed onto one propulsion definition"
    );
}

void testClearOverrideReturnsToDescriptorConstruction()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);
    state.selectConstructionForDiagnostic(Model::NewtonianY);
    state.clearDiagnosticConstructionOverride();

    require(
        state.construction.physicalControlModel == Model::AssistedI,
        "clear override did not restore descriptor construction"
    );
    require(
        state.actuators.source == Source::ModuleRuntime,
        "clear override did not restore module-runtime ownership"
    );
    require(state.valid(), "restored production state is invalid");
}

} // namespace

int main()
{
    try
    {
        testAllSixConstructionsAreStateTransitions();
        testDiagnosticDamageChangesCapabilityWithoutChangingConstruction();
        testModuleProjectionCannotOverwriteDiagnosticOverride();
        testDifferentShipDescriptorsRemainIndependent();
        testClearOverrideReturnsToDescriptorConstruction();

        std::cout
            << "SHIP STATE: PASS\n"
            << " - all six physical constructions are state transitions\n"
            << " - actuator degradation changes capability, not construction\n"
            << " - diagnostic state is not overwritten by module projection\n"
            << " - different ship marks keep independent definitions/capability\n"
            << " - production descriptor binding is restorable\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SHIP STATE: FAIL: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
