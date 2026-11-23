#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::presentation
{

// Runtime presentation of one composite object (ship, station) for ONE
// camera. Pure functions of (asset, object state, camera, previous selection):
// no GL, no gameplay. The renderer draws the DrawItems; simulation supplies
// the per-node state ids, detach flags and motion transforms.
//
// Ladder (by projected object diameter, with hysteresis):
//   Culled < Point < Proxy < Shell < Component < Full
// Point: no mesh (caller draws a sprite). Proxy / Shell: one whole-object
// representation (role Proxy / Shell, falling back to the coarsest component
// LOD). Component: per-component screen-space-error LOD + module HLOD
// collapse. Full: Component + interiors behind visible open portals + small
// exterior parts.
enum class Representation : std::uint8_t
{
    Culled = 0,
    Point = 1,
    Proxy = 2,
    Shell = 3,
    Component = 4,
    Full = 5
};

const char* representationName(Representation r);

struct Thresholds
{
    // Projected object DIAMETER in pixels where the next representation
    // starts. Leaving uses threshold / hysteresis, entering threshold * it.
    float pointPx = 1.0f;       // below: culled (sub-pixel)
    float proxyPx = 6.0f;       // point -> proxy
    float shellPx = 40.0f;      // proxy -> shell
    float componentPx = 180.0f; // shell -> component assembly
    float fullPx = 700.0f;      // component -> full close object
    float hysteresis = 1.2f;

    // Per component: culled below this projected diameter (Full: half).
    float componentCullPx = 2.0f;
    // Module HLOD: a module group collapses to its proxy below this size.
    float moduleCollapsePx = 90.0f;
    // Screen-space error targets (ModelAssetLodSelection defaults).
    float sseCoarsenPx = 1.8f;
    float sseRefinePx = 2.2f;

    // Interiors: portal must project at least this size; BFS depth.
    float portalMinPx = 8.0f;
    int interiorDepth = 2;
};

// Built once per asset (all LOD payloads must be loaded).
struct Index
{
    struct RenderNodeInfo
    {
        glm::mat4 assetTransform {1.0f}; // render-graph chain, asset space
        glm::vec3 minBounds {0.0f};      // asset-space AABB
        glm::vec3 maxBounds {0.0f};
        std::uint32_t triangles = 0;
        bool drawable = false;
    };
    struct Component
    {
        std::int32_t node = NoIndex;
        std::string id;
        std::string moduleId;
        bool interior = false;
        std::int32_t compartment = NoIndex;
        glm::vec3 minBounds {0.0f};
        glm::vec3 maxBounds {0.0f};
        bool hasBounds = false;
        // render nodes per LOD (index = LOD index), only drawable ones
        std::vector<std::vector<std::int32_t>> renderNodes;
    };
    struct Group
    {
        std::string moduleId;
        std::vector<std::int32_t> components;
        glm::vec3 minBounds {0.0f};
        glm::vec3 maxBounds {0.0f};
        std::vector<std::int32_t> proxyRenderNodes; // in moduleProxyLod
    };

    std::vector<std::vector<RenderNodeInfo>> renderNodes; // [lod][renderNode]
    std::vector<Component> components;                  // index = semantic node
    std::vector<Group> groups;
    std::vector<std::int32_t> componentLods; // role Component, ascending level
    std::int32_t shellLod = NoIndex;
    std::int32_t proxyLod = NoIndex;
    std::int32_t moduleProxyLod = NoIndex;
    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};
    float characteristicM = 1.0f; // lodCharacteristicSize(asset bounds)
    // Module HLOD by error (an asset socket kind "hlod", extent.x = metres): a group collapses to its proxy once
    // the proxy's absolute geometric error projects under the coarsen target - a big group (a city block of the
    // hub megastructure) goes to its simplified mesh by what the eye can tell apart, not by its own size.
    float moduleProxyErrorM = 0.0f;
};

Index buildIndex(const ModelAsset& asset);

// Simulation-owned per-object values (sizes = asset.nodes.size(), or empty).
struct ObjectState
{
    std::vector<std::string> nodeState;   // "" = node.defaultStateId
    std::vector<std::uint8_t> nodeHidden; // detached / destroyed: not drawn here
    std::vector<glm::mat4> nodeMotion;    // joint / state motion, asset space
};

const std::string& currentState(const ModelAsset& asset, const ObjectState& state, std::int32_t node);

