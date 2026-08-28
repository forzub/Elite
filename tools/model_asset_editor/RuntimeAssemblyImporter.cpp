#include "tools/model_asset_editor/RuntimeAssemblyImporter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "src/model_asset/ModelAssetIdentity.h"
#include "src/model_asset/ModelAssetVariantNaming.h"
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

    const auto fromProjectRoot = sourceRoot / "src" / p;
    if (std::filesystem::exists(fromProjectRoot)) return fromProjectRoot;

    const std::string generic = p.generic_string();
    constexpr const char* AssetsPrefix = "assets/";
    constexpr const char* ModelsPrefix = "assets/models/";
    if (generic.rfind(ModelsPrefix, 0) == 0)
    {
        const auto fromModelsRoot = sourceRoot / generic.substr(std::char_traits<char>::length(ModelsPrefix));
        if (std::filesystem::exists(fromModelsRoot)) return fromModelsRoot;
    }
    if (generic.rfind(AssetsPrefix, 0) == 0)
    {
        const auto fromAssetsRoot = sourceRoot / generic.substr(std::char_traits<char>::length(AssetsPrefix));
        if (std::filesystem::exists(fromAssetsRoot)) return fromAssetsRoot;
    }
    return direct;
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

bool isLodDirectoryName(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (name.size() <= 3 || name.rfind("lod", 0) != 0)
        return false;
    return std::all_of(name.begin() + 3, name.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

std::string sourcePathKey(const std::filesystem::path& path)
{
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (ec) normalized = std::filesystem::absolute(path, ec).lexically_normal();
    std::string key = normalized.generic_string();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
#endif
    return key;
}

std::filesystem::path lodRootForSourceFile(const std::filesystem::path& sourceFile)
{
    auto directory = sourceFile.parent_path();
    while (!directory.empty())
    {
        if (isLodDirectoryName(directory.filename().string()))
            return directory;
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    return {};
}

std::filesystem::path runtimeLodRootForPath(const std::string& runtimePath)
{
    if (runtimePath.empty()) return {};
    const std::filesystem::path path(runtimePath);
    if (path.is_absolute()) return {};
    std::filesystem::path prefix;
    for (const auto& component : path)
    {
        prefix /= component;
        if (isLodDirectoryName(component.string()))
            return prefix;
    }
    return {};
}

std::string additionalMeshRuntimePath(
    const std::filesystem::path& lodRoot,
    const std::filesystem::path& file,
    const std::string& representativeRuntimePath)
{
    const std::filesystem::path runtime(representativeRuntimePath);
    if (runtime.is_absolute())
        return file.generic_string();

    const auto runtimeRoot = runtimeLodRootForPath(representativeRuntimePath);
    if (runtimeRoot.empty())
        return file.generic_string();

    std::error_code ec;
    const auto relative = std::filesystem::relative(file, lodRoot, ec);
    if (ec || relative.empty())
        return file.generic_string();
    return (runtimeRoot / relative).lexically_normal().generic_string();
}

}

std::vector<std::string> runtimeAssemblyLodSourcePaths(
    ObjectType typeId,
    std::size_t lodIndex)
{
    game::ship::geometry::ObjectAssemblyRegistry::ensureInitialized();
    const ObjectAssemblyDesc& assembly = game::ship::geometry::ObjectAssemblyRegistry::get(typeId);
    std::set<std::string> paths;
    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            const std::string path = lodIndex == 0 ? part.lod0Path :
                (lodIndex == 1 ? part.lod1Path : std::string());
            if (path.empty()) continue;
            if (lodIndex == 1 && path == part.lod0Path) continue;
            paths.insert(path);
        }
    }
    return std::vector<std::string>(paths.begin(), paths.end());
}

std::vector<SourceAdditionalMesh> discoverAdditionalLodMeshes(
    const std::filesystem::path& sourceRoot,
    const std::vector<std::string>& knownRuntimePaths,
    std::vector<std::string>* warnings)
{
    std::set<std::string> knownFiles;
    struct ScanRoot
    {
        std::filesystem::path path;
        std::string representativeRuntimePath;
    };
    std::map<std::string, ScanRoot> roots;

    for (const auto& runtimePath : knownRuntimePaths)
    {
        if (runtimePath.empty()) continue;
        const auto sourceFile = sourceMeshPath(sourceRoot, runtimePath);
        knownFiles.insert(sourcePathKey(sourceFile));
        const auto lodRoot = lodRootForSourceFile(sourceFile);
        if (lodRoot.empty())
        {
            if (warnings)
                warnings->push_back(
                    "cannot discover additional meshes for source outside a LOD<N> directory: " +
                    sourceFile.generic_string());
            continue;
        }
        roots.emplace(sourcePathKey(lodRoot), ScanRoot{lodRoot, runtimePath});
    }

    std::map<std::string, SourceAdditionalMesh> byPath;
    for (const auto& [rootKey, root] : roots)
    {
        (void)rootKey;
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(
            root.path, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec))
        {
            const auto& entry = *it;
            if (!entry.is_regular_file(ec)) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (ext != ".obj") continue;

            const auto fileKey = sourcePathKey(entry.path());
            if (knownFiles.find(fileKey) != knownFiles.end())
                continue;

            SourceAdditionalMesh candidate;
            candidate.file = entry.path();
            candidate.runtimePath = additionalMeshRuntimePath(
                root.path, entry.path(), root.representativeRuntimePath);
            byPath.emplace(fileKey, std::move(candidate));
        }
        if (ec && warnings)
            warnings->push_back(
                "cannot fully scan LOD directory tree " + root.path.generic_string() + ": " +
                ec.message());
    }

    std::vector<SourceAdditionalMesh> result;
    result.reserve(byPath.size());
    for (auto& [pathKey, mesh] : byPath)
    {
        (void)pathKey;
        result.push_back(std::move(mesh));
    }
    return result;
}

