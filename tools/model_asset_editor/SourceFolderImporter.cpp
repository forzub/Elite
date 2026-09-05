#include "tools/model_asset_editor/SourceFolderImporter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "src/model_asset/ModelAssetIdentity.h"
#include "tools/model_asset_editor/NativeObjImporter.h"

namespace elite::model_asset::editor
{
namespace
{

void setError(std::string* error, const std::string& text)
{
    if (error) *error = text;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isObjFile(const std::filesystem::path& path)
{
    return lower(path.extension().string()) == ".obj";
}

bool isLodDirectoryName(const std::string& raw, std::size_t* level = nullptr)
{
    const std::string name = lower(raw);
    if (name.size() <= 3 || name.rfind("lod", 0) != 0)
        return false;
    if (!std::all_of(name.begin() + 3, name.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        }))
        return false;
    if (level)
    {
        try
        {
            *level = static_cast<std::size_t>(std::stoull(name.substr(3)));
        }
        catch (...)
        {
            return false;
        }
    }
    return true;
}

std::string sourcePathFor(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& file)
{
    std::error_code ec;
    const auto relative = std::filesystem::relative(file, sourceRoot, ec);
    if (!ec && !relative.empty())
    {
        const std::string generic = relative.lexically_normal().generic_string();
        if (generic != "." && generic.rfind("../", 0) != 0 && generic != "..")
            return generic;
    }
    return file.lexically_normal().generic_string();
}

std::vector<std::filesystem::path> directObjFiles(
    const std::filesystem::path& directory,
    std::vector<std::string>* warnings)
{
    std::vector<std::filesystem::path> result;
    std::error_code ec;
    std::filesystem::directory_iterator it(
        directory, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        const auto& entry = *it;
        if (!entry.is_regular_file(ec)) continue;
        if (!isObjFile(entry.path())) continue;
        result.push_back(entry.path());
    }
    if (ec && warnings)
        warnings->push_back("cannot fully scan source directory " + directory.generic_string() + ": " + ec.message());

    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        const std::string al = lower(a.filename().string());
        const std::string bl = lower(b.filename().string());
        return al == bl ? a.generic_string() < b.generic_string() : al < bl;
    });
    return result;
}

std::string stripLodPrefix(std::string stem, std::size_t lodIndex)
{
    const std::string lowered = lower(stem);
    const std::string prefix = "lod" + std::to_string(lodIndex);
    if (lowered.rfind(prefix, 0) != 0)
        return stem;
    if (stem.size() == prefix.size())
        return stem;
    const char separator = stem[prefix.size()];
    if (separator != '_' && separator != '-' && separator != '.')
        return stem;
    stem.erase(0, prefix.size() + 1);
    return stem.empty() ? prefix : stem;
}

std::string sourceObjectPreferredId(const std::filesystem::path& file, std::size_t lodIndex)
{
    return stripLodPrefix(file.stem().string(), lodIndex);
}

std::string identityKey(const std::string& id)
{
    return lower(id);
}

void expandBounds(glm::vec3& minB, glm::vec3& maxB, const MeshLod& mesh, bool& haveBounds)
{
    if (mesh.vertices.empty()) return;
    if (!haveBounds)
    {
        minB = mesh.minBounds;
        maxB = mesh.maxBounds;
        haveBounds = true;
        return;
    }
    minB = glm::min(minB, mesh.minBounds);
    maxB = glm::max(maxB, mesh.maxBounds);
}

struct LodFolder
{
    std::size_t level = 0;
    std::filesystem::path path;
};

std::vector<LodFolder> discoverLodFolders(
    const std::filesystem::path& assetRoot,
    std::vector<std::string>* warnings)
{
    std::map<std::size_t, std::filesystem::path> byLevel;
    std::error_code ec;
    std::filesystem::directory_iterator it(
        assetRoot, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        const auto& entry = *it;
        if (!entry.is_directory(ec)) continue;
        std::size_t level = 0;
        if (!isLodDirectoryName(entry.path().filename().string(), &level)) continue;
        const auto [pos, inserted] = byLevel.emplace(level, entry.path());
        if (!inserted && warnings)
            warnings->push_back("duplicate LOD directory level " + std::to_string(level) + ": " + entry.path().generic_string());
    }
    if (ec && warnings)
        warnings->push_back("cannot fully scan asset source directory " + assetRoot.generic_string() + ": " + ec.message());

    // A dedicated flat source directory is also accepted as LOD0. This keeps
    // simple one-mesh tools compatible without changing the modern LOD layout.
    if (byLevel.empty())
    {
        const auto direct = directObjFiles(assetRoot, warnings);
        if (!direct.empty()) byLevel.emplace(0, assetRoot);
    }

    std::vector<LodFolder> result;
    result.reserve(byLevel.size());
    for (const auto& [level, path] : byLevel)
        result.push_back({level, path});
    return result;
}

