#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"
#include "ShipCore.h"
#include <iostream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#include "core/log.h"
#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/ShipPropulsionState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/NewtonianRcsAllocator.h"
#include "src/game/equipment/radar/RadarDesc.h"
#include "src/game/equipment/data/radars.h"

#include "src/world/modules/ObjectHitBuilder.h"
#include "src/world/modules/ObjectRuntimeHitBuilder.h"

#include "src/game/geometry/ObjLoader.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/game/ship/view/ShipAttachmentResolver.h"
#include "src/game/ship/ShipAttachmentUtils.h"

#include "src/world/navigation/NavigationBodyAdapter.h"


using namespace game::ship::geometry;

namespace game::ship::core
{
namespace
{
// Cobra publishes one immutable descriptor for each of SIX installed layouts.
// Module definitions (including dormant layouts) are identical across these
// profiles; only the active engine/reactor bindings and model differ.
const std::array<ShipDescriptor,6>& cobraInstalledProfiles(
    const ShipDescriptor& original)
{
    static const std::array<ShipDescriptor,6> profiles = [&] {
        std::array<ShipDescriptor,6> installed {
            original,original,original,original,original,original};
        for(std::size_t k=0;k<installed.size();++k)
        {
            const auto model=static_cast<
                game::ship::plant::PhysicalControlModelId>(k);
            auto& desc=installed[k];
            desc.physicalControlModel=model;
            desc.localFlightControlLaw=
                game::ship::plant::nativeFlightLaw(model);
            const bool assisted=game::ship::plant::familyOf(model)==
                game::ship::plant::PhysicalControlFamily::Assisted;
            desc.physicalControl.slotModuleIds=assisted
                ? game::ship::presets::cobra::installedAssistedEngineModuleIds(model)
                : game::ship::presets::cobra::installedNewtonianEngineModuleIds(model);
            desc.powerThermalInstallation.engineReactorModuleIds=assisted
                ? game::ship::presets::cobra::installedAssistedReactorModuleIds(model)
                : game::ship::presets::cobra::installedNewtonianReactorModuleIds(model);
            // Construction is a mandatory installed equipment mapping, not
            // optional damage telemetry. Detect missing/duplicate IDs and
            // absent hardware passports before this profile can fly.
            const auto& installedIds=desc.physicalControl.slotModuleIds;
            const auto& reactorIds=
                desc.powerThermalInstallation.engineReactorModuleIds;
            const auto& topology=
                game::ship::presets::cobra::propulsionSlots(model);
            const auto& groups=
                game::ship::presets::cobra::installationSlots(model).engineReactors;
            if(installedIds.size()!=topology.size() ||
               reactorIds.size()!=groups.size())
                throw std::logic_error(
                    "Invalid Cobra installed installation: slot count");
            const auto requirePassport=[&](const std::string& id,
                                            bool engine) {
                if(id.empty())
                    throw std::logic_error(
                        "Invalid Cobra installed installation: empty module ID");
                const auto it=std::find_if(desc.modules.begin(),desc.modules.end(),
                    [&](const ModuleDescriptor& module) {
                        return module.moduleId==id;
                    });
                if(it==desc.modules.end() ||
                   (engine ? !it->engine : !it->engineReactor))
                    throw std::logic_error(
                        "Invalid Cobra installed installation: missing passport "+id);
            };
            for(std::size_t i=0;i<installedIds.size();++i)
            {
                if(std::find(installedIds.begin(),installedIds.begin()+i,
                             installedIds[i])!=installedIds.begin()+i)
                    throw std::logic_error(
                        "Invalid Cobra installed installation: duplicate engine ID");
                requirePassport(installedIds[i],true);
            }
            for(std::size_t i=0;i<reactorIds.size();++i)
            {
                if(std::find(reactorIds.begin(),reactorIds.begin()+i,
                             reactorIds[i])!=reactorIds.begin()+i)
                    throw std::logic_error(
                        "Invalid Cobra installed installation: duplicate reactor ID");
                requirePassport(reactorIds[i],false);
            }
            desc.mainPropulsion.aftEngineModuleIds.clear();
            desc.mainPropulsion.foreEngineModuleIds.clear();
            for(std::size_t i=0;i<topology.size();++i)
            {
                const auto role=topology[i].role;
                if(role==game::ship::plant::EngineSlotRole::AftMain)
                    desc.mainPropulsion.aftEngineModuleIds.push_back(
                        desc.physicalControl.slotModuleIds[i]);
                else if(role==game::ship::plant::EngineSlotRole::ForeMain)
                    desc.mainPropulsion.foreEngineModuleIds.push_back(
                        desc.physicalControl.slotModuleIds[i]);
            }
        }
        return installed;
    }();
    return profiles;
}
} // namespace



// // -------------------------------------------------
// //    ##                ##       ##
// //                               ##
// //   ###     #####     ###      #####
// //    ##     ##  ##     ##       ##
// //    ##     ##  ##     ##       ##
// //    ##     ##  ##     ##       ## ##
// //   ####    ##  ##    ####       ###
// // -------------------------------------------------
void ShipCore::init(
    ShipRole                inRole,
    const ShipDescriptor&   descriptor,
    glm::vec3               position,
    const ShipInitData&     initData,
    const glm::mat4&        orientation
)
{
    // устанавливаем роль корабля - ИГРОК / НПС
    // общий контекст - глобальные данные, viewport etc
    // геттер из файла описания корабля (f.e. Cobra MKIII)
    // позиция корабля в пространстве

    // Reinitializing an entity must never resurrect motor programs or stale
    // autopilot authority from its previous installed ship.
    m_assistedManeuvers = {};
    m_newtonianRcsAngularDemandRadPerSec2 = {};
    m_newtonianRcsDelivered01.fill(0.0);
    m_newtonianRcsElectricalDemandMW=0.0;
    m_newtonianRcsPhysical = false;
    m_propulsionReactorsOn = true;
    m_serviceThrottleSaved = 0.0;
    m_propulsionSwitchRevision = 0;
    m_propulsionReactorsStartupRemainingSeconds = 0.0;
    m_role = inRole;
    m_initialPhysicalModel=descriptor.physicalControlModel;
    m_cobraHardwareProfiles=nullptr;
    m_nativeDescriptor=&descriptor;
    m_desc=&descriptor;
    if(descriptor.typeId==ObjectType::CobraMk1)
    {
        m_cobraHardwareProfiles=&cobraInstalledProfiles(descriptor);
        const auto idx=static_cast<std::size_t>(descriptor.physicalControlModel);
        m_desc=&(*m_cobraHardwareProfiles)[idx];
    }
    m_state.initialize(*m_desc);
    m_transform.setWorldPositionMeters(glm::dvec3(position));

    // Every Ship -- player, NPC, bot or diagnostic actor -- starts from the
    // same construction state. Runtime control reads this state rather than
    // branching on ownership/role.
    m_transform.motion.localControlLaw =
        m_state.construction.nativeFlightControlLaw;


    // собираем корабль
    // Устанавливаем оборудование по умолчанию из дескриптора
    initShipSlotsFromDescriptor(*m_desc);
    installDefaultEquipment(*m_desc);




    // ===============================================
    // --------------- сердце корабля ----------------
    // ===============================================
    m_reactor = ReactorSystem(descriptor.reactor);
    m_reactor.setThrottle(0.8);

    m_thermal = ThermalSystem(descriptor.thermal);

    m_cooling.init(
        descriptor.cooling.radiatorArea,    // 1600 м²
        descriptor.cooling.panelCount,      // 160 панелей
        descriptor.cooling.emissivity,      // 0.85
        descriptor.cooling.maxTransferPower, // legacy coolant boundary (deferred)
        descriptor.cooling.radiatorMaterialId
    );

    m_powerBus = game::ship::core::PowerBus(m_reactor);
    m_powerBus.registerConsumer(&m_cooling);
    m_powerBus.registerConsumer(&m_lifeSupport);
    m_powerBus.registerConsumer(&m_avionics);
    m_powerBus.registerConsumer(&m_radiationShield);

    m_powerBus.registerConsumer(&m_equipment.radar);
    
    m_powerBus.registerConsumer(&m_equipment.transmitter);
    m_powerBus.registerConsumer(&m_equipment.receiver);



    
    
    
    // visual / UI
    // registry / legal
    // стартовый инвентарь
    
    m_visual = initData.visual;
    m_registry = initData.registry;
    for (Item* item : initData.initialInventory)
    {
        m_inventory.add(item);
    }

    // Автоматическая установка криптокарт в декрипторы
       

    // m_transform.orientation = glm::mat4(1.0f);
    m_transform.orientation = orientation;

    m_transform.pitchRate = 0.0f;
    m_transform.yawRate   = 0.0f;
    m_transform.rollRate  = 0.0f;

    m_transform.forwardVelocity = 0.0f;
    m_transform.localVelocity   = glm::vec3(0.0f);
    m_transform.targetSpeed     = 0.0f;


    // 2. базовое состояние
    SignalPayload payload;
    payload.type   = SignalType::Transponder;
    payload.actor  = m_registry.ownerActor;
    payload.channel = 0;
    payload.setMessage(m_desc->identity.shipName);

    equipment().transmitter.setPayload(payload);
    equipment().transmitter.enableSignal();
    
    

    debugPrintCoreSystems();


    // -------- DAMAGE / MODULE / ASSEMBLY -----------
    m_damageHandler.ship = this;

    initModuleRuntime();
    refreshRuntimeState();

    if (game::ship::geometry::AssemblyMeshLibrary::has(descriptor.typeId))
    {
        const auto& assembly = game::ship::geometry::AssemblyMeshLibrary::get(descriptor.typeId);
        m_assemblyRuntime.init(assembly);
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        descriptor.typeId,
        descriptor,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;
    
}



// // -------------------------------------------------
// //                       ###              ##
// //                        ##              ##
// //  ##  ##   ######       ##    ####     #####    ####
// //  ##  ##    ##  ##   #####       ##     ##     ##  ##
// //  ##  ##    ##  ##  ##  ##    #####     ##     ######
// //  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
// //   ######   ##       ######   #####      ###    #####
// //           ####

void ShipCore::refreshRuntimeState()
{
    if (!m_state.construction.valid() ||
        m_state.actuators.source !=
            game::ship::state::ShipActuatorStateSource::ModuleRuntime)
    {
        return;
    }

    const std::uint64_t moduleRevision =
        m_moduleRuntime.revision();

    if (m_lastModuleProjectionRevision == moduleRevision &&
        m_state.actuators.topologyMatches(
            m_state.construction.physicalControlModel))
    {
        return;
    }

    m_state.actuators.synchronizeFromModules(
        game::ship::capability::buildPhysicalControlSlotState(
            *m_state.construction.definition,
            m_state.construction.physicalControlModel,
            m_moduleRuntime
        )
    );

    m_lastModuleProjectionRevision = moduleRevision;
}

bool ShipCore::selectPhysicalConstructionForDiagnostic(
    game::ship::plant::PhysicalControlModelId model
)
{
    if (!m_state.construction.valid())
        return false;

    // Ctrl+F9 on a Cobra selects real already-installed equipment. The
    // construction binding changes, but module damage/temperatures and ship
    // position, linear/angular velocity are deliberately untouched.
    if(m_cobraHardwareProfiles)
    {
        const auto idx=static_cast<std::size_t>(model);
        if(idx>=m_cobraHardwareProfiles->size())
            return false;
        m_desc=&(*m_cobraHardwareProfiles)[idx];
        m_state.construction.bind(*m_desc);
        m_state.actuators.resetHealthy(
            model,game::ship::state::ShipActuatorStateSource::ModuleRuntime);
        ++m_state.revision;
        m_assistedManeuvers={};
        m_lastModuleProjectionRevision=~std::uint64_t{0};
        m_transform.motion.localControlLaw=
            m_state.construction.nativeFlightControlLaw;
        refreshRuntimeState();
        return m_state.valid();
    }

    // Non-Assisted diagnostics retain native construction bindings.
    if(m_nativeDescriptor)
    {
        m_desc=m_nativeDescriptor;
        m_state.construction.definition=m_desc;
    }
    m_state.selectConstructionForDiagnostic(model);
    m_transform.motion.localControlLaw =
        m_state.construction.nativeFlightControlLaw;
    return true;
}

void ShipCore::clearPhysicalConstructionDiagnosticOverride()
{
    if(m_cobraHardwareProfiles)
    {
        (void)selectPhysicalConstructionForDiagnostic(m_initialPhysicalModel);
        return;
    }
    m_state.clearDiagnosticConstructionOverride();
    m_lastModuleProjectionRevision = ~std::uint64_t{0};
    if (m_state.construction.valid())
    {
        m_transform.motion.localControlLaw =
            m_state.construction.nativeFlightControlLaw;
        refreshRuntimeState();
    }
}

game::ship::capability::ShipCapabilitySnapshot
ShipCore::capabilities() const
{
    if (!m_state.valid())
        return {};

    const std::uint64_t constructionRevision =
        m_state.construction.revision;
    const std::uint64_t actuatorRevision =
        m_state.actuators.revision;
    const std::uint64_t massRevision =
        m_state.massProperties.revision;

    if (m_cachedCapability.valid &&
        m_cachedConstructionRevision == constructionRevision &&
        m_cachedActuatorRevision == actuatorRevision &&
        m_cachedMassRevision == massRevision)
    {
        return m_cachedCapability;
    }

    auto out =
        game::ship::capability::buildShipCapability(
            *m_state.construction.definition,
            m_state.construction.physicalControlModel,
            m_state.actuators.propulsionSlots
        );

    if (out.valid)
    {
        m_state.massProperties.applyTo(out.effectivePhysics);
        // After CURRENT mass is known, calculate force/mass from the actual
        // mounted engine TYPES and their module-derived availability.
        // Diagnostic construction overrides preserve the legacy matrix
        // envelope so I/Y/X probe fixtures cannot invent engine mounts.
        if (m_state.actuators.source ==
                game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
            m_state.construction.physicalControlModel ==
                m_state.construction.definition->physicalControlModel)
        {
            game::ship::capability::applyMountedEngineAxialAuthority(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel,
                m_state.actuators.propulsionSlots,
                out
            );
        }
    }

    m_cachedCapability = out;
    m_cachedConstructionRevision = constructionRevision;
    m_cachedActuatorRevision = actuatorRevision;
    m_cachedMassRevision = massRevision;

    return m_cachedCapability;
}

game::ship::telemetry::ShipControlAuthorityTelemetry
ShipCore::controlAuthorityTelemetry() const
{
    using namespace game::ship::plant;
    using game::equipment::engine::MountedEngine;

    if (!m_state.construction.valid() ||
        familyOf(m_state.construction.physicalControlModel) !=
            PhysicalControlFamily::Assisted)
        return {};

    // The signed actuator basis is NOT usable flight authority while
    // propulsion reactors are switched off or completing their real 10 s
    // restart. Do not advertise a mechanically healthy but unpowered ship
    // to MotherComputer as physically flyable.
    if (!m_propulsionReactorsOn ||
        m_propulsionReactorsStartupRemainingSeconds>0.0)
        return {};

    const auto model = m_state.construction.physicalControlModel;
    const std::uint64_t revision = m_state.actuators.revision;
    const ShipParams physics = effectivePhysics();
    const auto& topology = physicalControlSlots(model);

    // System/thermal updates may run AFTER the engine control tick, changing
    // actuator revision before GameSimulation publishes its snapshot. Refresh
    // the SAME motor-program cache as the executor, without applying forces.
    // Checking revision alone would make live authority invalid every frame
    // of continuous engine thermal evolution.
    std::vector<MountedEngine> installed(topology.size());
    std::array<game::equipment::engine::EngineUnitDescriptor,16> diagnostic {};
    std::array<game::equipment::EquipmentDamageLayer,16> diagnosticDamage {};
    std::array<game::equipment::engine::EngineOperatingState,16>
        diagnosticOperating {};
    if (topology.size() > diagnostic.size())
        return {};

    if (m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
        model == m_state.construction.definition->physicalControlModel)
    {
        const auto& desc = *m_state.construction.definition;
        const auto& ids = desc.physicalControl.slotModuleIds;
        if (ids.size() != topology.size())
            return {};

        const auto& runtime =
            static_cast<const world::modules::ObjectModuleRuntime&>(
                m_moduleRuntime
            );
        for (std::size_t i=0;i<ids.size();++i)
        {
            if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
                continue;
            const auto def = std::find_if(
                desc.modules.begin(), desc.modules.end(),
                [&](const auto& m) { return m.moduleId == ids[i]; }
            );
            const auto* state = runtime.findModule(ids[i]);
            if (def == desc.modules.end() || !def->engine || !state)
                return {};
            installed[i] = {
                &*def->engine,
                &state->equipmentDamage,
                &state->engineOperating,
                ids[i],
                state->state ==
                    world::modules::ObjectModuleRuntimeState::Attached &&
                state->health > 0.0f
            };
        }
    }
    else
    {
        // Diagnostic I/Y/X construction uses the same intentionally
        // synthetic healthy passports as updateTranslationControl(). Do
        // not synthesize those motors for a genuine installed model.
        for (std::size_t i=0;i<topology.size();++i)
        {
            if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
                continue;
            diagnostic[i] =
                game::equipment::engine::presets::
                    CobrettyCR1VectorMainDrive();
            diagnosticDamage[i].failure.failureAtDamage01 = 0.95;
            diagnosticOperating[i].temperatureK =
                diagnostic[i].nominalOperatingTemperatureK;
            installed[i] = {
                &diagnostic[i],
                &diagnosticDamage[i],
                &diagnosticOperating[i],
                topology[i].id,
                true
            };
        }
    }

    // matches() is cheap for unchanged engines; the expensive solver runs
    // only when actual motor force/nozzle availability has changed.
    if (!m_assistedManeuvers.matches(model, installed, physics))
        m_assistedManeuvers.build(model, installed, physics);
    if (!m_assistedManeuvers.valid)
        return {};
    return game::ship::telemetry::readAssistedControlAuthority(
        m_assistedManeuvers,revision
    );
}

ShipParams ShipCore::effectivePhysics() const
{
    const auto capability=capabilities();
    if(capability.valid)
        return capability.effectivePhysics;

    // ShipParams{} is NOT a safe "disabled" preset: its legacy default
    // forwardMainEngineAvailable=true combines with the zero-as-load-limit
    // adapter and can manufacture thrust from a failed capability.
    // Invalid means no capability to accelerate or turn.
    ShipParams disabled {};
    disabled.forwardMainEngineAvailable=false;
    disabled.reverseMainEngineAvailable=false;
    disabled.forwardMainEngineAccelerationMps2=0.0f;
    disabled.reverseMainEngineAccelerationMps2=0.0f;
    disabled.manoeuvreThrusterAuthorityExplicit=true;
    disabled.manoeuvreThrusterAccel=0.0f;
    disabled.strafeAccel=0.0f;
    disabled.angularAccel=0.0f;
    disabled.maxGs=0.0f;
    disabled.maxLinearGs=0.0f;
    return disabled;
}

void ShipCore::updatePhysics(float dt, const WorldParams& world)
{
    updateMotionPhysics(dt, world);
    updateSystems(dt);
}

void ShipCore::updateMotionPhysics(float dt, const WorldParams& world)
{
    const ShipParams physics = effectivePhysics();
    game::ship::control::ShipControlExecutionGateway::integrateAttitude(
        m_transform,
        physics,
        m_state.control,
        world,
        dt
    );
}

void ShipCore::updateMotionControl(float dt, const WorldParams& world)
{
    const ShipParams physics = effectivePhysics();
    const glm::dvec3 angularBefore {
        m_transform.pitchRate,
        m_transform.yawRate,
        m_transform.rollRate
    };
    m_attitudeRatesAtTickStart=angularBefore;
    game::ship::control::ShipControlExecutionGateway::
        evaluateAttitudeControl(
            m_transform,
            physics,
            m_state.control,
            world,
            dt
        );

    auto& motion = m_transform.motion;
    m_newtonianRcsAngularDemandRadPerSec2 = {};
    m_newtonianRcsPhysical = false;
    // Only the native installed Newtonian construction uses the new nozzle
    // executor. Diagnostic override and Assisted retain existing behavior.
    if (dt > 0.0f &&
        motion.mode == game::navigation::MotionMode::HubTactical &&
        m_state.construction.valid() &&
        game::ship::plant::familyOf(
            m_state.construction.physicalControlModel) ==
            game::ship::plant::PhysicalControlFamily::Newtonian &&
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime)
    {
        // The physical 24-column actuator path is mandatory for a native
        // Newtonian craft. A missing/destroyed module provides ZERO thrust,
        // but must never select the old virtual RCS / attitude path.
        m_newtonianRcsPhysical = true;
        const glm::dvec3 after {
            m_transform.pitchRate, m_transform.yawRate,
            m_transform.rollRate
        };
        m_newtonianRcsAngularDemandRadPerSec2 =
            (after-angularBefore)/static_cast<double>(dt);
        // Requested angular rates are not physical until RCS torque acts.
        m_transform.pitchRate=static_cast<float>(angularBefore.x);
        m_transform.yawRate=static_cast<float>(angularBefore.y);
        m_transform.rollRate=static_cast<float>(angularBefore.z);
    }
    motion.requestedMainAngularAccelerationBodyRadPerSec2 = {};
    // The accepted Assisted attitude law still computes the desired angular
    // acceleration (including damping/autopilot). It may NO LONGER create
    // that acceleration without actually commanding installed engines.
    // Convert its rate delta into a six-axis MATRIX demand. Only the engine
    // force/torque result later changes the angular velocity.
    if (dt > 0.0f &&
        motion.mode == game::navigation::MotionMode::HubTactical &&
        m_state.construction.valid() &&
        game::ship::plant::familyOf(
            m_state.construction.physicalControlModel
        ) == game::ship::plant::PhysicalControlFamily::Assisted)
    {
        const glm::dvec3 angularAfter {
            m_transform.pitchRate,
            m_transform.yawRate,
            m_transform.rollRate
        };
        motion.requestedMainAngularAccelerationBodyRadPerSec2 =
            (angularAfter-angularBefore) / static_cast<double>(dt);
        m_transform.pitchRate = static_cast<float>(angularBefore.x);
        m_transform.yawRate = static_cast<float>(angularBefore.y);
        m_transform.rollRate = static_cast<float>(angularBefore.z);
    }
}

void ShipCore::propagateMotionOrientation(float dt)
{
    game::ship::control::ShipControlExecutionGateway::propagateOrientation(
        m_transform,
        dt
    );
}

void ShipCore::updateTranslationControl(float dt)
{
    // Production may bypass the gateway when its installed main-module
    // bindings are missing. Never reuse a previous tick's "executed main"
    // provenance on that fail-closed path.
    m_transform.motion.installedMainPlantExecuted = false;
    const ShipParams physics = effectivePhysics();
    using game::equipment::engine::MountedEngine;
    std::vector<MountedEngine> installedEngines;
    const std::vector<MountedEngine>* physicalEngines = nullptr;

    // A slot is ONLY the location/bank and a binding to an engine ID.
    // Read the ACTUAL installed engine passport, accumulated causes and
    // operating state. No thrust or other physics is authored on the slot.
    // Diagnostic layouts that deliberately substitute a different topology
    // keep the existing isolated slot-only proof path.
    if (m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
        m_state.construction.valid())
    {
        const auto& ship = *m_state.construction.definition;
        const auto& topology = game::ship::plant::physicalControlSlots(
            m_state.construction.physicalControlModel
        );
        const auto& ids = ship.physicalControl.slotModuleIds;
        if (ids.size() == topology.size())
        {
            installedEngines.resize(ids.size());
            bool mainEnginesBound = true;
            const auto& runtime =
                static_cast<const world::modules::ObjectModuleRuntime&>(
                    m_moduleRuntime
                );
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                if (topology[i].role ==
                    game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                    continue;

                const auto descModule = std::find_if(
                    ship.modules.begin(), ship.modules.end(),
                    [&](const auto& module) {
                        return module.moduleId == ids[i];
                    }
                );
                const auto* state = runtime.findModule(ids[i]);
                if (descModule == ship.modules.end() ||
                    !descModule->engine || !state)
                {
                    mainEnginesBound = false;
                    break;
                }
                installedEngines[i] = {
                    &*descModule->engine,
                    &state->equipmentDamage,
                    &state->engineOperating,
                    ids[i],
                    state->state ==
                        world::modules::ObjectModuleRuntimeState::Attached &&
                        state->health > 0.0f
                };
            }
            if (mainEnginesBound)
                physicalEngines = &installedEngines;
        }
    }

    // Ctrl+F9 I/Y/X layouts are synthetic diagnostic ships: their topology
    // has no real module IDs. Mount healthy instances of the SAME generic
    // engine type so diagnosis still tests ΣF / Σ(r×F), not a fake slot
    // impulse. No wear/damage is written back to native ship modules.
    std::array<game::equipment::engine::EngineUnitDescriptor, 16>
        diagnosticPassports {};
    std::array<game::equipment::EquipmentDamageLayer, 16>
        diagnosticDamage {};
    std::array<game::equipment::engine::EngineOperatingState, 16>
        diagnosticOperating {};
    if (!physicalEngines && m_state.construction.valid() &&
        (m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::DiagnosticOverride ||
         m_state.construction.physicalControlModel !=
            m_state.construction.definition->physicalControlModel))
    {
        const auto& topology = game::ship::plant::physicalControlSlots(
            m_state.construction.physicalControlModel
        );
        if (topology.size() <= diagnosticPassports.size())
        {
            installedEngines.assign(topology.size(), MountedEngine {});
            for (std::size_t i=0;i<topology.size();++i)
            {
                if (topology[i].role ==
                    game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                    continue;
                diagnosticPassports[i] =
                    game::equipment::engine::presets::
                        CobrettyCR1VectorMainDrive();
                diagnosticDamage[i].failure.failureAtDamage01=0.95;
                diagnosticOperating[i].temperatureK =
                    diagnosticPassports[i].nominalOperatingTemperatureK;
                installedEngines[i]={
                    &diagnosticPassports[i],
                    &diagnosticDamage[i],
                    &diagnosticOperating[i],
                    topology[i].id,
                    true
                };
            }
            physicalEngines=&installedEngines;
        }
    }

    // Resolve the already-installed propulsion generators by slot identity.
    // Missing/destroyed ER-5 equipment grants NO free propulsion electricity.
    // One assisted reactor shares its rated output across its fore/aft pair.
    std::vector<game::ship::control::MainEnginePowerGroup> powerGroups;
    if (physicalEngines && m_desc)
    {
        const auto& installation =
            game::ship::presets::cobra::installationSlots(
                m_state.construction.physicalControlModel);
        const auto& reactorIds =
            m_desc->powerThermalInstallation.engineReactorModuleIds;
        const auto& engineTopology =
            game::ship::plant::physicalControlSlots(
                m_state.construction.physicalControlModel);
        const auto& runtime =
            static_cast<const world::modules::ObjectModuleRuntime&>(
                m_moduleRuntime);
        {
            powerGroups.reserve(installation.engineReactors.size());
            for (std::size_t n=0; n<installation.engineReactors.size(); ++n)
            {
                game::ship::control::MainEnginePowerGroup group;
                for (const auto* engineId :
                     installation.engineReactors[n].poweredEngineSlots)
                    for (std::size_t i=0; i<engineTopology.size(); ++i)
                        if (std::string_view(engineTopology[i].id) == engineId)
                            group.engineIndices.push_back(i);

                const std::string reactorId =
                    n < reactorIds.size() ? reactorIds[n] : std::string {};
                const auto* reactor = reactorId.empty()
                    ? nullptr : runtime.findModule(reactorId);
                const auto definition = std::find_if(
                    m_desc->modules.begin(),m_desc->modules.end(),
                    [&](const auto& module) {
                        return module.moduleId == reactorId;
                    });
                if (reactor && definition != m_desc->modules.end() &&
                    definition->engineReactor &&
                    reactor->state ==
                        world::modules::ObjectModuleRuntimeState::Attached &&
                    reactor->health > 0.0f)
                {
                    const auto function =
                        game::equipment::evaluateEquipmentFunctionAtDamage01(
                            game::equipment::totalAccumulatedDamage01(
                                reactor->equipmentDamage.accumulated),
                            definition->engineReactor->failure);
                    const auto& reactorType=*definition->engineReactor;
                    const double t=reactor->engineReactorOperating.temperatureK;
                    const double ready=std::clamp(
                        (t-game::physics::thermal::DeepSpaceTemperatureK)/
                        std::max(1.0,reactorType.idleTemperatureK-
                            game::physics::thermal::DeepSpaceTemperatureK),
                        0.0,1.0);
                    const double thermal= t<=reactorType.safeTemperatureK
                        ? 1.0 : std::clamp(
                            (reactorType.criticalTemperatureK-t)/
                            std::max(1.0,reactorType.criticalTemperatureK-
                                reactorType.safeTemperatureK),0.0,1.0);
                    const double age=std::clamp(
                        reactor->engineReactorOperating.equivalentOperatingHours/
                        std::max(1.0,reactorType.endOfLifeEquivalentHours),
                        0.0,1.0);
                    group.availableMW =
                        (m_propulsionReactorsOn &&
                        m_propulsionReactorsStartupRemainingSeconds<=0.0 ? 1.0:0.0) *
                        std::max(0.0,reactorType.ratedElectricalMW) *
                        function.authority01 * ready * thermal *
                        (1.0-0.20*age);
                }
                // I/Y/X diagnostic layouts already mount synthetic CR-1
                // motors. They must also have matching synthetic ER-5
                // electrical budgets, otherwise non-native layouts have
                // zero thrust even though motors are visibly installed.
                // Native construction NEVER receives this fallback.
                const bool diagnosticLayout=
                    m_state.actuators.source ==
                        game::ship::state::ShipActuatorStateSource::DiagnosticOverride ||
                    m_state.construction.physicalControlModel !=
                        m_state.construction.definition->physicalControlModel;
                if(diagnosticLayout && group.availableMW<=0.0 &&
                   m_propulsionReactorsOn &&
                   m_propulsionReactorsStartupRemainingSeconds<=0.0)
                {
                    group.availableMW=5000.0;
                }
                powerGroups.push_back(std::move(group));
            }
        }
    }

    const bool productionInstalled =
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
        m_state.construction.valid() &&
        m_state.construction.physicalControlModel ==
            m_state.construction.definition->physicalControlModel;

    // A genuine installed craft must NEVER silently change to a slot-only
    // diagnostic model if an engine module binding is missing. Nor may
    // an unassigned engine draw electricity from a nonexistent generator.
    // Leave the deliberately synthetic Ctrl+F9 layouts independent.
    if (productionInstalled && physicalEngines)
    {
        const auto& topology=game::ship::plant::physicalControlSlots(
            m_state.construction.physicalControlModel);
        for (std::size_t i=0;i<topology.size();++i)
        {
            if (topology[i].role ==
                game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                continue;
            bool assigned=false;
            for (const auto& group:powerGroups)
                assigned |= std::find(group.engineIndices.begin(),
                    group.engineIndices.end(),i)!=group.engineIndices.end();
            if (!assigned)
            {
                // Factual lack of a power feed: zero available MW.
                game::ship::control::MainEnginePowerGroup missing;
                missing.engineIndices.push_back(i);
                powerGroups.push_back(std::move(missing));
            }
        }
    }

    if (productionInstalled && !physicalEngines)
    {
        // Fail closed without short-circuiting the separate Newtonian RCS
        // executor below. The control law's requests are NOT executed.
        auto& motion=m_transform.motion;
        motion.requestedMainEngineAccelerationMps2=
            motion.mainEngineAccelerationMps2;
        motion.requestedManoeuvreAccelerationMps2=
            motion.manoeuvreAccelerationMps2;
        motion.requestedAssistedStabilizationAccelerationMps2=
            motion.assistedStabilizationAccelerationMps2;
        motion.mainEngineAccelerationMps2={};
        motion.assistedStabilizationAccelerationMps2={};
        if (game::ship::plant::familyOf(
                m_state.construction.physicalControlModel) ==
            game::ship::plant::PhysicalControlFamily::Assisted)
            motion.manoeuvreAccelerationMps2={};
        motion.engineAccelerationMps2=motion.manoeuvreAccelerationMps2;
        motion.mainPropulsionTorqueBodyNm={};
        motion.mainPropulsionAngularAccelerationBodyRadPerSec2={};
        motion.mainEngineDeliveredThrust01.fill(0.0);
        motion.mainEngineReportedSlotCount=0;
        motion.engineTelemetry.reset(m_state.control.controlTick);
    }
    else
    {
        (void)game::ship::control::ShipControlExecutionGateway::
            evaluateTranslationControl(
                m_transform,
                physics,
                m_state.control,
                m_state.construction.physicalControlModel,
                m_state.actuators.propulsionSlots,
                dt,
                physicalEngines,
                &m_assistedManeuvers,
                productionInstalled ? &powerGroups :
                    (powerGroups.empty() ? nullptr : &powerGroups)
            );
    }
    // Every genuine Newtonian control tick must account for REAL fixed-nozzle
    // force/torque. Missing modules are null MountedEngine columns (zero force),
    // NEVER permission to fall back to the old omnidirectional RCS model.
    m_newtonianRcsDelivered01.fill(0.0);
    m_newtonianRcsElectricalDemandMW=0.0;
    auto& rcsMotion=m_transform.motion;
    rcsMotion.installedRcsPlantExecuted=false;
    const bool installedNewtonian =
        m_state.construction.valid() &&
        game::ship::plant::familyOf(
            m_state.construction.physicalControlModel) ==
                game::ship::plant::PhysicalControlFamily::Newtonian &&
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime;
    if (installedNewtonian && dt>0.0f &&
        rcsMotion.mode==game::navigation::MotionMode::HubTactical)
    {
        m_newtonianRcsPhysical=true;
        rcsMotion.installedRcsPlantExecuted=true;
        using game::equipment::engine::MountedEngine;
        std::array<MountedEngine,24> installed {};
        const auto& desc=*m_state.construction.definition;
        const auto& runtime=
            static_cast<const world::modules::ObjectModuleRuntime&>(
                m_moduleRuntime);
        const auto& mounts =
            game::ship::presets::cobra::newtonianRcsNozzles();
        for(std::size_t i=0;i<mounts.size();++i)
        {
            const std::string id(mounts[i].nozzleId);
            const auto def=std::find_if(
                desc.modules.begin(),desc.modules.end(),
                [&](const auto& module){return module.moduleId==id;});
            const auto* state=runtime.findModule(id);
            if(def==desc.modules.end() || !def->engine || !state)
                continue; // Missing equipment is a ZERO-thrust column.
            installed[i]={
                &*def->engine,&state->equipmentDamage,
                &state->engineOperating,mounts[i].nozzleId,
                state->state ==
                    world::modules::ObjectModuleRuntimeState::Attached &&
                state->health>0.0f &&
                m_propulsionReactorsOn &&
                m_propulsionReactorsStartupRemainingSeconds<=0.0
            };
        }

        const glm::dvec3 oldRcsWorld=rcsMotion.manoeuvreAccelerationMps2;
        const glm::dvec3 right(m_transform.right());
        const glm::dvec3 up(m_transform.up());
        const glm::dvec3 forward(m_transform.forward());
        const double mass=std::max(1.0,physics.massKg);
        const glm::dvec3 inertia {
            std::max(1.0,physics.pitchInertiaKgM2),
            std::max(1.0,physics.yawInertiaKgM2),
            std::max(1.0,physics.rollInertiaKgM2)
        };
        game::ship::plant::NewtonianRcsDemand demand {
            glm::dvec3 {
                glm::dot(oldRcsWorld,right),
                glm::dot(oldRcsWorld,up),
                -glm::dot(oldRcsWorld,forward)
            }*mass,
            // ShipTransform rolls around hull FORWARD (-Z), so the physical
            // torque about BODY +Z must have the OPPOSITE sign.
            m_newtonianRcsAngularDemandRadPerSec2*
                glm::dvec3{inertia.x,inertia.y,-inertia.z},
            physics.physicalActuatorHalfExtentsMeters,
            m_state.control.controlTick
        };
        demand.centerOfMassLocalMeters=physics.centerOfMassLocalMeters;

        // RCS electricity belongs to the ship-services reactor, not the
        // independent ER-5 main-drive power groups. Essential loads have
        // priority. No spare generated MW means no nozzle thrust.
        double committedServicesMW=0.0;
        for(const auto* consumer:m_powerBus.getConsumers())
            if(consumer)
                committedServicesMW+=std::max(
                    0.0,consumer->getAvailablePower());
        demand.availableElectricalMW=std::max(
            0.0,m_reactor.getCurrentOutput()-committedServicesMW);
        if(!m_propulsionReactorsOn ||
           m_propulsionReactorsStartupRemainingSeconds>0.0)
            demand.availableElectricalMW=0.0;

        // Replenished gas is a gameplay duty budget. BOTH manual translation
        // and angular damping consume it; nozzles are throttled BEFORE firing.
        // This cannot remove executed Δv or Δω during later integration.
        const double lateralAuthority=std::max(
            1.0e-9,game::ship::manoeuvreAccelerationLimitMps2(physics));
        const double angularAuthority=std::max(
            1.0e-9,game::ship::angularAccelerationLimitRadPerSec2(physics));
        bool nozzleCanActuallyFire=false;
        if(demand.availableElectricalMW>1.0e-9)
            for(std::size_t i=0;i<installed.size();++i)
                if(game::equipment::engine::Engine(installed[i]).
                    instantaneousAvailableThrustNewtons(demand.tick)>1.0e-9)
                {
                    nozzleCanActuallyFire=true;
                    break;
                }
        const double intensity=nozzleCanActuallyFire
            ? std::clamp(std::max(
                glm::length(oldRcsWorld)/lateralAuthority,
                glm::length(m_newtonianRcsAngularDemandRadPerSec2)/
                    angularAuthority),0.0,1.0)
            : 0.0;
        const double gasScale=game::ship::plant::advanceNewtonianRcsGas(
            rcsMotion.manoeuvreGasPressure01,
            rcsMotion.manoeuvreGasDepleted,
            static_cast<double>(dt),intensity,
            physics.manoeuvreGasUsePerSecond,
            physics.manoeuvreGasRechargePerSecond,
            physics.manoeuvreGasRestartFraction);
        demand.forceBodyN*=gasScale;
        demand.momentBodyNm*=gasScale;

        const auto executed=
            game::ship::plant::allocateNewtonianRcs(installed,demand);
        // The gateway may already have delivered main-engine thrust.
        // Replace ONLY its old RCS request, retaining the real main impulse.
        rcsMotion.manoeuvreAccelerationMps2={};
        rcsMotion.engineAccelerationMps2-=oldRcsWorld;
        if(executed.valid)
        {
            const glm::dvec3 a=executed.deliveredForceN/mass;
            const glm::dvec3 actualWorld=
                right*a.x+up*a.y-forward*a.z;
            rcsMotion.manoeuvreAccelerationMps2=actualWorld;
            rcsMotion.engineAccelerationMps2+=actualWorld;
            const auto angular=executed.deliveredMomentNm/inertia;
            m_transform.pitchRate+=static_cast<float>(angular.x*dt);
            m_transform.yawRate+=static_cast<float>(angular.y*dt);
            m_transform.rollRate-=static_cast<float>(angular.z*dt);
            m_newtonianRcsDelivered01=executed.throttle01;
            for(const double drawMW:executed.electricalMW)
                m_newtonianRcsElectricalDemandMW+=std::max(0.0,drawMW);
        }
    }

    // Bounded player-only server instrumentation. Does NOT change flight
    // dynamics, actuator commands, allocator state or timing of control.
    // Starts at the first manual attitude or autopilot angular command and
    // records 90 s into D:/tmp for correlation with the user's video.
    if (m_role==ShipRole::Player && dt>0.0f &&
        m_transform.motion.mode==
            game::navigation::MotionMode::HubTactical)
    {
        static std::ofstream trace;
        static bool attempted=false;
        static bool finished=false;
        static double seconds=0.0;
        static std::uint64_t records=0;
        const auto& ctrl=m_state.control;
        const auto& motion=m_transform.motion;
        const auto& commanded=
            motion.requestedMainAngularAccelerationBodyRadPerSec2;
        const bool triggered=
            std::abs(ctrl.pitchInput)>0.01f ||
            std::abs(ctrl.yawInput)>0.01f ||
            std::abs(ctrl.rollInput)>0.01f ||
            glm::length(commanded)>1.0e-4 ||
            ctrl.navigationAccelerationDemandValid;
        if (!attempted && !finished && triggered)
        {
            attempted=true;
            std::error_code ec;
            std::filesystem::create_directories("D:/tmp",ec);
            trace.open("D:/tmp/elite_attitude_jitter.csv",
                std::ios::out | std::ios::trunc);
            if(trace)
            {
                trace << std::fixed << std::setprecision(8);
                trace << "seconds,dt,control_tick,model,source,manual_pitch,"
                      << "manual_yaw,manual_roll,nav_angular,reactor_on,"
                      << "reactor_start_s,angular_before_pitch,"
                      << "angular_before_yaw,angular_before_roll,"
                      << "angular_after_pitch,angular_after_yaw,"
                      << "angular_after_roll,request_pitch_acc,"
                      << "request_yaw_acc,request_roll_acc,"
                      << "actual_pitch_acc,actual_yaw_acc,"
                      << "actual_roll_acc,actual_pitch_torque,"
                      << "actual_yaw_torque,actual_roll_torque,"
                      << "linear_body_ax,linear_body_ay,linear_body_az,"
                      << "velocity_mps,program_valid,engine_sample_valid,"
                      << "engine_count,total_generator_MW";
                for(std::size_t i=0;i<8;++i)
                    trace << ",motor" << i << "_thrust01"
                          << ",motor" << i << "_yaw_rad"
                          << ",motor" << i << "_pitch_rad"
                          << ",motor" << i << "_MW";
                trace << ",axis_emergency,axis_body_x,axis_body_y,"
                         "axis_body_z,axis_offset_deg,axis_motor_index";
                trace << '\n';
                std::cerr << "[AttitudeJitterTrace] BEGIN "
                             "D:/tmp/elite_attitude_jitter.csv"
                          << std::endl;
            }
            else
            {
                finished=true;
                std::cerr << "[AttitudeJitterTrace] ERROR cannot open "
                             "D:/tmp/elite_attitude_jitter.csv"
                          << std::endl;
            }
        }
        if(trace.is_open() && !finished)
        {
            seconds+=static_cast<double>(dt);
            ++records;
            double totalAvailableMW=0.0;
            for(const auto& group:powerGroups)
                totalAvailableMW+=std::max(0.0,group.availableMW);
            const auto& a=
                motion.mainPropulsionAngularAccelerationBodyRadPerSec2;
            const auto& torque=motion.mainPropulsionTorqueBodyNm;
            const auto& acc=motion.mainEngineAccelerationMps2;
            const glm::dvec3 currentRate {
                m_transform.pitchRate,m_transform.yawRate,
                m_transform.rollRate
            };
            trace << seconds << ',' << dt << ','
                  << ctrl.controlTick << ','
                  << static_cast<int>(
                      m_state.construction.physicalControlModel) << ','
                  << static_cast<int>(m_state.actuators.source) << ','
                  << ctrl.pitchInput << ',' << ctrl.yawInput << ','
                  << ctrl.rollInput << ','
                  << (ctrl.navigationAccelerationDemandValid?1:0) << ','
                  << (m_propulsionReactorsOn?1:0) << ','
                  << m_propulsionReactorsStartupRemainingSeconds << ','
                  << m_attitudeRatesAtTickStart.x << ','
                  << m_attitudeRatesAtTickStart.y << ','
                  << m_attitudeRatesAtTickStart.z << ','
                  << currentRate.x << ',' << currentRate.y << ','
                  << currentRate.z << ','
                  << commanded.x << ',' << commanded.y << ','
                  << commanded.z << ','
                  << a.x << ',' << a.y << ',' << a.z << ','
                  << torque.x << ',' << torque.y << ','
                  << torque.z << ','
                  << acc.x << ',' << acc.y << ',' << acc.z << ','
                  << glm::length(motion.localVelocityMps) << ','
                  << (m_assistedManeuvers.valid?1:0) << ','
                  << (motion.engineTelemetry.valid?1:0) << ','
                  << static_cast<int>(motion.engineTelemetry.count) << ','
                  << totalAvailableMW;
            for(std::size_t i=0;i<8;++i)
            {
                if(motion.engineTelemetry.valid &&
                   i<motion.engineTelemetry.count)
                    trace << ',' << motion.engineTelemetry.thrust01[i]
                          << ',' << motion.engineTelemetry.nozzleYawRad[i]
                          << ',' << motion.engineTelemetry.nozzlePitchRad[i]
                          << ',' << motion.engineTelemetry.electricalMW[i];
                else
                    trace << ",0,0,0,0";
            }
            trace << ','
                  << (physics.assistedWorkingThrustAxisEmergency?1:0) << ','
                  << physics.assistedWorkingThrustAxisBody.x << ','
                  << physics.assistedWorkingThrustAxisBody.y << ','
                  << physics.assistedWorkingThrustAxisBody.z << ','
                  << physics.assistedWorkingThrustAxisAngleRad*
                       57.29577951308232 << ','
                  << physics.assistedWorkingThrustMotorIndex;
            trace << '\n';
            if(records%30==0)trace.flush();
            if(seconds>=90.0)
            {
                trace.flush();
                trace.close();
                finished=true;
                std::cerr << "[AttitudeJitterTrace] COMPLETE rows="
                          << records << " path="
                             "D:/tmp/elite_attitude_jitter.csv"
                          << std::endl;
            }
        }
    }
}

bool ShipCore::propagateHubTacticalTranslation(double dt)
{
    const ShipParams physics = effectivePhysics();
    return game::ship::control::ShipControlExecutionGateway::
        propagateHubTacticalTranslation(
            m_transform,
            physics,
            dt
        );
}

void ShipCore::togglePropulsionReactors() noexcept
{
    ++m_propulsionSwitchRevision;
    if(m_propulsionReactorsOn)
    {
        m_serviceThrottleSaved=m_reactor.getThrottle();
        m_propulsionReactorsOn=false;
        m_propulsionReactorsStartupRemainingSeconds=0.0;
    }
    else
    {
        m_propulsionReactorsOn=true;
        m_propulsionReactorsStartupRemainingSeconds=10.0;
    }
    // The service reactor is also part of "ALL reactors". No electrical
    // output before the explicit startup countdown has finished.
    m_reactor.setThrottle(0.0);
}

void ShipCore::toggleAssistedFreeRotation() noexcept
{
    auto& motion=m_transform.motion;
    if(motion.localControlLaw!=game::navigation::LocalFlightControlLaw::Assisted)
        return;
    motion.assistedFreeRotation=!motion.assistedFreeRotation;
    motion.assistedTargetSpeedHold=false;
    motion.assistedThrottleTrimWasActive=false;
    motion.velocityAlignmentMode=motion.assistedFreeRotation
        ? game::navigation::VelocityAlignmentMode::None
        : game::navigation::VelocityAlignmentMode::ForwardToVelocity;
}

void ShipCore::updateSystems(float dt)
{
    if (dt <= 0.0f)
        return;
    if(m_propulsionReactorsOn && m_propulsionReactorsStartupRemainingSeconds>0.0)
        m_propulsionReactorsStartupRemainingSeconds=std::max(0.0,
            m_propulsionReactorsStartupRemainingSeconds-static_cast<double>(dt));
    if(m_propulsionReactorsOn &&
       m_propulsionReactorsStartupRemainingSeconds<=0.0)
        m_reactor.setThrottle(m_serviceThrottleSaved);

    // Internal service simulation is deliberately separated from motion.
    // Activation may decimate this lane for non-player ships while world
    // kinematics remain on the authoritative fixed step. The accumulated dt
    // passed by GameSimulation preserves elapsed service time.

    m_thermal.resetHeatVolume();

    m_cooling.setReactorState(
        m_reactor.getTemperature(),
        m_reactor.getWorkTemp(),
        m_reactor.getDeltaTemp(),
        m_reactor.getWarningTemp(),
        m_thermal.getTemperature(),
        m_thermal.getCriticalTemp()
    );
    m_cooling.calcRequestedPower();

    m_powerBus.setAuxiliaryDemandMW(
        m_newtonianRcsPhysical ? m_newtonianRcsElectricalDemandMW : 0.0);
    m_powerBus.update();
    // Do not feed the reactor last frame's circulation capacity.
    m_cooling.prepareTransport();
    m_reactor.update(dt, m_thermal, m_cooling.getPumpCapacity());

    for (auto* consumer : m_powerBus.getConsumers())
    {
        if (consumer == &m_cooling)
            continue;

        auto* heatSource =
            dynamic_cast<game::equipment::IHeatSource*>(consumer);

        if (heatSource)
        {
            const double heatMW = heatSource->getHeatGeneration();
            if (heatMW > 0.0)
                m_thermal.addHeat(heatMW * dt);
        }
    }

    // Radiators run AFTER installed engines have deposited their physically
    // transferred heat in the same shared coolant volume. The engine nozzle
    // already radiated its own part directly into space.
    m_lifeSupport.update(dt);
    m_avionics.update(dt);
    m_radiationShield.update(dt);

    if (m_powerBus.overloaded())
        m_thermal.addHeat(5.0 * dt);

    m_thermal.update(dt);

    // Engine life belongs to the real installed module, never to a pilot
    // command, diagnostic slot, or ship-wide time counter. The actual
    // physical allocator reports the delivered fraction of healthy thrust.
    // Use the actual common coolant temperature: damaged pumps/radiator
    // panels now heat every engine through its real finite thermal inertia.
    if (m_desc &&
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime)
    {
        const auto& topology =
            game::ship::plant::physicalControlSlots(
                m_state.construction.physicalControlModel
            );
        const auto& bindings =
            m_desc->physicalControl.slotModuleIds;
        const auto& motion = m_transform.motion;
        if (bindings.size() == topology.size())
        {
            // Keep cooling on idle/docked/non-tactical engines. Only a
            // currently valid allocator sample can consume operating life.
            const bool actualBurnSample =
                motion.mode == game::navigation::MotionMode::HubTactical &&
                motion.mainEngineReportedSlotCount == bindings.size();
            bool changed = false;
            // One shared coolant snapshot for this systems tick. Each engine
            // deposits only its CONDUCTED part, never the nozzle's radiated
            // heat. Engine internal heat storage remains on its own module.
            const double coolantK=m_thermal.getTemperature();
            for (std::size_t i = 0; i < bindings.size(); ++i)
            {
                if (bindings[i].empty() ||
                    topology[i].role ==
                        game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                {
                    continue;
                }

                const auto definition = std::find_if(
                    m_desc->modules.begin(),
                    m_desc->modules.end(),
                    [&](const auto& module)
                    {
                        return module.moduleId == bindings[i];
                    }
                );
                if (definition == m_desc->modules.end() ||
                    !definition->engine)
                    continue;

                double transferredMW=0.0;
                changed |= m_moduleRuntime.advanceEngineOperatingLife(
                    bindings[i],
                    *definition->engine,
                    actualBurnSample
                        ? motion.mainEngineDeliveredThrust01[i] : 0.0,
                    static_cast<double>(dt),
                    coolantK,
                    &transferredMW
                );
                if(std::isfinite(transferredMW))
                {
                    const double energyMJ=transferredMW*dt;
                    if(energyMJ>=0.0)
                        m_thermal.addHeat(energyMJ);
                    else
                        m_thermal.removeHeat(-energyMJ);
                }
            }

            if (changed)
                refreshRuntimeState();
        }
    }
    // Installed ER-5 generator thermal clocks run from EXECUTED motor
    // electrical draw. Passive standby is accounted for every service tick.
    // Heat not accepted by a real emitter is stored in its thermal mass;
    // shared-coolant transfer remains zero until a dedicated generator
    // transfer-capacity interface is available (never invent a pump).
    if(m_desc &&
       m_state.actuators.source ==
           game::ship::state::ShipActuatorStateSource::ModuleRuntime)
    {
        const auto& generators=
            game::ship::presets::cobra::installationSlots(
                m_state.construction.physicalControlModel).engineReactors;
        const auto& reactorIds=
            m_desc->powerThermalInstallation.engineReactorModuleIds;
        const auto& engineSlots=
            game::ship::plant::physicalControlSlots(
                m_state.construction.physicalControlModel);
        const auto& engineIds=m_desc->physicalControl.slotModuleIds;
        const auto& motors=m_transform.motion.engineTelemetry;
        for(std::size_t g=0;g<generators.size() &&
            g<reactorIds.size();++g)
        {
            auto* module=m_moduleRuntime.findModule(reactorIds[g]);
            const auto def=std::find_if(
                m_desc->modules.begin(),m_desc->modules.end(),
                [&](const auto& item){
                    return item.moduleId==reactorIds[g];});
            if(!module || def==m_desc->modules.end() ||
                !def->engineReactor) continue;
            double electricalMW=0.0;
            for(const char* slotId:generators[g].poweredEngineSlots)
                for(std::size_t i=0;i<engineSlots.size() &&
                    i<engineIds.size();++i)
                    if(std::string_view(engineSlots[i].id)==slotId &&
                       motors.valid && i<motors.count &&
                       std::isfinite(motors.electricalMW[i]))
                        electricalMW+=std::max(0.0,motors.electricalMW[i]);
            // ER-5 has its OWN square heat pad, never borrows a main motor
            // emitter. One universe cooling density applies to ALL pads.
            const double padHeadroomMW=
                game::physics::thermal::PropulsionPadCoolingMWPerM2*
                def->engineReactor->padSideM*
                def->engineReactor->padSideM;
            auto& state=module->engineReactorOperating;
            state.damage=module->equipmentDamage.accumulated;
            const bool installed=module->state ==
                world::modules::ObjectModuleRuntimeState::Attached &&
                module->health>0.0f;
            const auto tick=game::equipment::reactor::advanceEngineReactor(
                *def->engineReactor,state,
                installed && m_propulsionReactorsOn &&
                    m_propulsionReactorsStartupRemainingSeconds<=0.0
                    ? electricalMW:0.0,
                padHeadroomMW,0.0,static_cast<double>(dt));
            (void)tick;
        }
    }
    // Newtonian's 24 real RCS units have independent operating clocks and
    // thermal state. The duty values come from the executed physical wrench,
    // never from pilot key states. A destroyed unit remains disabled by the
    // same ObjectModuleRuntime damage law as a main engine.
    if (m_desc && m_newtonianRcsPhysical &&
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime)
    {
        const auto& nozzles=
            game::ship::presets::cobra::newtonianRcsNozzles();
        const double coolantK=m_thermal.getTemperature();
        bool changed=false;
        for(std::size_t i=0;i<nozzles.size();++i)
        {
            const std::string id(nozzles[i].nozzleId);
            const auto definition=std::find_if(
                m_desc->modules.begin(),m_desc->modules.end(),
                [&](const auto& module){return module.moduleId==id;});
            if(definition==m_desc->modules.end() ||
                !definition->engine) continue;
            double transferredMW=0.0;
            changed |= m_moduleRuntime.advanceEngineOperatingLife(
                id,*definition->engine,
                m_transform.motion.mode ==
                    game::navigation::MotionMode::HubTactical
                    ? m_newtonianRcsDelivered01[i] : 0.0,
                static_cast<double>(dt),coolantK,&transferredMW
            );
            if(std::isfinite(transferredMW))
            {
                const double energyMJ=transferredMW*dt;
                if(energyMJ>=0.0)
                    m_thermal.addHeat(energyMJ);
                else
                    m_thermal.removeHeat(-energyMJ);
            }
        }
        if(changed) refreshRuntimeState();
    }
    m_cooling.update(dt, m_thermal);
}



void ShipCore::updatePerception(
    float dt,
    const std::vector<world::RadarContactInput>& radarInputs
)
{
    if (m_equipment.radar.isOperational())
    {
        m_equipment.radar.update(
            dt,
            radarInputs,
            m_transform.worldPosition,
            m_transform.orientation
        );

        const auto& contacts = m_equipment.radar.getContacts();

        m_radarDebugTimer += dt;
    }

    if (m_role == ShipRole::Player)
    {
        // HUD / визуальная семантика
        // m_signalPresentation.update(dt, m_signalResults);
    }
    else
    {
        // AI-восприятие
        m_npcAwareness.update(m_signalResults);
    }
}



//                                        ##                                 ###
//                                                                            ##
//  ##  ##   ######             #####    ###      ### ##  #####     ####      ##      #####
//  ##  ##    ##  ##           ##         ##     ##  ##   ##  ##       ##     ##     ##
//  ##  ##    ##  ##            #####     ##     ##  ##   ##  ##    #####     ##      #####
//  ##  ##    #####                 ##    ##      #####   ##  ##   ##  ##     ##          ##
//   ######   ##               ######    ####        ##   ##  ##    #####    ####    ######
//           ####                                #####

void ShipCore::updateSignals(
         float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources,
        EntityId ownerId
    )
{
    m_signalResults.clear();

    m_signalReceiver.update(
        dt,
        m_transform.worldPosition,
        m_equipment.receiver,
        worldSignals,
        planets,
        interferenceSources,
        m_signalResults,
        ownerId
    );
}

                                                                            

//                                                          ##     ######    ####
//                                                         ####     ##  ##    ##
//   ####     ####    ######    ### ##   ####             ##  ##    ##  ##    ##
//  ##  ##       ##    ##  ##  ##  ##   ##  ##            ##  ##    #####     ##
//  ##        #####    ##      ##  ##   ##  ##            ######    ##        ##
//  ##  ##   ##  ##    ##       #####   ##  ##            ##  ##    ##        ##
//   ####     #####   ####         ##    ####             ##  ##   ####      ####
//                             #####


int ShipCore::freeCargoMass() const
{
    return m_cargo.mass.free();
}

int ShipCore::freeCargoVolume() const
{
    return m_cargo.volume.free();
}

bool ShipCore::addCargo(int mass, int volume)
{
    if (!m_cargo.mass.canAdd(mass)) return false;
    if (!m_cargo.volume.canAdd(volume)) return false;

    m_cargo.mass.add(mass);
    m_cargo.volume.add(volume);
    return true;
}

bool ShipCore::removeCargo(int mass, int volume)
{
    if (!m_cargo.mass.canRemove(mass)) return false;
    if (!m_cargo.volume.canRemove(volume)) return false;

    m_cargo.mass.remove(mass);
    m_cargo.volume.remove(volume);
    return true;
}

// //            ###                ##                         ##                ##       ##
// //             ##                ##                                                    ##
// //   #####     ##      ####     #####    #####             ###     #####     ###      #####
// //  ##         ##     ##  ##     ##     ##                  ##     ##  ##     ##       ##
// //   #####     ##     ##  ##     ##      #####              ##     ##  ##     ##       ##
// //       ##    ##     ##  ##     ## ##       ##             ##     ##  ##     ##       ## ##
// //  ######    ####     ####       ###   ######             ####    ##  ##    ####       ###


void ShipCore::initShipSlotsFromDescriptor(const ShipDescriptor& descriptor)
{
    m_desc = &descriptor;

    // Cargo
    m_cargo.mass.capacity   = m_desc->storage.cargoMass;
    m_cargo.mass.value      = 0;
    m_cargo.volume.capacity = m_desc->storage.cargoVolume;
    m_cargo.volume.value    = 0;

    // System slots
    m_systems.reactor.capacity            = m_desc->systems.reactorSlots;
    m_systems.engine.capacity             = m_desc->systems.engineSlots;
    m_systems.radar.capacity              = m_desc->systems.radarSlots;
    m_systems.weapon.capacity             = m_desc->systems.weaponSlots;
    m_systems.dockingComputer.capacity    = m_desc->systems.dockingComputerSlots;
    m_systems.decryptor.capacity          = m_desc->systems.decryptorSlots;
    m_systems.jammer.capacity             = m_desc->systems.jammerSlots;
    m_systems.receiver.capacity           = m_desc->systems.receiverSlots;
    m_systems.transmitter.capacity        = m_desc->systems.transmitterSlots;
    m_systems.utility.capacity            = m_desc->systems.utilitySlots;
    m_systems.fuelScop.capacity           = m_desc->systems.fuelScopSlots;
    m_systems.tractorBeam.capacity        = m_desc->systems.tractorBeamSlots;

    // Dock slots
    m_docks.fighter.capacity              = m_desc->docks.fighterSlots;
    m_docks.shuttle.capacity              = m_desc->docks.shuttleSlots;
    m_docks.drone.capacity                = m_desc->docks.droneSlots;
}


// //    ##                         ##               ###      ###                                           ##                          ###              ###
// //                               ##                ##       ##                                                                        ##             ## ##
// //   ###     #####     #####    #####    ####      ##       ##               ####     ######  ##  ##    ###     ######                ##    ####      #
// //    ##     ##  ##   ##         ##         ##     ##       ##              ##  ##   ##  ##   ##  ##     ##      ##  ##            #####   ##  ##   ####
// //    ##     ##  ##    #####     ##      #####     ##       ##              ######   ##  ##   ##  ##     ##      ##  ##           ##  ##   ######    ##
// //    ##     ##  ##        ##    ## ##  ##  ##     ##       ##              ##        #####   ##  ##     ##      #####            ##  ##   ##        ##
// //   ####    ##  ##   ######      ###    #####    ####     ####              #####       ##    ######   ####     ##                ######   #####   ####
// //                                                                                      ####                    ####

    void ShipCore::installDefaultEquipment(const ShipDescriptor& descriptor)
    {
        const auto& presets = descriptor.defaultEquipment;
    
        // Устанавливаем оборудование, если оно есть в пресетах
    
        if (presets.receiver.has_value()) {
            
            installEquipment("receiver", 
            m_systems.receiver, 
            m_equipment.receiver, 
            *presets.receiver);
        }
        
        if (presets.transmitter.has_value()) {

            installEquipment("transmitter", 
            m_systems.transmitter, 
            m_equipment.transmitter, 
            *presets.transmitter);
        }




        if (presets.radar.has_value())
        {
            // Development contract: the player radar slot is occupied by the
            // synthetic ideal sensor until a production radar backend is ready.
            // Consumers see only RadarScanReport and therefore do not change
            // when TEST_IDEAL_RADAR is replaced later.
            const RadarDesc& radarDesc =
                m_role == ShipRole::Player
                    ? game::TEST_IDEAL_RADAR
                    : *presets.radar;

            if (canInstallRadar(descriptor, radarDesc))
            {
                installEquipment(
                    "radar",
                    m_systems.radar,
                    m_equipment.radar,
                    radarDesc
                );

                
            }
            else
            {
                LOG("[ShipCore] Radar incompatible with platform");
            }
        }


    }

// //    ##                         ##               ###      ###                                           ##                                           ##
// //                               ##                ##       ##                                                                                        ##
// //   ###     #####     #####    #####    ####      ##       ##               ####     ######  ##  ##    ###     ######   ##  ##    ####    #####     #####
// //    ##     ##  ##   ##         ##         ##     ##       ##              ##  ##   ##  ##   ##  ##     ##      ##  ##  #######  ##  ##   ##  ##     ##
// //    ##     ##  ##    #####     ##      #####     ##       ##              ######   ##  ##   ##  ##     ##      ##  ##  ## # ##  ######   ##  ##     ##
// //    ##     ##  ##        ##    ## ##  ##  ##     ##       ##              ##        #####   ##  ##     ##      #####   ##   ##  ##       ##  ##     ## ##
// //   ####    ##  ##   ######      ###    #####    ####     ####              #####       ##    ######   ####     ##      ##   ##   #####   ##  ##      ###
// //                                                                                      ####                    ####


    template<typename ModuleType, typename DescType>
    bool ShipCore::installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        std::vector<ModuleType>& modules,
        const DescType& desc
    ) {

        // 1. Проверяем наличие слота
        if (slot.capacity <= 0) {
            printf("[ERROR] No slot for %s\n", equipmentName);
            return false;
        }
        
        // 2. Проверяем свободный слот
        if (slot.value >= slot.capacity) {
            printf("[ERROR] No free slots for %s\n", equipmentName);
            return false;
        }
        
        
        // 3. Устанавливаем
        // module.init(desc);
        modules.emplace_back();
        modules.back().init(desc);
        slot.value++;
        
        printf("[INFO] %s installed successfully\n", equipmentName);
        return true;
    }




    template<typename ModuleType, typename DescType>
    bool ShipCore::installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module,
        const DescType& desc
    ) {

        // 1. Проверяем наличие слота
        if (slot.capacity <= 0) {
            printf("[ERROR] No slot for %s\n", equipmentName);
            return false;
        }
        
        // 2. Проверяем свободный слот
        if (slot.value >= slot.capacity) {
            printf("[ERROR] No free slots for %s\n", equipmentName);
            return false;
        }
        
        
        // 3. Устанавливаем
        module.init(desc);
        slot.value++;
        
        printf("[INFO] %s installed successfully\n", equipmentName);
        return true;
    }

// //                                                                                              ##                                           ##
// //                                                                                                                                           ##
// //  ######    ####    ##  ##    ####    ##  ##    ####              ####     ######  ##  ##    ###     ######   ##  ##    ####    #####     #####
// //   ##  ##  ##  ##   #######  ##  ##   ##  ##   ##  ##            ##  ##   ##  ##   ##  ##     ##      ##  ##  #######  ##  ##   ##  ##     ##
// //   ##      ######   ## # ##  ##  ##   ##  ##   ######            ######   ##  ##   ##  ##     ##      ##  ##  ## # ##  ######   ##  ##     ##
// //   ##      ##       ##   ##  ##  ##    ####    ##                ##        #####   ##  ##     ##      #####   ##   ##  ##       ##  ##     ## ##
// //  ####      #####   ##   ##   ####      ##      #####             #####       ##    ######   ####     ##      ##   ##   #####   ##  ##      ###
// //                                                                             ####                    ####

    template<typename ModuleType>
    bool ShipCore::removeEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module
    ) {
        if (!module.enabled) {
            printf("[ERROR] %s {} not installed\n", equipmentName);
            return false;
        }
        
        module.enabled = false;
        slot.value--;
        printf("[ERROR] %s {} removed successfully\n", equipmentName);
        return true;
    }




    void ShipCore::debugPrintCoreSystems() const
    {
        std::cout << "\n[ShipCore] CoreSystems initialized:\n";

        std::cout << "  |- Reactor\n";
        std::cout << "       maxOutput: " << m_reactor.maxOutput() << " MW\n";
        std::cout << "       throttle : " << m_reactor.getThrottle() << "\n";
        std::cout << "       integrity: " << m_reactor.structuralIntegrity() << "\n";
        std::cout << "       temp     : " << m_reactor.getTemperature() << " K\n";

        std::cout << "  |- ThermalSystem\n";
        std::cout << "       temperature : " << m_thermal.getTemperature() << " K\n";
        // Убираем вызовы maxSafeTemperature и integrity - их больше нет!
        
        std::cout << "  |- PowerBus\n";
        std::cout << "       availablePower: " << m_powerBus.availablePower() << "\n";
        std::cout << "       overloaded    : "
                << (m_powerBus.overloaded() ? "YES" : "NO") << "\n";

        std::cout << "[Radar] Installed\n";
        std::cout << "  |- maxRange: " << m_equipment.radar.getDesc().maxRange << "\n";
        std::cout << "  |- power   : " << m_equipment.radar.getDesc().powerConsumption << "\n";

        std::cout << std::endl;
    }



    bool ShipCore::canInstallRadar(
        const ShipDescriptor& shipDesc,
        const RadarDesc& radarDesc
    ) const
    {
        const auto& mount = shipDesc.radarMount;

        if (radarDesc.requiredMountSize > mount.maxSupportedMountSize)
            return false;

        if (std::find(
                mount.allowedProfiles.begin(),
                mount.allowedProfiles.end(),
                radarDesc.type
            ) == mount.allowedProfiles.end())
            return false;

        return true;
    }




game::ShipCoreStatus ShipCore::getCoreStatus() const
{
    game::ShipCoreStatus status;
    
    status.newtonianRcsSampleValid=m_newtonianRcsPhysical;
    if(status.newtonianRcsSampleValid)
        for(std::size_t i=0;i<status.newtonianRcsExecuted01.size();++i)
            status.newtonianRcsExecuted01[i]=static_cast<float>(
                std::clamp(m_newtonianRcsDelivered01[i],0.0,1.0));

    // ----- Реактор (все методы есть) -----
    status.reactor.temperature = m_reactor.getTemperature();
    status.reactor.criticalTemp = m_reactor.getCriticalTemperature();
    status.reactor.outputMW = m_reactor.getCurrentOutput();
    status.reactor.efficiency01 = m_reactor.getEfficiency01();
    status.reactor.minOperatingTemperatureK = m_reactor.getMinimumOperatingTemperatureK();
    status.reactor.optimalTemperatureK = m_reactor.getOptimalTemperatureK();
    status.reactor.maxOutputMW = m_reactor.getMaxOutput();
    status.reactor.throttle = m_reactor.getThrottle();
    status.reactor.instability = m_reactor.getInstability();
    status.reactor.status = static_cast<uint8_t>(m_reactor.getStatus());
    status.reactor.integrity = m_reactor.structuralIntegrity();
    status.reactor.generatedHeat = m_reactor.getHeatVolume();
    status.reactor.heatGenerationMW = m_reactor.getGeneratedHeatMJ();
    
    status.propulsionReactorsOn = m_propulsionReactorsOn;
    status.propulsionStartupSeconds =
        m_propulsionReactorsStartupRemainingSeconds;
    status.propulsionSwitchRevision = m_propulsionSwitchRevision;
    // One factual status per installed propulsion generator. The legacy
    // ship-services reactor above is NOT one of these ER-5 modules.
    if (m_desc)
    {
        const auto& ids =
            m_desc->powerThermalInstallation.engineReactorModuleIds;
        const auto& runtime =
            static_cast<const world::modules::ObjectModuleRuntime&>(
                m_moduleRuntime);
        status.engineReactors.reserve(ids.size());
        for (const auto& id : ids)
        {
            game::EngineReactorStatus sample;
            sample.moduleId = id;
            const auto* module=runtime.findModule(id);
            const auto definition=std::find_if(
                m_desc->modules.begin(),m_desc->modules.end(),
                [&](const auto& d){return d.moduleId==id;});
            if(module && definition!=m_desc->modules.end() &&
                definition->engineReactor)
            {
                sample.installed=module->state ==
                    world::modules::ObjectModuleRuntimeState::Attached &&
                    module->health>0.0f;
                // A ready idle ER-5 is a valid core temperature even when
                // no thrust has ever been demanded.
                const auto& state=module->engineReactorOperating;
                if(std::isfinite(state.temperatureK) &&
                   state.temperatureK>0.0)
                {
                    sample.coreTemperatureK=state.temperatureK;
                    sample.safeTemperatureK=
                        definition->engineReactor->safeTemperatureK;
                    sample.generatedHeatMW=state.lastGeneratedHeatMW;
                    sample.electricalOutputMW=
                        state.lastElectricalOutputMW;
                    sample.padHeatMW=state.lastPadHeatMW;
                    sample.coolantHeatMW=state.lastCoolantHeatMW;
                    sample.storedHeatMW=state.lastStoredHeatMW;
                }
            }
            status.engineReactors.push_back(std::move(sample));
        }
    }

    // ----- Thermal (все методы есть) -----
    status.thermal.temperature = m_thermal.getTemperature();
    status.thermal.thermalMass = m_thermal.getThermalMass();
    status.thermal.heatVolume = m_thermal.getHeatVolume();
    status.thermal.storedHeat = m_thermal.getStoredHeat(); 
    status.thermal.thermalCriticalTemp = m_thermal.getCriticalTemp(); 

    
    // ----- Cooling (только то, что реально есть) -----
    status.cooling.coolantTemp = m_thermal.getTemperature(); // берем из thermal
    status.cooling.totalEfficiency = m_cooling.getTotalEfficiency();
    status.cooling.panelHealthPercent = 100.0 * status.cooling.totalEfficiency;
    status.cooling.panelThermalCritical =
        m_cooling.isPanelThermalCritical();
    status.cooling.allocatedPowerMW = m_cooling.getAllocatedPower();
    status.cooling.requestedPowerMW = m_cooling.getRequestedPower();
    status.cooling.radiatedPowerMW = m_cooling.getLastRadiatedPower();
    status.cooling.heatIntoCoolantMW = m_cooling.getDT() > 0.0
        ? std::max(0.0,m_thermal.getHeatVolume()/m_cooling.getDT())
        : 0.0;
    
    status.cooling.pumpCapacity = m_cooling.getPumpCapacity();
    status.cooling.reactorPumpActive = m_cooling.isReactorPumpActive();
    status.cooling.coolantOverheatLatched = m_cooling.isCoolantOverheatLatched();
    status.cooling.radiatorPotentialMW = m_cooling.getRadiatorPotentialMW();
    status.cooling.effectiveRadiatorTemperatureK = m_cooling.getEffectiveRadiatorTemperatureK();
    status.cooling.pumpHeatMJ = m_cooling.getPumpHeatMJ();
    status.cooling.dt = m_cooling.getDT();
    
    // Панели
    int panelCount = m_cooling.getPanelCount();
    status.cooling.damagedPanelCount = 0;
    status.cooling.criticalPanelCount = 0;
    
    for (int i = 0; i < panelCount; ++i) {
        game::RadiatorPanelStatus panel;
        panel.health = m_cooling.getPanelHealth(i);
        panel.efficiency = m_cooling.getPanelEfficiency(i);
        status.cooling.panels.push_back(panel);
        
        if (panel.health < 0.7 && panel.health >= 0.3) 
            status.cooling.damagedPanelCount++;
        if (panel.health < 0.3) {
            status.cooling.criticalPanelCount++;
            status.cooling.failedPanelIndices.push_back(i);
        }
    }
    
    // ----- PowerBus -----
    status.powerBus.availablePowerMW = m_powerBus.availablePower();
    status.powerBus.overloaded = m_powerBus.overloaded();
    
    // Считаем totalRequested сами
    status.powerBus.totalRequestedMW =
        m_powerBus.auxiliaryDemandMW();
    for (auto* c : m_powerBus.getConsumers()) {
        game::PowerConsumerStatus cs;
        cs.name = c->getLabel();
        cs.requestedPowerMW = c->getRequestedPower();
        cs.allocatedPowerMW = c->getAvailablePower();
        cs.priority = static_cast<int>(c->getPriority());
        cs.operational = true;
        
        // Пытаемся привести к IHeatSource
        auto* heatSource = dynamic_cast<game::equipment::IHeatSource*>(c);
        if (heatSource) {
            cs.heatTransfer = heatSource->getHeatGeneration();
        } else {
            cs.heatTransfer = 1.0;
        }

        status.powerBus.consumers.push_back(cs);
        status.powerBus.totalRequestedMW += cs.requestedPowerMW;
    }
    
    // ----- Алерты -----
    
    // Реактор горячий
    if (status.reactor.temperature > status.reactor.criticalTemp * 0.85) {
        game::AlertStatus alert;
        alert.severity = status.reactor.temperature > status.reactor.criticalTemp * 0.95 ? 2 : 1;
        alert.system = "Reactor";
        alert.message = status.reactor.temperature > status.reactor.criticalTemp * 0.95 
            ? "CRITICAL TEMPERATURE" : "High temperature";
        alert.value = status.reactor.temperature;
        alert.threshold = status.reactor.criticalTemp * 0.85;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Reactor");
        
        if (alert.severity == 2) {
            status.criticalSystems.push_back("Reactor");
        }
    }
    
    // Антифриз горячий (лимит 900K)
    if (status.thermal.temperature > 1100.0) {
        game::AlertStatus alert;
        alert.severity = status.thermal.temperature > 1250.0 ? 2 : 1;
        alert.system = "Coolant";
        alert.message = status.thermal.temperature > 1250.0 
            ? "COOLANT SATURATED" : "Coolant high";
        alert.value = status.thermal.temperature;
        alert.threshold = 1250.0;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Coolant");
        
        if (alert.severity == 2) {
            status.criticalSystems.push_back("Coolant");
        }
    }
    
    if (status.cooling.panelThermalCritical) {
        game::AlertStatus alert;
        alert.severity = 2;
        alert.system = "Radiators";
        alert.message = "RADIATOR THERMAL DAMAGE";
        alert.value = status.cooling.effectiveRadiatorTemperatureK;
        alert.threshold = m_cooling.getPanelDamageOnsetK();
        status.alerts.push_back(alert);
        status.criticalSystems.push_back("Radiators");
    }
    if(status.reactor.integrity < 0.95 ||
       status.reactor.temperature < status.reactor.minOperatingTemperatureK)
    {
        game::AlertStatus alert;
        alert.severity = 1;
        alert.system = "Reactor";
        alert.message = status.reactor.integrity < 0.95
            ? "REACTOR DAMAGED" : "REACTOR BELOW OPERATING TEMPERATURE";
        alert.value = status.reactor.temperature;
        alert.threshold = status.reactor.minOperatingTemperatureK;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Reactor");
    }
    // Повреждения панелей
    if (status.cooling.criticalPanelCount > 0) {
        game::AlertStatus alert;
        alert.severity = 2;
        alert.system = "Radiators";
        alert.message = std::to_string(status.cooling.criticalPanelCount) + " panels critical";
        alert.value = status.cooling.criticalPanelCount;
        alert.threshold = 0;
        status.alerts.push_back(alert);
        status.criticalSystems.push_back("Radiators");
    } else if (status.cooling.damagedPanelCount > 0) {
        game::AlertStatus alert;
        alert.severity = 1;
        alert.system = "Radiators";
        alert.message = std::to_string(status.cooling.damagedPanelCount) + " panels damaged";
        alert.value = status.cooling.damagedPanelCount;
        alert.threshold = 0;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Radiators");
    }
    
    // Перегрузка энергосистемы
    if (status.powerBus.overloaded) {
        game::AlertStatus alert;
        alert.severity = 1;
        alert.system = "Power";
        alert.message = "Power bus overloaded";
        alert.value = status.powerBus.totalRequestedMW;
        alert.threshold = status.powerBus.availablePowerMW;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Power");
    }
    
    return status;
}



// ------------ DAMAGE -----------
void ShipCore::applyDamage(const game::damage::DamageEvent& event)
{
    using namespace game::damage;

    DamageEvent localEvent = event;

    localEvent.setLocalPositionForResolve(
        m_transform.worldPositionToLocalCell(event.worldPosition)
    );

    auto result = m_hitComponent.resolve(localEvent);

    const float healthBefore =
        result.volume ? result.volume->health : 0.0f;
    const float maxHealth =
        result.volume ? result.volume->maxHealth : 0.0f;
    const std::string damagedModuleId =
        result.volume ? result.volume->moduleId : std::string {};

    m_damageHandler.handleDamage(result);

    // Functional equipment damage records the CAUSE of actual health loss.
    // The amount is normalized from the real hit-volume loss; sensitivity
    // weights are not damage values and never enter this accumulation step.
    if (result.volume &&
        !result.volume->supportLinkVolume &&
        !damagedModuleId.empty() &&
        maxHealth > 0.0f)
    {
        const double lost01 = std::clamp(
            static_cast<double>(
                healthBefore - result.volume->health
            ) / static_cast<double>(maxHealth),
            0.0,
            1.0
        );

        using Kind = game::equipment::EquipmentDamageKind;
        bool hasEquipmentCause = true;
        Kind cause = Kind::Impact;

        switch (event.type)
        {
            case DamageType::Laser:
                cause = Kind::Laser;
                break;

            case DamageType::Explosion:
            case DamageType::Collision:
                cause = Kind::Impact;
                break;

            case DamageType::Radiation:
                cause = Kind::Radiation;
                break;

            case DamageType::EMP:
                // EMP is not one of the five physical equipment-damage
                // accumulators. Electrical effects need their own system.
                hasEquipmentCause = false;
                break;
        }

        if (hasEquipmentCause && lost01 > 0.0)
        {
            bool isInstalledEngine=false;
            if (m_desc)
                for (const auto& module : m_desc->modules)
                    if (module.moduleId==damagedModuleId && module.engine)
                    {
                        isInstalledEngine=true;
                        break;
                    }
            if (cause==Kind::Impact && isInstalledEngine)
            {
                // Incident-dependent seed. The same engine ID occurs on many
                // ships, so module ID alone must not yield identical outcomes
                // across the entire universe.
                std::uint64_t incidentKey=0xcbf29ce484222325ULL;
                const auto mix=[&](std::uint64_t value) {
                    incidentKey^=value;
                    incidentKey*=0x100000001b3ULL;
                };
                mix(static_cast<std::uint64_t>(
                    event.worldPosition.cell.x
                ));
                mix(static_cast<std::uint64_t>(
                    event.worldPosition.cell.y
                ));
                mix(static_cast<std::uint64_t>(
                    event.worldPosition.cell.z
                ));
                mix(static_cast<std::uint64_t>(std::llround(
                    event.worldPosition.localMeters.x*1000.0
                )));
                mix(static_cast<std::uint64_t>(std::llround(
                    event.worldPosition.localMeters.y*1000.0
                )));
                mix(static_cast<std::uint64_t>(std::llround(
                    event.worldPosition.localMeters.z*1000.0
                )));
                m_moduleRuntime.addEngineImpactShock(
                    damagedModuleId,lost01,incidentKey
                );
            }
            else
                m_moduleRuntime.addEquipmentDamage(
                    damagedModuleId,cause,lost01
                );
        }
    }

    syncDestroyedModulesFromHitVolumes();

    if (m_desc)
    {
        world::modules::ObjectRuntimeHitBuilder::rebuild(
            m_hitComponent,
            m_desc->typeId,
            *m_desc,
            m_moduleRuntime,
            m_structuralLinkRuntime,
            m_assemblyRuntime
        );

        m_hitVolumesDirty = false;
    }
    // An engine shock is observable immediately to the pilot/autopilot; no
    // stale pre-impact capability snapshot may survive until a later system
    // update. This rebuild is incident-driven, never per physics tick.
    refreshRuntimeState();
}



bool ShipCore::debugDestroyModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    auto* mod = m_moduleRuntime.findModule(moduleId);
    if (!mod)
    {
        std::cout
            << "[ShipCore] debugDestroyModuleById: module not found: "
            << moduleId << "\n";
        return false;
    }

    m_moduleRuntime.markModuleDestroyed(moduleId, 0.0f);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugDestroyModuleById: destroyed moduleId="
        << moduleId << "\n";

    return true;
}


bool ShipCore::debugRestoreModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    auto* mod = m_moduleRuntime.findModule(moduleId);
    if (!mod)
    {
        std::cout
            << "[ShipCore] debugRestoreModuleById: module not found: "
            << moduleId << "\n";
        return false;
    }

