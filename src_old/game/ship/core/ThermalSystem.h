#pragma once
#include "src/game/ship/ShipDescriptor.h"

namespace game::ship::core
{

class ThermalSystem
{
public:
    ThermalSystem() = default;
    explicit ThermalSystem(const ThermalDescriptor& desc);

    void addHeat(double energyMJ);
    void removeHeat(double energyMJ);

    void setTemperature(double tempK) { m_temperature = tempK; }
    double getTemperature() const { return m_temperature; }
    
    void setThermalMass(double mass) { m_thermalMass = mass; }
    double getThermalMass() const { return m_thermalMass; }
    
    void update(double dt) { } // пока пусто
    double getStoredHeat() const { return m_storedHeat; }


    void resetHeatVolume() { m_heatVolume = 0; m_storedHeat = 0;}
    double getHeatVolume() const { return m_heatVolume; } 
    double getCriticalTemp() const { return m_thermalCriticalTemp; } 

    


private:
    double m_thermalMass = 100.0;
    double m_temperature = 900.0;
    double m_recomendTemp = 1100.0;
    double m_thermalCriticalTemp = 1350.0;

    double m_storedHeat = 0;
    double m_heatVolume = 0;
    
};

}