std::filesystem::path lodFolderFor(
    const std::filesystem::path& assetRoot,
    std::size_t lodIndex)
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(
             assetRoot, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (ec) break;
        if (!entry.is_directory(ec)) continue;
        std::size_t level = 0;
        if (isLodDirectoryName(entry.path().filename().string(), &level) && level == lodIndex)
            return entry.path();
    }
    if (lodIndex == 0 && directObjFiles(assetRoot, nullptr).size() > 0)
        return assetRoot;
    return {};
}

}

std::filesystem::path resolveSourceFolderAssetRoot(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory)
{
    if (relativeDirectory.empty()) return {};
    if (relativeDirectory.is_absolute()) return relativeDirectory.lexically_normal();

    const std::vector<std::filesystem::path> candidates {
        sourceRoot / relativeDirectory,
        sourceRoot / "assets" / "models" / relativeDirectory,
        sourceRoot / "src" / "assets" / "models" / relativeDirectory
    };
    std::error_code ec;
    for (const auto& candidate : candidates)
    {
        ec.clear();
        if (std::filesystem::is_directory(candidate, ec) && !ec)
            return candidate.lexically_normal();
    }

    // If settings point directly at one asset directory, accept it only when
    // the last component matches the configured asset directory name.
    if (!sourceRoot.empty() &&
        lower(sourceRoot.filename().string()) == lower(relativeDirectory.filename().string()))
    {
        ec.clear();
        if (std::filesystem::is_directory(sourceRoot, ec) && !ec)
            return sourceRoot.lexically_normal();
    }
    return {};
}

bool sourceFolderAssetAvailable(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory)
{
    const auto root = resolveSourceFolderAssetRoot(sourceRoot, relativeDirectory);
    if (root.empty()) return false;
    const auto lods = discoverLodFolders(root, nullptr);
    return !lods.empty() && lods.front().level == 0 && !directObjFiles(lods.front().path, nullptr).empty();
}