    m_moduleRuntime.restoreModuleBranch(moduleId);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugRestoreModuleById: restored moduleId="
        << moduleId << "\n";

    return true;
}

bool ShipCore::debugResetStructure()
{
    if (!m_desc)
        return false;

    m_moduleRuntime.init(m_desc->moduleDescriptors());
    m_structuralLinkRuntime.init(m_desc->moduleDescriptors());
    m_detachedFragmentRuntime.clear();
    m_repairJobRuntime.clear();

    if (game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
    {
        const auto& assembly =
            game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);
        m_assemblyRuntime.init(assembly);
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugResetStructure\n";
    return true;
}



void ShipCore::exportHitVolumes() const
{
    for (const auto& v : m_hitComponent.volumes)
    {
        std::cout
            << "HIT_VOLUME "
            << v.m_label << " "
            << v.center.x << " "
            << v.center.y << " "
            << v.center.z << " "
            << v.halfSize.x << " "
            << v.halfSize.y << " "
            << v.halfSize.z
            << std::endl;
    }
}



void ShipCore::restoreVolume(int index)
{
    if (index < 0 || index >= m_hitComponent.volumes.size())
        return;

    auto& v = m_hitComponent.volumes[index];

    v.health = 1.0f;
    v.destroyed = false;
}









void ShipCore::initModuleRuntime()
{
    if (!m_desc)
        return;

    m_moduleRuntime.init(m_desc->moduleDescriptors());
    m_structuralLinkRuntime.init(m_desc->moduleDescriptors());

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
}






void ShipCore::syncDestroyedModulesFromHitVolumes()
{
    bool supportChanged = false;

    for (const auto& volume : m_hitComponent.volumes)
    {
        if (volume.supportLinkVolume)
        {
            if (m_structuralLinkRuntime.setLinkHealth(
                    volume.supportLinkId,
                    volume.health,
                    volume.destroyed))
            {
                supportChanged = true;
            }

            // legacy mirror: пока оставляем для совместимости
            if (m_moduleRuntime.setSupportLinkHealth(
                    volume.moduleId,
                    volume.supportLinkId,
                    volume.health,
                    volume.destroyed))
            {
                supportChanged = true;
            }

            continue;
        }

        m_moduleRuntime.setModuleHealth(volume.moduleId, volume.health);

        if (volume.destroyed)
        {
            m_moduleRuntime.markModuleDestroyed(volume.moduleId, volume.health);
        }
    }

    if (supportChanged)
{
    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
}
    syncDetachedFragmentsFromModuleRuntime();
}




void ShipCore::updateAssemblyRuntime(double dt)
{
    // Обычная анимация/вращение модулей не должна пересобирать hit-volumes.
    // Rebuild нужен только при структурных изменениях:
    // damage, detach, restore, descriptor/assembly edit.
    m_assemblyRuntime.update(dt);
}



bool ShipCore::debugReattachModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_detachedFragmentRuntime.hasFragment(moduleId))
    {
        std::cout
            << "[ShipCore] debugReattachModuleById failed: no detached fragment for moduleId="
            << moduleId << "\n";
        return false;
    }

    if (!m_moduleRuntime.reattachModule(moduleId))
    {
        std::cout
            << "[ShipCore] debugReattachModuleById failed: module runtime rejected moduleId="
            << moduleId << "\n";
        return false;
    }

    const int restoredLinks =
        m_structuralLinkRuntime.restoreLinksTouchingModule(moduleId);

    m_detachedFragmentRuntime.removeFragment(moduleId);

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugReattachModuleById moduleId="
        << moduleId
        << " restoredLinks=" << restoredLinks
        << "\n";

    return true;
}






