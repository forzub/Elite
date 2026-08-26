#include "tools/model_asset_editor/RuntimeAssemblyImporter.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "src/game/ship/ShipAttachmentPoint.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "tools/model_asset_editor/NativeObjImporter.h"

namespace elite::model_asset::editor
{
namespace
{
using game::ship::geometry::ObjectAssemblyDesc;

std::string attachmentKindToString(ShipAttachmentKind kind)
{
    switch (kind)
    {
        case ShipAttachmentKind::CameraCockpit: return "camera_cockpit";
        case ShipAttachmentKind::CameraRear: return "camera_rear";
        case ShipAttachmentKind::CameraDrone: return "camera_drone";
        case ShipAttachmentKind::DroneDock: return "drone_dock";
        case ShipAttachmentKind::DroneLaunch: return "drone_launch";
        case ShipAttachmentKind::DroneRecovery: return "drone_recovery";
        case ShipAttachmentKind::RepairWorkPoint: return "repair_work_point";
        case ShipAttachmentKind::EquipmentMount: return "equipment_mount";
        case ShipAttachmentKind::MissileRack: return "missile_rack";
        case ShipAttachmentKind::ContainerMount: return "container_mount";
        case ShipAttachmentKind::WeaponMuzzle: return "weapon_muzzle";
        case ShipAttachmentKind::Generic: return "generic";
    }
    return "generic";
}

std::filesystem::path sourceMeshPath(
    const std::filesystem::path& sourceRoot,
    const std::string& runtimePath
)
{
    std::filesystem::path p(runtimePath);
    if (p.is_absolute()) return p;
    const auto direct = sourceRoot / p;
    if (std::filesystem::exists(direct)) return direct;
    return sourceRoot / "src" / p;
}

void setError(std::string* error, const std::string& text)
{
    if (error) *error = text;
}

void expandBounds(glm::vec3& minB, glm::vec3& maxB, const glm::vec3& p)
{
    minB = glm::min(minB, p);
    maxB = glm::max(maxB, p);
}
}

bool importRuntimeAssembly(
    const std::filesystem::path& sourceRoot,
    ObjectType typeId,
    const std::string& assetId,
    const std::string& displayName,
    ModelAsset& out,
    std::string* error
)
{
    if (error) error->clear();
    try
    {
        game::ship::geometry::ObjectAssemblyRegistry::ensureInitialized();
        const ObjectAssemblyDesc& assembly = game::ship::geometry::ObjectAssemblyRegistry::get(typeId);
        const IObjectDescriptor& descriptor = ObjectDescriptorRegistry::get(typeId);

        ModelAsset asset;
        asset.assetId = assetId;
        asset.displayName = displayName;
        asset.sourceObjectType = static_cast<std::uint16_t>(typeId);
        asset.lodSwitchDistance = assembly.lodSwitchDistance > 0.0f
            ? assembly.lodSwitchDistance : 2500.0f;
        asset.sourceBasis.preset = "game_current";

        std::unordered_map<std::string, std::int32_t> geometryBySource;
        std::unordered_map<std::string, std::int32_t> moduleNodeById;

        for (const auto& module : assembly.modules)
        {
            Node node;
            node.id = module.moduleId;
            node.moduleId = module.moduleId;
            node.localPosition = module.localPosition;
            node.localRotationDeg = module.localRotationDeg;
            node.pivot = module.pivot;
            node.joint.pivot = module.pivot;
            node.joint.type = module.rotates ? JointType::Revolute : JointType::Fixed;
            node.joint.axis = glm::length(module.rotationAxis) > 1.0e-6f
                ? glm::normalize(module.rotationAxis) : glm::vec3(0.0f, 1.0f, 0.0f);
            // Legacy assembly rotationSpeed is radians/second (runtime integrates rotationAngleRad).
            node.joint.defaultRateDegPerSec = glm::degrees(module.rotationSpeed);
            const auto index = static_cast<std::int32_t>(asset.nodes.size());
            moduleNodeById[module.moduleId] = index;
            asset.nodes.push_back(std::move(node));
        }

        glm::vec3 minBounds(std::numeric_limits<float>::max());
        glm::vec3 maxBounds(-std::numeric_limits<float>::max());
        bool haveBounds = false;

        for (std::size_t mi = 0; mi < assembly.modules.size(); ++mi)
        {
            const auto& module = assembly.modules[mi];
            auto& moduleNode = asset.nodes[mi];
            if (!module.parentModuleId.empty())
            {
                const auto parentIt = moduleNodeById.find(module.parentModuleId);
                if (parentIt != moduleNodeById.end()) moduleNode.parentIndex = parentIt->second;
            }

            for (const auto& part : module.meshes)
            {
                const std::string sourceKey = part.lod0Path + "\n" + part.lod1Path;
                std::int32_t geometryIndex = NoIndex;
                const auto existing = geometryBySource.find(sourceKey);
                if (existing != geometryBySource.end())
                {
                    geometryIndex = existing->second;
                }
                else
                {
                    GeometryDefinition geometry;
                    geometry.id = part.meshId;
                    geometry.sourceLod0 = part.lod0Path;
                    geometry.sourceLod1 = part.lod1Path;

                    MeshLod lod0;
                    std::string importError;
                    if (!importObjNative(sourceMeshPath(sourceRoot, part.lod0Path), asset, lod0, &importError))
                        throw std::runtime_error(importError);
                    geometry.lods.push_back(std::move(lod0));

                    if (!part.lod1Path.empty() && part.lod1Path != part.lod0Path)
                    {
                        MeshLod lod1;
                        if (!importObjNative(sourceMeshPath(sourceRoot, part.lod1Path), asset, lod1, &importError))
                            throw std::runtime_error(importError);
                        geometry.lods.push_back(std::move(lod1));
                    }

                    geometryIndex = static_cast<std::int32_t>(asset.geometries.size());
                    geometryBySource[sourceKey] = geometryIndex;
                    asset.geometries.push_back(std::move(geometry));
                }

                Node meshNode;
                meshNode.id = part.meshId;
                meshNode.moduleId = module.moduleId;
                meshNode.parentIndex = static_cast<std::int32_t>(mi);
                meshNode.geometryIndex = geometryIndex;
                meshNode.localPosition = part.localOffset;
                asset.nodes.push_back(std::move(meshNode));

                // Transitional collision seed: one local OBB per mesh part, not
                // one giant module/assembly AABB. The editor can replace these
                // with capsules/spheres/compound shapes before production export.
                const auto& lod = asset.geometries[static_cast<std::size_t>(geometryIndex)].lods.front();
                CollisionVolume collision;
                collision.id = "hit." + part.meshId;
                collision.moduleId = module.moduleId;
                collision.parentNodeIndex = static_cast<std::int32_t>(mi);
                collision.shape = CollisionShape::Box;
                collision.localPosition = part.localOffset + (lod.minBounds + lod.maxBounds) * 0.5f;
                collision.halfSize = glm::max((lod.maxBounds - lod.minBounds) * 0.5f, glm::vec3(0.001f));
                asset.collisionVolumes.push_back(std::move(collision));

                const glm::vec3 localMin = lod.minBounds + module.localPosition + part.localOffset;
                const glm::vec3 localMax = lod.maxBounds + module.localPosition + part.localOffset;
                expandBounds(minBounds, maxBounds, localMin);
                expandBounds(minBounds, maxBounds, localMax);
                haveBounds = true;
            }
        }

        if (const auto* ship = dynamic_cast<const ShipDescriptor*>(&descriptor))
        {
            for (const auto& attachment : ship->attachments)
            {
                Socket socket;
                socket.id = attachment.id;
                socket.kind = attachmentKindToString(attachment.kind);
                socket.moduleId = attachment.parentModuleId;
                const auto parentIt = moduleNodeById.find(attachment.parentModuleId);
                if (parentIt != moduleNodeById.end()) socket.parentNodeIndex = parentIt->second;
                socket.localPosition = attachment.localPosition;
                socket.localRotationDeg = attachment.localRotationDeg;
                socket.enabled = attachment.enabled;
                asset.sockets.push_back(std::move(socket));
            }
        }

        asset.minBounds = haveBounds ? minBounds : glm::vec3(-1.0f);
        asset.maxBounds = haveBounds ? maxBounds : glm::vec3(1.0f);
        out = std::move(asset);
        return true;
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

} // namespace elite::model_asset::editor
