#include "ShipAttachmentResolver.h"

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/ship/ShipAttachmentUtils.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/coordinates/WorldPosition.h"

using namespace game::ship::geometry;

static glm::mat4 buildAssemblyModuleOwnStaticLocalModel(
    const AssemblyModule& module
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

    return m;
}

static const AssemblyModule* findAssemblyModuleByIdLocal(
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

static glm::mat4 buildAssemblyModuleHierarchicalStaticLocalModel(
    const ObjectAssembly& assembly,
    const std::string& moduleId
)
{
    const AssemblyModule* module = findAssemblyModuleByIdLocal(assembly, moduleId);
    if (!module)
        return glm::mat4(1.0f);

    glm::mat4 own = buildAssemblyModuleOwnStaticLocalModel(*module);

    if (module->parentModuleId.empty())
        return own;

    return buildAssemblyModuleHierarchicalStaticLocalModel(
               assembly,
               module->parentModuleId
           ) * own;
}

std::optional<ShipAttachmentResolved> resolveShipAttachment(
    const ShipDescriptor* desc,
    const ShipTransform& shipTransform,
    const std::string& attachmentId,
    const ShipAttachmentOverrideMap* overrides,
    const std::vector<game::simulation::ObjectDetachedFragmentSnapshot>* detachedFragments
)
{
    if (!desc)
        return std::nullopt;

    const ShipAttachmentPoint* basePoint = findShipAttachmentPoint(desc, attachmentId);
    if (!basePoint)
        return std::nullopt;

    ShipAttachmentPoint resolvedPoint = *basePoint;

    if (overrides)
    {
        auto it = overrides->find(attachmentId);
        if (it != overrides->end())
        {
            resolvedPoint.localPosition = it->second.localPosition;
            resolvedPoint.localRotationDeg = it->second.localRotationDeg;
            resolvedPoint.enabled = it->second.enabled;
        }
    }

    if (!resolvedPoint.enabled)
        return std::nullopt;

    // ВАЖНО:
    // Resolver теперь работает в локальном frame игрока.
    // Сам корабль игрока находится в (0,0,0), а не в полном world-float.
    glm::mat4 shipLocalModel =
        shipTransform.orientation;

    glm::mat4 parentModel = shipLocalModel;



    // Если attachment сидит на модуле, который уже оторвался,
    // берём parent transform от detached fragment, а не от корпуса корабля.
    if (detachedFragments && !resolvedPoint.parentModuleId.empty())
    {
        for (const auto& fragment : *detachedFragments)
        {
            if (fragment.moduleId != resolvedPoint.parentModuleId)
                continue;

            const glm::vec3 fragmentLocalPosition =
                world::coordinates::relativeMetersFloat(
                    fragment.worldPosition,
                    shipTransform.worldPosition
                );

            parentModel =
                glm::translate(glm::mat4(1.0f), fragmentLocalPosition) *
                fragment.orientation;

            goto found_parent;
        }
    }

    

    if (!resolvedPoint.parentModuleId.empty() &&
        AssemblyMeshLibrary::has(desc->typeId))
    {
        const auto& assembly = AssemblyMeshLibrary::get(desc->typeId);

        const AssemblyModule* module = findAssemblyModuleByIdLocal(
            assembly,
            resolvedPoint.parentModuleId
        );

        if (module)
        {
            glm::mat4 moduleModel =
                buildAssemblyModuleHierarchicalStaticLocalModel(
                    assembly,
                    module->id
                );

            parentModel = shipLocalModel * moduleModel;
        }
        else
        {
            for (const auto& mod : assembly.modules)
            {
                for (const auto& part : mod.meshes)
                {
                    if (part.id != resolvedPoint.parentModuleId)
                        continue;

                    glm::mat4 moduleModel =
                        buildAssemblyModuleHierarchicalStaticLocalModel(
                            assembly,
                            mod.id
                        );

                    glm::mat4 partModel =
                        moduleModel * glm::translate(glm::mat4(1.0f), part.localOffset);

                    parentModel = shipLocalModel * partModel;
                    goto found_parent;
                }
            }
        }
    }

found_parent:

    glm::mat4 local(1.0f);
    local = glm::translate(local, resolvedPoint.localPosition);

    local = glm::rotate(
        local,
        glm::radians(resolvedPoint.localRotationDeg.x),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );
    local = glm::rotate(
        local,
        glm::radians(resolvedPoint.localRotationDeg.y),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    local = glm::rotate(
        local,
        glm::radians(resolvedPoint.localRotationDeg.z),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    glm::mat4 resolvedLocal = parentModel * local;

    ShipAttachmentResolved out;

    out.localPosition = glm::vec3(resolvedLocal[3]);

    out.worldPosition =
        world::coordinates::translated(
            shipTransform.worldPosition,
            glm::dvec3(out.localPosition)
        );

    out.worldOrientation =
        glm::mat4(glm::mat3(resolvedLocal));

    return out;
}