bool ShipCore::startRepairJobForModule(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    // Repair runtime работает в локальной системе корабля.
    // Никакой глобальной трансляции сюда передавать нельзя.
    const glm::mat4 ownerModel =
        m_transform.orientation;

        
    glm::vec3 ownerLinearVelocity =
        m_transform.forward() * m_transform.forwardVelocity +
        m_transform.right()   * m_transform.localVelocity.x +
        m_transform.up()      * m_transform.localVelocity.y;

    glm::vec3 dockWorldPosition =
        glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

    if (const auto dock =
            resolveShipAttachment(
                m_desc,
                m_transform,
                "repair_drone_dock_main",
                nullptr,
                nullptr))
    {
        dockWorldPosition =
            glm::vec3(ownerModel * glm::vec4(dock->localPosition, 1.0f));
    }


    glm::vec3 launchWorldPosition = dockWorldPosition;
    glm::vec3 recoveryWorldPosition = dockWorldPosition;

if (const auto launch =
        resolveShipAttachment(
            m_desc,
            m_transform,
            "repair_drone_launch_main",
            nullptr,
            nullptr))
{
    launchWorldPosition =
        glm::vec3(ownerModel * glm::vec4(launch->localPosition, 1.0f));
}

if (const auto recovery =
        resolveShipAttachment(
            m_desc,
            m_transform,
            "repair_drone_recovery_main",
            nullptr,
            nullptr))
{
    recoveryWorldPosition =
        glm::vec3(ownerModel * glm::vec4(recovery->localPosition, 1.0f));
}



std::vector<glm::vec3> repairWorkWorldPoints;

for (const auto* point :
        findShipAttachmentPointsByKind(
            m_desc,
            ShipAttachmentKind::RepairWorkPoint
        ))
{
    if (!point)
        continue;

    if (const auto resolved =
            resolveShipAttachment(
                m_desc,
                m_transform,
                point->id,
                nullptr,
                nullptr))
    {
        repairWorkWorldPoints.push_back(
            glm::vec3(ownerModel * glm::vec4(resolved->localPosition, 1.0f))
        );
    }
}

    const bool ok = m_repairJobRuntime.startJob(
    moduleId,
    ownerModel,
    ownerLinearVelocity,
    dockWorldPosition,
    launchWorldPosition,
    recoveryWorldPosition,
    repairWorkWorldPoints,
    m_detachedFragmentRuntime
);

    std::cout
        << "[ShipCore] startRepairJobForModule moduleId="
        << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}










