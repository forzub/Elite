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
    d.physics.maxGs = 20.0f;
    d.physics.maxLinearGs = 20.0f;
    d.physics.massKg = 1000.0;
    d.physics.pitchInertiaKgM2 = 1000.0;
    d.physics.yawInertiaKgM2 = 1000.0;
    d.physics.rollInertiaKgM2 = 1000.0;

    game::equipment::engine::EngineUnitDescriptor engine;
    engine.nominalThrustNewtons = 50000.0;
    engine.nominalEfficiency01 = 0.80;
    // Fixture power is independently authored, not derived from F*v.
    engine.nominalElectricalPowerMW = 31.25;
    engine.nominalGimbalRangeRad = 0.2;
    engine.nominalGimbalRateRadPerSec = 0.4;
    engine.gimbalFailure.failureAtDamage01 = 0.90;
    engine.maxRelativeEfficiencyLossAtFullDamage01 = 0.002;
    engine.nominalCoolantTemperatureK = 1050.0;
    engine.nominalOperatingTemperatureK = 1200.0;
    engine.safeOperatingTemperatureK = 1300.0;
    engine.thermalConductanceMWperK =
        engine.nominalElectricalPowerMW *
        (1.0 - engine.nominalEfficiency01) / 150.0;
    engine.thermalCapacityMJperK = 12.0;

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

void testFiveCauseDamageFlowsIntoLiveSlotAndCapability()
{
    const auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);

    // 20% laser + 15% impact + 8% thermal + 2% radiation + 5% wear
    // = 50% direct engine damage. No cause-specific weighting at output.
    const auto id = std::string("aft_left");
    require(runtime.addEquipmentDamage(
                id, EquipmentDamageKind::Laser, 0.20),
            "laser damage did not enter module state");
    require(runtime.addEquipmentDamage(
                id, EquipmentDamageKind::Impact, 0.15),
            "impact damage did not enter module state");
    require(runtime.addEquipmentDamage(
                id, EquipmentDamageKind::Thermal, 0.08),
            "thermal damage did not enter module state");
    require(runtime.addEquipmentDamage(
                id, EquipmentDamageKind::Radiation, 0.02),
            "radiation damage did not enter module state");
    require(runtime.addEquipmentDamage(
                id, EquipmentDamageKind::Wear, 0.05),
            "wear damage did not enter module state");

    const auto* module = runtime.findModule(id);
    require(module && near(module->equipmentDamage.accumulated.laser01, 0.20)
                       && near(module->equipmentDamage.accumulated.wear01, 0.05),
            "individual damage histories were merged or lost");

    const auto slots =
        game::ship::capability::buildPhysicalControlSlotState(
            d, d.physicalControlModel, runtime
        );

    require(slots.size() == 4, "Assisted-I slot projection size changed");
    require(slots[0].operational,
            "functional damage falsely detached the engine");
    require(near(slots[0].thrust01, 0.50),
            "five percentages did not halve live available thrust");
    require(near(slots[0].vectoring01, 0.50),
            "five percentages did not reduce nozzle authority");

    const auto capability =
        game::ship::capability::buildShipCapability(
            d, d.physicalControlModel, slots
        );

    require(capability.valid, "damaged capability invalid");
    require(near(capability.effectivePhysics.forwardMainEngineAccelerationMps2,
                 75.0, 1.0e-6),
            "damaged aft-left engine did not reduce actual bank capability");
    require(near(capability.effectivePhysics.reverseMainEngineAccelerationMps2,
                 100.0, 1.0e-6),
            "damaged aft engine modified the opposite bank");
}