bool importRuntimeAssembly(
    const std::filesystem::path& sourceRoot,
    ObjectType typeId,
    const std::string& assetId,
    const std::string& displayName,
    ModelAsset& out,
    std::string* error,
    std::string* warning,
    ImportProgressCallback progress
)
{
    if (error) error->clear();
    if (warning) warning->clear();
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
        std::set<std::string> semanticNodeIds;
        std::vector<std::string> warnings;

        std::unordered_set<std::string> countedGeometrySources;
        std::size_t totalObjLoads = 0;
        for (const auto& module : assembly.modules)
        {
            for (const auto& part : module.meshes)
            {
                const std::string sourceKey = part.lod0Path + "\n" + part.lod1Path;
                if (!countedGeometrySources.insert(sourceKey).second)
                    continue;
                if (!part.lod0Path.empty()) ++totalObjLoads;
                if (!part.lod1Path.empty() && part.lod1Path != part.lod0Path) ++totalObjLoads;
            }
        }

        // Additional sibling OBJ files are intentionally not imported here.
        // Their authoring identity is created by the editor session, where it can
        // be persisted independently of filenames and independently for every LOD.
        std::size_t completedObjLoads = 0;
        auto report = [&](
            const std::string& stage,
            const std::filesystem::path& path = std::filesystem::path())
        {
            if (progress)
                progress(ImportProgress{stage, completedObjLoads, totalObjLoads, path});
        };
        report("ASSEMBLY");

        for (const auto& module : assembly.modules)
        {
            if (module.moduleId.empty())
                throw std::runtime_error("source assembly contains a module with empty moduleId");
            if (moduleNodeById.find(module.moduleId) != moduleNodeById.end())
                throw std::runtime_error("source assembly contains duplicate moduleId: " + module.moduleId);

            Node node;
            node.id = allocateStableId(module.moduleId, "module", semanticNodeIds);
            semanticNodeIds.insert(node.id);
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
            moduleNodeById.emplace(module.moduleId, index);
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
                    if (geometry.id.empty()) geometry.id = "geometry";
                    const std::string baseGeometryId = geometry.id;
                    for (std::size_t suffix = 2; std::any_of(
                             asset.geometries.begin(), asset.geometries.end(),
                             [&](const GeometryDefinition& existingGeometry) { return existingGeometry.id == geometry.id; }); ++suffix)
                        geometry.id = baseGeometryId + "_" + std::to_string(suffix);
                    geometry.sourceLods.push_back(part.lod0Path);

                    MeshLod lod0;
                    std::string importError;
                    const auto lod0Path = sourceMeshPath(sourceRoot, part.lod0Path);
                    report("READ / PARSE / TOPOLOGY", lod0Path);
                    if (!importObjNative(lod0Path, asset, lod0, &importError))
                        throw std::runtime_error(importError);
                    ++completedObjLoads;
                    report("ASSEMBLE", lod0Path);
                    geometry.lods.push_back(std::move(lod0));

                    if (!part.lod1Path.empty() && part.lod1Path != part.lod0Path)
                    {
                        MeshLod lod1;
                        const auto lod1Path = sourceMeshPath(sourceRoot, part.lod1Path);
                        report("READ / PARSE / TOPOLOGY", lod1Path);
                        const bool lod1Ok = importObjNative(
                            lod1Path, asset, lod1, &importError);
                        ++completedObjLoads;
                        report("ASSEMBLE", lod1Path);
                        if (!lod1Ok)
                        {
                            // The editor is a repair tool. A broken optional LOD1
                            // must not make the whole assembly impossible to open;
                            // keep the valid LOD0 and surface the exact diagnostic.
                            warnings.push_back(part.meshId + " LOD1 skipped: " + importError);
                        }
                        else
                        {
                            geometry.lods.push_back(std::move(lod1));
                            geometry.sourceLods.push_back(part.lod1Path);
                        }
                    }

                    geometryIndex = static_cast<std::int32_t>(asset.geometries.size());
                    geometryBySource[sourceKey] = geometryIndex;
                    asset.geometries.push_back(std::move(geometry));


                }

                Node meshNode;
                meshNode.id = allocateChildStableId(
                    module.moduleId, part.meshId, "mesh", semanticNodeIds);
                semanticNodeIds.insert(meshNode.id);
                const std::string meshNodeId = meshNode.id;
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
                collision.id = "hit." + meshNodeId;
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
        completedObjLoads = totalObjLoads;
        report("ASSEMBLY");
        out = std::move(asset);
        if (warning && !warnings.empty())
        {
            std::ostringstream text;
            for (std::size_t i = 0; i < warnings.size(); ++i)
            {
                if (i) text << " | ";
                text << warnings[i];
            }
            *warning = text.str();
        }
        return true;
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

} // namespace elite::model_asset::editor
