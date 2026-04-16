#include "ShipAttachmentResolver.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/ship/ShipAttachmentUtils.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

using namespace game::ship::geometry;

static glm::mat4 buildModuleLocalModel(
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

std::optional<ShipAttachmentResolved> resolveShipAttachment(
    const ShipDescriptor* desc,
    const ShipTransform& shipTransform,
    const std::string& attachmentId,
    const ShipAttachmentOverrideMap* overrides
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

    glm::mat4 shipModel =
        glm::translate(glm::mat4(1.0f), shipTransform.position) *
        shipTransform.orientation;

    glm::mat4 parentModel = shipModel;

    if (!resolvedPoint.parentModuleId.empty() &&
        AssemblyMeshLibrary::has(desc->typeId))
    {
        const auto& assembly = AssemblyMeshLibrary::get(desc->typeId);

        bool found = false;

        for (const auto& module : assembly.modules)
        {
            // 1) сначала пробуем как module.id
            if (module.id == resolvedPoint.parentModuleId)
            {
                parentModel = shipModel * buildModuleLocalModel(module);
                found = true;
                break;
            }

            // 2) потом пробуем как mesh part.id
            for (const auto& part : module.meshes)
            {
                if (part.id != resolvedPoint.parentModuleId)
                    continue;

                glm::mat4 partModel = buildModuleLocalModel(module);
                partModel = glm::translate(partModel, part.localOffset);

                parentModel = shipModel * partModel;
                found = true;
                break;
            }

            if (found)
                break;
        }
    }

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

    glm::mat4 world = parentModel * local;

    ShipAttachmentResolved out;
    out.worldPosition = glm::vec3(world[3]);
    out.worldOrientation = glm::mat4(glm::mat3(world));

    


    return out;
}