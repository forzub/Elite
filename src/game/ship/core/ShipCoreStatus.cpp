// ShipCore: ShipCoreStatus publication (HUD / telemetry view of the authoritative state).

#include "src/game/ship/core/ShipCoreInternal.h"

using namespace game::ship::geometry;

namespace game::ship::core
{





game::ShipCoreStatus ShipCore::getCoreStatus() const
{
    game::ShipCoreStatus status;
    
    status.newtonianRcsSampleValid=m_newtonianRcsPhysical;
    if(status.newtonianRcsSampleValid)
    {
        for(std::size_t i=0;i<status.newtonianRcsExecuted01.size();++i)
            status.newtonianRcsExecuted01[i]=static_cast<float>(
                std::clamp(m_newtonianRcsDelivered01[i],0.0,1.0));
        status.newtonianRcsFluidKnown=true;
        status.newtonianRcsManifold01=static_cast<float>(
            std::clamp(m_transform.motion.manoeuvreGasPressure01,0.0,1.0));
        status.newtonianRcsManifoldDepleted=
            m_transform.motion.manoeuvreGasDepleted;
        status.newtonianRcsCompressorMW=static_cast<float>(
            m_newtonianRcsCompressorElectricalMW);
        status.newtonianRcsFlowUse01PerSec=
            m_desc ? m_desc->physics.manoeuvreGasUsePerSecond : 0.0f;
        status.newtonianRcsFlowRecovery01PerSec=
            m_desc
                ? static_cast<float>(
                    m_desc->physics.manoeuvreGasRechargePerSecond *
                    (m_newtonianRcsCompressorElectricalMW /
                     game::ship::plant::rcsWorkingFluid::CompressorRatedElectricalMW))
                : 0.0f;
        status.newtonianRcsArgonInventoryKg=static_cast<float>(
            game::ship::plant::rcsWorkingFluid::ArgonInventoryKg);
        status.newtonianRcsNetFlow01PerSec=static_cast<float>(
            m_newtonianRcsNetFlow01PerSecond);
    }

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
                    sample.padIntegrity01=std::clamp(state.padIntegrity01,0.0,1.0);
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

} // namespace game::ship::core