bool importSourceFolderAsset(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    ObjectType typeId,
    const std::string& assetId,
    const std::string& displayName,
    ModelAsset& out,
    std::string* error,
    std::string* warning,
    ImportProgressCallback progress)
{
    if (error) error->clear();
    if (warning) warning->clear();
    try
    {
        const auto assetRoot = resolveSourceFolderAssetRoot(sourceRoot, relativeDirectory);
        if (assetRoot.empty())
            throw std::runtime_error("source asset directory not found: " + relativeDirectory.generic_string());

        std::vector<std::string> warnings;
        const auto lodFolders = discoverLodFolders(assetRoot, &warnings);
        if (lodFolders.empty() || lodFolders.front().level != 0)
            throw std::runtime_error("source asset requires LOD0 (or a dedicated flat LOD0 directory): " + assetRoot.generic_string());
        for (std::size_t i = 0; i < lodFolders.size(); ++i)
        {
            if (lodFolders[i].level != i)
                throw std::runtime_error(
                    "source LOD directories must be contiguous from LOD0; expected LOD" +
                    std::to_string(i) + " but found LOD" + std::to_string(lodFolders[i].level));
        }

        std::vector<std::vector<std::filesystem::path>> ordinaryFiles(lodFolders.size());
        std::size_t totalObjLoads = 0;
        for (std::size_t i = 0; i < lodFolders.size(); ++i)
        {
            ordinaryFiles[i] = directObjFiles(lodFolders[i].path, &warnings);
            if (ordinaryFiles[i].empty())
                throw std::runtime_error("LOD" + std::to_string(i) + " contains no ordinary OBJ files: " + lodFolders[i].path.generic_string());
            totalObjLoads += ordinaryFiles[i].size();
        }

        ModelAsset asset;
        asset.formatVersion = ModelAssetFormatVersion;
        asset.assetId = assetId;
        asset.displayName = displayName;
        asset.sourceObjectType = static_cast<std::uint16_t>(typeId);
        asset.sourceBasis.preset = "game_current";

        std::size_t completed = 0;
        const auto report = [&](const std::string& stage, const std::filesystem::path& path = {})
        {
            if (progress) progress(ImportProgress{stage, completed, totalObjLoads, path});
        };
        report("SOURCE FOLDER");

        std::set<std::string> semanticIds;
        std::unordered_map<std::string, std::int32_t> semanticByKey;
        for (std::size_t lodIndex = 0; lodIndex < ordinaryFiles.size(); ++lodIndex)
        {
            RenderLod lod;
            lod.level = static_cast<std::uint32_t>(lodIndex);
            lod.sourceKind = "source";
            lod.relativeGeometricError = lodIndex == 0 ? 0.0f : -1.0f;
            bool haveLodBounds = false;
            std::set<std::string> geometryIds;
            std::set<std::string> renderNodeIds;

            for (const auto& file : ordinaryFiles[lodIndex])
            {
                const std::string preferred = sourceObjectPreferredId(file, lodIndex);
                std::string visualId = allocateStableId(preferred, "mesh", geometryIds);

                MeshLod mesh;
                std::string importError;
                report("READ / PARSE / TOPOLOGY", file);
                if (!importObjNative(file, asset, mesh, &importError))
                    throw std::runtime_error(importError);
                ++completed;
                report("ASSEMBLE", file);

                std::int32_t semanticNodeIndex = NoIndex;
                if (lodIndex == 0)
                {
                    Node semantic;
                    semantic.id = allocateStableId(visualId, "part", semanticIds);
                    semanticIds.insert(semantic.id);
                    semantic.moduleId = semantic.id;
                    semanticNodeIndex = static_cast<std::int32_t>(asset.nodes.size());
                    semanticByKey.emplace(identityKey(semantic.id), semanticNodeIndex);
                    asset.nodes.push_back(std::move(semantic));
                    visualId = asset.nodes.back().id;

                    CollisionVolume collision;
                    collision.id = "hit." + visualId;
                    collision.moduleId = visualId;
                    collision.parentNodeIndex = semanticNodeIndex;
                    collision.shape = CollisionShape::Box;
                    collision.localPosition = (mesh.minBounds + mesh.maxBounds) * 0.5f;
                    collision.halfSize = glm::max((mesh.maxBounds - mesh.minBounds) * 0.5f, glm::vec3(0.001f));
                    asset.collisionVolumes.push_back(std::move(collision));
                }
                else
                {
                    const auto semanticIt = semanticByKey.find(identityKey(visualId));
                    if (semanticIt != semanticByKey.end())
                    {
                        semanticNodeIndex = semanticIt->second;
                        visualId = asset.nodes[static_cast<std::size_t>(semanticNodeIndex)].id;
                    }
                }

                RenderGeometryDefinition geometry;
                geometry.id = allocateStableId(visualId, "geometry", geometryIds);
                geometryIds.insert(geometry.id);
                geometry.sourcePath = sourcePathFor(sourceRoot, file);
                geometry.mesh = std::move(mesh);
                const auto geometryIndex = static_cast<std::int32_t>(lod.geometries.size());
                expandBounds(lod.minBounds, lod.maxBounds, geometry.mesh, haveLodBounds);
                lod.geometries.push_back(std::move(geometry));

                RenderNode renderNode;
                renderNode.id = allocateStableId(visualId, "render_node", renderNodeIds);
                renderNodeIds.insert(renderNode.id);
                renderNode.geometryIndex = geometryIndex;
                renderNode.semanticNodeIndex = semanticNodeIndex;
                // Shared-source-origin contract: OBJ vertex coordinates are
                // already authored in one common assembly frame.
                renderNode.localPosition = glm::vec3(0.0f);
                renderNode.localRotationDeg = glm::vec3(0.0f);
                renderNode.pivot = glm::vec3(0.0f);
                lod.nodes.push_back(std::move(renderNode));
            }

            lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
            lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
            asset.renderLods.push_back(std::move(lod));
        }

        asset.minBounds = asset.renderLods.empty() ? glm::vec3(-1.0f) : asset.renderLods.front().minBounds;
        asset.maxBounds = asset.renderLods.empty() ? glm::vec3(1.0f) : asset.renderLods.front().maxBounds;
        report("SOURCE FOLDER");
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

std::vector<SourceFolderMesh> discoverSourceFolderOrdinaryMeshes(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    std::vector<std::string>* warnings)
{
    std::vector<SourceFolderMesh> result;
    const auto assetRoot = resolveSourceFolderAssetRoot(sourceRoot, relativeDirectory);
    if (assetRoot.empty())
    {
        if (warnings) warnings->push_back("source asset directory not found: " + relativeDirectory.generic_string());
        return result;
    }

    const auto lodFolders = discoverLodFolders(assetRoot, warnings);
    for (const auto& lod : lodFolders)
    {
        const auto files = directObjFiles(lod.path, warnings);
        for (const auto& file : files)
            result.push_back({lod.level, file, sourcePathFor(sourceRoot, file)});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.lodIndex != b.lodIndex) return a.lodIndex < b.lodIndex;
        return lower(a.sourcePath) < lower(b.sourcePath);
    });
    return result;
}

std::vector<SourceFolderVariant> discoverSourceFolderVariants(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& relativeDirectory,
    std::size_t lodIndex,
    std::vector<std::string>* warnings)
{
    std::vector<SourceFolderVariant> result;
    const auto assetRoot = resolveSourceFolderAssetRoot(sourceRoot, relativeDirectory);
    if (assetRoot.empty())
    {
        if (warnings) warnings->push_back("source asset directory not found: " + relativeDirectory.generic_string());
        return result;
    }
    const auto lodRoot = lodFolderFor(assetRoot, lodIndex);
    if (lodRoot.empty()) return result;

    const auto variantsRoot = lodRoot / "variants";
    std::error_code ec;
    if (!std::filesystem::is_directory(variantsRoot, ec) || ec)
        return result;

    std::filesystem::recursive_directory_iterator it(
        variantsRoot, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        const auto& entry = *it;
        if (!entry.is_regular_file(ec) || !isObjFile(entry.path())) continue;
        result.push_back({entry.path(), sourcePathFor(sourceRoot, entry.path())});
    }
    if (ec && warnings)
        warnings->push_back("cannot fully scan variants directory " + variantsRoot.generic_string() + ": " + ec.message());

    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return lower(a.sourcePath) < lower(b.sourcePath);
    });
    return result;
}

} // namespace elite::model_asset::editor
