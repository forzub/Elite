#include "src/model_asset/binary/ModelAssetBinaryLodIO.h"
#include "src/model_asset/binary/ModelAssetBinaryMeshCodec.h"
#include "src/model_asset/binary/ModelAssetBinaryValidation.h"
#include "src/model_asset/binary/ModelAssetBinaryWire.h"

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace elite::model_asset::binary
{
namespace
{
constexpr std::array<char, 8> MeshMagicV4 {{'E','L','M','S','H','0','0','4'}};
constexpr std::array<char, 8> MeshMagicV2 {{'E','L','M','S','H','0','0','2'}};
constexpr std::uint32_t MeshPayloadFormatVersion = 4;

bool readLegacyLodPayloadV3(
    Reader& r,
    ModelAsset& asset,
    std::size_t expectedLodIndex,
    std::string* error)
{
    std::uint32_t version = 0, lodIndex = 0, entryCount = 0;
    r.pod(version); r.pod(lodIndex);
    if (!r.count(entryCount) || version != 2u || lodIndex != expectedLodIndex)
    {
        setError(error, "unsupported or mismatched legacy LOD payload");
        return false;
    }
    std::map<std::string, std::size_t> geometryById;
    for (std::size_t i = 0; i < asset.geometries.size(); ++i)
        geometryById.emplace(asset.geometries[i].id, i);
    for (std::uint32_t entry = 0; entry < entryCount; ++entry)
    {
        std::string geometryId; r.string(geometryId);
        const auto it = geometryById.find(geometryId);
        if (!r.ok || it == geometryById.end())
        {
            setError(error, "legacy LOD payload contains unknown geometry id: " + geometryId);
            return false;
        }
        auto& geometry = asset.geometries[it->second];
        if (expectedLodIndex >= geometry.lods.size())
        {
            setError(error, "legacy LOD representation not declared for: " + geometryId);
            return false;
        }
        readMeshLod(r, geometry.lods[expectedLodIndex]);
        if (!r.ok) { setError(error, "corrupt legacy LOD payload"); return false; }
    }
    return true;
}
}

bool writeLodPayload(
    const std::filesystem::path& path,
    const ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    if (lodIndex >= asset.renderLods.size())
    {
        setError(error, "render LOD is not declared: " + std::to_string(lodIndex));
        return false;
    }
    const auto& lod = asset.renderLods[lodIndex];
    if (!validateRenderLod(lod, asset.nodes.size(), error)) return false;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        setError(error, "cannot open LOD payload: " + path.string());
        return false;
    }

    file.write(MeshMagicV4.data(), static_cast<std::streamsize>(MeshMagicV4.size()));
    Writer w {file};
    w.pod(MeshPayloadFormatVersion);
    w.pod(static_cast<std::uint32_t>(lodIndex));
    w.string(lod.sourceKind); w.pod(lod.generatedFromLod);
    w.vec3(lod.minBounds); w.vec3(lod.maxBounds);

    w.pod(static_cast<std::uint32_t>(lod.geometries.size()));
    for (const auto& geometry : lod.geometries)
    {
        w.string(geometry.id); w.string(geometry.sourcePath);
        w.pod(static_cast<std::uint8_t>(geometry.surfaceMode));
        writeMeshLod(w, geometry.mesh);
    }

    w.pod(static_cast<std::uint32_t>(lod.nodes.size()));
    for (const auto& node : lod.nodes)
    {
        w.string(node.id); w.pod(node.parentIndex); w.pod(node.geometryIndex); w.pod(node.semanticNodeIndex);
        writeStrings(w, node.activeStates);
        w.vec3(node.localPosition); w.vec3(node.localRotationDeg); w.vec3(node.pivot);
        w.pod(static_cast<std::uint8_t>(node.enabled ? 1 : 0));
    }
    if (!w.ok)
    {
        setError(error, "failed writing LOD payload: " + path.string());
        return false;
    }
    return true;
}

bool readLodPayload(
    const std::filesystem::path& path,
    ModelAsset& asset,
    std::size_t expectedLodIndex,
    std::string* error)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        setError(error, "missing LOD payload: " + path.string());
        return false;
    }
    const auto endPosition = file.tellg();
    if (endPosition < static_cast<std::streamoff>(8))
    {
        setError(error, "invalid LOD payload header: " + path.string());
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(endPosition);
    if (fileSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        fileSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
    {
        setError(error, "LOD payload is too large to map into editor memory: " + path.string());
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
        setError(error, "failed reading LOD payload: " + path.string());
        return false;
    }

    std::array<char, 8> magic {};
    std::memcpy(magic.data(), bytes.data(), magic.size());
    Reader r(bytes.data() + magic.size(), bytes.size() - magic.size());
    if (magic == MeshMagicV2)
        return readLegacyLodPayloadV3(r, asset, expectedLodIndex, error);
    if (magic != MeshMagicV4)
    {
        setError(error, "invalid LOD payload magic: " + path.string());
        return false;
    }

    std::uint32_t version = 0, lodIndex = 0;
    r.pod(version); r.pod(lodIndex);
    if (!r.ok || version != MeshPayloadFormatVersion || lodIndex != expectedLodIndex)
    {
        setError(error, "unsupported or mismatched LOD payload: " + path.string());
        return false;
    }
    if (expectedLodIndex >= asset.renderLods.size())
    {
        setError(error, "LOD payload is not declared by v4 manifest");
        return false;
    }

    const float manifestRelativeGeometricError = asset.renderLods[expectedLodIndex].relativeGeometricError;
    RenderLod loaded;
    loaded.level = lodIndex;
    loaded.relativeGeometricError = manifestRelativeGeometricError;
    r.string(loaded.sourceKind); r.pod(loaded.generatedFromLod);
    r.vec3(loaded.minBounds); r.vec3(loaded.maxBounds);
    std::uint32_t geometryCount = 0; if (!r.count(geometryCount)) return false;
    loaded.geometries.resize(geometryCount);
    for (auto& geometry : loaded.geometries)
    {
        r.string(geometry.id); r.string(geometry.sourcePath);
        std::uint8_t surface = 0; r.pod(surface); geometry.surfaceMode = static_cast<SurfaceMode>(surface);
        readMeshLod(r, geometry.mesh);
    }
    std::uint32_t nodeCount = 0; if (!r.count(nodeCount)) return false;
    loaded.nodes.resize(nodeCount);
    for (auto& node : loaded.nodes)
    {
        r.string(node.id); r.pod(node.parentIndex); r.pod(node.geometryIndex); r.pod(node.semanticNodeIndex);
        readStrings(r, node.activeStates);
        r.vec3(node.localPosition); r.vec3(node.localRotationDeg); r.vec3(node.pivot);
        std::uint8_t enabled = 0; r.pod(enabled); node.enabled = enabled != 0;
    }
    if (!r.ok || !validateRenderLod(loaded, asset.nodes.size(), error))
    {
        if (r.ok && error && error->empty()) *error = "invalid render LOD graph";
        else if (!r.ok) setError(error, "corrupt LOD payload: " + path.string());
        return false;
    }
    loaded.declaredGeometryCount = static_cast<std::uint32_t>(loaded.geometries.size());
    loaded.declaredNodeCount = static_cast<std::uint32_t>(loaded.nodes.size());
    asset.renderLods[expectedLodIndex] = std::move(loaded);
    return true;
}

} // namespace elite::model_asset::binary
