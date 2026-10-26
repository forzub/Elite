#include "src/game/presentation/EngineThrustHudPresentation.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h"
#include "src/game/ship/capability/ReplicatedInstalledAssistedDescriptor.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"
#include "src/game/ship/computer/MotherComputerClientAdapter.h"

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
        require(p.styleId == "human_amber",
                "engine HUD did not inherit default ship HUD style");
        require(!p.warning,
                "healthy Assisted-I incorrectly raised engine warning");

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
    auto descriptor = descriptorFor(Model::AssistedI);
    descriptor.hud.engineStatusStyleId = "alien_blue";
    auto ship = shipFor(descriptor);

    ship.physicalControlModel = Model::AssistedY;
    auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    require(p.modelLabel == "ASSISTED-Y",
            "HUD label ignored replicated runtime model");
    require(p.styleId == "alien_blue",
            "engine HUD style did not follow ship HUD profile");
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

void testFaultFlagComesFromPhysicalSlotAvailability()
{
    auto descriptor = descriptorFor(Model::AssistedI);

    // Bind the first physical slot to a module id that is absent from the
    // runtime module snapshot. The capability builder must therefore expose
    // that slot as unavailable rather than allowing the renderer to guess.
    descriptor.physicalControl.slotModuleIds = {
        "engine:aft_left",
        "",
        "",
        ""
    };

    auto ship = shipFor(descriptor);
    game::simulation::ObjectModuleSnapshot unrelated;
    unrelated.moduleId = "unrelated:module";
    unrelated.state = static_cast<std::uint8_t>(
        world::modules::ObjectModuleRuntimeState::Attached
    );
    unrelated.health = 100.0f;
    ship.modules.push_back(unrelated);

    const auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    require(p.visible, "damaged engine HUD unexpectedly hidden");
    require(p.warning,
            "unavailable physical engine slot did not raise widget warning");
    require(totalBars(p)==4,
            "broken installed binding hid physical engine bars");

    bool sawFault = false;
    for (const auto& group : p.groups)
    {
        for (const auto& bar : group.bars)
        {
            sawFault = sawFault || bar.fault;
            require(bar.capability01==0.0f && bar.thrust01==0.0f,
                "invalid installation invented healthy engine capability");
        }
    }

    require(sawFault,
            "unavailable physical engine slot did not mark its bar faulty");
}

void testAssistedForwardThrustUsesAftBars()
{
    const auto descriptor = descriptorFor(Model::AssistedI);
    auto ship = shipFor(descriptor);

    // A net motion vector is NOT evidence of per-motor thrust: gravity,
    // inertia and asymmetric installed engines make such inference invalid.
    const glm::dvec3 forward =
        glm::normalize(glm::dvec3(ship.transform.forward()));
    ship.transform.motion.engineAccelerationMps2 =
        forward * (73.549875 * 0.5);

    const auto withoutActuatorSample =
        game::presentation::buildEngineThrustHudPresentation(ship);
    require(totalBars(withoutActuatorSample)==4,
            "missing live telemetry hid healthy Assisted-I layout");
    for(const auto& group:withoutActuatorSample.groups)
        for(const auto& bar:group.bars)
            requireNear(bar.thrust01,0.0,1.0e-6,
                "HUD reconstructed fictitious motor thrust from acceleration");

    // Simulate an authoritative, already-executed actuator telemetry sample.
    // This is a presentation test; no flight command or force is synthesized.
    auto& telemetry=ship.transform.motion.engineTelemetry;
    telemetry.valid=true;
    telemetry.count=4;
    for(std::size_t i=0;i<4;++i)
    {
        telemetry.available01[i]=1.0;
        telemetry.thrust01[i]=i<2 ? 0.5 : 0.0;
    }

    const auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    require(totalBars(p)==4,
            "executed aft-bank sample changed engine bar count");
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
                "forward Assisted executed thrust mapped to wrong engine bars"
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

    // A net side acceleration does not tell the HUD which motor contributed.
    // Present only per-engine values explicitly reported by the executor.
    auto& telemetry=ship.transform.motion.engineTelemetry;
    telemetry.valid=true;
    telemetry.count=4;
    for(std::size_t i=0;i<4;++i)
    {
        telemetry.available01[i]=1.0;
        telemetry.thrust01[i]=0.5;
    }

    const auto p =
        game::presentation::buildEngineThrustHudPresentation(ship);

    require(totalBars(p)==4,
            "vectoring sample changed Assisted-I physical bar count");
    for (const auto& group : p.groups)
    {
        for (const auto& bar : group.bars)
        {
            requireNear(
                bar.thrust01,
                0.5,
                1.0e-5,
                "Assisted vectoring HUD ignored executed thrust from both banks"
            );
        }
    }
}

