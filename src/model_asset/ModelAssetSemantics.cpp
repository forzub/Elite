#include "src/model_asset/ModelAssetSemantics.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace elite::model_asset
{
namespace
{

template <typename T, typename Predicate>
std::size_t eraseCount(std::vector<T>& values, Predicate predicate)
{
    const auto before = values.size();
    values.erase(std::remove_if(values.begin(), values.end(), predicate), values.end());
    return before - values.size();
}

void remapNodeIndex(std::int32_t& value, std::size_t removed)
{
    const auto removedIndex = static_cast<std::int32_t>(removed);
    if (value == removedIndex) value = NoIndex;
    else if (value > removedIndex) --value;
}

bool nearZero(const glm::vec3& value)
{
    constexpr float Epsilon = 1.0e-6f;
    return std::abs(value.x) <= Epsilon && std::abs(value.y) <= Epsilon &&
        std::abs(value.z) <= Epsilon;
}

bool hasLegacyVisualBootstrapPattern(const ModelAsset& asset, std::size_t nodeIndex)
{
    if (nodeIndex >= asset.nodes.size()) return false;
    const auto& node = asset.nodes[nodeIndex];
    if (node.parentIndex < 0 || static_cast<std::size_t>(node.parentIndex) >= asset.nodes.size()) return false;
    const auto parentIndex = static_cast<std::size_t>(node.parentIndex);
    if (parentIndex >= nodeIndex) return false; // legacy importer always emitted module before visual child
    const auto& parent = asset.nodes[parentIndex];
    if (node.moduleId.empty() || node.moduleId != parent.moduleId) return false;
    if (node.defaultStateId != "intact" || node.joint.type != JointType::Fixed || node.joint.breakable ||
        std::abs(node.joint.defaultRateDegPerSec) > 1.0e-6f || !nearZero(node.localRotationDeg) ||
        !nearZero(node.pivot) || !nearZero(node.joint.pivot))
        return false;

    const auto usage = inspectSemanticNodeUsage(asset, nodeIndex);
    if (usage.renderBindings == 0 || usage.hasRuntimePayload()) return false;

    bool geometryBindingWithLegacyIdentity = false;
    bool parentTransformOnlyBinding = false;
    for (const auto& lod : asset.renderLods)
    {
        for (const auto& renderNode : lod.nodes)
        {
            if (renderNode.semanticNodeIndex == static_cast<std::int32_t>(nodeIndex) &&
                renderNode.geometryIndex >= 0 && renderNode.id == node.id)
                geometryBindingWithLegacyIdentity = true;
            if (renderNode.semanticNodeIndex == static_cast<std::int32_t>(parentIndex) &&
                renderNode.geometryIndex < 0 && renderNode.id == parent.id)
                parentTransformOnlyBinding = true;
        }
    }
    return geometryBindingWithLegacyIdentity && parentTransformOnlyBinding;
}

} // namespace

bool SemanticNodeUsage::hasRuntimePayload() const
{
    return children > 0 || collisions > legacySourceBootstrapCollisions || sockets > 0 ||
        stateVariants > 0 || hitRegions > 0 || openings > 0 || repairTargets > 0 || structuralLinks > 0 || physicsEnabled;
}

bool SemanticNodeUsage::isOrphanCandidate() const
{
    return renderBindings == 0 && !hasRuntimePayload();
}

bool isLegacySourceBootstrapCollision(
    const ModelAsset& asset,
    std::size_t nodeIndex,
    const CollisionVolume& collision)
{
    if (nodeIndex >= asset.nodes.size()) return false;
    const auto& node = asset.nodes[nodeIndex];
    return collision.parentNodeIndex == static_cast<std::int32_t>(nodeIndex) &&
        collision.id == "hit." + node.id &&
        collision.moduleId == node.moduleId &&
        collision.shape == CollisionShape::Box &&
        collision.activeStates.empty();
}

SemanticNodeUsage inspectSemanticNodeUsage(const ModelAsset& asset, std::size_t nodeIndex)
{
    if (nodeIndex >= asset.nodes.size()) throw std::runtime_error("invalid semantic node index");
    SemanticNodeUsage usage;
    const auto target = static_cast<std::int32_t>(nodeIndex);
    for (const auto& lod : asset.renderLods)
        for (const auto& renderNode : lod.nodes)
            if (renderNode.semanticNodeIndex == target) ++usage.renderBindings;
    for (const auto& node : asset.nodes)
        if (node.parentIndex == target) ++usage.children;
    for (const auto& collision : asset.collisionVolumes)
        if (collision.parentNodeIndex == target)
        {
            ++usage.collisions;
            if (isLegacySourceBootstrapCollision(asset, nodeIndex, collision))
                ++usage.legacySourceBootstrapCollisions;
        }
    for (const auto& socket : asset.sockets) if (socket.parentNodeIndex == target) ++usage.sockets;
    for (const auto& state : asset.stateVariants) if (state.nodeIndex == target) ++usage.stateVariants;
    for (const auto& hit : asset.hitRegions) if (hit.parentNodeIndex == target) ++usage.hitRegions;
    for (const auto& opening : asset.openings) if (opening.parentNodeIndex == target) ++usage.openings;
    for (const auto& repair : asset.repairTargets) if (repair.parentNodeIndex == target) ++usage.repairTargets;
    for (const auto& link : asset.structuralLinks) if (link.nodeAIndex == target || link.nodeBIndex == target) ++usage.structuralLinks;
    usage.physicsEnabled = asset.nodes[nodeIndex].physics.mode != MassPropertyMode::Disabled;
    return usage;
}

SemanticEraseResult eraseSemanticNode(ModelAsset& asset, std::size_t nodeIndex, bool removeOwnedPayload)
{
    if (nodeIndex >= asset.nodes.size()) throw std::runtime_error("invalid semantic node index");
    const auto usage = inspectSemanticNodeUsage(asset, nodeIndex);
    if (usage.children > 0)
        throw std::runtime_error("cannot delete semantic node with children; reparent or delete child branches first");
    const auto nonBootstrapCollisions = usage.collisions - usage.legacySourceBootstrapCollisions;
    const bool hasOwnedPayload = nonBootstrapCollisions > 0 || usage.sockets > 0 || usage.stateVariants > 0 ||
        usage.hitRegions > 0 || usage.openings > 0 || usage.repairTargets > 0 || usage.structuralLinks > 0 || usage.physicsEnabled;
    if (hasOwnedPayload && !removeOwnedPayload)
        throw std::runtime_error("semantic node still owns gameplay payload; confirm removal of owned semantic data first");

    SemanticEraseResult result;
    result.deletedId = asset.nodes[nodeIndex].id;
    const auto target = static_cast<std::int32_t>(nodeIndex);

    for (std::size_t li = 0; li < asset.renderLods.size(); ++li)
    {
        bool affected = false;
        for (auto& renderNode : asset.renderLods[li].nodes)
        {
            if (renderNode.semanticNodeIndex == target)
            {
                renderNode.semanticNodeIndex = NoIndex;
                renderNode.activeStates.clear();
                ++result.unboundRenderNodes;
                affected = true;
            }
            else if (renderNode.semanticNodeIndex > target)
            {
                --renderNode.semanticNodeIndex;
                affected = true;
            }
        }
        if (affected) result.affectedRenderLods.push_back(li);
    }

    // Legacy SOURCE bootstrap collision never constitutes intentional semantic payload.
    result.removedCollisions = eraseCount(asset.collisionVolumes, [&](const CollisionVolume& c) {
        if (c.parentNodeIndex != target) return false;
        return removeOwnedPayload || isLegacySourceBootstrapCollision(asset, nodeIndex, c);
    });
    if (removeOwnedPayload)
    {
        result.removedSockets = eraseCount(asset.sockets, [&](const Socket& s) { return s.parentNodeIndex == target; });
        result.removedStateVariants = eraseCount(asset.stateVariants, [&](const StateVariant& v) { return v.nodeIndex == target; });
        result.removedHitRegions = eraseCount(asset.hitRegions, [&](const HitRegion& h) { return h.parentNodeIndex == target; });
        result.removedOpenings = eraseCount(asset.openings, [&](const Opening& o) { return o.parentNodeIndex == target; });
        result.removedRepairTargets = eraseCount(asset.repairTargets, [&](const RepairTarget& r) { return r.parentNodeIndex == target; });
        result.removedStructuralLinks = eraseCount(asset.structuralLinks, [&](const StructuralLinkDefinition& link) { return link.nodeAIndex == target || link.nodeBIndex == target; });
    }

    asset.nodes.erase(asset.nodes.begin() + static_cast<std::ptrdiff_t>(nodeIndex));
    for (auto& node : asset.nodes) remapNodeIndex(node.parentIndex, nodeIndex);
    for (auto& collision : asset.collisionVolumes) remapNodeIndex(collision.parentNodeIndex, nodeIndex);
    for (auto& socket : asset.sockets) remapNodeIndex(socket.parentNodeIndex, nodeIndex);
    for (auto& state : asset.stateVariants) remapNodeIndex(state.nodeIndex, nodeIndex);
    for (auto& hit : asset.hitRegions) remapNodeIndex(hit.parentNodeIndex, nodeIndex);
    for (auto& opening : asset.openings) remapNodeIndex(opening.parentNodeIndex, nodeIndex);
    for (auto& repair : asset.repairTargets) remapNodeIndex(repair.parentNodeIndex, nodeIndex);
    for (auto& link : asset.structuralLinks)
    {
        remapNodeIndex(link.nodeAIndex, nodeIndex);
        remapNodeIndex(link.nodeBIndex, nodeIndex);
        for (auto& proxy : link.damageProxies) remapNodeIndex(proxy.parentNodeIndex, nodeIndex);
    }
    return result;
}

LegacySemanticCleanupResult cleanLegacySyntheticVisualSemanticNodes(ModelAsset& asset)
{
    LegacySemanticCleanupResult result;
    if (asset.nodes.empty() || asset.renderLods.empty()) return result;

    // Snapshot the exact legacy candidates before mutating indices. Erasing in
    // descending order keeps every lower module/child index stable until its turn.
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < asset.nodes.size(); ++i)
        if (hasLegacyVisualBootstrapPattern(asset, i)) candidates.push_back(i);
    if (candidates.empty()) return result;

    std::set<std::string> parentIds;
    std::set<std::size_t> affectedLods;

    // First move every render semantic binding off the soon-to-be-deleted child.
    // Render hierarchy/transforms are intentionally untouched, so world pose and
    // part.localOffset remain exactly where the old importer authored them.
    for (const auto nodeIndex : candidates)
    {
        const auto parentIndex = static_cast<std::size_t>(asset.nodes[nodeIndex].parentIndex);
        parentIds.insert(asset.nodes[parentIndex].id);
        for (std::size_t li = 0; li < asset.renderLods.size(); ++li)
        {
            bool changed = false;
            for (auto& renderNode : asset.renderLods[li].nodes)
            {
                if (renderNode.semanticNodeIndex != static_cast<std::int32_t>(nodeIndex)) continue;
                renderNode.semanticNodeIndex = static_cast<std::int32_t>(parentIndex);
                ++result.reboundRenderNodes;
                changed = true;
            }
            if (changed) affectedLods.insert(li);
        }
    }

    std::sort(candidates.rbegin(), candidates.rend());
    for (const auto nodeIndex : candidates)
    {
        result.removedNodeIds.push_back(asset.nodes[nodeIndex].id);
        const auto erased = eraseSemanticNode(asset, nodeIndex, false);
        affectedLods.insert(erased.affectedRenderLods.begin(), erased.affectedRenderLods.end());
    }
    std::reverse(result.removedNodeIds.begin(), result.removedNodeIds.end());

    // The old v2/v3->v4 bootstrap also emitted one geometry-less RenderNode for
    // the logical module. Keep that transform node in the RenderLod hierarchy,
    // but it is no longer a visual semantic binding once real geometry children
    // have been rebound to the module semantic Node.
    for (const auto& parentId : parentIds)
    {
        const auto parentIt = std::find_if(asset.nodes.begin(), asset.nodes.end(), [&](const Node& node) {
            return node.id == parentId;
        });
        if (parentIt == asset.nodes.end()) continue;
        const auto parentIndex = static_cast<std::int32_t>(std::distance(asset.nodes.begin(), parentIt));
        for (std::size_t li = 0; li < asset.renderLods.size(); ++li)
        {
            bool changed = false;
            for (auto& renderNode : asset.renderLods[li].nodes)
            {
                if (renderNode.semanticNodeIndex == parentIndex && renderNode.geometryIndex < 0 &&
                    renderNode.id == parentId)
                {
                    renderNode.semanticNodeIndex = NoIndex;
                    renderNode.activeStates.clear();
                    ++result.clearedTransformOnlyBindings;
                    changed = true;
                }
            }
            if (changed) affectedLods.insert(li);
        }
    }

    result.affectedRenderLods.assign(affectedLods.begin(), affectedLods.end());
    return result;
}

} // namespace elite::model_asset