std::vector<world::modules::ObjectMissingPartRequest>
ShipCore::buildMissingPartRequests() const
{
    std::vector<world::modules::ObjectMissingPartRequest> out;

    if (!m_desc)
        return out;

    if (!game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
        return out;

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);

    for (const auto& module : m_moduleRuntime.modules())
    {
        const bool missing =
            module.state == world::modules::ObjectModuleRuntimeState::Detached ||
            module.state == world::modules::ObjectModuleRuntimeState::Destroyed;

        if (!missing)
            continue;

        if (!module.repairable && !module.salvageable)
            continue;

        const auto* assemblyModule =
            world::modules::findAssemblyModuleById(
                assembly,
                module.moduleId
            );

        if (!assemblyModule)
            continue;

        world::modules::ObjectMissingPartRequest req;
        req.targetModuleId = module.moduleId;

        req.moduleClass = module.moduleClass;
        if (req.moduleClass.empty())
            req.moduleClass = module.moduleId;

        req.acceptedReplacementTags = module.acceptedReplacementTags;
        if (req.acceptedReplacementTags.empty())
            req.acceptedReplacementTags.push_back(module.moduleId);

        req.homeLocalModel =
            world::modules::buildAssemblyModuleHierarchicalLocalModel(
                assembly,
                m_assemblyRuntime,
                module.moduleId
            );



            // ВАЖНО:
// Если модуль уже Detached, его active hit-volume мог быть удалён из m_hitComponent
// после rebuild. Поэтому сначала берём homeCenterLocal из уже созданного detached fragment.
if (const auto* existingFragment =
        m_detachedFragmentRuntime.findFragment(module.moduleId))
{
    req.homeCenterLocal = existingFragment->homeCenterLocal;

    std::cout
        << "[ShipCore] missing slot center from detached fragment moduleId="
        << module.moduleId
        << " center=("
        << req.homeCenterLocal.x << ", "
        << req.homeCenterLocal.y << ", "
        << req.homeCenterLocal.z << ")"
        << "\n";
}
else
{
    glm::vec3 accumulatedCenter(0.0f);
    int accumulatedCount = 0;

    for (const auto& v : m_hitComponent.volumes)
    {
        if (v.moduleId != module.moduleId)
            continue;

        if (v.supportLinkVolume)
            continue;

        accumulatedCenter += v.center;
        ++accumulatedCount;
    }

    if (accumulatedCount > 0)
    {
        req.homeCenterLocal =
            accumulatedCenter / static_cast<float>(accumulatedCount);
    }
    else
    {
        req.homeCenterLocal =
            glm::vec3(req.homeLocalModel * glm::vec4(0, 0, 0, 1));

        std::cout
            << "[ShipCore] WARNING missing slot fallback center used moduleId="
            << module.moduleId
            << " center=("
            << req.homeCenterLocal.x << ", "
            << req.homeCenterLocal.y << ", "
            << req.homeCenterLocal.z << ")"
            << "\n";
    }
}








        out.push_back(std::move(req));
    }

    return out;
}

