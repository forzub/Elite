// ShipCore: motion control and the installed propulsion executor (main drive, gimbal, Newtonian RCS, ER-5 draw), reactor / free-rotation toggles.

#include "src/game/ship/core/ShipCoreInternal.h"

using namespace game::ship::geometry;

namespace game::ship::core
{


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
        const auto& topology = game::ship::propulsionTopology(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel);
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
            {
                // The same switch-off law as every capability projection:
                // a main engine damaged beyond the limit no longer fires
                // while its bank keeps a usable engine.
                std::vector<game::ship::plant::MainEngineCutoffInput> cut(
                    topology.size());
                for (std::size_t i = 0; i < topology.size(); ++i)
                {
                    cut[i].role = topology[i].role;
                    if (topology[i].role ==
                        game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                        continue;
                    cut[i].attached = installedEngines[i].attached;
                    cut[i].damage01 =
                        game::ship::plant::replicatedTotalDamage01(
                            installedEngines[i].damage->accumulated);
                }
                const auto off = game::ship::plant::mainEngineCutoff(cut);
                for (std::size_t i = 0; i < off.size(); ++i)
                    if (off[i])
                        installedEngines[i].attached = false;
                physicalEngines = &installedEngines;
            }
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
        const auto& topology = game::ship::propulsionTopology(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel);
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
        const auto& groups =
            game::ship::engineReactorGroups(
                *m_desc, m_state.construction.physicalControlModel);
        const auto& reactorIds =
            m_desc->powerThermalInstallation.engineReactorModuleIds;
        const auto& engineTopology =
            game::ship::propulsionTopology(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel);
        const auto& runtime =
            static_cast<const world::modules::ObjectModuleRuntime&>(
                m_moduleRuntime);
        {
            powerGroups.reserve(groups.size());
            for (std::size_t n=0; n<groups.size(); ++n)
            {
                game::ship::control::MainEnginePowerGroup group;
                for (const auto& engineId :
                     groups[n].poweredEngineSlotIds)
                    for (std::size_t i=0; i<engineTopology.size(); ++i)
                        if (std::string_view(engineTopology[i].id) == engineId)
                            group.engineIndices.push_back(i);

                group.availableMW = engineReactorAvailableMW(n);
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
        const auto& topology=game::ship::propulsionTopology(
                *m_state.construction.definition,
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
    m_newtonianRcsEr5DrawMW.assign(powerGroups.size(),0.0);
    m_newtonianRcsCompressorElectricalMW=0.0;
    m_newtonianRcsNetFlow01PerSecond=0.0;
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
        // Cancel the parasitic torque of a deflected main nozzle executed
        // this tick, so trajectory trims do not rotate the hull.
        demand.momentBodyNm-=rcsMotion.mainPropulsionTorqueBodyNm;

        // Installed ER-5 already powers the main engine. Its unused output
        // can ALSO supply RCS. Main engine has first claim, so each MW is
        // used at most once. Newtonian I has ONE 5-GW ER-5, enough for
        // 24 * 20 MW = 480 MW RCS with idle main; Y/X retain their already
        // installed groups. No new reactor, no raid on 70-MW service bus.
        std::vector<double> er5SpareMW(powerGroups.size(),0.0);
        const auto& mainMotors=rcsMotion.engineTelemetry;
        double actualEr5SpareMW=0.0;
        if(m_propulsionReactorsOn &&
           m_propulsionReactorsStartupRemainingSeconds<=0.0)
            for(std::size_t groupIndex=0;
                groupIndex<powerGroups.size();++groupIndex)
            {
                const auto& group=powerGroups[groupIndex];
                double mainDrawMW=0.0;
                for(const auto slot:group.engineIndices)
                    if(mainMotors.valid && slot<mainMotors.count)
                        mainDrawMW+=std::max(
                            0.0,mainMotors.electricalMW[slot]);
                const double spare=std::max(
                    0.0,group.availableMW-mainDrawMW);
                er5SpareMW[groupIndex]=spare;
                actualEr5SpareMW+=spare;
            }
        // Argon recirculation compressor is a REAL ER-5 consumer. Reserve
        // its 2 MW first, then RCS takes the remaining watts. The buffer
        // can only recover if a generator actually powers this compressor.
        const double ArgonCompressorRatedMW=
            game::ship::plant::rcsWorkingFluid::CompressorRatedElectricalMW;
        // It refills only while the nozzles draw nothing (the manifold
        // feeds them otherwise); last tick's net flow tells whether they did.
        const bool nozzlesDrewGas=m_newtonianRcsNetFlow01PerSecond<0.0;
        const double compressorMW=
            rcsMotion.manoeuvreGasPressure01<0.999 && !nozzlesDrewGas
                ? std::min(ArgonCompressorRatedMW,actualEr5SpareMW):0.0;
        m_newtonianRcsCompressorElectricalMW=compressorMW;
        double compressorRemainingMW=compressorMW;
        for(std::size_t i=0;i<er5SpareMW.size();++i)
        {
            const double draw=std::min(
                compressorRemainingMW,er5SpareMW[i]);
            m_newtonianRcsEr5DrawMW[i]+=draw;
            er5SpareMW[i]-=draw;
            compressorRemainingMW-=draw;
        }
        demand.availableElectricalMW=std::max(
            0.0,actualEr5SpareMW-compressorMW);

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
        const double compressorRecovery01=
            compressorMW/ArgonCompressorRatedMW;
        const double argonManifoldBefore01=
            rcsMotion.manoeuvreGasPressure01;
        // Installed tank (36A2B1): a damaged vessel holds less usable gas,
        // so the same flow empties and refills its fill share faster; a
        // failed one leaves the nozzles without gas. Below the regulator
        // pressure the nozzles weaken (and draw) in proportion to the fill.
        double tankCapacity01=1.0;
        const auto* gasTank=newtonianRcsGasTank(tankCapacity01);
        const double gasThrustShare=newtonianRcsGasThrustShare01();
        double gasScale=0.0;
        if(gasTank && !(tankCapacity01>1.0e-9))
        {
            rcsMotion.manoeuvreGasPressure01=0.0;
            rcsMotion.manoeuvreGasDepleted=true;
        }
        else
            gasScale=gasThrustShare*game::ship::plant::advanceNewtonianRcsGas(
                rcsMotion.manoeuvreGasPressure01,
                rcsMotion.manoeuvreGasDepleted,
                static_cast<double>(dt),intensity*gasThrustShare,
                physics.manoeuvreGasUsePerSecond/tankCapacity01,
                physics.manoeuvreGasRechargePerSecond*compressorRecovery01/
                    tankCapacity01,
                physics.manoeuvreGasRestartFraction);
        m_newtonianRcsNetFlow01PerSecond=(
            rcsMotion.manoeuvreGasPressure01-argonManifoldBefore01)/
            static_cast<double>(dt);
        const glm::dvec3 requestedRcsForceBeforeGasN=demand.forceBodyN;
        const glm::dvec3 requestedRcsMomentBeforeGasNm=demand.momentBodyNm;
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

            // Account for the REAL RCS watts in their owning ER-5s, so
            // their temperature/aging and main thrust share the same budget.
            double remainingRcsMW=m_newtonianRcsElectricalDemandMW;
            for(std::size_t i=0;i<er5SpareMW.size();++i)
            {
                const double draw=std::min(
                    remainingRcsMW,er5SpareMW[i]);
                m_newtonianRcsEr5DrawMW[i]+=draw;
                remainingRcsMW-=draw;
            }

            // Passive native gameplay diagnostic: log ACTUAL installed main
            // engine and 24-nozzle execution. Unlike isolated CTest fixtures,
            // this observes a real player ShipCore with its true equipment
            // damage, centre of mass, control inputs, power and gas state.
            // Nothing is injected into the flight controller by this tracer.
            if(m_role==ShipRole::Player)
            {
                static std::ofstream newtonTrace;
                static bool traceAttempted=false;
                static bool traceFinished=false;
                static double traceSeconds=0.0;
                static std::uint64_t traceRows=0;
                if(!traceAttempted)
                {
                    traceAttempted=true;
                    std::error_code error;
                    std::filesystem::create_directories("D:/tmp",error);
                    newtonTrace.open("D:/tmp/elite_newtonian_runtime.csv",
                        std::ios::trunc);
                    if(newtonTrace)
                    {
                        newtonTrace<<std::fixed<<std::setprecision(8);
                        newtonTrace
                            <<"time_s,model,source,tick,dt,manual_forward,"
                              "manual_strafe,manual_lift,manual_pitch,manual_yaw,"
                              "manual_roll,reactors_on,reactor_startup_s,"
                              "service_reactor_MW,service_free_budget_MW,"
                              "gas01,gas_depleted,gas_scale,active_rcs_modules,"
                              "body_com_x_m,body_com_y_m,body_com_z_m,"
                              "raw_rcs_force_x_N,raw_rcs_force_y_N,"
                              "raw_rcs_force_z_N,raw_rcs_moment_x_Nm,"
                              "raw_rcs_moment_y_Nm,raw_rcs_moment_z_Nm,"
                              "request_rcs_force_x_N,request_rcs_force_y_N,"
                              "request_rcs_force_z_N,request_rcs_moment_x_Nm,"
                              "request_rcs_moment_y_Nm,request_rcs_moment_z_Nm,"
                              "actual_rcs_force_x_N,actual_rcs_force_y_N,"
                              "actual_rcs_force_z_N,actual_rcs_moment_x_Nm,"
                              "actual_rcs_moment_y_Nm,actual_rcs_moment_z_Nm,"
                              "actual_rcs_power_MW,actual_rcs_heat_MW,"
                              "main_accel_world_x_mps2,main_accel_world_y_mps2,"
                              "main_accel_world_z_mps2,main_torque_body_x_Nm,"
                              "main_torque_body_y_Nm,main_torque_body_z_Nm,"
                              "rcs_delta_v_x_mps,rcs_delta_v_y_mps,"
                              "rcs_delta_v_z_mps,rcs_delta_pitch_radps,"
                              "rcs_delta_yaw_radps,rcs_delta_roll_radps,"
                              "pitch_rate_radps,yaw_rate_radps,roll_rate_radps,"
                              "velocity_preintegrator_mps";
                        for(std::size_t i=0;i<4;++i)
                            newtonTrace<<",main"<<i<<"_thrust01"
                                       <<",main"<<i<<"_available01"
                                       <<",main"<<i<<"_electric_MW"
                                       <<",main"<<i<<"_heat_MW";
                        for(std::size_t i=0;i<24;++i)
                            newtonTrace<<",rcs"<<i<<"_throttle01"
                                       <<",rcs"<<i<<"_available_N"
                                       <<",rcs"<<i<<"_electric_MW"
                                       <<",rcs"<<i<<"_heat_MW";
                        // Requested MAIN throttle is distinct from the
                        // Num8/Num2 RCS forward axis. Compare both to actual
                        // engine output to isolate any 14% throttle ceiling.
                        newtonTrace
                            <<",main_target_speed_rate"
                              ",requested_main_accel_world_x_mps2"
                              ",requested_main_accel_world_y_mps2"
                              ",requested_main_accel_world_z_mps2"
                              ",requested_main_accel_norm_mps2"
                              ",main_forward_capability_mps2"
                              ",rcs_translation_capability_mps2"
                              ",rcs_angular_capability_radps2"
                              ",main_engine_runtime_temp_K"
                              ",main_engine_total_damage01"
                              ",main_ER5_group_available_MW"
                              ",main_ER5_module_temp_K"
                              ",main_ER5_total_damage01"
                              ",main_ER5_function_authority01"
                              ",main_engine_nominal_MW"
                              ",main_engine_nominal_thrust_N"
                              ",rcs_gas_intensity01";
                        newtonTrace<<'\n';
                        std::cerr<<"[NewtonianTrace] BEGIN "
                                    "D:/tmp/elite_newtonian_runtime.csv"
                                 <<std::endl;
                    }
                    else
                    {
                        traceFinished=true;
                        std::cerr<<"[NewtonianTrace] ERROR cannot write "
                                    "D:/tmp/elite_newtonian_runtime.csv"
                                 <<std::endl;
                    }
                }
                if(newtonTrace.is_open() && !traceFinished)
                {
                    traceSeconds+=static_cast<double>(dt);
                    ++traceRows;
                    unsigned connectedNozzles=0;
                    double totalWasteHeatMW=0.0;
                    for(std::size_t i=0;i<24;++i)
                    {
                        if(executed.availabilityN[i]>1.0e-9)
                            ++connectedNozzles;
                        totalWasteHeatMW+=executed.wasteHeatMW[i];
                    }
                    const auto& ctrl=m_state.control;
                    const auto& motors=rcsMotion.engineTelemetry;
                    const glm::dvec3 rcsDeltaVelocity=
                        executed.deliveredForceN*(dt/mass);
                    const glm::dvec3 rcsDeltaOmega={
                        executed.deliveredMomentNm.x*dt/inertia.x,
                        executed.deliveredMomentNm.y*dt/inertia.y,
                        -executed.deliveredMomentNm.z*dt/inertia.z
                    };
                    newtonTrace
                        <<traceSeconds<<','
                        <<static_cast<int>(
                            m_state.construction.physicalControlModel)<<','
                        <<static_cast<int>(m_state.actuators.source)<<','
                        <<ctrl.controlTick<<','<<dt<<','
                        <<ctrl.forwardInput<<','<<ctrl.strafeInput<<','
                        <<ctrl.liftInput<<','<<ctrl.pitchInput<<','
                        <<ctrl.yawInput<<','<<ctrl.rollInput<<','
                        <<(m_propulsionReactorsOn?1:0)<<','
                        <<m_propulsionReactorsStartupRemainingSeconds<<','
                        <<m_reactor.getCurrentOutput()<<','
                        <<demand.availableElectricalMW<<','
                        <<rcsMotion.manoeuvreGasPressure01<<','
                        <<(rcsMotion.manoeuvreGasDepleted?1:0)<<','
                        <<gasScale<<','<<connectedNozzles;
                    const auto add3=[&](const glm::dvec3& value) {
                        newtonTrace<<','<<value.x<<','<<value.y
                                   <<','<<value.z;
                    };
                    add3(physics.centerOfMassLocalMeters);
                    add3(requestedRcsForceBeforeGasN);
                    add3(requestedRcsMomentBeforeGasNm);
                    add3(demand.forceBodyN);
                    add3(demand.momentBodyNm);
                    add3(executed.deliveredForceN);
                    add3(executed.deliveredMomentNm);
                    newtonTrace<<','<<m_newtonianRcsElectricalDemandMW
                               <<','<<totalWasteHeatMW;
                    add3(rcsMotion.mainEngineAccelerationMps2);
                    add3(rcsMotion.mainPropulsionTorqueBodyNm);
                    add3(rcsDeltaVelocity);
                    add3(rcsDeltaOmega);
                    newtonTrace<<','<<m_transform.pitchRate
                               <<','<<m_transform.yawRate
                               <<','<<m_transform.rollRate
                               <<','<<glm::length(rcsMotion.localVelocityMps);
                    for(std::size_t i=0;i<4;++i)
                        if(motors.valid && i<motors.count)
                            newtonTrace<<','<<motors.thrust01[i]
                                       <<','<<motors.available01[i]
                                       <<','<<motors.electricalMW[i]
                                       <<','<<motors.wasteHeatMW[i];
                        else
                            newtonTrace<<",0,0,0,0";
                    for(std::size_t i=0;i<24;++i)
                        newtonTrace<<','<<executed.throttle01[i]
                                   <<','<<executed.availabilityN[i]
                                   <<','<<executed.electricalMW[i]
                                   <<','<<executed.wasteHeatMW[i];
                    const glm::dvec3& mainRequest=
                        rcsMotion.requestedMainEngineAccelerationMps2;
                    newtonTrace<<','<<ctrl.targetSpeedRate;
                    add3(mainRequest);
                    newtonTrace
                        <<','<<glm::length(mainRequest)
                        <<','<<game::ship::forwardMainAccelerationLimitMps2(physics)
                        <<','<<game::ship::manoeuvreAccelerationLimitMps2(physics)
                        <<','<<game::ship::angularAccelerationLimitRadPerSec2(physics);
                    // The 14.32% main-drive result is impossible to
                    // attribute without BOTH pre-scaling ER-5 entitlement
                    // and the motor passport/runtime state. Do not infer
                    // group budget from the already scaled output MW.
                    const auto& slots=
                        game::ship::propulsionTopology(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel);
                    const auto& engineIds=desc.physicalControl.slotModuleIds;
                    const auto& reactorIds=
                        desc.powerThermalInstallation.engineReactorModuleIds;
                    double motorTempK=0.0,motorDamage01=0.0;
                    double motorRatedMW=0.0,motorRatedN=0.0;
                    for(std::size_t i=0;i<slots.size() &&
                        i<engineIds.size();++i)
                        if(slots[i].role ==
                            game::ship::plant::EngineSlotRole::AftMain)
                        {
                            const auto* module=
                                runtime.findModule(engineIds[i]);
                            if(module)
                            {
                                motorTempK=module->engineOperating.temperatureK;
                                motorDamage01=
                                    game::equipment::totalAccumulatedDamage01(
                                        module->equipmentDamage.accumulated);
                            }
                            const auto it=std::find_if(
                                desc.modules.begin(),desc.modules.end(),
                                [&](const auto& m) {
                                    return m.moduleId==engineIds[i];});
                            if(it!=desc.modules.end() && it->engine)
                            {
                                motorRatedMW=it->engine->nominalElectricalPowerMW;
                                motorRatedN=it->engine->nominalThrustNewtons;
                            }
                            break;
                        }
                    double generatorBudgetMW=0.0,generatorTempK=0.0;
                    double generatorDamage01=0.0;
                    double generatorAuthority01=0.0;
                    if(!reactorIds.empty())
                    {
                        const auto* generator=
                            runtime.findModule(reactorIds.front());
                        if(generator)
                        {
                            generatorTempK=
                                generator->engineReactorOperating.temperatureK;
                            generatorDamage01=
                                game::equipment::totalAccumulatedDamage01(
                                    generator->equipmentDamage.accumulated);
                            const auto it=std::find_if(
                                desc.modules.begin(),desc.modules.end(),
                                [&](const auto& m) {
                                    return m.moduleId==reactorIds.front();});
                            if(it!=desc.modules.end() && it->engineReactor)
                                generatorAuthority01=
                                    game::equipment::
                                    evaluateEquipmentFunctionAtDamage01(
                                        generatorDamage01,
                                        it->engineReactor->failure).authority01;
                        }
                    }
                    if(!powerGroups.empty())
                        generatorBudgetMW=powerGroups.front().availableMW;
                    newtonTrace<<','<<motorTempK<<','<<motorDamage01
                        <<','<<generatorBudgetMW<<','<<generatorTempK
                        <<','<<generatorDamage01<<','<<generatorAuthority01
                        <<','<<motorRatedMW<<','<<motorRatedN
                        <<','<<intensity;
                    newtonTrace<<'\n';
                    if(traceRows%30==0) newtonTrace.flush();
                    if(traceSeconds>=240.0)
                    {
                        newtonTrace.flush();
                        newtonTrace.close();
                        traceFinished=true;
                        std::cerr<<"[NewtonianTrace] COMPLETE rows="
                                 <<traceRows
                                 <<" D:/tmp/elite_newtonian_runtime.csv"
                                 <<std::endl;
                    }
                }
            }
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

} // namespace game::ship::core
