#include "src/model_asset/ModelAssetPresentation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>

#include "src/model_asset/ModelAssetLodSelection.h"

namespace elite::model_asset::presentation
{
namespace
{

void expand(glm::vec3& mn, glm::vec3& mx, bool& has, const glm::vec3& a, const glm::vec3& b)
{
    if (!has) { mn = a; mx = b; has = true; return; }
    mn = glm::min(mn, a);
    mx = glm::max(mx, b);
}

void transformAabb(const glm::mat4& m, const glm::vec3& mn, const glm::vec3& mx, glm::vec3& outMin, glm::vec3& outMax)
{
    bool has = false;
    for (int i = 0; i < 8; ++i)
    {
        const glm::vec3 c((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
        const glm::vec3 p = glm::vec3(m * glm::vec4(c, 1.0f));
        expand(outMin, outMax, has, p, p);
    }
}

float distanceToAabb(const glm::vec3& p, const glm::vec3& mn, const glm::vec3& mx)
{
    const glm::vec3 d = glm::max(glm::max(mn - p, p - mx), glm::vec3(0.0f));
    return glm::length(d);
}

// Projected DIAMETER in pixels of an asset-space AABB (camera inside: huge).
float projectedPx(const View& view, const glm::vec3& mn, const glm::vec3& mx, float* distanceOut = nullptr)
{
    const float scale = view.assetToWorldScale;
    const float diameter = glm::length(mx - mn) * scale;
    const float dist = distanceToAabb(view.cameraAsset, mn, mx) * scale;
    if (distanceOut) *distanceOut = dist;
    if (dist <= 1.0e-4f) return 1.0e6f;
    return std::min(1.0e6f, diameter * view.focalPx / dist);
}

bool aabbInFrustum(const View& view, const glm::vec3& mn, const glm::vec3& mx)
{
    if (!view.cullFrustum) return true;
    for (const glm::vec4& pl : view.planes)
    {
        const glm::vec3 n(pl);
        const glm::vec3 positive(n.x >= 0 ? mx.x : mn.x, n.y >= 0 ? mx.y : mn.y, n.z >= 0 ? mx.z : mn.z);
        if (glm::dot(n, positive) + pl.w < 0.0f) return false;
    }
    return true;
}

float thresholdFor(const Thresholds& t, Representation r)
{
    switch (r)
    {
        case Representation::Culled: return 0.0f;
        case Representation::Point: return t.pointPx;
        case Representation::Proxy: return t.proxyPx;
        case Representation::Shell: return t.shellPx;
        case Representation::Component: return t.componentPx;
        case Representation::Full: return t.fullPx;
    }
    return 0.0f;
}

Representation ladder(const Thresholds& t, Representation previous, bool fresh, float px)
{
    const float h = fresh ? 1.0f : std::max(1.0f, t.hysteresis);
    int r = fresh ? 0 : static_cast<int>(previous);
    while (r < static_cast<int>(Representation::Full) &&
           px >= thresholdFor(t, static_cast<Representation>(r + 1)) * h)
        ++r;
    while (r > 0 && px < thresholdFor(t, static_cast<Representation>(r)) / h)
        --r;
    return static_cast<Representation>(r);
}

glm::mat4 motionFor(const ObjectState& state, std::int32_t node)
{
    if (node >= 0 && static_cast<std::size_t>(node) < state.nodeMotion.size())
        return state.nodeMotion[static_cast<std::size_t>(node)];
    return glm::mat4(1.0f);
}

bool nodeHidden(const ObjectState& state, std::int32_t node)
{
    return node >= 0 && static_cast<std::size_t>(node) < state.nodeHidden.size() &&
           state.nodeHidden[static_cast<std::size_t>(node)] != 0;
}

bool renderNodeActive(const ModelAsset& asset, const ObjectState& state, const RenderNode& rn)
{
    if (!rn.enabled) return false;
    if (rn.semanticNodeIndex == NoIndex) return rn.activeStates.empty();
    if (nodeHidden(state, rn.semanticNodeIndex)) return false;
    return stateScopedActive(rn.activeStates, currentState(asset, state, rn.semanticNodeIndex));
}

float lodErrorMeters(const ModelAsset& asset, const Index& index, std::int32_t lod)
{
    const float ratio = runtimeLodErrorRatio(asset.renderLods[static_cast<std::size_t>(lod)]);
    return ratio < 0.0f ? std::numeric_limits<float>::infinity() : ratio * index.characteristicM;
}

} // namespace

const char* representationName(Representation r)
{
    switch (r)
    {
        case Representation::Culled: return "CULLED";
        case Representation::Point: return "POINT";
        case Representation::Proxy: return "PROXY";
        case Representation::Shell: return "SHELL";
        case Representation::Component: return "COMPONENT";
        case Representation::Full: return "FULL";
    }
    return "?";
}

glm::mat4 composeLocal(const glm::vec3& position, const glm::vec3& rotationDeg, const glm::vec3& pivot)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position + pivot);
    m = glm::rotate(m, glm::radians(rotationDeg.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotationDeg.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotationDeg.z), glm::vec3(0, 0, 1));
    return glm::translate(m, -pivot);
}

glm::mat4 jointMotion(const NodeJoint& joint, float value)
{
    const glm::vec3 axis = glm::length(joint.axis) > 1.0e-6f ? glm::normalize(joint.axis) : glm::vec3(0, 1, 0);
    switch (joint.type)
    {
        case JointType::Revolute:
        {
            const bool continuous = joint.maxAngleDeg - joint.minAngleDeg >= 360.0f;
            const float deg = continuous ? value : std::clamp(value, joint.minAngleDeg, joint.maxAngleDeg);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), joint.pivot);
            m = glm::rotate(m, glm::radians(deg), axis);
            return glm::translate(m, -joint.pivot);
        }
        case JointType::Prismatic:
            return glm::translate(glm::mat4(1.0f), axis * std::clamp(value, joint.minTravel, joint.maxTravel));
        case JointType::Fixed:
            break;
    }
    return glm::mat4(1.0f);
}