bool ShipCore::startRepairJobForClaimedReplacement(
    const std::string& targetModuleId,
    world::modules::ObjectDetachedFragmentRuntime& sourceRuntime,
    const std::string& sourceModuleId
)
{
    if (!m_desc)
        return false;

    const auto requests = buildMissingPartRequests();

    const world::modules::ObjectMissingPartRequest* targetRequest = nullptr;

    for (const auto& req : requests)
    {
        if (req.targetModuleId == targetModuleId)
        {
            targetRequest = &req;
            break;
        }
    }

    if (!targetRequest)
    {
        std::cout
            << "[ShipCore] repair failed: no missing slot targetModuleId="
            << targetModuleId << "\n";
        return false;
    }

    if (!m_detachedFragmentRuntime.claimFragmentAsReplacement(
        sourceRuntime,
        sourceModuleId,
        targetModuleId,
        targetRequest->homeLocalModel,
        targetRequest->homeCenterLocal))
    {
        return false;
    }

    // Важно: repair job теперь всегда работает с targetModuleId
    // внутри runtime целевого корабля.
    return startRepairJobForModule(targetModuleId);
}







bool ShipCore::debugDetachModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_moduleRuntime.debugForceDetach(moduleId))
        return false;

    syncDetachedFragmentsFromModuleRuntime();

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugDetachModuleById: moduleId="
              << moduleId << "\n";
    return true;
}




