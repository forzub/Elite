#pragma once

#include "src/game/equipment/capabilities/IPowerConsumer.h"
#include "src/game/equipment/capabilities/IHeatSource.h"
#include "ThermalSystem.h"
#include "src/game/physics/UniverseMaterials.h"
#include <string_view>
#include <vector>

namespace game::ship::core
{

struct RadiatorPanel
{
    double area;        // м²
    double efficiency;  // 0..1 (повреждения)
    double health;      // 0..1
    std::string_view materialId {};
};

struct PrimaryCircuit {
    // ==========================
    // ----------- НАСОС - первый контур ------
    // ==========================
    double maxPumpPower = 8.0;          // МВт (эл. макс насоса)
    double maxHeatTransfer = 180.0;     // МВт (макс тепла от насоса)
    double lastReactorTemp = 0;         // сохраняем последнюю температуру реактора.
    double powerSteep = 1;              // шаг уеличения мощности насоса
    
    double requeistedPower = 0.0;        // Потребление энергии (насосы циркуляции)
    double allocatedPower = 0.0;        // Потребление энергии (насосы циркуляции)

    double pumpCapacity = 0.0;          // переносимая мощность (allocatedPower * PUMP_EFFICIENCY)
    double pumpHeatMJ = 0.0;            // тепло от насосов за dt

    double lastRadiatedPower = 0.0;     // МВт (фактически отведённая мощность)
    
    // коэфиуиент теплообмена 1 контура
    double m_heatExchangeCoeff  = 10.0;   // МВт/К
    bool isPumpActive           = false;
    bool coolantOverheatFlag    = false;
};

struct SecondaryCircuit
{
    // ==========================
    // ----------- ПАНЕЛИ - второй контур------
    // ==========================
    std::vector<RadiatorPanel> panels;
    double emissivity = 0.9;           // fallback for legacy non-catalog hulls
    std::string_view materialId {};
    double maxCoolantTemp = 900.0;      // K (макс температура антифриза)
};


struct ReactorState{
    double reactorT         = 0; 
    double reactorWorkT     = 0;
    double reactorDeltaT    = 0;
    double reactorWarringT  = 0;
    double coolantT         = 0;
    double coolantCritT     = 0;

};

class CoolingSystem : public game::equipment::IPowerConsumer,
                      public game::equipment::IHeatSource
{
public:
    CoolingSystem() = default;
    
    // Инициализация
    void init(double totalArea,          // общая площадь радиаторов
              int panelCount,            // количество панелей
              double emissivity,         // 0..1
              double maxCoolantTemp,     // K (legacy cooling boundary)
              std::string_view materialId = {});
    
    // IPowerConsumer
    double getRequestedPower() const override;
    void setAvailablePower(double power) override;
    double getAvailablePower() const override { return m_primaryCircuit.allocatedPower; }
    game::equipment::PowerPriority getPriority() const override { 
        return m_priority; 
    }
    std::string getLabel() const override { return "CoolingSystem"; }
    

    // IHeatSource - НАСОСЫ ГРЕЮТ АНТИФРИЗ!
    double getHeatGeneration() const override {
        // 10% энергии насоса уходит в нагрев антифриза (КПД 90%)
        return m_primaryCircuit.allocatedPower * 0.1;
    }
    

    // Основной метод: забираем тепло из антифриза и излучаем
    void update(double dt, ThermalSystem& coolant);
    // Resolve newly allocated electric power BEFORE reactor heat extraction.
    void prepareTransport() noexcept;
    
    // Расчет максимальной излучаемой мощности при данной температуре
    double calculateRadiatedPower(double temperature) const;
    
    // Повреждения панелей
    void damagePanel(int index, double amount);
    void damageRandomPanel(double amount);
    double getTotalEfficiency() const;
    bool isPanelThermalCritical() const { return m_panelThermalCritical; }
    void repairAllPanels();
    
    // Геттеры для PrimaryCircuit
    double getAllocatedPower() const { return m_primaryCircuit.allocatedPower; }
    double getLastRadiatedPower() const { return m_primaryCircuit.lastRadiatedPower; }
    double getMaxPumpPower() const { return m_primaryCircuit.maxPumpPower; }
    double getMaxHeatTransfer() const { return m_primaryCircuit.maxHeatTransfer; }
    double getPumpCapacity() const { return m_primaryCircuit.pumpCapacity; }
    bool isReactorPumpActive() const { return m_primaryCircuit.isPumpActive; }
    bool isCoolantOverheatLatched() const { return m_primaryCircuit.coolantOverheatFlag; }
    double getRadiatorPotentialMW() const { return calculateRadiatedPower(m_coolantTemp); }
    double getEffectiveRadiatorTemperatureK() const { return m_coolantTemp; }
    double getPumpHeatMJ() const { return m_primaryCircuit.pumpHeatMJ; }
    
    // Геттеры для SecondaryCircuit
    double getMaxCoolantTemp() const { return m_secondaryCircuit.maxCoolantTemp; }
    double getEmissivity() const { return m_secondaryCircuit.emissivity; }
    int getPanelCount() const { return m_secondaryCircuit.panels.size(); }
    
    double getPanelHealth(int index) const {
        if (index >= 0 && index < m_secondaryCircuit.panels.size())
            return m_secondaryCircuit.panels[index].health;
        return 0.0;
    }
    
    double getPanelEfficiency(int index) const {
        if (index >= 0 && index < m_secondaryCircuit.panels.size())
            return m_secondaryCircuit.panels[index].efficiency;
        return 0.0;
    }
    
    // Общие геттеры
    double getCoolantTemp() const { return m_coolantTemp; }
    double getDT() const { return m_dt; }
    double getPumpEfficiency() const { return PUMP_EFFICIENCY; }
    
    // Сеттеры
    void setCoolantTemp(double temp) { m_coolantTemp = temp; }
    void setMaxPumpPower(double power) { m_primaryCircuit.maxPumpPower = power; }
    void setMaxHeatTransfer(double transfer) { m_primaryCircuit.maxHeatTransfer = transfer; }

    void setReactorState(
        double reactorT, 
        double reactorWorkT,
        double reactorDeltaT,
        double reactorWarringT,
        double coolantT,
        double coolantCritT
    );


    void setLastReactorTemp(double temperatere){ m_primaryCircuit.lastReactorTemp = temperatere;}
    void calcRequestedPower();
    
private:
    double getEffectiveArea() const;

    PrimaryCircuit      m_primaryCircuit;
    game::equipment::PowerPriority m_priority = game::equipment::PowerPriority::Critical;
    
    SecondaryCircuit    m_secondaryCircuit;
    ReactorState        m_reactorState;
    
    // Временные переменные для отладки
    double m_coolantTemp = 293.0;
    double m_dt = 0.0;
    bool m_panelThermalCritical = false;

    
    // Conversion between electric pump demand and coolant transport is a
    // subsystem parameter, NOT the universe's thermal radiation law.
    static constexpr double PUMP_EFFICIENCY = 20.0;
};

}