const std::string& currentState(const ModelAsset& asset, const ObjectState& state, std::int32_t node)
{
    static const std::string empty;
    if (node < 0 || static_cast<std::size_t>(node) >= asset.nodes.size()) return empty;
    const auto i = static_cast<std::size_t>(node);
    if (i < state.nodeState.size() && !state.nodeState[i].empty()) return state.nodeState[i];
    return asset.nodes[i].defaultStateId;
}

bool stateScopedActive(const std::vector<std::string>& activeStates, const std::string& current)
{
    return activeStates.empty() || std::find(activeStates.begin(), activeStates.end(), current) != activeStates.end();
}

std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& m)
{
    const glm::mat4 t = glm::transpose(m);
    std::array<glm::vec4, 6> p {t[3] + t[0], t[3] - t[0], t[3] + t[1], t[3] - t[1], t[3] + t[2], t[3] - t[2]};
    for (auto& plane : p)
    {
        const float len = glm::length(glm::vec3(plane));
        if (len > 0.0f) plane /= len;
    }
    return p;
}

Index buildIndex(const ModelAsset& asset)
{
    Index index;
    const std::size_t lodCount = asset.renderLods.size();
    index.renderNodes.resize(lodCount);
    index.components.resize(asset.nodes.size());
    for (std::size_t n = 0; n < asset.nodes.size(); ++n)
    {
        auto& c = index.components[n];
        c.node = static_cast<std::int32_t>(n);
        c.id = asset.nodes[n].id;
        c.moduleId = asset.nodes[n].moduleId;
        c.renderNodes.resize(lodCount);
    }
    for (std::size_t ci = 0; ci < asset.compartments.size(); ++ci)
        for (const std::int32_t node : asset.compartments[ci].interiorNodeIndices)
            if (node >= 0 && static_cast<std::size_t>(node) < index.components.size())
            {
                index.components[static_cast<std::size_t>(node)].interior = true;
                index.components[static_cast<std::size_t>(node)].compartment = static_cast<std::int32_t>(ci);
            }

    std::vector<std::pair<std::uint32_t, std::int32_t>> componentLevels;
    bool hasAssetBounds = false;
    for (std::size_t li = 0; li < lodCount; ++li)
    {
        const RenderLod& lod = asset.renderLods[li];
        switch (lod.role)
        {
            case RenderLodRole::Component: componentLevels.emplace_back(lod.level, static_cast<std::int32_t>(li)); break;
            case RenderLodRole::ModuleProxy: index.moduleProxyLod = static_cast<std::int32_t>(li); break;
            case RenderLodRole::Shell: index.shellLod = static_cast<std::int32_t>(li); break;
            case RenderLodRole::Proxy: index.proxyLod = static_cast<std::int32_t>(li); break;
        }
        auto& infos = index.renderNodes[li];
        infos.resize(lod.nodes.size());
        // Render-graph chain (parents may follow children: resolve lazily).
        std::vector<std::uint8_t> done(lod.nodes.size(), 0);
        std::function<const glm::mat4&(std::size_t)> chain;
        chain = [&](std::size_t i) -> const glm::mat4& {
            if (!done[i])
            {
                const RenderNode& rn = lod.nodes[i];
                glm::mat4 local = composeLocal(rn.localPosition, rn.localRotationDeg, rn.pivot);
                done[i] = 1; // cycle guard
                if (rn.parentIndex >= 0 && static_cast<std::size_t>(rn.parentIndex) < lod.nodes.size() &&
                    static_cast<std::size_t>(rn.parentIndex) != i)
                    local = chain(static_cast<std::size_t>(rn.parentIndex)) * local;
                infos[i].assetTransform = local;
            }
            return infos[i].assetTransform;
        };
        for (std::size_t i = 0; i < lod.nodes.size(); ++i)
        {
            chain(i);
            const RenderNode& rn = lod.nodes[i];
            auto& info = infos[i];
            if (rn.geometryIndex < 0 || static_cast<std::size_t>(rn.geometryIndex) >= lod.geometries.size())
                continue;
            const MeshLod& mesh = lod.geometries[static_cast<std::size_t>(rn.geometryIndex)].mesh;
            if (mesh.triangles.empty()) continue;
            info.triangles = static_cast<std::uint32_t>(mesh.triangles.size());
            info.drawable = true;
            transformAabb(info.assetTransform, mesh.minBounds, mesh.maxBounds, info.minBounds, info.maxBounds);
            if (rn.semanticNodeIndex >= 0 && static_cast<std::size_t>(rn.semanticNodeIndex) < index.components.size())
            {
                auto& c = index.components[static_cast<std::size_t>(rn.semanticNodeIndex)];
                c.renderNodes[li].push_back(static_cast<std::int32_t>(i));
                if (lod.role == RenderLodRole::Component)
                    expand(c.minBounds, c.maxBounds, c.hasBounds, info.minBounds, info.maxBounds);
            }
            if (lod.role == RenderLodRole::Component && lod.level == 0)
                expand(index.minBounds, index.maxBounds, hasAssetBounds, info.minBounds, info.maxBounds);
        }
    }
    std::sort(componentLevels.begin(), componentLevels.end());
    for (const auto& [level, li] : componentLevels) index.componentLods.push_back(li);
    if (!hasAssetBounds)
    {
        index.minBounds = asset.minBounds;
        index.maxBounds = asset.maxBounds;
    }
    index.characteristicM = lodCharacteristicSize(glm::max(index.maxBounds - index.minBounds, glm::vec3(0.0f)));

    std::map<std::string, std::size_t> groupOf;
    for (auto& c : index.components)
    {
        if (c.moduleId.empty() || !c.hasBounds || c.interior) continue;
        auto [it, inserted] = groupOf.emplace(c.moduleId, index.groups.size());
        if (inserted) index.groups.push_back(Index::Group {c.moduleId, {}, c.minBounds, c.maxBounds, {}});
        auto& g = index.groups[it->second];
        g.components.push_back(c.node);
        g.minBounds = glm::min(g.minBounds, c.minBounds);
        g.maxBounds = glm::max(g.maxBounds, c.maxBounds);
    }
    if (index.moduleProxyLod != NoIndex)
    {
        const RenderLod& lod = asset.renderLods[static_cast<std::size_t>(index.moduleProxyLod)];
        for (std::size_t i = 0; i < lod.nodes.size(); ++i)
        {
            const std::int32_t node = lod.nodes[i].semanticNodeIndex;
            if (node < 0 || static_cast<std::size_t>(node) >= asset.nodes.size()) continue;
            const auto it = groupOf.find(asset.nodes[static_cast<std::size_t>(node)].moduleId);
            if (it != groupOf.end() && index.renderNodes[static_cast<std::size_t>(index.moduleProxyLod)][i].drawable)
                index.groups[it->second].proxyRenderNodes.push_back(static_cast<std::int32_t>(i));
        }
    }
    return index;
}

