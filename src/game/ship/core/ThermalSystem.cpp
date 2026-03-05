#include "ThermalSystem.h"

namespace game::ship::core
{



ThermalSystem::ThermalSystem(const ThermalDescriptor& desc)
    : m_temperature(desc.initialTempK)
    , m_thermalMass(desc.thermalMassMJperK)
{
}


void ThermalSystem::addHeat(double energyMJ)
{
    if (m_thermalMass > 0.0) {
            m_temperature += energyMJ / m_thermalMass;
            m_storedHeat += energyMJ;   // накопленное от всех тепло - отведенное на радиаторы, MJ
            m_heatVolume += energyMJ;   // общее количество накопленного тепла, MJ
        }

}


void ThermalSystem::removeHeat(double energyMJ) {
        if (m_thermalMass > 0.0) {
            m_temperature -= energyMJ / m_thermalMass;
            m_storedHeat -= energyMJ;
            if (m_temperature < 0.0) m_temperature = 0.0;
        }
    }

}