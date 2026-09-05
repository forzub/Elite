#include "src/model_asset/ModelAssetMigration.h"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

#include "src/model_asset/ModelAssetIdentity.h"

namespace elite::model_asset
{
namespace
{
void expandBounds(RenderLod& lod, const MeshLod& mesh, bool& haveBounds)
{
    if (mesh.vertices.empty())
        return;
    if (!haveBounds)
    {
        lod.minBounds = mesh.minBounds;
        lod.maxBounds = mesh.maxBounds;
        haveBounds = true;
        return;
    }
    lod.minBounds = glm::min(lod.minBounds, mesh.minBounds);
    lod.maxBounds = glm::max(lod.maxBounds, mesh.maxBounds);
}
}

std::size_t legacyRenderLodCount(const ModelAsset& asset)
{
    std::size_t count = 0;
    for (const auto& geometry : asset.geometries)
        count = std::max(count, geometry.lods.size());
    return count;
}

void buildIndependentRenderLodsFromLegacy(ModelAsset& asset)
{
    if (!asset.renderLods.empty())
        return;

    // Legacy v2/v3 did not enforce global semantic-node ID uniqueness. Repair
    // that deterministically before the IDs become RenderNode IDs in v4. All
    // semantic references are index-based, so this normalization is safe.
    std::set<std::string> semanticNodeIds;
    for (auto& node : asset.nodes)
    {
        node.id = allocateStableId(node.id, "node", semanticNodeIds);
        semanticNodeIds.insert(node.id);
    }

    const std::size_t count = legacyRenderLodCount(asset);
    asset.renderLods.reserve(count);
    for (std::size_t lodIndex = 0; lodIndex < count; ++lodIndex)
    {
        RenderLod renderLod;
        renderLod.level = static_cast<std::uint32_t>(lodIndex);
        renderLod.sourceKind = "source";
        renderLod.relativeGeometricError = lodIndex == 0 ? 0.0f : -1.0f;

        std::vector<std::int32_t> geometryMap(asset.geometries.size(), NoIndex);
        std::set<std::string> renderGeometryIds;
        bool haveBounds = false;
        for (std::size_t geometryIndex = 0; geometryIndex < asset.geometries.size(); ++geometryIndex)
        {
            const auto& legacy = asset.geometries[geometryIndex];
            if (lodIndex >= legacy.lods.size())
                continue;

            RenderGeometryDefinition geometry;
            geometry.id = allocateStableId(legacy.id, "geometry", renderGeometryIds);
            renderGeometryIds.insert(geometry.id);
            geometry.surfaceMode = legacy.surfaceMode;
            if (lodIndex < legacy.sourceLods.size())
                geometry.sourcePath = legacy.sourceLods[lodIndex];
            geometry.mesh = legacy.lods[lodIndex];
            expandBounds(renderLod, geometry.mesh, haveBounds);
            geometryMap[geometryIndex] = static_cast<std::int32_t>(renderLod.geometries.size());
            renderLod.geometries.push_back(std::move(geometry));
        }

        // Preserve the old visual hierarchy as this LOD's initial render graph.
        // It is now free to diverge completely from other LODs after migration.
        renderLod.nodes.reserve(asset.nodes.size());
        for (std::size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
        {
            const auto& semantic = asset.nodes[nodeIndex];
            RenderNode render;
            render.id = semantic.id;
            render.parentIndex = semantic.parentIndex;
            render.semanticNodeIndex = static_cast<std::int32_t>(nodeIndex);
            if (semantic.geometryIndex >= 0 &&
                static_cast<std::size_t>(semantic.geometryIndex) < geometryMap.size())
            {
                render.geometryIndex = geometryMap[static_cast<std::size_t>(semantic.geometryIndex)];
            }
            render.localPosition = semantic.localPosition;
            render.localRotationDeg = semantic.localRotationDeg;
            render.pivot = semantic.pivot;
            render.enabled = semantic.enabled;
            renderLod.nodes.push_back(std::move(render));
        }
        asset.renderLods.push_back(std::move(renderLod));
    }

    // In v4 geometryIndex is no longer a semantic property. Keep legacy
    // geometry arrays resident only so old editor/import code can still inspect
    // or diagnose the migration until those paths are retired.
    for (auto& node : asset.nodes)
        node.geometryIndex = NoIndex;
}

} // namespace elite::model_asset
