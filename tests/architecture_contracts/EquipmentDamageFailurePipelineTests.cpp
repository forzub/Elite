#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/equipment/engine/EngineUnit.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/world/modules/ObjectModuleRuntime.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using game::equipment::EquipmentDamageKind;
using game::ship::plant::PhysicalControlModelId;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b, double eps = 1.0e-9)
{
    return std::abs(a - b) <= eps;
}

ShipDescriptor descriptor()
{
    ShipDescriptor d;
    d.physicalControlModel = PhysicalControlModelId::AssistedI;
    d.localFlightControlLaw =
        game::ship::plant::nativeFlightLaw(d.physicalControlModel);

    d.physics.forwardMainEngineAvailable = true;
    d.physics.reverseMainEngineAvailable = true;
    d.physics.forwardMainEngineAccelerationMps2 = 100.0f;
    d.physics.reverseMainEngineAccelerationMps2 = 100.0f;
    d.physics.strafeAccel = 100.0f;
    d.physics.angularAccel = 3.0f;
    d.physics.massKg = 1000.0;
    d.physics.pitchInertiaKgM2 = 1000.0;
    d.physics.yawInertiaKgM2 = 1000.0;
    d.physics.rollInertiaKgM2 = 1000.0;

    game::equipment::engine::EngineUnitDescriptor engine;
    engine.nominalThrustNewtons = 50000.0;
    engine.nominalElectricalPowerMW = 10.0;
    engine.nominalEfficiency01 = 1.0;
    engine.nominalGimbalRangeRad = 0.2;
    engine.nominalGimbalRateRadPerSec = 0.4;
    engine.gimbalDamage = {
        0.15, 0.65, 0.05, 0.00, 0.15
    };
    engine.gimbalFailure.failureAtDamage01 = 0.95;

    const game::equipment::EquipmentDamageSensitivity thrustDamage {
        0.30, 0.45, 0.15, 0.01, 0.09
    };

    const char* ids[] = {
        "aft_left",
        "aft_right",
        "fore_left",
        "fore_right"
    };

    for (const char* id : ids)
    {
        ModuleDescriptor module;
        module.moduleId = id;
        module.maxHealth = 100.0f;
        module.equipmentDamageSensitivity = thrustDamage;
        module.equipmentFailure.failureAtDamage01 = 0.95;
        module.engine = engine;
        d.modules.push_back(std::move(module));
    }

    d.physicalControl.slotModuleIds = {
        "aft_left",
        "aft_right",
        "fore_left",
        "fore_right"
    };

    return d;
}

void applyThrustFailureDamage(
    world::modules::ObjectModuleRuntime& runtime,
    const std::string& moduleId
)
{
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Laser, 1.0),
            "laser damage was not accumulated");
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Impact, 1.0),
            "impact damage was not accumulated");
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Thermal, 1.0),
            "thermal damage was not accumulated");
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Radiation, 1.0),
            "radiation damage was not accumulated");
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Wear, 4.0 / 9.0),
            "wear damage was not accumulated");
}

void applyTvcFailureDamage(
    world::modules::ObjectModuleRuntime& runtime,
    const std::string& moduleId
)
{
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Laser, 1.0),
            "laser damage was not accumulated");
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Impact, 1.0),
            "impact damage was not accumulated");
    require(runtime.addEquipmentDamage(
                moduleId, EquipmentDamageKind::Wear, 1.0),
            "wear damage was not accumulated");
}

void testThrustFailureFlowsFromEquipmentDamageToCapability()
{
    const auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);

    applyThrustFailureDamage(runtime, "aft_left");

    const auto slots =
        game::ship::capability::buildPhysicalControlSlotState(
            d,
            d.physicalControlModel,
            runtime
        );

    require(slots.size() == 4, "Assisted-I slot projection size changed");
    require(slots[0].operational,
            "functional thrust failure deleted the installed slot");
    require(near(slots[0].thrust01, 0.0),
            "95% thrust damage did not zero slot thrust");
    require(slots[0].vectoring01 > 0.0,
            "thrust-only failure incorrectly destroyed TVC function");
    require(near(game::ship::plant::effectiveSlotThrust01(slots[0]), 0.0),
            "failed thrust function still contributed physical force");
    require(near(game::ship::plant::effectiveSlotVectoring01(slots[0]), 0.0),
            "TVC produced authority without surviving thrust");

    const auto capability =
        game::ship::capability::buildShipCapability(
            d,
            d.physicalControlModel,
            slots
        );

    require(capability.valid, "damaged capability projection became invalid");
    require(near(
                capability.effectivePhysics.forwardMainEngineAccelerationMps2,
                50.0,
                1.0e-6),
            "one failed aft thrust function did not halve Assisted-I forward authority");
    require(near(
                capability.effectivePhysics.reverseMainEngineAccelerationMps2,
                100.0,
                1.0e-6),
            "aft thrust failure incorrectly damaged fore-bank authority");
}

void testTvcFailureLeavesAxialThrustInCapability()
{
    const auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);

    applyTvcFailureDamage(runtime, "aft_left");

    const auto slots =
        game::ship::capability::buildPhysicalControlSlotState(
            d,
            d.physicalControlModel,
            runtime
        );

    require(slots.size() == 4, "Assisted-I slot projection size changed");
    require(slots[0].operational,
            "TVC failure deleted the installed slot");
    require(near(slots[0].thrust01, 0.16, 1.0e-9),
            "TVC-only failure changed surviving thrust authority");
    require(near(slots[0].vectoring01, 0.0),
            "95% TVC damage did not zero vectoring authority");

    const auto capability =
        game::ship::capability::buildShipCapability(
            d,
            d.physicalControlModel,
            slots
        );

    require(capability.valid, "TVC-damaged capability projection became invalid");
    require(near(
                capability.effectivePhysics.forwardMainEngineAccelerationMps2,
                58.0,
                1.0e-6),
            "TVC failure removed or fabricated axial thrust");
}

} // namespace

int main()
{
    try
    {
        testThrustFailureFlowsFromEquipmentDamageToCapability();
        testTvcFailureLeavesAxialThrustInCapability();

        std::cout
            << "EQUIPMENT DAMAGE FAILURE PIPELINE: PASS\n"
            << " - five-channel module damage drives thrust failure without deleting the slot\n"
            << " - 95% thrust damage zeros physical thrust authority\n"
            << " - 95% TVC damage zeros vectoring while axial thrust survives\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "EQUIPMENT DAMAGE FAILURE PIPELINE: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