// The camera in ASSET space (meters after BUILD).
struct View
{
    glm::vec3 cameraAsset {0.0f};
    float focalPx = 1000.0f;               // viewportHeight / (2 tan(fovY/2))
    std::array<glm::vec4, 6> planes {};    // asset-space frustum planes (ax+by+cz+d >= 0 inside)
    bool cullFrustum = false;
    float assetToWorldScale = 1.0f;        // extra uniform scale applied by the object transform
    // LOD CROSS-FADE (author 2026-11-23: the LOD pop of the hub city): a switch draws the old and the new LOD together
    // for fadeSeconds, as complementary screen-door dithers (DrawItem::fade). nowSeconds < 0: no fade (instant).
    double nowSeconds = -1.0;
    float fadeSeconds = 0.4f;
    // the other variant (author 2026-11-23, «как у нас сделаны рёбра»): a BAND in screen error instead of time - while
    // the coarser LOD's error is within [coarsen, coarsen x bandFactor] both are drawn, its share growing as the error
    // falls to the switch (the same for a group's proxy against its components)
    bool fadeBand = false;
    float bandFactor = 1.6f;
};

std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& viewProjFromAsset);

// Kept between frames per object and camera (hysteresis).
struct Selection
{
    Representation representation = Representation::Culled;
    std::vector<std::int32_t> componentLod;  // chosen LOD index, NoIndex = culled
    std::vector<std::uint8_t> groupCollapsed;
    // the cross-fade: the LOD drawn before the last switch, the switch time (per component / group)
    std::vector<std::int32_t> fadeFromLod;
    std::vector<double> fadeStart;
    std::vector<std::int32_t> lastDrawnLod;   // survives a collapse (the components fade out under the proxy)
    std::vector<double> groupFadeStart;
};

struct DrawItem
{
    std::int32_t lod = 0;
    std::int32_t renderNode = 0;
    std::int32_t component = NoIndex;
    bool interior = false;
    glm::mat4 assetTransform {1.0f};
    // the cross-fade coverage: 0 = drawn whole; c in (0, 1] = the INCOMING LOD (pixels with dither < c); -c = the
    // OUTGOING one (pixels with dither >= c) - together they cover every pixel once
    float fade = 0.0f;
};

struct ComponentReport
{
    std::int32_t component = NoIndex;
    std::int32_t lod = NoIndex; // NoIndex: culled / not drawn individually
    float projectedPx = 0.0f;
    float errorPx = 0.0f;
    std::uint32_t triangles = 0; // drawn for this component this frame
    bool culled = false;
    bool collapsed = false; // drawn by its module proxy (HLOD)
    bool interior = false;
};

struct Frame
{
    Representation representation = Representation::Culled;
    float objectPx = 0.0f;
    std::vector<DrawItem> draws;
    std::vector<ComponentReport> components;
    std::vector<std::int32_t> activeOpenings;      // state-active openings
    std::vector<std::int32_t> visiblePortals;      // exterior + interior portals passed
    std::vector<std::int32_t> visibleCompartments;
    std::uint32_t triangles = 0;
    std::uint32_t interiorDraws = 0;
    std::uint32_t culledComponents = 0;
    std::uint32_t collapsedGroups = 0;
};

Frame select(const ModelAsset& asset, const Index& index, const ObjectState& state, const View& view,
             Selection& selection, const Thresholds& thresholds = {});

// State-scoped data resolved from ONE state snapshot (atomic switch): every
// list holds the indices active for the current node states.
struct ActiveSets
{
    std::vector<std::int32_t> collisionVolumes;
    std::vector<std::int32_t> hitRegions;
    std::vector<std::int32_t> openings;
    std::vector<std::int32_t> sockets;
    std::vector<std::int32_t> repairTargets;
    std::vector<std::vector<std::int32_t>> renderNodes; // [lod]
};

bool stateScopedActive(const std::vector<std::string>& activeStates, const std::string& current);
ActiveSets resolveActiveSets(const ModelAsset& asset, const ObjectState& state);

// Structural graph -> connected components. linkBroken[i] marks
// structuralLinks[i] as broken (empty = none). A node without any structural
// link hangs on its transform parent. Returns an island id per node; the
// island containing rootNode is 0, every other island is a detached body.
std::vector<std::int32_t> structuralIslands(const ModelAsset& asset, const std::vector<std::uint8_t>& linkBroken,
                                            std::int32_t rootNode = 0);

// Node local -> asset matrix (editor composeMatrix convention:
// T(p + pivot) * Rxyz * T(-pivot)).
glm::mat4 composeLocal(const glm::vec3& position, const glm::vec3& rotationDeg, const glm::vec3& pivot);

// Joint motion for a node at joint value (degrees for Revolute, travel for
// Prismatic), asset space about the joint pivot / along the joint axis.
glm::mat4 jointMotion(const NodeJoint& joint, float value);

std::string formatFrame(const ModelAsset& asset, const Index& index, const Frame& frame, const std::string& objectId);

} // namespace elite::model_asset::presentation
