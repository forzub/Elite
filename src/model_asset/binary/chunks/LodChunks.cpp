#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

void writeLodManifestV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.renderLods.size()));
    for (const auto& lod : a.renderLods)
    {
        w.pod(lod.level); w.string(lod.sourceKind); w.pod(lod.generatedFromLod);
        w.vec3(lod.minBounds); w.vec3(lod.maxBounds);
        const auto geometryCount = !lod.geometries.empty()
            ? static_cast<std::uint32_t>(lod.geometries.size())
            : lod.declaredGeometryCount;
        const auto nodeCount = !lod.nodes.empty()
            ? static_cast<std::uint32_t>(lod.nodes.size())
            : lod.declaredNodeCount;
        w.pod(geometryCount);
        w.pod(nodeCount);
    }
}

void readLodManifestV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0; if (!r.count(count)) return; a.renderLods.resize(count);
    for (auto& lod : a.renderLods)
    {
        std::uint32_t geometryCount = 0, nodeCount = 0;
        r.pod(lod.level); r.string(lod.sourceKind); r.pod(lod.generatedFromLod);
        r.vec3(lod.minBounds); r.vec3(lod.maxBounds); r.pod(geometryCount); r.pod(nodeCount);
        lod.declaredGeometryCount = geometryCount;
        lod.declaredNodeCount = nodeCount;
    }
}

void writeLodScreenErrorV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.renderLods.size()));
    for (const auto& lod : a.renderLods)
    {
        w.pod(lod.level);
        const float value = lod.level == 0 ? 0.0f : lod.relativeGeometricError;
        w.pod(value);
    }
}

void readLodScreenErrorV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint32_t level = 0;
        float value = -1.0f;
        r.pod(level); r.pod(value);
        if (!r.ok) return;
        if (level < a.renderLods.size())
            a.renderLods[level].relativeGeometricError = level == 0 ? 0.0f : value;
    }
}

} // namespace elite::model_asset::binary
