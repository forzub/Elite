#include "ThermalSystem.h"
#include <algorithm>

namespace game::ship::core
{

ThermalSystem::ThermalSystem(double thermalMass,
                             double maxSafeTemp)
    : m_thermalMass(thermalMass),
      m_maxSafeTemperature(maxSafeTemp)
{
}

void ThermalSystem::addHeat(double amount)
{
    m_temperature += amount / m_thermalMass;
}

void ThermalSystem::setCoolingRate(double rate)
{
    m_coolingRate = rate;
}

void ThermalSystem::update(double dt)
{
    // охлаждение
    m_temperature -= m_coolingRate * dt;

    if (m_temperature < 0.0)
        m_temperature = 0.0;

    // если перегрев — деградация
    if (m_temperature > m_maxSafeTemperature)
    {
        double overload = m_temperature - m_maxSafeTemperature;
        m_structuralIntegrity -= overload * 0.0001 * dt;
        m_structuralIntegrity = std::max(0.0, m_structuralIntegrity);
    }
}

double ThermalSystem::temperature() const
{
    return m_temperature;
}

double ThermalSystem::maxSafeTemperature() const
{
    return m_maxSafeTemperature;
}

double ThermalSystem::integrity() const
{
    return m_structuralIntegrity;
}

}