void testTvcFailurePreservesPartialAxialThrust()
{
    const auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);

    require(runtime.addEquipmentDamage(
                "aft_left", EquipmentDamageKind::Impact, 0.90),
            "impact damage did not enter engine module");

    const auto slots =
        game::ship::capability::buildPhysicalControlSlotState(
            d, d.physicalControlModel, runtime
        );

    require(slots[0].operational,
            "nozzle failure falsely removed the physical engine");
    require(near(slots[0].thrust01, 0.10),
            "nozzle failure erased remaining 10% axial thrust");
    require(near(slots[0].vectoring01, 0.0),
            "90% total damage did not disable TVC");
    require(slots[0].thrustIrregularity01 > 0.0,
            "severe surviving axial thrust lost instability");

    const auto capability =
        game::ship::capability::buildShipCapability(
            d, d.physicalControlModel, slots
        );
    require(capability.valid &&
            near(capability.effectivePhysics.forwardMainEngineAccelerationMps2,
                 55.0, 1.0e-6),
            "TVC failure removed or created extra axial thrust");
}

void testThrustFailurePreservesInstalledModule()
{
    const auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);

    require(runtime.addEquipmentDamage(
                "aft_left", EquipmentDamageKind::Laser, 0.95),
            "laser damage did not enter engine module");

    const auto slots =
        game::ship::capability::buildPhysicalControlSlotState(
            d, d.physicalControlModel, runtime
        );

    require(slots[0].operational,
            "95% functional damage falsely detached engine module");
    require(near(slots[0].thrust01, 0.0) &&
            near(slots[0].vectoring01, 0.0),
            "95% engine damage did not disable both functions");

    const auto capability =
        game::ship::capability::buildShipCapability(
            d, d.physicalControlModel, slots
        );
    require(capability.valid &&
            near(capability.effectivePhysics.forwardMainEngineAccelerationMps2,
                 50.0, 1.0e-6),
            "failed aft engine did not halve longitudinal bank authority");
}

void testInstalledEngineWearAndThermalAffectRealModuleProjection()
{
    const auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);

    const auto& engine = *d.modules.front().engine;
    const auto firstRevision = runtime.revision();
    // Two actual full-thrust hours for aft_left only (wear quantum = 1e-5).
    require(runtime.advanceEngineOperatingLife(
                "aft_left", engine, 1.0, 7200.0, 1050.0),
            "real engine burn did not cross a quantized wear increment");

    const auto* active = runtime.findModule("aft_left");
    const auto* inactive = runtime.findModule("aft_right");
    require(active && inactive &&
            near(active->engineOperating.equivalentOperatingHours, 2.0) &&
            near(inactive->engineOperating.equivalentOperatingHours, 0.0),
            "operating hours leaked to an unpowered installed engine");
    require(active->equipmentDamage.accumulated.wear01 > 0.0 &&
            near(inactive->equipmentDamage.accumulated.wear01, 0.0),
            "per-engine wear was not isolated");
    require(runtime.revision() > firstRevision,
            "new wear damage did not invalidate module capability cache");

    const auto slots =
        game::ship::capability::buildPhysicalControlSlotState(
            d, d.physicalControlModel, runtime
        );
    require(slots[0].thrust01 < 1.0 &&
            near(slots[1].thrust01, 1.0),
            "operating wear did not propagate into physical engine slots");

    require(runtime.advanceEngineOperatingLife(
                "aft_left", engine, 0.0, 3600.0, 1500.0),
            "overheated engine did not accumulate thermal damage while idle");
    active = runtime.findModule("aft_left");
    require(active->equipmentDamage.accumulated.thermal01 > 0.0 &&
            active->engineOperating.temperatureK > 1300.0,
            "real installed engine thermal damage was not stored");
}

} // namespace

int main()
{
    try
    {
        testFiveCauseDamageFlowsIntoLiveSlotAndCapability();
        testTvcFailurePreservesPartialAxialThrust();
        testThrustFailurePreservesInstalledModule();
        testInstalledEngineWearAndThermalAffectRealModuleProjection();
        std::cout
            << "EQUIPMENT DAMAGE FAILURE PIPELINE: PASS\n"
            << " - all five accumulated causes sum into real module thrust/TVC\n"
            << " - TVC fails at 90% while 10% axial thrust remains\n"
            << " - thrust fails at 95% without detaching engine module\n";
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
