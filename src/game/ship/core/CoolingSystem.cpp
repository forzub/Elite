
#include "CoolingSystem.h"
#include "src/game/physics/UniverseThermalRadiation.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace game::ship::core
{

void CoolingSystem::init(double totalArea, int panelCount, 
                         double emissivity, double maxCoolantTemp,
                         std::string_view materialId)
{
    m_secondaryCircuit.materialId = materialId;
    const auto* material = game::physics::materials::findMaterial(materialId);
    m_secondaryCircuit.emissivity = material
        ? material->emissivity01 : std::clamp(emissivity, 0.0, 1.0);
    m_secondaryCircuit.maxCoolantTemp = maxCoolantTemp;
    
    double panelArea = totalArea / panelCount;
    m_secondaryCircuit.panels.clear();
    
    for (int i = 0; i < panelCount; ++i)
    {
        m_secondaryCircuit.panels.push_back(
            {panelArea, 1.0, 1.0, materialId}
        );
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
    return game::physics::thermal::radiatedPowerMW(
        getEffectiveArea(),
        temperature,
        m_secondaryCircuit.emissivity
    );
}











double CoolingSystem::getRequestedPower() const
{
    return m_primaryCircuit.requeistedPower;
}





void CoolingSystem::setAvailablePower(double power)
{
    m_primaryCircuit.allocatedPower = power;
}







void CoolingSystem::prepareTransport() noexcept
{
    m_primaryCircuit.pumpCapacity = std::clamp(
        m_primaryCircuit.allocatedPower, 0.0,
        m_primaryCircuit.maxPumpPower) * PUMP_EFFICIENCY;
}

void CoolingSystem::update(double dt, ThermalSystem& coolant)
{
    m_primaryCircuit.lastReactorTemp = m_reactorState.reactorT;
    
    // ===== ПЕРВЫЙ КОНТУР (НАСОСЫ) ===== 
    // 1. Насосы греют антифриз (10% от allocatedPower)
    double pumpHeatMW = getHeatGeneration();
    coolant.addHeat(pumpHeatMW * dt);
    
    // 2. Рассчитываем переносимую мощность насосов
    prepareTransport();
    
    // Сохраняем dt для отладки
    m_dt = dt;
    m_primaryCircuit.pumpHeatMJ = m_primaryCircuit.pumpCapacity * dt;
  


    // ===== ВТОРОЙ КОНТУР (РАДИАТОРЫ) =====
    // 3. Сколько могут излучить радиаторы при текущей температуре
    m_coolantTemp = coolant.getTemperature();
    double maxRadiatedPower = calculateRadiatedPower(m_coolantTemp);
    
    // 4. The radiator is NOT an infinite heat sink disconnected from the
    // cooling pumps. Its radiation capability is limited by real coolant
    // circulation and the plant's rated heat-transport capacity. Failed
    // pumps/overheated primary circuit therefore warm all mounted engines.
    const double transportedMW = std::max(
        0.0, std::min(
            m_primaryCircuit.pumpCapacity,
            m_primaryCircuit.maxHeatTransfer
        )
    );
    const double actualRemovedPower =
        std::min(maxRadiatedPower, transportedMW);
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





void CoolingSystem::calcRequestedPower()
{
    double reactorT = m_reactorState.reactorT;
    double coolantT = m_reactorState.coolantT;

    double workT = m_reactorState.reactorWorkT;
    double delta = m_reactorState.reactorDeltaT;

    // ===== ГИСТЕРЕЗИС ВКЛЮЧЕНИЯ НАСОСА ПО ТЕМПЕРАТУРЕ РЕАКТОРА =====

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

    // ===== ГИСТЕРЕЗИС ПО КРИТИЧЕСКОЙ ТЕМПЕРАТУРЕ АНТИФРИЗА =====
    
    if (coolantT >= m_reactorState.coolantCritT)
    {
        // Достигли критической температуры - выключаем насос и устанавливаем флаг
        m_primaryCircuit.requeistedPower = 0.0;
        m_primaryCircuit.coolantOverheatFlag = true;
        return;
    }
    else if (m_primaryCircuit.coolantOverheatFlag && 
             coolantT <= m_reactorState.coolantCritT - 2.0 * delta)
    {
        // Остыли на 2*delta ниже критической - сбрасываем флаг
        m_primaryCircuit.coolantOverheatFlag = false;
    }
    
    // Если флаг перегрева установлен - насос не работает
    if (m_primaryCircuit.coolantOverheatFlag)
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

    if(reactorT >= m_reactorState.reactorWarringT){
        m_primaryCircuit.requeistedPower = m_primaryCircuit.maxPumpPower;
    }
}



}