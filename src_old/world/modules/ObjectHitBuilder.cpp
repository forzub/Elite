#include "ObjectHitBuilder.h"

#include <iostream>
#include <unordered_set>

#include "src/game/geometry/AssemblyMeshLibrary.h"

using namespace game::damage;
using namespace game::ship::geometry;

namespace world::modules
{

static bool collectBoundsForDescriptor(
    const ObjectAssembly& assembly,
    const ModuleDescriptor& desc,
    glm::vec3& outMin,
    glm::vec3& outMax
)
{
    if (desc.meshPartIds.empty())
        return false;

    std::unordered_set<std::string> wanted(
        desc.meshPartIds.begin(),
        desc.meshPartIds.end()
    );

    bool found = false;
    glm::vec3 minV(0.0f);
    glm::vec3 maxV(0.0f);

    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            if (wanted.find(part.id) == wanted.end())
                continue;

            if (!found)
            {
                minV = part.minBounds;
                maxV = part.maxBounds;
                found = true;
            }
            else
            {
                minV = glm::min(minV, part.minBounds);
                maxV = glm::max(maxV, part.maxBounds);
            }
        }
    }

    if (!found)
        return false;

    outMin = minV;
    outMax = maxV;
    return true;
}

void ObjectHitBuilder::build(
    HitComponent& hitComponent,
    ObjectType typeId,
    const IObjectDescriptor& descriptor
)
{
    hitComponent.volumes.clear();

    if (!AssemblyMeshLibrary::has(typeId))
    {
        std::cout << "[ObjectHitBuilder] no assembly for type "
                  << static_cast<int>(typeId) << "\n";
        return;
    }

    const auto& assembly = AssemblyMeshLibrary::get(typeId);

    for (const auto& modDesc : descriptor.moduleDescriptors())
    {
        if (!modDesc.enabled)
            continue;

        glm::vec3 minV(0.0f);
        glm::vec3 maxV(0.0f);

        if (!collectBoundsForDescriptor(assembly, modDesc, minV, maxV))
        {
            std::cout << "[ObjectHitBuilder] no mesh parts found for descriptor "
                      << modDesc.moduleId << "\n";
            continue;
        }

        HitVolume v;
        v.zone = modDesc.zone;
        v.priority = modDesc.hitPriority;
        v.layerIndex = modDesc.layerIndex;
        v.center = (minV + maxV) * 0.5f;
        v.halfSize = (maxV - minV) * 0.5f;
        v.m_label = modDesc.moduleId;
        v.moduleId = modDesc.moduleId;
        v.subsystemId = modDesc.subsystemId;
        v.destructible = modDesc.destructible;
        v.health = modDesc.maxHealth;
        v.maxHealth = modDesc.maxHealth;
        v.armor = modDesc.armor;
        v.penetrationResistance = modDesc.penetrationResistance;

        hitComponent.volumes.push_back(v);
    }
}

} // namespace world::modules