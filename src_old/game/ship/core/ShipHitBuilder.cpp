#include "ShipHitBuilder.h"

#include <iostream>
#include <unordered_set>
#include <limits>

#include "game/geometry/AssemblyMeshLibrary.h"

using namespace game::damage;
using namespace game::ship::geometry;

namespace game::ship
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

void ShipHitBuilder::build(
    HitComponent& hitComponent,
    const ShipDescriptor& descriptor
)
{
    hitComponent.volumes.clear();

    if (!AssemblyMeshLibrary::has(descriptor.typeId))
    {
        std::cout << "[ShipHitBuilder] no assembly for type "
                  << static_cast<int>(descriptor.typeId) << "\n";
        return;
    }

    const auto& assembly = AssemblyMeshLibrary::get(descriptor.typeId);

    for (const auto& modDesc : descriptor.modules)
    {
        if (!modDesc.enabled)
            continue;

        glm::vec3 minV(0.0f);
        glm::vec3 maxV(0.0f);

        if (!collectBoundsForDescriptor(assembly, modDesc, minV, maxV))
        {
            std::cout << "[ShipHitBuilder] no mesh parts found for descriptor "
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

        std::cout << "[ShipHitBuilder] volume module=" << v.moduleId
                  << " center=("
                  << v.center.x << "," << v.center.y << "," << v.center.z << ")"
                  << " half=("
                  << v.halfSize.x << "," << v.halfSize.y << "," << v.halfSize.z << ")"
                  << " layer=" << v.layerIndex
                  << " hp=" << v.health
                  << "\n";
    }
}

} // namespace game::ship