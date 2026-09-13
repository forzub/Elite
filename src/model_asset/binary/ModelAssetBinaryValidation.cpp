#include "src/model_asset/binary/ModelAssetBinaryValidation.h"
#include "src/model_asset/binary/ModelAssetBinaryWire.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace elite::model_asset::binary
{

bool validateRenderLod(const RenderLod& lod, std::size_t semanticNodeCount, std::string* error)
{
    std::map<std::string, std::size_t> geometryIds;
    for (std::size_t geometryIndex = 0; geometryIndex < lod.geometries.size(); ++geometryIndex)
    {
        const auto& geometry = lod.geometries[geometryIndex];
        if (geometry.id.empty())
        {
            setError(error, "LOD" + std::to_string(lod.level) + " render geometry[" +
                std::to_string(geometryIndex) + "] has empty stable id");
            return false;
        }
        const auto [it, inserted] = geometryIds.emplace(geometry.id, geometryIndex);
        if (!inserted)
        {
            setError(error, "LOD" + std::to_string(lod.level) + " duplicate RenderGeometryDefinition id '" +
                geometry.id + "': geometry[" + std::to_string(it->second) + "] and geometry[" +
                std::to_string(geometryIndex) + "]");
            return false;
        }
    }

    std::map<std::string, std::size_t> nodeIds;
    for (std::size_t nodeIndex = 0; nodeIndex < lod.nodes.size(); ++nodeIndex)
    {
        const auto& node = lod.nodes[nodeIndex];
        if (node.id.empty())
        {
            setError(error, "LOD" + std::to_string(lod.level) + " RenderNode[" +
                std::to_string(nodeIndex) + "] has empty id");
            return false;
        }
        const auto [it, inserted] = nodeIds.emplace(node.id, nodeIndex);
        if (!inserted)
        {
            setError(error, "LOD" + std::to_string(lod.level) + " duplicate RenderNode id '" + node.id +
                "': node[" + std::to_string(it->second) + "] and node[" + std::to_string(nodeIndex) + "]");
            return false;
        }
        if (node.parentIndex < NoIndex ||
            node.parentIndex >= static_cast<std::int32_t>(lod.nodes.size()) ||
            node.geometryIndex < NoIndex ||
            node.geometryIndex >= static_cast<std::int32_t>(lod.geometries.size()) ||
            node.semanticNodeIndex < NoIndex ||
            node.semanticNodeIndex >= static_cast<std::int32_t>(semanticNodeCount))
        {
            setError(error, "render node index out of range in LOD" + std::to_string(lod.level) + ": " + node.id);
            return false;
        }
        if (node.parentIndex == static_cast<std::int32_t>(nodeIndex))
        {
            setError(error, "render node cannot parent itself in LOD" + std::to_string(lod.level) + ": " + node.id);
            return false;
        }
    }
    return true;
}

bool validateSemanticAsset(const ModelAsset& asset, std::string* error)
{
    std::map<std::string, std::size_t> semanticNodeIds;
    for (std::size_t i = 0; i < asset.nodes.size(); ++i)
    {
        const auto& node = asset.nodes[i];
        if (node.id.empty())
        {
            setError(error, "semantic Node[" + std::to_string(i) + "] has empty id");
            return false;
        }
        const auto [it, inserted] = semanticNodeIds.emplace(node.id, i);
        if (!inserted)
        {
            setError(error, "duplicate semantic Node id '" + node.id + "': node[" +
                std::to_string(it->second) + "] and node[" + std::to_string(i) + "]");
            return false;
        }
        if (node.parentIndex < NoIndex || node.parentIndex >= static_cast<std::int32_t>(asset.nodes.size()) ||
            node.parentIndex == static_cast<std::int32_t>(i))
        {
            setError(error, "semantic node parent index out of range: " + node.id);
            return false;
        }
    }

    std::set<std::pair<std::int32_t, std::string>> stateIds;
    for (const auto& state : asset.stateVariants)
    {
        if (state.nodeIndex < 0 || state.nodeIndex >= static_cast<std::int32_t>(asset.nodes.size()) || state.id.empty() || state.id == "intact")
        {
            setError(error, "invalid semantic state variant");
            return false;
        }
        if (!stateIds.emplace(state.nodeIndex, state.id).second)
        {
            setError(error, "duplicate semantic state variant: " + state.id);
            return false;
        }
    }

    const auto stateDeclared = [&](std::int32_t nodeIndex, const std::string& stateId) {
        if (stateId == "intact") return true;
        return stateIds.count({nodeIndex, stateId}) != 0;
    };
    for (std::size_t i = 0; i < asset.nodes.size(); ++i)
    {
        if (!stateDeclared(static_cast<std::int32_t>(i), asset.nodes[i].defaultStateId))
        {
            setError(error, "semantic node default state is not declared: " + asset.nodes[i].id + " / " + asset.nodes[i].defaultStateId);
            return false;
        }
    }

    const auto validParent = [&](std::int32_t parent) {
        return parent >= NoIndex && parent < static_cast<std::int32_t>(asset.nodes.size());
    };
    const auto validateScopedStates = [&](std::int32_t parent, const std::vector<std::string>& states, const std::string& label) {
        if (!validParent(parent)) { setError(error, label + " parent index out of range"); return false; }
        if (!states.empty() && parent == NoIndex) { setError(error, label + " has state scope but no semantic parent"); return false; }
        for (const auto& stateId : states)
        {
            if (!stateDeclared(parent, stateId)) { setError(error, label + " references undeclared state: " + stateId); return false; }
        }
        return true;
    };
    for (const auto& c : asset.collisionVolumes) if (!validateScopedStates(c.parentNodeIndex, c.activeStates, "collision " + c.id)) return false;
    for (const auto& socket : asset.sockets) if (!validateScopedStates(socket.parentNodeIndex, socket.activeStates, "socket " + socket.id)) return false;
    for (const auto& h : asset.hitRegions) if (!validateScopedStates(h.parentNodeIndex, h.activeStates, "hit region " + h.id)) return false;
    for (const auto& o : asset.openings) if (!validateScopedStates(o.parentNodeIndex, o.activeStates, "opening " + o.id)) return false;
    for (const auto& r : asset.repairTargets)
    {
        if (!validateScopedStates(r.parentNodeIndex, r.activeStates, "repair target " + r.id)) return false;
        if (!r.repairedStateId.empty() && r.parentNodeIndex >= 0 && !stateDeclared(r.parentNodeIndex, r.repairedStateId))
        {
            setError(error, "repair target references undeclared repaired state: " + r.id + " / " + r.repairedStateId);
            return false;
        }
    }

    {
        const auto axis = static_cast<std::uint8_t>(asset.physicalSize.axis);
        const auto space = static_cast<std::uint8_t>(asset.physicalSize.geometrySpace);
        if (axis > static_cast<std::uint8_t>(PhysicalSizeAxis::Z) ||
            space > static_cast<std::uint8_t>(PhysicalGeometrySpace::LegacyUnknown))
        {
            setError(error, "invalid physical-scale profile enum");
            return false;
        }
        if (asset.physicalSize.enabled && asset.physicalSize.geometrySpace != PhysicalGeometrySpace::LegacyUnknown)
        {
            if (!std::isfinite(asset.physicalSize.targetMeters) || asset.physicalSize.targetMeters <= 0.0f ||
                !std::isfinite(asset.physicalSize.sourceExtent) || asset.physicalSize.sourceExtent <= 0.0f ||
                !std::isfinite(asset.physicalSize.sourceToMeters) || asset.physicalSize.sourceToMeters <= 0.0f)
            {
                setError(error, "invalid physical-scale calibration");
                return false;
            }
            const float expected = asset.physicalSize.sourceExtent * asset.physicalSize.sourceToMeters;
            const float tolerance = std::max(1.0e-4f, std::abs(asset.physicalSize.targetMeters) * 1.0e-4f);
            if (std::abs(expected - asset.physicalSize.targetMeters) > tolerance)
            {
                setError(error, "physical-scale calibration coefficient does not match reference extent");
                return false;
            }
        }
        if (asset.physicalSize.gameLinked)
        {
            const auto& d = asset.physicalSize.gameDimensionsMeters;
            if (!std::isfinite(d.x) || !std::isfinite(d.y) || !std::isfinite(d.z) ||
                d.x <= 0.0f || d.y <= 0.0f || d.z <= 0.0f)
            {
                setError(error, "invalid linked game dimensions");
                return false;
            }
        }
    }

    std::set<std::string> structuralIds;
    for (const auto& link : asset.structuralLinks)
    {
        if (link.id.empty() || !structuralIds.insert(link.id).second)
        {
            setError(error, "empty/duplicate structural link id");
            return false;
        }
        if (link.nodeAIndex < 0 || link.nodeBIndex < 0 ||
            link.nodeAIndex >= static_cast<std::int32_t>(asset.nodes.size()) ||
            link.nodeBIndex >= static_cast<std::int32_t>(asset.nodes.size()) ||
            link.nodeAIndex == link.nodeBIndex)
        {
            setError(error, "structural link endpoint out of range: " + link.id);
            return false;
        }
        if (!std::isfinite(link.breakForceN) || !std::isfinite(link.breakTorqueNm) ||
            link.breakForceN < 0.0f || link.breakTorqueNm < 0.0f)
        {
            setError(error, "invalid structural link strength: " + link.id);
            return false;
        }
        for (const auto& proxy : link.damageProxies)
        {
            if (proxy.parentNodeIndex < 0 || proxy.parentNodeIndex >= static_cast<std::int32_t>(asset.nodes.size()))
            {
                setError(error, "structural damage proxy parent out of range: " + link.id + " / " + proxy.id);
                return false;
            }
            if (!std::isfinite(proxy.radius) || !std::isfinite(proxy.halfHeight) || proxy.radius < 0.0f || proxy.halfHeight < 0.0f)
            {
                setError(error, "invalid structural damage proxy dimensions: " + link.id + " / " + proxy.id);
                return false;
            }
        }
    }

    for (const auto& socket : asset.sockets)
    {
        if (!std::isfinite(socket.previewFovDeg) || socket.previewFovDeg <= 1.0f || socket.previewFovDeg >= 179.0f)
        {
            setError(error, "invalid socket preview FOV: " + socket.id);
            return false;
        }
    }

    float previousAuthoredError = 0.0f;
    bool havePreviousAuthoredError = true;
    for (const auto& lod : asset.renderLods)
    {
        if (lod.level == 0)
        {
            if (std::isfinite(lod.relativeGeometricError) && lod.relativeGeometricError > 1.0e-6f)
            {
                setError(error, "LOD0 relative geometric error must be zero/unspecified");
                return false;
            }
        }
        else if (lod.relativeGeometricError >= 0.0f)
        {
            if (!std::isfinite(lod.relativeGeometricError) || lod.relativeGeometricError <= 0.0f)
            {
                setError(error, "LOD" + std::to_string(lod.level) + " has invalid relative geometric error");
                return false;
            }
            if (havePreviousAuthoredError && lod.relativeGeometricError + 1.0e-7f < previousAuthoredError)
            {
                setError(error, "LOD relative geometric error must be non-decreasing");
                return false;
            }
            previousAuthoredError = lod.relativeGeometricError;
            havePreviousAuthoredError = true;
        }
        else
        {
            havePreviousAuthoredError = false;
        }
        if (!validateRenderLod(lod, asset.nodes.size(), error)) return false;
        for (const auto& renderNode : lod.nodes)
        {
            if (!renderNode.activeStates.empty() && renderNode.semanticNodeIndex == NoIndex)
            {
                setError(error, "state-scoped render node has no semantic binding in LOD" + std::to_string(lod.level) + ": " + renderNode.id);
                return false;
            }
            for (const auto& stateId : renderNode.activeStates)
            {
                if (!stateDeclared(renderNode.semanticNodeIndex, stateId))
                {
                    setError(error, "render node references undeclared semantic state in LOD" + std::to_string(lod.level) + ": " + renderNode.id + " / " + stateId);
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace elite::model_asset::binary