bool ShipCore::ejectCockpitCapsule()
{
    if (!m_desc)
        return false;

    constexpr const char* COCKPIT_MODULE_ID = "ship_cockpit";

    auto* cockpit = m_moduleRuntime.findModule(COCKPIT_MODULE_ID);
    if (!cockpit)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: cockpit module not found\n";
        return false;
    }

    if (!cockpit->detachable)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: cockpit is not detachable\n";
        return false;
    }

    if (cockpit->state == world::modules::ObjectModuleRuntimeState::Detached)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule ignored: cockpit already detached\n";
        return false;
    }

    if (!m_moduleRuntime.debugForceDetach(COCKPIT_MODULE_ID))
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: detach rejected\n";
        return false;
    }

    syncDetachedFragmentsFromModuleRuntime();

    const glm::vec3 forward = m_transform.forward();

    // Скорость подбери потом по вкусу.
    // Сейчас это именно "катапульта", не медленный отпад.
    const glm::vec3 ejectVelocity = forward * 35.0f;

    const glm::vec3 ejectAngularVelocity =
        glm::vec3(0.0f, 0.15f, 0.0f);

    if (!m_detachedFragmentRuntime.setFragmentMotion(
            COCKPIT_MODULE_ID,
            ejectVelocity,
            ejectAngularVelocity))
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: detached fragment was not spawned\n";
        return false;
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] COCKPIT CAPSULE EJECTED forward=("
        << forward.x << ", "
        << forward.y << ", "
        << forward.z << ")"
        << "\n";

    return true;
}





