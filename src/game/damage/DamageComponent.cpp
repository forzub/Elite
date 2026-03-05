#include "DamageComponent.h"

namespace game::damage
{

void DamageComponent::apply(const DamageEvent& event)
{
    m_active.push_back(event);
}

void DamageComponent::update(float dt)
{
    for (auto& e : m_active)
    {
        if (e.duration > 0.0f)
        {
            e.elapsed += dt;
        }
    }

    // удалить завершённые временные эффекты
    m_active.erase(
        std::remove_if(
            m_active.begin(),
            m_active.end(),
            [](const DamageEvent& e)
            {
                return (e.duration > 0.0f) && (e.elapsed >= e.duration);
            }),
        m_active.end()
    );
}

const std::vector<DamageEvent>& DamageComponent::active() const
{
    return m_active;
}

bool DamageComponent::hasAny() const
{
    return !m_active.empty();
}

void DamageComponent::clear()
{
    m_active.clear();
}

}