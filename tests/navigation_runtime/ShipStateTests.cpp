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
    d.physics.centerOfMassLocalMeters = {1.5, -0.25, 3.0};
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

void testAssistedLayoutRoundTripRestoresBaselineCapability()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);

    const auto capability = [&]()
    {
        return game::ship::capability::buildShipCapability(
            d,
            state.construction.physicalControlModel,
            state.actuators.propulsionSlots
        );
    };

    const auto baseline = capability();
    require(baseline.valid, "baseline Assisted-I capability invalid");

    state.selectConstructionForDiagnostic(Model::AssistedY);
    require(
        state.construction.physicalControlModel == Model::AssistedY,
        "Assisted-I -> Y diagnostic switch failed"
    );

    state.selectConstructionForDiagnostic(Model::AssistedX);
    require(
        state.construction.physicalControlModel == Model::AssistedX,
        "Assisted-Y -> X diagnostic switch failed"
    );

    state.selectConstructionForDiagnostic(Model::AssistedI);
    require(
        state.construction.physicalControlModel == Model::AssistedI,
        "Assisted-X -> I diagnostic switch failed"
    );
    require(
        state.construction.nativeFlightControlLaw ==
            game::navigation::LocalFlightControlLaw::Assisted,
        "Assisted round-trip changed native flight law"
    );

    const auto restored = capability();
    require(restored.valid, "round-trip Assisted-I capability invalid");

    require(
        restored.effectivePhysics.forwardMainEngineAvailable ==
            baseline.effectivePhysics.forwardMainEngineAvailable,
        "round-trip changed forward-main availability"
    );
    require(
        restored.effectivePhysics.reverseMainEngineAvailable ==
            baseline.effectivePhysics.reverseMainEngineAvailable,
        "round-trip changed reverse-main availability"
    );
    require(
        restored.effectivePhysics.forwardMainEngineAccelerationMps2 ==
            baseline.effectivePhysics.forwardMainEngineAccelerationMps2,
        "round-trip changed forward-main authority"
    );
    require(
        restored.effectivePhysics.reverseMainEngineAccelerationMps2 ==
            baseline.effectivePhysics.reverseMainEngineAccelerationMps2,
        "round-trip changed reverse-main authority"
    );
    require(
        restored.effectivePhysics.strafeAccel ==
            baseline.effectivePhysics.strafeAccel,
        "round-trip changed Assisted lateral authority"
    );
    require(
        restored.effectivePhysics.angularAccel ==
            baseline.effectivePhysics.angularAccel,
        "round-trip changed angular authority"
    );
    require(
        restored.effectivePhysics.maxPitchRate ==
            baseline.effectivePhysics.maxPitchRate &&
        restored.effectivePhysics.maxYawRate ==
            baseline.effectivePhysics.maxYawRate &&
        restored.effectivePhysics.maxRollRate ==
            baseline.effectivePhysics.maxRollRate,
        "round-trip changed angular-rate limits"
    );

    require(
        state.actuators.propulsionSlots.size() == 4,
        "round-trip Assisted-I topology is not four main actuators"
    );
    for (const auto& slot : state.actuators.propulsionSlots)
    {
        require(slot.operational, "round-trip produced dead Assisted-I actuator");
        require(slot.thrust01 == 1.0, "round-trip lost Assisted-I thrust");
        require(slot.vectoring01 == 1.0, "round-trip lost Assisted-I vectoring");
    }
}

