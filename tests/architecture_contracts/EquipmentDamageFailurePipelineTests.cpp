#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/equipment/engine/EngineUnit.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/simulation/ObjectModuleSnapshotBuilder.h"
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
    auto d = descriptor();
    // This first accelerated 7200-s step tests accumulated OPERATING HOURS,
    // not the engine's thermal integration. A full-power 2-hour *single*
    // thermal tick with the fixture's 20% waste heat would overheat the
    // engine to ~4950 K and saturate thermal damage at 100% before the
    // second part of this test even begins. Isolate lifetime wear here.
    d.modules.front().engine->nominalEfficiency01 = 1.0;
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

    // Test the thermal-damage path separately with an engine that is
    // ALREADY overheated. Hot coolant alone no longer heats an idle motor:
    // the canonical engine model cools through its own radiating pad.
    // One real-duration tick at 1400 K produces 0.05/3600 ~= 1.39e-5
    // damage, just above the module's 1e-5 commitment quantum.
    runtime.findModule("aft_left")->engineOperating.temperatureK = 1400.0;
    require(runtime.advanceEngineOperatingLife(
                "aft_left", engine, 0.0, 1.0, 1500.0),
            "preheated engine did not accumulate thermal damage while idle");
    active = runtime.findModule("aft_left");
    require(active->equipmentDamage.accumulated.thermal01 > 0.0 &&
            active->engineOperating.temperatureK > 1300.0,
            "real installed engine thermal damage was not stored");
}

void testIndependentEmitterDamageReachesInstalledEngineSlot()
{
    auto d = descriptor();
    for (auto& module : d.modules)
    {
        auto& engine = *module.engine;
        engine.nominalElectricalPowerMW = 4850.0;
        engine.nominalEfficiency01 = 0.9855;
        engine.thermalEmitter.outerDiameterM = 4.5;
        engine.thermalEmitter.apertureDiameterM = 2.5;
        engine.thermalEmitter.surfaceTemperatureK = 2500.0;
        engine.thermalEmitter.viewFactor01 = 0.90;
        engine.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
        engine.thermalConductanceMWperK =
            (70.325 -
             game::equipment::engine::ratedThermalEmitterPowerMW(engine)) /
            150.0;
    }
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    const auto* before = runtime.findModule("aft_left");
    require(before && near(before->engineOperating.emitter.surface.intactArea01, 1.0),
            "initial emitter area not pristine");
    const auto revisionBefore = runtime.revision();
    require(runtime.exposeEngineEmitter(
                "aft_left",
                *d.modules[0].engine,
                {game::physics::materials::SurfaceImpactKind::Laser,
                 60.0, 4.0, 2500.0, 0.0}
            ), "actual installed emitter exposure was not accepted");
    const auto* after = runtime.findModule("aft_left");
    require(after && runtime.revision() > revisionBefore,
            "emitter hit did not persist or invalidate physics projection");
    require(after->engineOperating.emitter.surface.intactArea01 < 0.50,
            "laser did not remove localized physical emitter area");
    require(near(
                game::equipment::totalAccumulatedDamage01(
                    after->equipmentDamage.accumulated
                ), 0.0
            ), "emitter hit wrongly injected direct thrust/TVC damage");
    require(after->engineOperating.emitter.damage.laser01 > 0.0 &&
            near(after->engineOperating.emitter.damage.impact01, 0.0),
            "emitter must accumulate its OWN five-cause damage");
    require(after->engineOperating.temperatureK < 1250.0,
            "new annulus hit should not instantly heat the engine");
    const auto cold = game::ship::capability::buildPhysicalControlSlotState(
        d, d.physicalControlModel, runtime
    );
    require(near(cold[0].thrust01, 1.0),
            "cold damaged engine should retain transient thrust");

    // Allow the physical engine to heat before thermal governor engages.
    auto* warmingEngine = runtime.findModule("aft_left");
    warmingEngine->engineOperating.temperatureK = 1300.0;

    const auto slots = game::ship::capability::buildPhysicalControlSlotState(
        d, d.physicalControlModel, runtime
    );
    require(slots.size() == 4 && slots[0].thrust01 < 0.85 &&
            slots[0].thrust01 > 0.0 &&
            near(slots[1].thrust01, 1.0) &&
            near(slots[2].thrust01, 1.0),
            "hot surface loss did not isolate derating to damaged engine");
    require(near(slots[0].vectoring01, 1.0),
            "annulus hit incorrectly damaged independent TVC function");

    const auto c = game::ship::capability::buildShipCapability(
        d, d.physicalControlModel, slots
    );
    require(c.valid &&
            c.effectivePhysics.forwardMainEngineAccelerationMps2 < 100.0f &&
            near(c.effectivePhysics.reverseMainEngineAccelerationMps2,
                 100.0),
            "emitter derating did not reach real physical propulsion bank");

    const double oldArea =
        runtime.findModule("aft_left")->engineOperating.emitter.surface.intactArea01;
    runtime.restoreSubtree(runtime.findModuleIndex("aft_left"));
    require(near(
                runtime.findModule("aft_left")->
                    engineOperating.emitter.surface.intactArea01,
                oldArea
            ), "ordinary engine repair illegally repaired the hot annulus");

    require(runtime.addEquipmentDamage(
                "aft_left", EquipmentDamageKind::Impact, 0.10
            ), "engine housing fixture damage failed");
    require(runtime.replaceEngineEmitter("aft_left", *d.modules[0].engine),
            "explicit annulus replacement failed");
    const auto* replaced = runtime.findModule("aft_left");
    require(replaced &&
            near(replaced->engineOperating.emitter.surface.intactArea01, 1.0) &&
            near(replaced->engineOperating.emitter.damage.laser01, 0.0) &&
            near(replaced->engineOperating.emitter.equivalentOperatingHours, 0.0),
            "replace-only annulus did not reset just its own lifecycle");
    require(near(replaced->equipmentDamage.accumulated.impact01, 0.10),
            "replacement incorrectly repaired the engine itself");
}


