#include "CoreSystem.h"


namespace game::ship::core
{


CoreSystem::CoreSystem(double nominalPowerMW, std::string label, game::equipment::PowerPriority priority)
    : m_nominalPowerMW(nominalPowerMW)
    , m_label(label)
    , m_priority(priority)
{
}

void CoreSystem::setMode(Mode mode)
{
    m_mode = mode;
}

double CoreSystem::getRequestedPower() const
{
    switch (m_mode)
    {
        case Mode::Normal:
            return m_nominalPowerMW;

        case Mode::Emergency:
            return m_nominalPowerMW * 1.5;

        case Mode::Damaged:
            return m_nominalPowerMW * 0.7;

        default:
            return m_nominalPowerMW;
    }
}

void CoreSystem::setAvailablePower(double powerMW)
{
    m_allocatedPowerMW = powerMW;
}


double  CoreSystem::getAvailablePower() const 
{
    return m_allocatedPowerMW;
}





void CoreSystem::update(double /*dt*/)
{
    // Практически вся электрическая энергия превращается в тепло
    m_heatGeneratedMW = m_allocatedPowerMW;
}

double CoreSystem::getHeatGeneration() const
{
    return m_heatGeneratedMW;
}




}