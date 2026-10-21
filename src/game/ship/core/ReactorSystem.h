#pragma once

#include "src/game/equipment/capabilities/IHeatSource.h"
#include "src/game/ship/ShipDescriptor.h"
#include "ThermalSystem.h"
#include "CoolingSystem.h"
#include <algorithm>

namespace game::ship::core
{

enum class ReactorStatus
{
    Normal,
    Overheating,
    Critical,
    Shutdown
};

class ReactorSystem : public game::equipment::IHeatSource
{
public:
    ReactorSystem() = default;
    explicit ReactorSystem(const ReactorDescriptor& desc);

    void init(const ReactorDescriptor& desc);
    

    // Управление
    void setThrottle(double value);
    double getThrottle() const { return m_throttle; }

    // Мощность
    double getMaxOutput() const { return m_peakPowerMW * m_structuralIntegrity;}
    double getCurrentOutput() const;

    // Повреждения
    void applyStructuralDamage(double amount);
    double structuralIntegrity() const { return m_structuralIntegrity; }

    // Тепло (IHeatSource)
    double getHeatGeneration() const override;

    // Связь с насосом (вызывается из PowerBus)
    void setPumpPower(double powerMW);       // Сколько эл. мощности дали насосу
    double getDesiredPumpPower() const;      // Сколько хотим (зависит от T)

    // Основной update - передаем антифриз
    // void update(double dt, ThermalSystem& coolant);
    void update(double dt, ThermalSystem& coolant, double pumpCapacityMW);
    

    // Состояние
    double getTemperature() const { return m_temperature; }
    double getEfficiency01() const
    {
        // The original live reactor remains on its existing 1300/1400/2000 K
        // operating envelope until the full reactor power model migrates.
        // Only efficiency is sampled against that currently active envelope.
        const double t=m_temperature;
        const double low=std::clamp(
            (t-m_minWorkTemp)/std::max(1.0,m_workTemp-m_minWorkTemp),
            0.0,1.0);
        const double high=std::clamp(
            (t-m_workTemp)/std::max(1.0,m_criticalTemp-m_workTemp),
            0.0,1.0);
        const double thermalEfficiency = t <= m_workTemp
            ? m_efficiencyMinTemp+(m_efficiency-m_efficiencyMinTemp)*low
            : m_efficiency+(m_efficiencyMaxTemp-m_efficiency)*high;
        // Structural integrity already reduces reactor output capacity.
        // Do NOT multiply conversion efficiency by integrity: at 10 GW
        // even 1% damage would create ~100 MW of fictitious waste heat.
        // A separate bounded 0.03 percentage-point loss at total damage
        // gives diagnostic degradation without inevitable thermal runaway.
        const double structuralLoss = 0.0003 *
            (1.0-std::clamp(m_structuralIntegrity,0.0,1.0));
        return std::clamp(thermalEfficiency-structuralLoss,0.05,1.0);
    }
    double getMinimumOperatingTemperatureK() const { return m_minWorkTemp; }
    double getOptimalTemperatureK() const { return m_workTemp; }
    double getCriticalTemperature() const { return m_criticalTemp; }
    ReactorStatus getStatus() const { return m_status; }
    
    
    
    
    // Для отладки
    double getLastHeatRemoved() const { return m_lastHeatRemoved; } // МВт
    double getRejectedHeat() const { return m_rejectedHeat; } // Тепло, не влезшее в антифриз
    double maxOutput() const {return m_peakPowerMW;}


    double getInstability() const { return m_instability; }
    double getStructuralIntegrity() const { return m_structuralIntegrity; }
    double getHeatVolume() const { return m_heatVolume; }

    double getGeneratedHeatMJ() const { return m_heatGenerationMW; }

    double getWorkTemp() const { return m_workTemp; }
    double getDeltaTemp() const { return m_deltaTemp; }
    double getWarningTemp() const { return m_maxWarringTemp; }

    bool isEmergencyMode() const { return m_emergencyMode; }
    void setEmergencyPowerLimit(double limit) { m_emergencyPowerLimit = limit; } 

    void updateEmergencyMode();

private:
    void updateStatus();

private:
    // Параметры
    double m_peakPowerMW    = 70.0;        // МВт (электрических)
    double m_efficiency     = 0.45;        // peak at optimal T
    double m_efficiencyMinTemp = 0.35;
    double m_efficiencyMaxTemp = 0.40;
    double m_thermalMass    = 30.0;        // МДж/К

    double m_minWorkTemp    = 1300.0;      // K
    double m_workTemp       = 1400.0;      // K
    double m_maxWorkTemp    = 1500.0;      // K
    double m_maxWarringTemp = 1700.0;      // K
    double m_criticalTemp   = 2000.0;      // K
    double m_deltaTemp        = 25;          // K, допустимое отклонение от рабочей температуры в обе стороны.
    
    
    
    
    // Состояние
    double m_throttle = 0.0;             // 0..1
    double m_temperature = 1500.0;        // K
    double m_structuralIntegrity = 1.0;  // 0..1
    
    double m_allocatedPumpPower = 0.0;   // МВт (сколько дали насосу)
    double m_lastHeatRemoved = 0.0;      // МВт (сколько тепла ушло)
    double m_rejectedHeat = 0.0;         // МДж (тепло, не влезшее в антифриз)
    
    ReactorStatus m_status = ReactorStatus::Normal;


    double m_instability = 0.0;     // 0..1
    double m_heatVolume = 0.0;      // MJ за последний dt
    double m_heatGenerationMW = 0.0;



    double m_emergencyPowerLimit = 12.0;     // МВт - лимит в аварийном режиме
    bool m_emergencyMode = false;             // Флаг аварийного режима
    double m_emergencyHysteresis = 50.0;      // K гистерезис для выхода из аварии
};

}