void testLiveAndReplicatedEngineCapabilityRemainIdentical()
{
    auto d = descriptor();
    for (auto& module : d.modules)
    {
        auto& motor = *module.engine;
        motor.structureMaterialId = "elite.material.engine.tial-ht.v1";
        motor.nominalElectricalPowerMW = 4850.0;
        motor.nominalEfficiency01 = 0.9855;
        motor.thermalEmitter.outerDiameterM = 4.5;
        motor.thermalEmitter.apertureDiameterM = 2.5;
        motor.thermalEmitter.surfaceTemperatureK = 2500.0;
        motor.thermalEmitter.viewFactor01 = 0.90;
        motor.thermalEmitter.materialId =
            "elite.material.engine.hfc-c.v1";
        motor.thermalConductanceMWperK =
            (70.325 -
             game::equipment::engine::ratedThermalEmitterPowerMW(motor)) /
            150.0;
    }

    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    const auto verify = [&]()
    {
        const auto live =
            game::ship::capability::buildPhysicalControlSlotState(
                d, d.physicalControlModel, runtime
            );
        std::vector<game::simulation::ObjectModuleSnapshot> replicated;
        for (const auto& module : d.modules)
        {
            replicated.push_back(
                game::simulation::makeObjectModuleSnapshot(
                    module, runtime.findModule(module.moduleId)
                )
            );
        }
        const auto client =
            game::ship::capability::buildPhysicalControlSlotState(
                d, d.physicalControlModel, replicated
            );
        require(live.size() == 4 && client.size() == live.size(),
                "engine slot count changed under replication");
        for (std::size_t i = 0; i < live.size(); ++i)
        {
            require(live[i].operational == client[i].operational &&
                    std::abs(live[i].thrust01 - client[i].thrust01) < 1.0e-5 &&
                    std::abs(live[i].vectoring01 -
                             client[i].vectoring01) < 1.0e-5 &&
                    std::abs(live[i].thrustIrregularity01 -
                             client[i].thrustIrregularity01) < 1.0e-5,
                    "server and client engine control authority diverged");
        }
        const auto serverShip =
            game::ship::capability::buildShipCapability(
                d, d.physicalControlModel, live
            );
        const auto clientShip =
            game::ship::capability::buildShipCapability(
                d, d.physicalControlModel, client
            );
        require(serverShip.valid && clientShip.valid &&
                std::abs(
                    serverShip.effectivePhysics.
                        forwardMainEngineAccelerationMps2 -
                    clientShip.effectivePhysics.
                        forwardMainEngineAccelerationMps2) < 1.0e-4,
                "server and replicated ship acceleration diverged");
        return live[0].thrust01;
    };

    require(near(verify(), 1.0),
            "healthy material must not change nominal flight authority");
    require(runtime.exposeEngineEmitter(
                "aft_left", *d.modules[0].engine,
                {game::physics::materials::SurfaceImpactKind::Laser,
                 60.0, 4.0, 2500.0, 0.0}
            ), "could not damage hot annulus in parity test");
    require(near(verify(), 1.0),
            "cold damaged emitter should still initially deliver full thrust");
    runtime.advanceEngineOperatingLife(
        "aft_left", *d.modules[0].engine, 1.0, 600.0, 1050.0
    );
    require(verify() < 0.85,
            "heated annulus did not reduce replicated thrust authority");
    require(runtime.replaceEngineEmitter(
                "aft_left", *d.modules[0].engine
            ), "replacement did not restore independent hot emitter");
    // Replaced annulus does not magically cool the already-hot engine.
    verify();

    // Thermal and radiation counters are intentionally passive for the
    // annulus; having them in the canonical five-channel DTO adds no force.
    auto* mounted = runtime.findModule("aft_left");
    mounted->engineOperating.emitter.damage.thermal01 = 1.0;
    mounted->engineOperating.emitter.damage.radiation01 = 1.0;
    verify();
}