void testMotherComputerCockpitBarsPreserveLayoutAndBite()
{
    using game::ship::computer::MotherComputerCockpitView;
    using game::ship::computer::MotherComputerEngineHudSlot;
    MotherComputerCockpitView cockpit;
    cockpit.valid=true;
    cockpit.physicalModelLabel="ASSISTED-I";
    cockpit.engineHudStyleId="human_amber";
    cockpit.engineSlots = {
        MotherComputerEngineHudSlot{"aft-left",-1.0,0.0,false,0.0f,0.0f,true},
        MotherComputerEngineHudSlot{"aft-right",1.0,0.0,false,1.0f,0.5f,false},
        MotherComputerEngineHudSlot{"fore-left",-1.0,0.0,true,1.0f,0.2f,false},
        MotherComputerEngineHudSlot{"fore-right",1.0,0.0,true,1.0f,0.2f,false}
    };
    const auto view =
        game::presentation::buildEngineThrustHudPresentation(cockpit);
    require(view.visible && view.warning &&
            view.groups.size()==2 && totalBars(view)==4,
            "MotherComputer cockpit did not preserve Assisted-I group layout");
    require(view.groups[0].bars[0].fault &&
            view.groups[0].bars[0].capability01==0.0f,
            "MotherComputer missing damaged aft-left BITE flag and yellow bar");
    require(view.groups[1].bars[1].thrust01==0.5f,
            "MotherComputer red bar diverged from executed aft-right thrust");
    require(view.groups[0].bars[0].fillDirection==Direction::BottomUp &&
            view.groups[1].bars[0].fillDirection==Direction::TopDown,
            "MotherComputer HUD silently rearranged engine ordering");
}


void testNewtonianArgonManifoldReachesEngineHud()
{
    using game::ship::computer::MotherComputerCockpitView;
    using game::ship::computer::MotherComputerEngineHudSlot;
    MotherComputerCockpitView cockpit;
    cockpit.valid=true;
    cockpit.physicalModelLabel="NEWTONIAN-X";
    cockpit.engineHudStyleId="human_amber";
    for(const auto& id : {"main_UL","main_UR","main_LL","main_LR"})
    {
        MotherComputerEngineHudSlot slot {};
        slot.id=id;
        slot.available01=1.0f;
        cockpit.engineSlots.push_back(slot);
    }
    cockpit.systems.newtonianRcsFluidKnown=true;
    cockpit.systems.newtonianRcsManifold01=0.42f;
    cockpit.systems.newtonianRcsManifoldDepleted=false;
    cockpit.systems.newtonianRcsCompressorMW=2.0f;
    cockpit.systems.newtonianRcsFlowUse01PerSec=0.018f;
    cockpit.systems.newtonianRcsFlowRecovery01PerSec=0.065f;
    cockpit.systems.newtonianRcsArgonInventoryKg=500.0f;
    const auto view=
        game::presentation::buildEngineThrustHudPresentation(cockpit);
    require(view.visible && view.groups.size()==1 &&
            totalBars(view)==4,
        "Newtonian-X HUD lost installed main motors");
    require(view.rcsManifoldKnown &&
            std::abs(view.rcsManifold01-0.42f)<1.e-6f &&
            std::abs(view.rcsCompressorMW-2.0f)<1.e-6f &&
            view.rcsArgonInventoryKg==500.0f,
        "Engine widget did not display authoritative argon pressure/recovery");
}