bool ShipCore::debugHangModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_moduleRuntime.debugForceHang(moduleId))
        return false;

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugHangModuleById: moduleId="
              << moduleId << "\n";
    return true;
}

bool ShipCore::debugReevaluateStructure()
{
    if (!m_desc)
        return false;

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
syncDetachedFragmentsFromModuleRuntime();

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugReevaluateStructure\n";
    return true;
}




bool ShipCore::debugSetStructuralLinkHealth(
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    if (!m_desc)
        return false;

    // if (!m_structuralLinkRuntime.setLinkHealth(linkId, health, destroyed))
    //     return false;

    if (!m_structuralLinkRuntime.setLinkHealth(linkId, health, destroyed))
{
    std::cout
        << "[ShipCore] debugSetStructuralLinkHealth FAILED linkId="
        << linkId << "\n";
    return false;
}

if (const auto* link = m_structuralLinkRuntime.findById(linkId))
{
    std::cout
        << "[ShipCore] set structural link health:"
        << " id=" << link->id
        << " A=" << link->moduleAId
        << " B=" << link->moduleBId
        << " kind=" << static_cast<int>(link->kind)
        << " health=" << link->health
        << " destroyed=" << link->destroyed
        << " loadBearing=" << link->loadBearing
        << "\n";
}


    for (auto& v : m_hitComponent.volumes)
    {
        if (v.supportLinkVolume && v.supportLinkId == linkId)
        {
            v.health = std::max(0.0f, health);
            v.destroyed = destroyed || v.health <= 0.0f;
        }
    }

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
    syncDetachedFragmentsFromModuleRuntime();


    if (const auto* link = m_structuralLinkRuntime.findById(linkId))
        {
            const auto* a = m_moduleRuntime.findModule(link->moduleAId);
            const auto* b = m_moduleRuntime.findModule(link->moduleBId);

            if (a)
            {
                std::cout
                    << "[ShipCore] module after link damage:"
                    << " module=" << a->moduleId
                    << " state=" << static_cast<int>(a->state)
                    << " aliveSupports=" << a->aliveSupportCount
                    << " detachable=" << a->detachable
                    << "\n";
            }

            if (b)
            {
                std::cout
                    << "[ShipCore] module after link damage:"
                    << " module=" << b->moduleId
                    << " state=" << static_cast<int>(b->state)
                    << " aliveSupports=" << b->aliveSupportCount
                    << " detachable=" << b->detachable
                    << "\n";
            }
        }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    return true;
}