Frame select(const ModelAsset& asset, const Index& index, const ObjectState& state, const View& view,
             Selection& selection, const Thresholds& t)
{
    Frame frame;
    const bool fresh = selection.componentLod.size() != index.components.size();
    if (fresh)
    {
        selection.componentLod.assign(index.components.size(), NoIndex);
        selection.groupCollapsed.assign(index.groups.size(), 0);
    }
    frame.objectPx = projectedPx(view, index.minBounds, index.maxBounds);
    Representation rep = ladder(t, selection.representation, fresh, frame.objectPx);
    if (!aabbInFrustum(view, index.minBounds, index.maxBounds) && rep > Representation::Point)
        rep = Representation::Culled; // off screen: keep nothing resident in the draw list
    selection.representation = rep;
    frame.representation = rep;
    if (rep <= Representation::Point || index.componentLods.empty())
        return frame;

    const auto emit = [&](std::int32_t lod, std::int32_t renderNode, std::int32_t component, bool interior) {
        const RenderNode& rn = asset.renderLods[static_cast<std::size_t>(lod)].nodes[static_cast<std::size_t>(renderNode)];
        if (!renderNodeActive(asset, state, rn)) return;
        const auto& info = index.renderNodes[static_cast<std::size_t>(lod)][static_cast<std::size_t>(renderNode)];
        DrawItem item;
        item.lod = lod;
        item.renderNode = renderNode;
        item.component = component;
        item.interior = interior;
        item.assetTransform = motionFor(state, rn.semanticNodeIndex) * info.assetTransform;
        frame.draws.push_back(item);
        frame.triangles += info.triangles;
        if (interior) ++frame.interiorDraws;
    };

    // Whole-object representations: one LOD, no interiors.
    if (rep == Representation::Proxy || rep == Representation::Shell)
    {
        std::int32_t lod = rep == Representation::Proxy ? index.proxyLod : index.shellLod;
        if (lod == NoIndex) lod = rep == Representation::Proxy ? index.shellLod : NoIndex;
        if (lod == NoIndex) lod = index.componentLods.back();
        const auto& infos = index.renderNodes[static_cast<std::size_t>(lod)];
        for (std::size_t i = 0; i < infos.size(); ++i)
        {
            if (!infos[i].drawable) continue;
            const std::int32_t node = asset.renderLods[static_cast<std::size_t>(lod)].nodes[i].semanticNodeIndex;
            if (node >= 0 && index.components[static_cast<std::size_t>(node)].interior) continue;
            emit(lod, static_cast<std::int32_t>(i), node, false);
        }
        return frame;
    }

    // ---- interiors: visible open exterior portals -> compartments (BFS) ----
    const ActiveSets active = resolveActiveSets(asset, state);
    frame.activeOpenings = active.openings;
    std::vector<std::uint8_t> compartmentVisible(asset.compartments.size(), 0);
    if (rep == Representation::Full && !asset.compartments.empty())
    {
        const auto portalOpen = [&](const CompartmentPortal& p) {
            if (!p.enabled) return false;
            if (p.alwaysOpen) return true;
            if (p.openingIndex != NoIndex &&
                std::find(active.openings.begin(), active.openings.end(), p.openingIndex) != active.openings.end())
                return true;
            return p.doorNodeIndex != NoIndex && !p.openStateId.empty() &&
                   currentState(asset, state, p.doorNodeIndex) == p.openStateId &&
                   !nodeHidden(state, p.doorNodeIndex);
        };
        const auto portalSeen = [&](const CompartmentPortal& p, bool requireFacing) {
            const glm::mat4 m = motionFor(state, p.parentNodeIndex) *
                                composeLocal(p.localPosition, p.localRotationDeg, glm::vec3(0.0f));
            const glm::vec3 centre = glm::vec3(m * glm::vec4(0, 0, 0, 1));
            const glm::vec3 normal = glm::normalize(glm::vec3(m * glm::vec4(0, 0, 1, 0)));
            const glm::mat3 rot(m);
            const glm::vec3 r = glm::abs(rot[0]) * p.halfSize.x + glm::abs(rot[1]) * p.halfSize.y + glm::abs(rot[2]) * p.halfSize.z;
            if (!aabbInFrustum(view, centre - r, centre + r)) return false;
            if (projectedPx(view, centre - r, centre + r) < t.portalMinPx) return false;
            // Exterior portal (+Z outward): the camera must be outside, looking in.
            return !requireFacing || glm::dot(normal, view.cameraAsset - centre) > 0.0f;
        };
        std::deque<std::pair<std::int32_t, int>> queue;
        for (std::size_t pi = 0; pi < asset.portals.size(); ++pi)
        {
            const auto& p = asset.portals[pi];
            if (p.compartmentB != NoIndex || !portalOpen(p) || !portalSeen(p, true)) continue;
            frame.visiblePortals.push_back(static_cast<std::int32_t>(pi));
            if (!compartmentVisible[static_cast<std::size_t>(p.compartmentA)])
            {
                compartmentVisible[static_cast<std::size_t>(p.compartmentA)] = 1;
                queue.emplace_back(p.compartmentA, 1);
            }
        }
        while (!queue.empty())
        {
            const auto [comp, depth] = queue.front();
            queue.pop_front();
            if (depth >= t.interiorDepth) continue;
            for (std::size_t pi = 0; pi < asset.portals.size(); ++pi)
            {
                const auto& p = asset.portals[pi];
                if (p.compartmentB == NoIndex) continue;
                std::int32_t other = NoIndex;
                if (p.compartmentA == comp) other = p.compartmentB;
                else if (p.compartmentB == comp) other = p.compartmentA;
                if (other == NoIndex || compartmentVisible[static_cast<std::size_t>(other)]) continue;
                if (!portalOpen(p) || !portalSeen(p, false)) continue;
                frame.visiblePortals.push_back(static_cast<std::int32_t>(pi));
                compartmentVisible[static_cast<std::size_t>(other)] = 1;
                queue.emplace_back(other, depth + 1);
            }
        }
        for (std::size_t ci = 0; ci < compartmentVisible.size(); ++ci)
            if (compartmentVisible[ci]) frame.visibleCompartments.push_back(static_cast<std::int32_t>(ci));
    }

    // ---- module HLOD collapse (Component representation only) ----
    std::vector<std::uint8_t> collapsedComponent(index.components.size(), 0);
    for (std::size_t gi = 0; gi < index.groups.size(); ++gi)
    {
        const auto& g = index.groups[gi];
        bool collapse = false;
        if (rep == Representation::Component && !g.proxyRenderNodes.empty())
        {
            const float px = projectedPx(view, g.minBounds, g.maxBounds);
            const float h = fresh ? 1.0f : t.hysteresis;
            collapse = selection.groupCollapsed[gi] ? px < t.moduleCollapsePx * h : px < t.moduleCollapsePx / h;
        }
        selection.groupCollapsed[gi] = collapse ? 1 : 0;
        if (!collapse) continue;
        ++frame.collapsedGroups;
        if (aabbInFrustum(view, g.minBounds, g.maxBounds))
            for (const std::int32_t rn : g.proxyRenderNodes)
                emit(index.moduleProxyLod, rn,
                     asset.renderLods[static_cast<std::size_t>(index.moduleProxyLod)].nodes[static_cast<std::size_t>(rn)].semanticNodeIndex,
                     false);
        for (const std::int32_t c : g.components) collapsedComponent[static_cast<std::size_t>(c)] = 1;
    }

    // ---- per-component screen-space-error LOD ----
    const float cullPx = (rep == Representation::Full ? 0.5f : 1.0f) * t.componentCullPx;
    for (std::size_t ci = 0; ci < index.components.size(); ++ci)
    {
        const auto& c = index.components[ci];
        if (!c.hasBounds) continue;
        if (c.interior && (c.compartment == NoIndex || !compartmentVisible[static_cast<std::size_t>(c.compartment)]))
        {
            selection.componentLod[ci] = NoIndex;
            continue;
        }
        if (nodeHidden(state, c.node)) continue;
        ComponentReport report;
        report.component = static_cast<std::int32_t>(ci);
        report.interior = c.interior;
        if (collapsedComponent[ci])
        {
            report.collapsed = true;
            selection.componentLod[ci] = NoIndex;
            frame.components.push_back(report);
            continue;
        }
        // Motion moves the component: test bounds where it actually is.
        glm::vec3 mn = c.minBounds, mx = c.maxBounds;
        if (c.node < static_cast<std::int32_t>(state.nodeMotion.size()))
            transformAabb(motionFor(state, c.node), c.minBounds, c.maxBounds, mn, mx);
        float dist = 0.0f;
        report.projectedPx = projectedPx(view, mn, mx, &dist);
        const bool wasVisible = selection.componentLod[ci] != NoIndex;
        const float h = fresh ? 1.0f : t.hysteresis;
        const bool bigEnough = wasVisible ? report.projectedPx >= cullPx / h : report.projectedPx >= cullPx * h;
        if (!bigEnough || !aabbInFrustum(view, mn, mx))
        {
            report.culled = true;
            ++frame.culledComponents;
            if (!bigEnough) selection.componentLod[ci] = NoIndex;
            frame.components.push_back(report);
            continue;
        }
        // Position in the component LOD chain (finest .. coarsest).
        const auto& chainLods = index.componentLods;
        const auto errorPx = [&](std::size_t k) {
            const float errM = lodErrorMeters(asset, index, chainLods[k]) * view.assetToWorldScale;
            return dist <= 1.0e-4f ? (k == 0 ? 0.0f : std::numeric_limits<float>::infinity())
                                   : errM * view.focalPx / dist;
        };
        std::size_t k = chainLods.size() - 1;
        if (wasVisible)
        {
            const auto it = std::find(chainLods.begin(), chainLods.end(), selection.componentLod[ci]);
            if (it != chainLods.end()) k = static_cast<std::size_t>(it - chainLods.begin());
        }
        const float refine = fresh ? t.sseCoarsenPx : t.sseRefinePx;
        while (k > 0 && errorPx(k) > refine) --k;
        while (k + 1 < chainLods.size() && errorPx(k + 1) <= t.sseCoarsenPx) ++k;
        const std::int32_t lod = chainLods[k];
        selection.componentLod[ci] = lod;
        report.lod = lod;
        report.errorPx = errorPx(k);
        if (c.renderNodes[static_cast<std::size_t>(lod)].empty())
        {
            // The component is intentionally omitted at this error level.
            report.culled = true;
            ++frame.culledComponents;
        }
        for (const std::int32_t rn : c.renderNodes[static_cast<std::size_t>(lod)])
            emit(lod, rn, static_cast<std::int32_t>(ci), c.interior);
        frame.components.push_back(report);
    }
    // Render nodes without a semantic binding in the finest component LOD
    // (decor) follow the object representation.
    {
        const std::int32_t lod = index.componentLods.front();
        const auto& nodes = asset.renderLods[static_cast<std::size_t>(lod)].nodes;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].semanticNodeIndex == NoIndex &&
                index.renderNodes[static_cast<std::size_t>(lod)][i].drawable)
                emit(lod, static_cast<std::int32_t>(i), NoIndex, false);
    }
    return frame;
}

