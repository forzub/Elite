// ShipCore: service power bus, reactors, ER-5 generators, propulsion and ship coolant loops, radiators (updateSystems).

#include "src/game/ship/core/ShipCoreInternal.h"

using namespace game::ship::geometry;

namespace game::ship::core
{


void ShipCore::updateSystems(float dt)
{
    if (dt <= 0.0f)
        return;
    if(m_propulsionReactorsOn && m_propulsionReactorsStartupRemainingSeconds>0.0)
        m_propulsionReactorsStartupRemainingSeconds=std::max(0.0,
            m_propulsionReactorsStartupRemainingSeconds-static_cast<double>(dt));
    // PowerBus owns service-reactor throttle while online. Never restore
    // the saved pre-shutdown value on every tick: that would erase the
    // just-calculated demand and starve the consumer allocations.
    // The explicit bus gate enforces zero output during startup/shutdown.

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

    const double serviceThrottleBeforeBus=m_reactor.getThrottle();
    const double serviceOutputBeforeBusMW=m_reactor.getCurrentOutput();
    // RCS is powered by the installed ER-5 group, not the independent
    // 70-MW service reactor. Do not double-charge RCS to both buses.
    m_powerBus.setAuxiliaryDemandMW(0.0);
    m_powerBus.update(
        m_propulsionReactorsOn &&
        m_propulsionReactorsStartupRemainingSeconds<=0.0);
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
    // Main engines conduct residual heat into the PROPULSION loop, never
    // directly into the separate service reactor coolant.
    if (m_desc &&
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime)
    {
        const auto& topology =
            game::ship::propulsionTopology(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel);
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
            // Engine pad radiation leaves directly to space; only conducted
            // heat enters the independent propulsion coolant reservoir.
            const double coolantK=m_propulsionThermal.getTemperature();
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
                        m_propulsionThermal.addHeat(energyMJ);
                    else
                        m_propulsionThermal.removeHeat(-energyMJ);
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
            game::ship::engineReactorGroups(
                *m_desc,m_state.construction.physicalControlModel);
        const auto& reactorIds=
            m_desc->powerThermalInstallation.engineReactorModuleIds;
        const auto& engineSlots=
            game::ship::propulsionTopology(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel);
        const auto& engineIds=m_desc->physicalControl.slotModuleIds;
        const auto& motors=m_transform.motion.engineTelemetry;
        bool reactorDamageChanged=false;
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
            for(const auto& slotId:generators[g].poweredEngineSlotIds)
                for(std::size_t i=0;i<engineSlots.size() &&
                    i<engineIds.size();++i)
                    if(std::string_view(engineSlots[i].id)==slotId &&
                       motors.valid && i<motors.count &&
                       std::isfinite(motors.electricalMW[i]))
                        electricalMW+=std::max(0.0,motors.electricalMW[i]);
            if(g<m_newtonianRcsEr5DrawMW.size())
                electricalMW+=std::max(
                    0.0,m_newtonianRcsEr5DrawMW[g]);
            // ER-5 has its OWN square heat pad, never borrows a main motor
            // emitter. One universe cooling density applies to ALL pads.
            const double padHeadroomMW=
                game::physics::thermal::PropulsionPadCoolingMWPerM2*
                def->engineReactor->padSideM*
                def->engineReactor->padSideM*
                std::clamp(module->engineReactorOperating.padIntegrity01,
                           0.0,1.0);
            auto& state=module->engineReactorOperating;
            state.damage=module->equipmentDamage.accumulated;
            const double thermalBefore=state.damage.thermal01;
            const bool installed=module->state ==
                world::modules::ObjectModuleRuntimeState::Attached &&
                module->health>0.0f;
            const auto tick=game::equipment::reactor::advanceEngineReactor(
                *def->engineReactor,state,
                installed && m_propulsionReactorsOn &&
                    m_propulsionReactorsStartupRemainingSeconds<=0.0
                    ? electricalMW:0.0,
                padHeadroomMW,0.0,static_cast<double>(dt));
            if(tick.catastrophic)
            {
                std::cerr<<"[ER5_CATASTROPHE] module="<<reactorIds[g]
                    <<" T_K="<<state.temperatureK
                    <<" damage01="
                    <<game::equipment::totalAccumulatedDamage01(state.damage)
                    <<" pad01="<<state.padIntegrity01<<std::endl;
                // Physical module is lost permanently; invalidate the real
                // installed topology rather than drawing a fake HUD failure.
                m_moduleRuntime.markModuleDestroyed(reactorIds[g],0.0f);
                reactorDamageChanged=true;
                continue;
            }

            // advanceEngineReactor() increases damage in the operating copy.
            // Commit it to the SAME ObjectModuleRuntime equipment channel
            // that all other failures use. Keep fractional increments between
            // systems ticks to avoid per-frame capability revision storms.
            state.pendingThermalDamage01+=std::max(
                0.0,state.damage.thermal01-thermalBefore);
            constexpr double damageQuantum01=1.0e-5;
            const double quanta=std::floor(
                (state.pendingThermalDamage01+1.0e-14)/damageQuantum01);
            if(quanta>=1.0)
            {
                const double increment=quanta*damageQuantum01;
                state.pendingThermalDamage01=std::max(
                    0.0,state.pendingThermalDamage01-increment);
                reactorDamageChanged |= m_moduleRuntime.addEquipmentDamage(
                    reactorIds[g],
                    game::equipment::EquipmentDamageKind::Thermal,
                    increment);
            }
            state.damage=module->equipmentDamage.accumulated;
        }
        if(reactorDamageChanged)
            refreshRuntimeState();
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
        const double coolantK=m_propulsionThermal.getTemperature();
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
                    m_propulsionThermal.addHeat(energyMJ);
                else
                    m_propulsionThermal.removeHeat(-energyMJ);
            }
        }
        if(changed) refreshRuntimeState();
    }
    // No cross-circuit emergency valve is installed in this first migration.
    // Preserve heat in propulsion modules/loop instead of silently deleting it.
    m_cooling.update(dt, m_thermal);

    // Read-only reconciliation trace for the user's D:/tmp acceptance.
    // Source of truth remains the server's installed equipment; this log
    // cannot enable or repair a damaged module or change reactor throttle.
    if(m_role==ShipRole::Player && m_desc)
    {
        static std::ofstream powerTrace;
        static bool attempted=false;
        static double timeSeconds=0.0;
        static std::uint64_t rows=0;
        if(!attempted)
        {
            attempted=true;
            std::error_code ec;
            std::filesystem::create_directories("D:/tmp",ec);
            powerTrace.open("D:/tmp/elite_power_thermal_audit.csv",
                std::ios::trunc);
            if(powerTrace)
            {
                powerTrace<<std::fixed<<std::setprecision(8);
                powerTrace
                    <<"seconds,tick,model,reactors_on,startup_seconds,"
                      "service_throttle_before_bus,service_MW_before_bus,"
                      "service_throttle_after_bus,service_MW_after_bus,"
                      "service_max_MW,service_overloaded,"
                      "consumers_requested_MW,consumers_granted_MW,"
                      "critical_requested_MW,critical_granted_MW,"
                      "cooling_requested_MW,cooling_granted_MW,"
                      "rcs_actual_electrical_MW,"
                      "service_reactor_T_K,coolant_T_K,"
                      "radiators_radiated_MW,reactor_pump_MW,"
                      "propulsion_reactor_count,"
                      "propulsion_coolant_T_K,propulsion_stored_MJ,"
                      "ship_coolant_stored_MJ,ship_radiator_potential_MW,"
                      "emergency_transfer_MW";
                for(std::size_t i=0;i<8;++i)
                    powerTrace<<",ER"<<i<<"_temp_K"
                              <<",ER"<<i<<"_thermal_damage01"
                              <<",ER"<<i<<"_electrical_MW"
                              <<",ER"<<i<<"_heat_MW"
                              <<",ER"<<i<<"_pad_MW"
                              <<",ER"<<i<<"_pad_integrity01";
                powerTrace<<'\n';
                std::cerr<<"[PowerThermalTrace] BEGIN "
                    "D:/tmp/elite_power_thermal_audit.csv"<<std::endl;
            }
            else
                std::cerr<<"[PowerThermalTrace] ERROR cannot write "
                    "D:/tmp/elite_power_thermal_audit.csv"<<std::endl;
        }
        if(powerTrace.is_open())
        {
            timeSeconds+=static_cast<double>(dt);
            ++rows;
            double requested=0.0, granted=0.0;
            double requestedCritical=0.0, grantedCritical=0.0;
            for(const auto* consumer:m_powerBus.getConsumers())
            {
                if(!consumer) continue;
                const double req=std::max(
                    0.0,consumer->getRequestedPower());
                const double got=std::max(
                    0.0,consumer->getAvailablePower());
                requested+=req;
                granted+=got;
                if(consumer->getPriority() ==
                    game::equipment::PowerPriority::Critical)
                {
                    requestedCritical+=req;
                    grantedCritical+=got;
                }
            }
            const auto& ids =
                m_desc->powerThermalInstallation.engineReactorModuleIds;
            powerTrace<<timeSeconds<<','<<m_state.control.controlTick
                <<','<<static_cast<int>(
                    m_state.construction.physicalControlModel)
                <<','<<(m_propulsionReactorsOn?1:0)
                <<','<<m_propulsionReactorsStartupRemainingSeconds
                <<','<<serviceThrottleBeforeBus
                <<','<<serviceOutputBeforeBusMW
                <<','<<m_reactor.getThrottle()
                <<','<<m_reactor.getCurrentOutput()
                <<','<<m_reactor.getMaxOutput()
                <<','<<(m_powerBus.overloaded()?1:0)
                <<','<<requested<<','<<granted
                <<','<<requestedCritical<<','<<grantedCritical
                <<','<<m_cooling.getRequestedPower()
                <<','<<m_cooling.getAllocatedPower()
                <<','<<m_powerBus.auxiliaryDemandMW()
                <<','<<m_reactor.getTemperature()
                <<','<<m_thermal.getTemperature()
                <<','<<m_cooling.getLastRadiatedPower()
                <<','<<m_cooling.getAllocatedPower()
                <<','<<ids.size()
                <<','<<m_propulsionThermal.getTemperature()
                <<','<<m_propulsionThermal.getStoredHeat()
                <<','<<m_thermal.getStoredHeat()
                <<','<<m_cooling.getRadiatorPotentialMW()
                <<",0"; // No installed emergency exchanger yet.
            const auto& runtime=
                static_cast<const world::modules::ObjectModuleRuntime&>(
                    m_moduleRuntime);
            for(std::size_t i=0;i<8;++i)
            {
                const auto* module=i<ids.size()
                    ? runtime.findModule(ids[i]) : nullptr;
                if(module)
                {
                    const auto& er=module->engineReactorOperating;
                    powerTrace<<','<<er.temperatureK
                        <<','<<module->equipmentDamage.accumulated.thermal01
                        <<','<<er.lastElectricalOutputMW
                        <<','<<er.lastGeneratedHeatMW
                        <<','<<er.lastPadHeatMW
                        <<','<<er.padIntegrity01;
                }
                else
                    powerTrace<<",0,0,0,0,0,0";
            }
            powerTrace<<'\n';
            if(rows%30==0)powerTrace.flush();
            if(timeSeconds>=240.0)
            {
                powerTrace.flush();
                powerTrace.close();
                std::cerr<<"[PowerThermalTrace] COMPLETE rows="<<rows
                    <<" D:/tmp/elite_power_thermal_audit.csv"
                    <<std::endl;
            }
        }
    }
}

} // namespace game::ship::core