void testRuntimeConstructionSwitchChangesEffectivePhysics()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);

    auto capabilityForCurrentState = [&]()
    {
        return game::ship::capability::buildShipCapability(
            d,
            state.construction.physicalControlModel,
            state.actuators.propulsionSlots
        );
    };

    // Baseline Assisted-I: opposed main banks, no ordinary RCS.
    auto assistedI = capabilityForCurrentState();
    require(assistedI.valid, "Assisted-I capability invalid");
    require(
        assistedI.effectivePhysics.reverseMainEngineAvailable,
        "Assisted-I lost its fore/reverse main bank"
    );
    require(
        assistedI.effectivePhysics.manoeuvreThrusterAccel == 0.0f,
        "Assisted-I fabricated ordinary RCS authority"
    );
    require(
        state.actuators.propulsionSlots.size() == 4,
        "Assisted-I runtime topology is not four main actuators"
    );

    // Family switch must change the physical capability, not only the label.
    state.selectConstructionForDiagnostic(Model::NewtonianI);
    auto newtonianI = capabilityForCurrentState();
    require(newtonianI.valid, "Newtonian-I capability invalid after switch");
    require(
        state.construction.nativeFlightControlLaw ==
            game::navigation::LocalFlightControlLaw::Newtonian,
        "Newtonian switch did not change native flight law"
    );
    require(
        !newtonianI.effectivePhysics.reverseMainEngineAvailable,
        "Newtonian-I retained an impossible fore/reverse main bank"
    );
    require(
        newtonianI.effectivePhysics.manoeuvreThrusterAccel > 0.0f,
        "Newtonian-I did not acquire RCS authority"
    );
    require(
        state.actuators.propulsionSlots.size() == 5,
        "Newtonian-I runtime topology is not one main plus four RCS clusters"
    );

    // Layout switch inside one family must rebuild the actuator topology.
    state.selectConstructionForDiagnostic(Model::NewtonianX);
    auto newtonianX = capabilityForCurrentState();
    require(newtonianX.valid, "Newtonian-X capability invalid after switch");
    require(
        state.actuators.propulsionSlots.size() == 12,
        "Newtonian-X runtime topology is not four mains plus eight RCS clusters"
    );
    require(
        !newtonianX.effectivePhysics.reverseMainEngineAvailable,
        "Newtonian-X incorrectly gained a fore main bank"
    );

    state.selectConstructionForDiagnostic(Model::AssistedY);
    auto assistedY = capabilityForCurrentState();
    require(assistedY.valid, "Assisted-Y capability invalid after switch");
    require(
        state.construction.nativeFlightControlLaw ==
            game::navigation::LocalFlightControlLaw::Assisted,
        "Assisted switch did not restore Assisted flight law"
    );
    require(
        assistedY.effectivePhysics.reverseMainEngineAvailable,
        "Assisted-Y did not restore its fore/reverse main bank"
    );
    require(
        assistedY.effectivePhysics.manoeuvreThrusterAccel == 0.0f,
        "Assisted-Y retained Newtonian RCS authority"
    );
    require(
        state.actuators.propulsionSlots.size() == 6,
        "Assisted-Y runtime topology is not three aft plus three fore mains"
    );
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

void testMassPropertiesLiveInInstanceState()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);

    require(
        state.massProperties.massKg == d.physics.massKg,
        "mass did not enter ShipState"
    );
    require(
        state.massProperties.centerOfMassLocalMeters ==
            d.physics.centerOfMassLocalMeters,
        "local CoM did not enter ShipState"
    );
    require(
        state.massProperties.inertiaTensorLocalKgM2[0][0] ==
            d.physics.pitchInertiaKgM2 &&
        state.massProperties.inertiaTensorLocalKgM2[1][1] ==
            d.physics.yawInertiaKgM2 &&
        state.massProperties.inertiaTensorLocalKgM2[2][2] ==
            d.physics.rollInertiaKgM2,
        "principal inertias did not enter state tensor"
    );

    ShipParams projected = d.physics;
    state.massProperties.centerOfMassLocalMeters = {-2.0, 0.5, 1.0};
    state.massProperties.applyTo(projected);

    require(
        projected.centerOfMassLocalMeters ==
            glm::dvec3(-2.0, 0.5, 1.0),
        "runtime CoM was not projected to effective physics"
    );
}

void testResponseProfileRevisionSeams()
{
    auto d = descriptor();
    game::ship::state::ShipState state;
    state.initialize(d);

    const auto actuatorRevision = state.actuators.revision;
    state.actuators.propulsionSlots[0].thrust01 = 0.5;
    state.actuators.touch();
    require(
        state.actuators.revision == actuatorRevision + 1,
        "actuator direct edit did not invalidate response-profile revision"
    );

    const auto massRevision = state.massProperties.revision;
    state.massProperties.centerOfMassLocalMeters.x += 0.5;
    state.massProperties.touch();
    require(
        state.massProperties.revision == massRevision + 1,
        "mass-property direct edit did not invalidate response-profile revision"
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
        testAssistedLayoutRoundTripRestoresBaselineCapability();
        testRuntimeConstructionSwitchChangesEffectivePhysics();
        testDiagnosticDamageChangesCapabilityWithoutChangingConstruction();
        testModuleProjectionCannotOverwriteDiagnosticOverride();
        testMassPropertiesLiveInInstanceState();
        testResponseProfileRevisionSeams();
        testDifferentShipDescriptorsRemainIndependent();
        testClearOverrideReturnsToDescriptorConstruction();

        std::cout
            << "SHIP STATE: PASS\n"
            << " - all six physical constructions are state transitions\n"
            << " - runtime family/layout switching changes effective physics and actuator topology\n"
            << " - Assisted I/Y/X/I round-trip restores the baseline capability exactly\n"
            << " - actuator degradation changes capability, not construction\n"
            << " - diagnostic state is not overwritten by module projection\n"
            << " - mass/CoM/inertia live in per-instance state\n"
            << " - response-profile cache has explicit revision invalidation seams\n"
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
