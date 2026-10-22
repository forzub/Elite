#include "src/game/presentation/EngineThrustHudPresentation.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h"
#include "src/game/ship/capability/ReplicatedInstalledAssistedDescriptor.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"

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

    bool sawFault = false;
    for (const auto& group : p.groups)
    {
        for (const auto& bar : group.bars)
            sawFault = sawFault || bar.fault;
    }

    require(sawFault,
            "unavailable physical engine slot did not mark its bar faulty");
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
        testInstalledYXSnapshotEngineOutNotMaskedAsDiagnostic();

        std::cout
            << "ENGINE THRUST HUD PRESENTATION: PASS\n"
            << " - Assisted I/Y/X render 2/3/4 paired groups\n"
            << " - runtime model overrides descriptor spawn model\n"
            << " - Newtonian HUD never renders fore-main bars\n"
            << " - widget style follows ShipHudProfile\n"
            << " - damaged/unavailable slots raise per-bar + group warning\n"
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