void testInstalledYXSnapshotEngineOutNotMaskedAsDiagnostic()
{
    using namespace game::ship;
    auto descriptor=descriptorFor(Model::AssistedI);
    // A native Assisted-I descriptor contains all dormant installed Cobra
    // engine and ER-5 passports. Replicated active model changes at runtime,
    // while the client's catalog descriptor remains Assisted-I.
    for(const auto model:{Model::AssistedI,Model::AssistedY,Model::AssistedX})
    {
        for(const auto& id:
                presets::cobra::installedAssistedEngineModuleIds(model))
        {
            ModuleDescriptor m;
            m.moduleId=id;
            m.engine=game::equipment::engine::presets::
                CobrettyCR1VectorMainDrive();
            m.equipmentFailure.failureAtDamage01=0.95;
            descriptor.modules.push_back(std::move(m));
        }
        for(const auto& id:
                presets::cobra::installedAssistedReactorModuleIds(model))
        {
            ModuleDescriptor m;
            m.moduleId=id;
            m.engineReactor=game::equipment::reactor::presets::
                CobraCobrettyER5PropulsionReactor();
            descriptor.modules.push_back(std::move(m));
        }
    }
    descriptor.physicalControl.slotModuleIds =
        presets::cobra::installedAssistedEngineModuleIds(Model::AssistedI);
    auto ship=shipFor(descriptor);
    for(const auto& def:descriptor.modules)
    {
        game::simulation::ObjectModuleSnapshot m;
        m.moduleId=def.moduleId;
        m.state=static_cast<std::uint8_t>(
            world::modules::ObjectModuleRuntimeState::Attached);
        m.health=100.0f;
        if(def.engine)
        {
            m.hasEngineThermalState=true;
            m.engineTemperatureK=static_cast<float>(
                def.engine->nominalOperatingTemperatureK);
            m.emitterIntactArea01=1.0f;
        }
        ship.modules.push_back(std::move(m));
    }
    for(const auto model:{Model::AssistedY,Model::AssistedX})
    {
        ship.physicalControlModel=model;
        const auto selected=
            capability::replicatedInstalledAssistedDescriptor(
                descriptor,model);
        require(selected.has_value(),
                "installed Y/X wrongly classified synthetic diagnostic");
        const auto ids=
            presets::cobra::installedAssistedEngineModuleIds(model);
        const auto reactors=
            presets::cobra::installedAssistedReactorModuleIds(model);
        require(selected->physicalControl.slotModuleIds==ids &&
                selected->powerThermalInstallation.engineReactorModuleIds==
                    reactors,
                "installed Y/X descriptor bound to native I hardware");
        const auto damaged=std::find_if(ship.modules.begin(),ship.modules.end(),
            [&](const auto& m){return m.moduleId==ids.front();});
        require(damaged!=ship.modules.end(),"missing Y/X engine snapshot");
        damaged->internalFailureDamage01=1.0f;
        const auto states=capability::buildPhysicalControlSlotState(
            *selected,model,ship.modules);
        require(states.size()==ids.size() &&
                std::abs(states.front().thrust01)<1.0e-6 &&
                states[1].thrust01>0.90f,
                "Y/X client projected healthy motor after server failure");
        const auto hud=game::presentation::buildEngineThrustHudPresentation(
            ship);
        require(totalBars(hud)==ids.size() && hud.warning,
                "Y/X engine-out HUD lost real slot fault or layout count");
        damaged->internalFailureDamage01=0.0f;
    }
}


