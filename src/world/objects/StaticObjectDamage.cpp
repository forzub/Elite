#include "StaticObjectDamage.h"

#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/world/modules/ObjectRuntimeHitBuilder.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include <glm/gtc/matrix_transform.hpp>

namespace world::objects
{

void applyDamage(StaticObject& obj, const game::damage::DamageEvent& event)
{
    game::damage::DamageEvent localEvent = event;

    localEvent.setLocalPositionForResolve(
        world::coordinates::relativeMetersFloat(
            event.worldPosition,
            obj.worldPosition
        )
    );


    auto result = obj.hitComponent.resolve(localEvent);

    if (!result.volume || !result.event)
        return;

    auto* volume = const_cast<game::damage::HitVolume*>(result.volume);
    if (!volume || !volume->destructible || volume->destroyed)
        return;

    float effectiveDamage =
        static_cast<float>(localEvent.energy * 0.1) - volume->armor;

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

    obj.hitVolumesDirty = true;

    const auto& desc = ObjectDescriptorRegistry::get(obj.type);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        obj.hitComponent,
        obj.type,
        desc,
        obj.moduleRuntime,
        obj.structuralLinkRuntime,
        obj.assemblyRuntime
    );

    obj.hitVolumesDirty = false;
    if (game::ship::geometry::AssemblyMeshLibrary::has(obj.type))
    {
        const auto& assembly =
            game::ship::geometry::AssemblyMeshLibrary::get(obj.type);

        // Detached runtime работает в локальной системе объекта.
        // Глобальную трансляцию сюда нельзя подмешивать.
        const glm::mat4 ownerModel =
            obj.orientation;

        obj.detachedFragmentRuntime.syncFromDetachedModules(
            ownerModel,
            assembly,
            obj.assemblyRuntime,
            obj.moduleRuntime,
            obj.hitComponent
        );
    }
}

}