void testMountedBankCapabilityMatchesBalanceAndNetwork()
{
    auto d = descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    // An asymmetric 50%-damaged aft-left and 100%-healthy aft-right pair
    // cannot safely accelerate at 75 m/s² with this bank-balancing law.
    require(runtime.addEquipmentDamage(
                "aft_left", EquipmentDamageKind::Impact, 0.50
            ), "could not damage the physical engine");
    const auto live = game::ship::capability::buildShipCapability(d, runtime);
    require(live.valid &&
            near(live.effectivePhysics.forwardMainEngineAccelerationMps2,
                 50.0, 1.0e-5) &&
            near(live.effectivePhysics.reverseMainEngineAccelerationMps2,
                 100.0, 1.0e-5),
            "engine-force capability over-promised asymmetric bank thrust");

    std::vector<game::simulation::ObjectModuleSnapshot> replicated;
    for (const auto& module : d.modules)
        replicated.push_back(game::simulation::makeObjectModuleSnapshot(
            module, runtime.findModule(module.moduleId)
        ));
    const auto remote = game::ship::capability::buildShipCapability(
        d, replicated
    );
    require(remote.valid &&
            near(remote.effectivePhysics.forwardMainEngineAccelerationMps2,
                 live.effectivePhysics.forwardMainEngineAccelerationMps2,
                 1.0e-5),
            "client and server differed on balanced main-engine envelope");
}