void testFailedAftMotorReachesMotherComputerHudAndEmergencySight()
{
    using namespace game::ship;
    // The client catalog stays Assisted-I; one real CR-1 is fully failed in
    // the replicated module graph, including its own internal-failure cause.
    auto desc=descriptorFor(Model::AssistedI);
    desc.physics.physicalActuatorHalfExtentsMeters={13.0,2.5,11.1};
    desc.physics.centerOfMassLocalMeters={0.0,0.0,0.0};
    desc.physicalControl.slotModuleIds=
        presets::cobra::installedAssistedEngineModuleIds(Model::AssistedI);
    for(const auto& id:desc.physicalControl.slotModuleIds)
    {
        ModuleDescriptor m;
        m.moduleId=id;
        m.maxHealth=100.0f;
        m.equipmentFailure.failureAtDamage01=0.95;
        m.engine=game::equipment::engine::presets::
            CobrettyCR1VectorMainDrive();
        desc.modules.push_back(std::move(m));
    }
    auto ship=shipFor(desc);
    ship.instanceId=84;
    for(const auto& id:desc.physicalControl.slotModuleIds)
    {
        game::simulation::ObjectModuleSnapshot module;
        module.moduleId=id;
        module.health=100.0f;
        module.state=static_cast<std::uint8_t>(
            world::modules::ObjectModuleRuntimeState::Attached);
        module.hasEngineThermalState=true;
        module.engineTemperatureK=450.0f;
        module.emitterIntactArea01=1.0f;
        if(id==desc.physicalControl.slotModuleIds.front())
            module.internalFailureDamage01=1.0f;
        ship.modules.push_back(std::move(module));
    }
    ship.transform.motion.engineTelemetry.valid=true;
    ship.transform.motion.engineTelemetry.count=4;
    // Deliberately stale healthy live sample: real damage must cap the HUD.
    ship.transform.motion.engineTelemetry.available01[0]=1.0;
    ship.transform.motion.engineTelemetry.thrust01[0]=0.0;
    ship.transform.motion.engineTelemetry.available01[1]=1.0;
    ship.transform.motion.engineTelemetry.thrust01[1]=0.65;

    const auto input=game::ship::computer::collectReplicatedShipFacts(
        ship,42);
    require(input.installedEquipmentBite.summary.failedCount>=1,
            "MotherComputer BITE lost the fully failed physical CR-1");
    require(input.physicalEnvelopeValid &&
            input.physicalEnvelope.assistedWorkingThrustAxisEmergency &&
            input.physicalEnvelope.assistedWorkingThrustMotorIndex==1,
            "failed motor did not activate physically projected emergency axis");

    game::ship::computer::MotherComputerSnapshot motherboard;
    motherboard.valid=true;
    motherboard.input=input;
    // Deliberately leave interpolated renderTransform emergency flag FALSE:
    // the HUD must never source emergency axis from that interpolated flag.
    const auto cockpit=game::ship::computer::collectCockpitView(
        ship,motherboard);
    require(cockpit.valid && cockpit.emergencyWorkingThrustAxis &&
            cockpit.workingThrustAxisBody.x<-0.7,
            "HUD emergency marker depends on stale rendered motion");
    auto observed=motherboard;
    observed.input.physicalEnvelopeValid=false;
    observed.input.emergencyWorkingThrustAxis=true;
    observed.input.workingThrustAxisBody={-0.76049403,0.0,-0.64934492};
    const auto serverSight=game::ship::computer::collectCockpitView(
        ship,observed);
    require(serverSight.valid && serverSight.emergencyWorkingThrustAxis &&
            serverSight.workingThrustAxisBody.x<-0.7,
            "HUD hid server-proven emergency thrust ray during sparse graph");
    // A lagging server emergency flag must not conceal its authoritative
    // non-axial physical direction. Read-only display, no control fallback.
    observed.input.emergencyWorkingThrustAxis=false;
    const auto vectorSight=game::ship::computer::collectCockpitView(
        ship,observed);
    require(vectorSight.valid && vectorSight.emergencyWorkingThrustAxis &&
            vectorSight.workingThrustAxisBody.x<-0.7,
            "HUD ignored authoritative non-axial thrust vector when flag lags");
    const auto hud=game::presentation::buildEngineThrustHudPresentation(
        cockpit);
    require(hud.warning,
            "CHECK lamp did not receive installed engine BITE failure");
    bool failedVisible=false;
    // IDs are topology-defined, not synthetic runtime IDs.
    const auto& slot0=
        game::ship::plant::physicalControlSlots(Model::AssistedI)[0];
    for(const auto& group:hud.groups)
        for(const auto& bar:group.bars)
            if(bar.slotId==std::string(slot0.id))
                failedVisible=bar.fault && bar.capability01<=1.0e-6f &&
                    bar.thrust01<=1.0e-6f;
    require(failedVisible,
            "dead aft motor was represented as an orange live-capability bar");
}


void testInstalledAftMotorMissingSnapshotNeverLooksHealthy()
{
    using namespace game::ship;
    auto d=descriptorFor(Model::AssistedI);
    d.physicalControl.slotModuleIds=
        presets::cobra::installedAssistedEngineModuleIds(Model::AssistedI);
    for(const auto& id:d.physicalControl.slotModuleIds)
    {
        ModuleDescriptor m;
        m.moduleId=id;
        m.maxHealth=100.0f;
        m.engine=game::equipment::engine::presets::
            CobrettyCR1VectorMainDrive();
        d.modules.push_back(std::move(m));
    }
    // Empty sparse graph: this is still a real installed construction, not
    // a diagnostic synthetic set of healthy engines.
    auto ship=shipFor(d);
    const auto states=capability::buildPhysicalControlSlotState(
        d,Model::AssistedI,ship.modules);
    require(states.size()==4 && !states[0].operational &&
            states[0].thrust01<=1.0e-6,
            "missing installed equipment graph manufactured healthy CR-1");
    // An accidentally blank SINGLE binding is not a request to recreate
    // one healthy engine in an otherwise installed physical bank.
    auto partiallyBound=d;
    partiallyBound.physicalControl.slotModuleIds.front().clear();
    const auto incomplete=capability::buildPhysicalControlSlotState(
        partiallyBound,Model::AssistedI,ship.modules);
    require(incomplete.empty(),
            "blank installed motor ID was accepted as a valid construction");
    const auto hud=game::presentation::buildEngineThrustHudPresentation(ship);
    require(hud.visible && hud.warning && totalBars(hud)==4,
            "installed motor with no equipment facts looks operable on HUD");
}

