#include "ReactorSystem.h"
#include <algorithm>

namespace game::ship::core
{

ReactorSystem::ReactorSystem(double maxOutputMW)
    : m_maxOutput(maxOutputMW)
{
}

void ReactorSystem::setThrottle(double value)
{
    m_throttle = std::clamp(value, 0.0, 1.0);
}

double ReactorSystem::throttle() const
{
    return m_throttle;
}

double ReactorSystem::maxOutput() const
{
    return m_maxOutput * m_structuralIntegrity;
}

double ReactorSystem::currentOutput() const
{
    return maxOutput() * m_throttle;
}

void ReactorSystem::applyStructuralDamage(double amount)
{
    m_structuralIntegrity = std::clamp(
        m_structuralIntegrity - amount,
        0.0, 1.0);
}

double ReactorSystem::structuralIntegrity() const
{
    return m_structuralIntegrity;
}

void ReactorSystem::update(double dt)
{
    // пока минимально — нестабильность растёт при высокой нагрузке
    if (m_throttle > 0.9)
        m_instability += 0.01 * dt;
    else
        m_instability = std::max(0.0, m_instability - 0.02 * dt);
}

double ReactorSystem::heatGeneration() const
{
    // упрощённо: тепло пропорционально мощности
    return currentOutput() * 0.5; // коэффициент подберём позже
}

}