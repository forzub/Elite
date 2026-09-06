#include "src/model_asset/ModelAssetSemantics.h"

#include <algorithm>
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

} // namespace

bool SemanticNodeUsage::hasRuntimePayload() const
{
    return children > 0 || collisions > legacySourceBootstrapCollisions || sockets > 0 ||
        stateVariants > 0 || hitRegions > 0 || openings > 0 || repairTargets > 0 || physicsEnabled;
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
        usage.hitRegions > 0 || usage.openings > 0 || usage.repairTargets > 0 || usage.physicsEnabled;
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
    }

    asset.nodes.erase(asset.nodes.begin() + static_cast<std::ptrdiff_t>(nodeIndex));
    for (auto& node : asset.nodes) remapNodeIndex(node.parentIndex, nodeIndex);
    for (auto& collision : asset.collisionVolumes) remapNodeIndex(collision.parentNodeIndex, nodeIndex);
    for (auto& socket : asset.sockets) remapNodeIndex(socket.parentNodeIndex, nodeIndex);
    for (auto& state : asset.stateVariants) remapNodeIndex(state.nodeIndex, nodeIndex);
    for (auto& hit : asset.hitRegions) remapNodeIndex(hit.parentNodeIndex, nodeIndex);
    for (auto& opening : asset.openings) remapNodeIndex(opening.parentNodeIndex, nodeIndex);
    for (auto& repair : asset.repairTargets) remapNodeIndex(repair.parentNodeIndex, nodeIndex);
    return result;
}

} // namespace elite::model_asset