void testHalfDamagedAftMotorLightsCheckAndLimitsBar()
{
    using namespace game::ship;
    auto d=descriptorFor(Model::AssistedI);
    d.physicalControl.slotModuleIds=
        presets::cobra::installedAssistedEngineModuleIds(Model::AssistedI);
    for(const auto& id:d.physicalControl.slotModuleIds)
    {
        ModuleDescriptor m;
        m.moduleId=id;
        m.maxHealth=100.0f;
        m.equipmentFailure.failureAtDamage01=0.95;
        m.engine=game::equipment::engine::presets::
            CobrettyCR1VectorMainDrive();
        d.modules.push_back(std::move(m));
    }
    auto ship=shipFor(d);
    ship.instanceId=97;
    for(const auto& id:d.physicalControl.slotModuleIds)
    {
        game::simulation::ObjectModuleSnapshot m;
        m.moduleId=id;
        m.health=100.0f;
        m.state=static_cast<std::uint8_t>(
            world::modules::ObjectModuleRuntimeState::Attached);
        m.hasEngineThermalState=true;
        m.engineTemperatureK=450.0f;
        m.emitterIntactArea01=1.0f;
        if(id==d.physicalControl.slotModuleIds[0])
            m.internalFailureDamage01=0.50f;
        ship.modules.push_back(std::move(m));
    }
    ship.transform.motion.engineTelemetry.valid=true;
    ship.transform.motion.engineTelemetry.count=4;
    ship.transform.motion.engineTelemetry.available01[0]=1.0; // stale
    ship.transform.motion.engineTelemetry.thrust01[0]=0.9;    // stale
    ship.transform.motion.engineTelemetry.available01[1]=1.0;
    const auto facts=game::ship::computer::collectReplicatedShipFacts(ship,85);
    require(!facts.installedEquipmentBite.faults.empty(),
        "50% installed fault never reached the BITE channel");
    game::ship::computer::MotherComputerSnapshot mother;
    mother.valid=true;
    mother.input=facts;
    const auto cockpit=game::ship::computer::collectCockpitView(ship,mother);
    const auto hud=game::presentation::buildEngineThrustHudPresentation(
        cockpit);
    require(hud.warning,"50% main engine loss failed to light CHECK");
    const auto id=std::string(plant::physicalControlSlots(Model::AssistedI)[0].id);
    bool observed=false;
    for(const auto& group:hud.groups)
        for(const auto& bar:group.bars)
            if(bar.slotId==id)
                observed=bar.fault && bar.capability01>0.0f &&
                    bar.capability01<0.75f &&
                    bar.thrust01<=bar.capability01;
    require(observed,
        "50% disabled CR-1 portrayed full orange capability or red thrust");
}

} // namespace

int main()
{
    try
    {
        testAssistedLayoutGrouping();
        testRuntimeModelOverridesDescriptorModel();
        testNewtonianNeverRendersForeMainBars();
        testFaultFlagComesFromPhysicalSlotAvailability();
        testAssistedForwardThrustUsesAftBars();
        testAssistedLateralVectoringUsesBothBanks();
        testMotherComputerCockpitBarsPreserveLayoutAndBite();
        testNewtonianArgonManifoldReachesEngineHud();
        testInstalledYXSnapshotEngineOutNotMaskedAsDiagnostic();
        testFailedAftMotorReachesMotherComputerHudAndEmergencySight();
        testInstalledAftMotorMissingSnapshotNeverLooksHealthy();
        testHalfDamagedAftMotorLightsCheckAndLimitsBar();

        std::cout
            << "ENGINE THRUST HUD PRESENTATION: PASS\n"
            << " - Assisted I/Y/X render 2/3/4 paired groups\n"
            << " - runtime model overrides descriptor spawn model\n"
            << " - Newtonian HUD never renders fore-main bars\n"
            << " - widget style follows ShipHudProfile\n"
            << " - damaged/unavailable slots raise per-bar + group warning\n"
            << " - yellow bars are per-slot capability\n"
            << " - red bars follow actual per-motor actuator telemetry, never net acceleration\n";
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
