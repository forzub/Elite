#include "ObjectAssemblyTransformUtils.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace world::modules
{

using namespace game::ship::geometry;

namespace
{
template <typename LocalModelBuilder>
glm::mat4 buildAssemblyModuleHierarchy(
    const ObjectAssembly& assembly,
    const std::string& moduleId,
    LocalModelBuilder&& buildLocalModel
)
{
    std::vector<const AssemblyModule*> hierarchy;
    std::unordered_set<std::string> visited;

    std::string currentId = moduleId;

    while (!currentId.empty())
    {
        if (!visited.insert(currentId).second)
        {
            // Invalid cyclic data must not recurse forever or corrupt a map.
            return glm::mat4(1.0f);
        }

        const AssemblyModule* module =
            findAssemblyModuleById(
                assembly,
                currentId
            );

        if (!module)
            return glm::mat4(1.0f);

        hierarchy.push_back(module);
        currentId = module->parentModuleId;
    }

    glm::mat4 model(1.0f);

    for (auto it = hierarchy.rbegin();
         it != hierarchy.rend();
         ++it)
    {
        model *= buildLocalModel(**it);
    }

    return model;
}
}

const AssemblyModule* findAssemblyModuleById(
    const ObjectAssembly& assembly,
    const std::string& moduleId
)
{
    const auto it =
        std::find_if(
            assembly.modules.begin(),
            assembly.modules.end(),
            [&](const AssemblyModule& module)
            {
                return module.id == moduleId;
            }
        );

    return it != assembly.modules.end()
        ? &*it
        : nullptr;
}

const AssemblyMeshPart* findAssemblyMeshPartById(
    const ObjectAssembly& assembly,
    const std::string& meshPartId,
    const AssemblyModule** outOwnerModule
)
{
    for (const auto& module : assembly.modules)
    {
        const auto partIt =
            std::find_if(
                module.meshes.begin(),
                module.meshes.end(),
                [&](const AssemblyMeshPart& part)
                {
                    return part.id == meshPartId;
                }
            );

        if (partIt == module.meshes.end())
            continue;

        if (outOwnerModule)
            *outOwnerModule = &module;

        return &*partIt;
    }

    if (outOwnerModule)
        *outOwnerModule = nullptr;

    return nullptr;
}

glm::mat4 buildAssemblyModuleStaticLocalModel(
    const AssemblyModule& module
)
{
    glm::mat4 model(1.0f);

    model =
        glm::translate(
            model,
            module.localPosition
        );

    model =
        glm::rotate(
            model,
            glm::radians(module.localRotationDeg.x),
            glm::vec3(1.0f, 0.0f, 0.0f)
        );

    model =
        glm::rotate(
            model,
            glm::radians(module.localRotationDeg.y),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

    model =
        glm::rotate(
            model,
            glm::radians(module.localRotationDeg.z),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );

    return model;
}

glm::mat4 buildAssemblyModuleStaticHierarchicalLocalModel(
    const ObjectAssembly& assembly,
    const std::string& moduleId
)
{
    return
        buildAssemblyModuleHierarchy(
            assembly,
            moduleId,
            [](const AssemblyModule& module)
            {
                return
                    buildAssemblyModuleStaticLocalModel(
                        module
                    );
            }
        );
}

glm::mat4 buildAssemblyModuleOwnLocalModel(
    const AssemblyModule& module,
    const ObjectAssemblyRuntime& assemblyRuntime
)
{
    glm::mat4 model =
        buildAssemblyModuleStaticLocalModel(
            module
        );

    if (!module.rotates)
        return model;

    const float angle =
        assemblyRuntime.getModuleRotationAngleRad(
            module.id
        );

    const float axisLength =
        glm::length(
            module.rotationAxis
        );

    if (axisLength <= 0.000001f)
        return model;

    const glm::vec3 rotationAxis =
        module.rotationAxis /
        axisLength;

    return
        model *
        glm::translate(
            glm::mat4(1.0f),
            module.pivot
        ) *
        glm::rotate(
            glm::mat4(1.0f),
            angle,
            rotationAxis
        ) *
        glm::translate(
            glm::mat4(1.0f),
            -module.pivot
        );
}

glm::mat4 buildAssemblyModuleHierarchicalLocalModel(
    const ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const std::string& moduleId
)
{
    return
        buildAssemblyModuleHierarchy(
            assembly,
            moduleId,
            [&](const AssemblyModule& module)
            {
                return
                    buildAssemblyModuleOwnLocalModel(
                        module,
                        assemblyRuntime
                    );
            }
        );
}

std::optional<glm::mat4> buildAssemblyMeshPartHierarchicalLocalModel(
    const ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const std::string& meshPartId
)
{
    const AssemblyModule* owner = nullptr;
    const AssemblyMeshPart* part =
        findAssemblyMeshPartById(
            assembly,
            meshPartId,
            &owner
        );

    if (!part || !owner)
        return std::nullopt;

    return
        buildAssemblyModuleHierarchicalLocalModel(
            assembly,
            assemblyRuntime,
            owner->id
        ) *
        glm::translate(
            glm::mat4(1.0f),
            part->localOffset
        );
}

} // namespace world::modules
