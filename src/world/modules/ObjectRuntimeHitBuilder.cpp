#include "ObjectRuntimeHitBuilder.h"

#include <unordered_set>
#include <unordered_map>
#include <array>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include "src/game/geometry/AssemblyMeshLibrary.h"

using namespace game::damage;
using namespace game::ship::geometry;

namespace world::modules
{

struct RuntimeVolumeBuildResult
{
    bool valid = false;

    glm::vec3 center {0.0f};
    glm::vec3 halfSize {0.5f};
    glm::mat3 orientation {1.0f};

    glm::vec3 aabbMin {0.0f};
    glm::vec3 aabbMax {0.0f};

    bool isObb = false;
};

static glm::mat4 buildModuleLocalModel(
    const AssemblyModule& module,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    glm::mat4 m(1.0f);

    m = glm::translate(m, module.localPosition);

    m = glm::rotate(
        m,
        glm::radians(module.localRotationDeg.x),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    m = glm::rotate(
        m,
        glm::radians(module.localRotationDeg.y),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    m = glm::rotate(
        m,
        glm::radians(module.localRotationDeg.z),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    if (module.rotates)
    {
        float angle = assemblyRuntime.getModuleRotationAngleRad(module.id);

        m = m
            * glm::translate(glm::mat4(1.0f), module.pivot)
            * glm::rotate(glm::mat4(1.0f), angle, glm::normalize(module.rotationAxis))
            * glm::translate(glm::mat4(1.0f), -module.pivot);
    }

    return m;
}

static void expandByTransformedCorners(
    const glm::vec3& minB,
    const glm::vec3& maxB,
    const glm::mat4& transform,
    bool& found,
    glm::vec3& outMin,
    glm::vec3& outMax
)
{
    std::array<glm::vec3, 8> p =
    {{
        {minB.x, minB.y, minB.z},
        {maxB.x, minB.y, minB.z},
        {maxB.x, maxB.y, minB.z},
        {minB.x, maxB.y, minB.z},

        {minB.x, minB.y, maxB.z},
        {maxB.x, minB.y, maxB.z},
        {maxB.x, maxB.y, maxB.z},
        {minB.x, maxB.y, maxB.z}
    }};

    for (auto& v : p)
    {
        glm::vec4 t = transform * glm::vec4(v, 1.0f);
        glm::vec3 q(t);

        if (!found)
        {
            outMin = q;
            outMax = q;
            found = true;
        }
        else
        {
            outMin = glm::min(outMin, q);
            outMax = glm::max(outMax, q);
        }
    }
}

static RuntimeVolumeBuildResult buildRuntimeVolumeForDescriptor(
    const ObjectAssembly& assembly,
    const ModuleDescriptor& desc,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    RuntimeVolumeBuildResult result;

    if (desc.meshPartIds.empty())
        return result;

    // single-part => строим OBB
    if (desc.meshPartIds.size() == 1)
    {
        const std::string& wantedId = desc.meshPartIds[0];

        for (const auto& module : assembly.modules)
        {
            glm::mat4 moduleLocalModel = buildModuleLocalModel(module, assemblyRuntime);

            for (const auto& part : module.meshes)
            {
                if (part.id != wantedId)
                    continue;

                glm::mat4 partLocalModel =
                    moduleLocalModel * glm::translate(glm::mat4(1.0f), part.localOffset);

                glm::vec3 localCenter =
                    (part.lod0Mesh.minBounds + part.lod0Mesh.maxBounds) * 0.5f;
                glm::vec3 localHalf =
                    (part.lod0Mesh.maxBounds - part.lod0Mesh.minBounds) * 0.5f;

                glm::vec4 transformedCenter =
                    partLocalModel * glm::vec4(localCenter, 1.0f);

                result.valid = true;
                result.isObb = true;
                result.center = glm::vec3(transformedCenter);
                result.halfSize = localHalf;
                result.orientation = glm::mat3(partLocalModel);

                // Нормализуем оси orientation
                result.orientation[0] = glm::normalize(result.orientation[0]);
                result.orientation[1] = glm::normalize(result.orientation[1]);
                result.orientation[2] = glm::normalize(result.orientation[2]);

                return result;
            }
        }

        return result;
    }

    // multi-part => fallback в AABB
    std::unordered_set<std::string> wanted(
        desc.meshPartIds.begin(),
        desc.meshPartIds.end()
    );

    bool found = false;
    glm::vec3 minV(0.0f);
    glm::vec3 maxV(0.0f);

    for (const auto& module : assembly.modules)
    {
        glm::mat4 moduleLocalModel = buildModuleLocalModel(module, assemblyRuntime);

        for (const auto& part : module.meshes)
        {
            if (wanted.find(part.id) == wanted.end())
                continue;

            glm::mat4 partLocalModel =
                moduleLocalModel * glm::translate(glm::mat4(1.0f), part.localOffset);

            expandByTransformedCorners(
                part.lod0Mesh.minBounds,
                part.lod0Mesh.maxBounds,
                partLocalModel,
                found,
                minV,
                maxV
            );
        }
    }

    if (!found)
        return result;

    result.valid = true;
    result.isObb = false;
    result.aabbMin = minV;
    result.aabbMax = maxV;
    result.center = (minV + maxV) * 0.5f;
    result.halfSize = (maxV - minV) * 0.5f;
    result.orientation = glm::mat3(1.0f);

    return result;
}

void ObjectRuntimeHitBuilder::rebuild(
    HitComponent& hitComponent,
    ObjectType typeId,
    const IObjectDescriptor& descriptor,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    if (!AssemblyMeshLibrary::has(typeId))
        return;

    const auto& assembly = AssemblyMeshLibrary::get(typeId);

    std::unordered_map<std::string, HitVolume> oldById;
    for (const auto& oldV : hitComponent.volumes)
    {
        oldById[oldV.moduleId] = oldV;
    }

    hitComponent.volumes.clear();

    for (const auto& modDesc : descriptor.moduleDescriptors())
    {
        if (!modDesc.enabled)
            continue;

        RuntimeVolumeBuildResult built =
            buildRuntimeVolumeForDescriptor(assembly, modDesc, assemblyRuntime);

        if (!built.valid)
            continue;

        HitVolume v;
        v.zone = modDesc.zone;
        v.priority = modDesc.hitPriority;
        v.layerIndex = modDesc.layerIndex;

        v.center = built.center;
        v.halfSize = built.halfSize;
        v.orientation = built.orientation;

        v.m_label = modDesc.moduleId;
        v.moduleId = modDesc.moduleId;
        v.subsystemId = modDesc.subsystemId;

        v.destructible = modDesc.destructible;
        v.health = modDesc.maxHealth;
        v.maxHealth = modDesc.maxHealth;
        v.armor = modDesc.armor;
        v.penetrationResistance = modDesc.penetrationResistance;

        auto itOld = oldById.find(v.moduleId);
        if (itOld != oldById.end())
        {
            v.health = itOld->second.health;
            v.maxHealth = itOld->second.maxHealth;
            v.destroyed = itOld->second.destroyed;
        }

        hitComponent.volumes.push_back(v);
    }
}

} // namespace world::modules