ActiveSets resolveActiveSets(const ModelAsset& asset, const ObjectState& state)
{
    ActiveSets sets;
    const auto on = [&](std::int32_t node, const std::vector<std::string>& states, bool enabled) {
        if (!enabled) return false;
        if (node != NoIndex && nodeHidden(state, node)) return false;
        return stateScopedActive(states, currentState(asset, state, node));
    };
    for (std::size_t i = 0; i < asset.collisionVolumes.size(); ++i)
        if (on(asset.collisionVolumes[i].parentNodeIndex, asset.collisionVolumes[i].activeStates, asset.collisionVolumes[i].enabled))
            sets.collisionVolumes.push_back(static_cast<std::int32_t>(i));
    for (std::size_t i = 0; i < asset.hitRegions.size(); ++i)
        if (on(asset.hitRegions[i].parentNodeIndex, asset.hitRegions[i].activeStates, asset.hitRegions[i].enabled))
            sets.hitRegions.push_back(static_cast<std::int32_t>(i));
    for (std::size_t i = 0; i < asset.openings.size(); ++i)
        if (on(asset.openings[i].parentNodeIndex, asset.openings[i].activeStates, asset.openings[i].enabled))
            sets.openings.push_back(static_cast<std::int32_t>(i));
    for (std::size_t i = 0; i < asset.sockets.size(); ++i)
        if (on(asset.sockets[i].parentNodeIndex, asset.sockets[i].activeStates, asset.sockets[i].enabled))
            sets.sockets.push_back(static_cast<std::int32_t>(i));
    for (std::size_t i = 0; i < asset.repairTargets.size(); ++i)
        if (on(asset.repairTargets[i].parentNodeIndex, asset.repairTargets[i].activeStates, asset.repairTargets[i].enabled))
            sets.repairTargets.push_back(static_cast<std::int32_t>(i));
    sets.renderNodes.resize(asset.renderLods.size());
    for (std::size_t li = 0; li < asset.renderLods.size(); ++li)
        for (std::size_t i = 0; i < asset.renderLods[li].nodes.size(); ++i)
            if (renderNodeActive(asset, state, asset.renderLods[li].nodes[i]))
                sets.renderNodes[li].push_back(static_cast<std::int32_t>(i));
    return sets;
}

