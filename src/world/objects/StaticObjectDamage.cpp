#include "StaticObjectDamage.h"

namespace world::objects
{

void applyDamage(StaticObject& obj, const game::damage::DamageEvent& event)
{
    auto result = obj.hitComponent.resolve(event);

    if (!result.volume || !result.event)
        return;

    auto* volume = const_cast<game::damage::HitVolume*>(result.volume);
    if (!volume || !volume->destructible || volume->destroyed)
        return;

    float effectiveDamage =
        static_cast<float>(event.energy * 0.1) - volume->armor;

    if (effectiveDamage < 0.0f)
        effectiveDamage = 0.0f;

    volume->health -= effectiveDamage;

    if (volume->health <= 0.0f)
    {
        volume->health = 0.0f;
        volume->destroyed = true;

        obj.moduleRuntime.markModuleDestroyed(volume->moduleId, volume->health);
    }
    else
    {
        obj.moduleRuntime.setModuleHealth(volume->moduleId, volume->health);
    }
}

}