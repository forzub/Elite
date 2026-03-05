
#include "CoolingSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace game::ship::core
{

void CoolingSystem::init(double totalArea, int panelCount, 
                         double emissivity, double maxCoolantTemp)
{
    m_secondaryCircuit.emissivity = emissivity;
    m_secondaryCircuit.maxCoolantTemp = maxCoolantTemp;
    
    double panelArea = totalArea / panelCount;
    m_secondaryCircuit.panels.clear();
    
    for (int i = 0; i < panelCount; ++i)
    {
        m_secondaryCircuit.panels.push_back({panelArea, 1.0, 1.0});
    }
}



double CoolingSystem::getEffectiveArea() const
{
    double area = 0.0;
    for (const auto& panel : m_secondaryCircuit.panels)
        area += panel.area * panel.efficiency;
    return area;
}



double CoolingSystem::calculateRadiatedPower(double temperature) const
{
    double T4 = std::pow(temperature, 4);
    double Tspace4 = std::pow(SPACE_TEMP, 4);
    double radiated = m_secondaryCircuit.emissivity * STEFAN_BOLTZMANN * 
                      getEffectiveArea() * (T4 - Tspace4);
    
    return std::max(0.0, radiated);
}











double CoolingSystem::getRequestedPower() const
{
    return m_primaryCircuit.requeistedPower;
}





void CoolingSystem::setAvailablePower(double power)
{
    m_primaryCircuit.allocatedPower = power;
}







void CoolingSystem::update(double dt, ThermalSystem& coolant)
{
    m_primaryCircuit.lastReactorTemp = m_reactorState.reactorT;
    
    // ===== ПЕРВЫЙ КОНТУР (НАСОСЫ) ===== 
    // 1. Насосы греют антифриз (10% от allocatedPower)
    double pumpHeatMW = getHeatGeneration();
    coolant.addHeat(pumpHeatMW * dt);
    
    // 2. Рассчитываем переносимую мощность насосов
    m_primaryCircuit.pumpCapacity = m_primaryCircuit.allocatedPower * PUMP_EFFICIENCY;
    
    // Сохраняем dt для отладки
    m_dt = dt;
    m_primaryCircuit.pumpHeatMJ = m_primaryCircuit.pumpCapacity * dt;
  


    // ===== ВТОРОЙ КОНТУР (РАДИАТОРЫ) =====
    // 3. Сколько могут излучить радиаторы при текущей температуре
    m_coolantTemp = coolant.getTemperature();
    double maxRadiatedPower = calculateRadiatedPower(m_coolantTemp);
    
    // 4. Реально отводимая мощность = из возможностей радиаторов
    double actualRemovedPower =  maxRadiatedPower;
    double heatRemovedMJ = actualRemovedPower * dt;
    
    // 5. Забираем тепло из антифриза
    if (heatRemovedMJ > 0.0) {
        coolant.removeHeat(heatRemovedMJ);
    }
    
    // 6. Сохраняем для отладки
    m_primaryCircuit.lastRadiatedPower = actualRemovedPower;
}




void CoolingSystem::damagePanel(int index, double amount)
{
    if (index >= 0 && index < m_secondaryCircuit.panels.size())
    {
        auto& panel = m_secondaryCircuit.panels[index];
        panel.health = std::max(0.0, panel.health - amount);
        panel.efficiency = panel.health; // линейная зависимость
    }
}

void CoolingSystem::damageRandomPanel(double amount)
{
    if (m_secondaryCircuit.panels.empty()) return;
    
    int index = rand() % m_secondaryCircuit.panels.size();
    damagePanel(index, amount);
}

double CoolingSystem::getTotalEfficiency() const
{
    if (m_secondaryCircuit.panels.empty()) return 0.0;
    
    double total = 0.0;
    for (const auto& panel : m_secondaryCircuit.panels)
        total += panel.efficiency;
    return total / m_secondaryCircuit.panels.size();
}

void CoolingSystem::repairAllPanels()
{
    for (auto& panel : m_secondaryCircuit.panels) {
        panel.health = 1.0;
        panel.efficiency = 1.0;
    }
}



void CoolingSystem::setReactorState(
        double reactorT, 
        double reactorWorkT,
        double reactorDeltaT,
        double reactorWarringT,
        double coolantT,
        double coolantCritT
    ){
        m_reactorState.coolantCritT = coolantCritT;
        m_reactorState.coolantT = coolantT;
        m_reactorState.reactorT = reactorT;
        m_reactorState.reactorWorkT = reactorWorkT;
        m_reactorState.reactorDeltaT = reactorDeltaT;
        m_reactorState.reactorWarringT = reactorWarringT;        

    }



    // void CoolingSystem::calcRequestedPower(){

    //     // если температура реактора меньше рабочей

    //     if(m_reactorState.reactorT < m_reactorState.reactorWorkT + m_reactorState.reactorDeltaT)   
    //         return 0;

    //     if(m_reactorState.coolantT >= m_reactorState.coolantCritT)
    //         return 0;

        
        
    //     // то, что физически может быть передано от реактора в антифриз
    //     // необходимая для этого мощность насоса.
    //     double deltaT = m_reactorState.reactorT - m_reactorState.coolantT;
    //     double heatExchangePower = 0;
    //     if (deltaT > 0.0){
    //         heatExchangePower = m_primaryCircuit.m_heatExchangeCoeff * deltaT / PUMP_EFFICIENCY; 
    //     }
        
    //     // смотрим, что бы мощность насоса не не превышала физику и была не больше максимума
    //     double requeistedPower = m_primaryCircuit.allocatedPower;
    //     if(m_reactorState.reactorT - m_primaryCircuit.lastReactorTemp > 0)
    //         {
    //             requeistedPower += m_primaryCircuit.powerSteep;
    //             requeistedPower = std::min(requeistedPower,heatExchangePower);
    //             requeistedPower = std::min(requeistedPower,m_primaryCircuit.maxPumpPower);
    //         }
    //     m_primaryCircuit.requeistedPower = requeistedPower;
    // }

    void CoolingSystem::calcRequestedPower()
{
    double reactorT = m_reactorState.reactorT;
    double coolantT = m_reactorState.coolantT;

    double workT = m_reactorState.reactorWorkT;
    double delta = m_reactorState.reactorDeltaT;

    // ===== ГИСТЕРЕЗИС ВКЛЮЧЕНИЯ НАСОСА =====

    if (!m_primaryCircuit.isPumpActive)
    {
        if (reactorT >= workT + delta)
            m_primaryCircuit.isPumpActive = true;
    }
    else
    {
        if (reactorT <= workT - delta)
            m_primaryCircuit.isPumpActive = false;
    }

    if (!m_primaryCircuit.isPumpActive)
    {
        m_primaryCircuit.requeistedPower = 0.0;
        return;
    }

    if (coolantT >= m_reactorState.coolantCritT)
    {
        m_primaryCircuit.requeistedPower = 0.0;
        return;
    }

    // ===== ФИЗИЧЕСКИЙ ЛИМИТ ТЕПЛООБМЕНА =====

    double deltaT = reactorT - coolantT;

    double heatExchangePower = 0.0;

    if (deltaT > 0.0)
    {
        heatExchangePower =
            m_primaryCircuit.m_heatExchangeCoeff *
            deltaT / PUMP_EFFICIENCY;
    }

    // ===== ПЛАВНОЕ УВЕЛИЧЕНИЕ МОЩНОСТИ НАСОСА =====

    double requestedPower = m_primaryCircuit.allocatedPower;

    if (reactorT > m_primaryCircuit.lastReactorTemp)
    {
        requestedPower += m_primaryCircuit.powerSteep + 1;
    }

    requestedPower =
        std::min(requestedPower, heatExchangePower);

    requestedPower =
        std::min(requestedPower, m_primaryCircuit.maxPumpPower);

    m_primaryCircuit.requeistedPower = requestedPower;

    m_primaryCircuit.lastReactorTemp = reactorT;
}

}