void ShipCore::syncDetachedFragmentsFromModuleRuntime()
{
    if (!m_desc)
        return;

    if (!game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
        return;

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);

    const glm::mat4 ownerModel =    m_transform.orientation;

    m_detachedFragmentRuntime.syncFromDetachedModules(
        ownerModel,
        assembly,
        m_assemblyRuntime,
        m_moduleRuntime,
        m_hitComponent
    );
}

void ShipCore::updateDetachedFragments(float dt)
{
    m_detachedFragmentRuntime.update(dt);
}


void ShipCore::updateRepairJobs(float dt)
{
    if (!m_desc)
        return;

    if (m_repairJobRuntime.activeJobCount() <= 0)
        return;

    const glm::mat4 ownerModel =   m_transform.orientation;

    std::vector<world::navigation::NavigationObstacle> obstacles =
        buildNavigationObstaclesForRepair("");

    const auto completed =
        m_repairJobRuntime.update(
            dt,
            ownerModel,
            m_detachedFragmentRuntime,
            obstacles
        );

    for (const auto& moduleId : completed)
    {
        debugReattachModuleById(moduleId);
    }
}





std::vector<game::simulation::ObjectRepairJobSnapshot>
ShipCore::buildRepairJobSnapshots() const
{
    const glm::mat4 ownerModel =
    m_transform.orientation;

    return m_repairJobRuntime.buildSnapshots(
        ownerModel,
        m_transform.worldPosition,
        m_detachedFragmentRuntime
    );
}

int ShipCore::activeRepairJobCount() const
{
    return m_repairJobRuntime.activeJobCount();
}






std::vector<world::navigation::NavigationObstacle>
ShipCore::buildNavigationObstaclesForRepair(
    const std::string& ignoredModuleId
) const
{
    (void)ignoredModuleId;

    std::vector<world::navigation::NavigationObstacle> out;

    const auto bodies =
        world::navigation::buildNavigationBodiesForShip(*this);

    const glm::mat4 ownerModel =
    m_transform.orientation;

    const glm::mat3 ownerRotation =
        glm::mat3(ownerModel);

    for (const auto& body : bodies)
    {
        world::navigation::NavigationObstacle o;

        o.shape =
            world::navigation::NavigationObstacleShape::Box;

        o.centerMeters =
            glm::dvec3(ownerModel * glm::vec4(body.center, 1.0f));

        o.halfExtentsMeters =
            glm::dvec3(body.halfExtents);

        o.localToWorldBasis =
            glm::dmat3(ownerRotation);

        // Conservative radius retained for contact/debug consumers.
        o.radiusMeters =
            glm::length(glm::dvec3(body.halfExtents));

        o.entityId = body.entityId;

        out.push_back(o);
    }

    if (out.empty())
    {
        world::navigation::NavigationObstacle o;
        o.shape = world::navigation::NavigationObstacleShape::Sphere;
        o.centerMeters = glm::dvec3(ownerModel * glm::vec4(0, 0, 0, 1));
        o.radiusMeters = 8.0;
        out.push_back(o);
    }

    return out;
}


}