void testCinematicEngineImpactShockAndRecovery()
{
    const auto d=descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    const auto& passport=*d.modules.front().engine;
    const std::string names[]={
        "aft_left","aft_right","fore_left","fore_right"
    };
    int recovered=0,stayedDamaged=0;
    for(const auto& name:names)
    {
        require(runtime.addEngineImpactShock(name,0.70),
                "collision did not schedule installed engine field shock");
        const auto* during=runtime.findModule(name);
        require(during &&
                near(during->equipmentDamage.accumulated.impact01,
                     0.70/3.0,1.0e-6) &&
                near(during->engineOperating.impactShock01,
                     2.0*0.70/3.0,1.0e-6) &&
                during->engineOperating.impactShockSecondsRemaining>=1.0 &&
                during->engineOperating.impactShockSecondsRemaining<=3.0,
                "impact did not separate durable third and transient two thirds");
        const auto transient=game::equipment::engine::evaluateEngineUnit(
            passport,during->equipmentDamage,during->engineOperating
        );
        require(near(transient.efficiencyAuthority01,0.30,1.0e-5) &&
                transient.thrustIrregularity01>0.30,
                "70 percent hit failed to cause immediate coughing force loss");
        const double beforeWear=during->equipmentDamage.accumulated.wear01;
        const auto beforeLaser=during->equipmentDamage.accumulated.laser01;
        require(runtime.advanceEngineOperatingLife(
                    name,passport,0.0,4.0,1050.0),
                "scheduled engine impact failed to resolve after four seconds");
        const auto* after=runtime.findModule(name);
        require(after && after->engineOperating.impactShock01==0.0 &&
                after->engineOperating.impactShockSecondsRemaining==0.0 &&
                after->engineOperating.impactShockPermanentOnExpiry01==0.0,
                "impact field shock persisted after its deadline");
        const double retained=after->equipmentDamage.accumulated.impact01;
        if(near(retained,0.70/3.0,1.0e-5))++recovered;
        else if(near(retained,0.70,1.0e-5))++stayedDamaged;
        else require(false,"impact had an invalid lasting damage outcome");
        require(near(after->equipmentDamage.accumulated.wear01,beforeWear) &&
                near(after->equipmentDamage.accumulated.laser01,beforeLaser),
                "impact rebound illegally repaired wear or laser damage");
    }
    require(recovered+stayedDamaged==4,
            "deterministic impact outcome was decided more than once");
}


void testInjectedInternalEquipmentFault()
{
    const auto d=descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    require(!runtime.injectEquipmentFailure("aft_left",0.0),
            "invalid zero-level fault accepted");
    require(!runtime.injectEquipmentFailure("not-installed",1.0),
            "nonexistent equipment accepted sudden fault");
    require(runtime.injectEquipmentFailure("aft_left",0.50),
            "sudden fault did not enter module damage state");
    require(!runtime.injectEquipmentFailure("aft_left",0.50),
            "reapplying unchanged fault should be idempotent");
    const auto* m=runtime.findModule("aft_left");
    require(m &&
            near(m->equipmentDamage.accumulated.internalFailure01,0.50) &&
            near(m->equipmentDamage.accumulated.impact01,0.0) &&
            near(m->health,100.0),
            "fault forged impact history or structural destruction");
    const auto slots=game::ship::capability::buildPhysicalControlSlotState(
        d,d.physicalControlModel,runtime);
    require(slots.size()==4 && near(slots[0].thrust01,0.50) &&
            near(slots[1].thrust01,1.0),
            "fault did not reduce only the installed target");
    const auto replicated=game::simulation::makeObjectModuleSnapshot(
        d.modules[0],m);
    require(near(replicated.internalFailureDamage01,0.50),
            "fault not represented in replicated module state");
    game::equipment::EquipmentDamageLayer generic;
    generic.accumulated.internalFailure01=1.0;
    require(game::equipment::evaluateEquipmentFunction(generic).failed,
            "generic equipment ignored internal failure cause");
    require(runtime.injectEquipmentFailure("aft_left",1.0),
            "full sudden fault failed to latch");
    const auto failed=game::ship::capability::buildPhysicalControlSlotState(
        d,d.physicalControlModel,runtime);
    require(failed.size()==4 && near(failed[0].thrust01,0.0) &&
            near(failed[0].vectoring01,0.0) &&
            near(failed[1].thrust01,1.0),
            "full internal fault failed to disable only target engine");
}