std::vector<std::int32_t> structuralIslands(const ModelAsset& asset, const std::vector<std::uint8_t>& linkBroken,
                                            std::int32_t rootNode)
{
    const std::size_t n = asset.nodes.size();
    std::vector<std::size_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<std::size_t(std::size_t)> find = [&](std::size_t x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    const auto join = [&](std::int32_t a, std::int32_t b) {
        if (a < 0 || b < 0 || static_cast<std::size_t>(a) >= n || static_cast<std::size_t>(b) >= n) return;
        parent[find(static_cast<std::size_t>(a))] = find(static_cast<std::size_t>(b));
    };
    std::vector<std::uint8_t> linked(n, 0);
    for (std::size_t i = 0; i < asset.structuralLinks.size(); ++i)
    {
        const auto& link = asset.structuralLinks[i];
        if (link.nodeAIndex >= 0 && static_cast<std::size_t>(link.nodeAIndex) < n) linked[static_cast<std::size_t>(link.nodeAIndex)] = 1;
        if (link.nodeBIndex >= 0 && static_cast<std::size_t>(link.nodeBIndex) < n) linked[static_cast<std::size_t>(link.nodeBIndex)] = 1;
        const bool broken = i < linkBroken.size() && linkBroken[i] != 0;
        if (link.enabled && !broken) join(link.nodeAIndex, link.nodeBIndex);
    }
    for (std::size_t i = 0; i < n; ++i)
        if (!linked[i]) join(static_cast<std::int32_t>(i), asset.nodes[i].parentIndex);

    std::vector<std::int32_t> island(n, -1);
    std::map<std::size_t, std::int32_t> ids;
    if (rootNode >= 0 && static_cast<std::size_t>(rootNode) < n) ids[find(static_cast<std::size_t>(rootNode))] = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const std::size_t r = find(i);
        auto it = ids.find(r);
        if (it == ids.end()) it = ids.emplace(r, static_cast<std::int32_t>(ids.size())).first;
        island[i] = it->second;
    }
    return island;
}

