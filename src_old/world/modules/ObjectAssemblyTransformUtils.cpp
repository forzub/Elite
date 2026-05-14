#include "ObjectAssemblyTransformUtils.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace world::modules
{

using namespace game::ship::geometry;

const AssemblyModule* findAssemblyModuleById(
    const ObjectAssembly& assembly,
    const std::string& moduleId
)
{
    for (const auto& module : assembly.modules)
    {
        if (module.id == moduleId)
            return &module;
    }

    return nullptr;
}

const AssemblyMeshPart* findAssemblyMeshPartById(
    const ObjectAssembly& assembly,
    const std::string& meshPartId,
    const AssemblyModule** outOwnerModule
)
{
    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            if (part.id == meshPartId)
            {
                if (outOwnerModule)
                    *outOwnerModule = &module;

                return &part;
            }
        }
    }

    if (outOwnerModule)
        *outOwnerModule = nullptr;

    return nullptr;
}

glm::mat4 buildAssemblyModuleOwnLocalModel(
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

glm::mat4 buildAssemblyModuleHierarchicalLocalModel(
    const ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const std::string& moduleId
)
{
    const AssemblyModule* module = findAssemblyModuleById(assembly, moduleId);
    if (!module)
        return glm::mat4(1.0f);

    glm::mat4 own = buildAssemblyModuleOwnLocalModel(*module, assemblyRuntime);

    if (module->parentModuleId.empty())
        return own;

    return buildAssemblyModuleHierarchicalLocalModel(
               assembly,
               assemblyRuntime,
               module->parentModuleId
           ) * own;
}

std::optional<glm::mat4> buildAssemblyMeshPartHierarchicalLocalModel(
    const ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const std::string& meshPartId
)
{
    const AssemblyModule* owner = nullptr;
    const AssemblyMeshPart* part =
        findAssemblyMeshPartById(assembly, meshPartId, &owner);

    if (!part || !owner)
        return std::nullopt;

    glm::mat4 moduleModel =
        buildAssemblyModuleHierarchicalLocalModel(
            assembly,
            assemblyRuntime,
            owner->id
        );

    glm::mat4 partModel =
        moduleModel * glm::translate(glm::mat4(1.0f), part->localOffset);

    return partModel;
}

} // namespace world::modules