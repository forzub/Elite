#include "DamageSystem.h"

namespace game::damage
{

void DamageSystem::applyDamage(
    IDamageable& target,
    const DamageEvent& event
)
{
    target.applyDamage(event);
}

}