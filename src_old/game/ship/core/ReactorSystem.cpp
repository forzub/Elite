#include "ReactorSystem.h"
#include <algorithm>
#include "src/game/ship/core/CoolingSystem.h"
#include <iostream>


namespace game::ship::core
{

ReactorSystem::ReactorSystem(const ReactorDescriptor& desc)
{
    init(desc);
}

void ReactorSystem::init(const ReactorDescriptor& desc)
{
    // m_peakElectricPowerMW = desc.peakPowerMW;
    // m_thermalMass         = desc.thermalMass;
    // m_temperature         = desc.temperature;
    // m_criticalTemp        = desc.maxTempK;

    m_throttle = 0.0;
    m_structuralIntegrity = 1.0;
    m_status = ReactorStatus::Normal;
    m_workTemp = 1500.0;


    m_emergencyMode = false;
    m_emergencyPowerLimit = 12.0; // Можно сделать параметром дескриптора
    m_emergencyHysteresis = 50.0;
}


void ReactorSystem::setThrottle(double value)
{
    m_throttle = std::clamp(value, 0.0, 1.0);
}




double ReactorSystem::getCurrentOutput() const
{
    if (m_status == ReactorStatus::Shutdown)
        return 0.0;

    double baseOutput = m_throttle * getMaxOutput();
    if (m_emergencyMode)
    {
        return std::min(baseOutput, m_emergencyPowerLimit);
    }
    
    return baseOutput;
}




double ReactorSystem::getHeatGeneration() const
{
    if (m_status == ReactorStatus::Shutdown)
        return 0.0;
    
    double P_el = getCurrentOutput();
    double P_heat = P_el * ((1.0 / m_efficiency) - 1.0);
    
    return std::max(0.0, P_heat);
}



void ReactorSystem::setPumpPower(double powerMW)
{
    m_allocatedPumpPower = getDesiredPumpPower();
    // m_allocatedPumpPower = std::clamp(powerMW, 0.0, m_maxPumpPower);
}




double ReactorSystem::getDesiredPumpPower() const
{
    // Если антифриз на пределе - насос бесполезен, вернем 0
    // (это будет проверяться в PowerBus)
    
    // Иначе - пропорционально температуре реактора
    double ratio = (m_temperature - 800.0) / (m_criticalTemp - 800.0);
    ratio = std::clamp(ratio, 0.0, 1.0);
    
    // При ratio=1 (1400K) хотим 6 МВт (полная мощность насоса)
    // При ratio=0 (800K) хотим 0 МВт
    // return ratio * m_maxPumpPower;
    return 0;
}





void ReactorSystem::update(double dt, ThermalSystem& coolant, double pumpCapacityMW)
{
    // генерирует тепло и кладет его в себя. повышает температуру
    double P_heat = getHeatGeneration();
    
    double generatedMJ = P_heat * dt;
    m_temperature += generatedMJ / m_thermalMass;
    
    double transferredMJ = pumpCapacityMW * dt;
    m_temperature -= transferredMJ / m_thermalMass;
    coolant.addHeat(transferredMJ);

    m_heatVolume =  pumpCapacityMW;
    m_heatGenerationMW =  transferredMJ;

    updateEmergencyMode();
    updateStatus();
}






void ReactorSystem::updateStatus()
{
    double ratio = m_temperature / m_criticalTemp;
    
    // Обновляем instability
    if (ratio < 0.9) {
        m_instability = 0.0;
        m_status = ReactorStatus::Normal;
    } else if (ratio < 1.0) {
        m_instability = (ratio - 0.9) / 0.1; // 0..1 при 0.9..1.0
        m_status = ReactorStatus::Overheating;
    } else {
        m_instability = 1.0;
        m_status = ReactorStatus::Critical;
    }
}





void ReactorSystem::updateEmergencyMode()
{
    // Логика входа в аварийный режим
    if (!m_emergencyMode)
    {
        // Входим в аварийный режим при достижении Warning температуры
        if (m_temperature >= m_maxWarringTemp)
        {
            m_emergencyMode = true;
            
        }
    }
    else // Мы в аварийном режиме
    {
        // Выходим из аварийного режима, когда температура упала ниже рабочей с гистерезисом
        if (m_temperature <= m_workTemp)
        {
            m_emergencyMode = false;
           
        }
    }
}




}