std::string formatFrame(const ModelAsset& asset, const Index& index, const Frame& frame, const std::string& objectId)
{
    std::string out;
    char line[512];
    std::uint32_t lodDraws[8] {};
    for (const auto& d : frame.draws)
        if (d.lod >= 0 && d.lod < 8) ++lodDraws[d.lod];
    std::snprintf(line, sizeof(line),
                  "OBJECT %s rep=%s px=%.1f draws=%zu tris=%u culled=%u collapsedGroups=%u lodDraws=[%u %u %u %u %u] "
                  "openings=%zu portals=%zu compartments=%zu interiorDraws=%u\n",
                  objectId.c_str(), representationName(frame.representation), frame.objectPx, frame.draws.size(),
                  frame.triangles, frame.culledComponents, frame.collapsedGroups, lodDraws[0], lodDraws[1], lodDraws[2],
                  lodDraws[3], lodDraws[4], frame.activeOpenings.size(), frame.visiblePortals.size(),
                  frame.visibleCompartments.size(), frame.interiorDraws);
    out += line;
    for (const auto& c : frame.components)
    {
        const auto& comp = index.components[static_cast<std::size_t>(c.component)];
        std::snprintf(line, sizeof(line), "  COMPONENT %s/%s rep=%s lod=%d px=%.1f errPx=%.2f%s%s%s group=%s\n",
                      objectId.c_str(), comp.id.c_str(),
                      c.collapsed ? "HLOD" : c.culled ? "CULLED" : representationName(frame.representation), c.lod,
                      c.projectedPx, c.errorPx, c.culled ? " culled" : "", c.collapsed ? " collapsed" : "",
                      c.interior ? " interior" : "", comp.moduleId.empty() ? "-" : comp.moduleId.c_str());
        out += line;
    }
    for (const std::int32_t ci : frame.visibleCompartments)
    {
        std::snprintf(line, sizeof(line), "  COMPARTMENT %s/%s visible\n", objectId.c_str(),
                      asset.compartments[static_cast<std::size_t>(ci)].id.c_str());
        out += line;
    }
    return out;
}

} // namespace elite::model_asset::presentation
