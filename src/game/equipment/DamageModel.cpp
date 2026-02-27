#include "DamageModel.h"
#include <algorithm>

namespace game
{

void DamageModel::applyStructuralDamage(double amount)
{
    m_integrity.structural =
        std::clamp(m_integrity.structural - amount, 0.0, 1.0);
}

void DamageModel::applyFunctionalDamage(double amount)
{
    m_integrity.functional =
        std::clamp(m_integrity.functional - amount, 0.0, 1.0);
}

void DamageModel::addHeat(double amount)
{
    m_integrity.thermal =
        std::clamp(m_integrity.thermal + amount, 0.0, 1.0);
}

void DamageModel::cool(double amount)
{
    m_integrity.thermal =
        std::clamp(m_integrity.thermal - amount, 0.0, 1.0);
}

void DamageModel::update(double dt)
{
    // Если перегрев высокий — начинаем разрушать структуру
    if (m_integrity.thermal > 0.8)
    {
        double overflow = m_integrity.thermal - 0.8;
        applyStructuralDamage(overflow * 0.1 * dt);
        applyFunctionalDamage(overflow * 0.2 * dt);
    }
}

const Integrity& DamageModel::getIntegrity() const
{
    return m_integrity;
}

}