void testDiagnosticFaultCanToggleWithoutRepairingRealDamage()
{
    const auto d=descriptor();
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    require(runtime.addEquipmentDamage(
                "aft_left",EquipmentDamageKind::Impact,0.12),
            "test setup could not apply genuine impact damage");
    require(runtime.addEquipmentDamage(
                "aft_left",EquipmentDamageKind::Wear,0.07),
            "test setup could not apply genuine wear damage");
    const auto* before=runtime.findModule("aft_left");
    const double previousInternal=
        before->equipmentDamage.accumulated.internalFailure01;
    const auto revision=runtime.revision();

    require(runtime.injectEquipmentFailure("aft_left",1.0),
            "temporary diagnostic main engine failure did not apply");
    auto failed=game::ship::capability::buildPhysicalControlSlotState(
        d,d.physicalControlModel,runtime);
    require(failed.size()==4 && near(failed[0].thrust01,0.0),
            "test fault did not disable installed motor");

    require(runtime.debugRestoreInternalFailure(
                "aft_left",previousInternal),
            "second F-style operation could not release test fault");
    const auto* recovered=runtime.findModule("aft_left");
    require(recovered &&
            near(recovered->equipmentDamage.accumulated.impact01,0.12) &&
            near(recovered->equipmentDamage.accumulated.wear01,0.07) &&
            near(recovered->equipmentDamage.accumulated.internalFailure01,
                 previousInternal) &&
            near(recovered->health,100.0) &&
            runtime.revision()>revision,
            "diagnostic restoration erased real damage or lacked revision");
    auto restored=game::ship::capability::buildPhysicalControlSlotState(
        d,d.physicalControlModel,runtime);
    require(restored.size()==4 && near(restored[0].thrust01,0.81) &&
            near(restored[1].thrust01,1.0),
            "diagnostic fault removal did not restore precisely the live authority");
    require(!runtime.debugRestoreInternalFailure(
                "aft_left",previousInternal),
            "identical second restore unexpectedly changed revision");
}

void testEngineOutWorkingAxisIsTorqueNeutral()
{
    auto d=descriptor();
    d.physics.physicalActuatorHalfExtentsMeters={13.0,2.5,11.1};
    d.physics.centerOfMassLocalMeters={0.0,0.0,0.0};
    // The CR-1's 75-degree physical gimbal is sufficient for the surviving
    // outboard motor's COM ray; a fictitious straight-ahead impulse is not.
    for(auto& module:d.modules)
        module.engine->nominalGimbalRangeRad=
            75.0*3.14159265358979323846/180.0;
    world::modules::ObjectModuleRuntime runtime;
    runtime.init(d.modules);
    require(runtime.injectEquipmentFailure("aft_left",1.0),
            "engine-out scenario could not fail real aft-left CR-1");
    const auto capability=
        game::ship::capability::buildShipCapability(d,runtime);
    require(capability.valid,
            "installed engine-out capability invalid");
    const auto& physics=capability.effectivePhysics;
    require(physics.assistedWorkingThrustAxisEmergency &&
            physics.assistedWorkingThrustMotorIndex==1 &&
            physics.assistedWorkingThrustAxisBody.x<0.0 &&
            physics.assistedWorkingThrustAxisBody.z<0.0,
            "dead aft-left failed to select outboard right drive axis");
    const auto arm=glm::dvec3(13.0,0.0,11.1);
    const auto axis=physics.assistedWorkingThrustAxisBody;
    require(glm::length(glm::cross(arm,axis))<1.0e-6,
            "emergency main thrust misses real center of mass");
    require(std::abs(physics.assistedWorkingThrustAxisAngleRad*
                180.0/3.14159265358979323846-49.5)<0.25,
            "outboard drive 49.5-degree fallback angle changed");
    require(physics.forwardMainEngineAvailable &&
            physics.forwardMainEngineAccelerationMps2>0.0f,
            "one surviving main drive falsely reported zero propulsion");
    const auto current=game::ship::capability::buildPhysicalControlSlotState(
        d,d.physicalControlModel,runtime);
    require(current.size()==4 && near(current[0].thrust01,0.0) &&
            current[1].thrust01>0.0,
            "one engine-out was not represented in real slot capabilities");
}

} // namespace

int main()
{
    try
    {
        testFiveCauseDamageFlowsIntoLiveSlotAndCapability();
        testInjectedInternalEquipmentFault();
        testDiagnosticFaultCanToggleWithoutRepairingRealDamage();
        testEngineOutWorkingAxisIsTorqueNeutral();
        testCinematicEngineImpactShockAndRecovery();
        testTvcFailurePreservesPartialAxialThrust();
        testThrustFailurePreservesInstalledModule();
        testInstalledEngineWearAndThermalAffectRealModuleProjection();
        testIndependentEmitterDamageReachesInstalledEngineSlot();
        testLiveAndReplicatedEngineCapabilityRemainIdentical();
        testMountedBankCapabilityMatchesBalanceAndNetwork();
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
