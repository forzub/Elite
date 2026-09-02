#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace elite::model_asset
{

constexpr std::uint32_t ModelAssetFormatVersion = 4;
constexpr std::int32_t NoIndex = -1;

enum class SurfaceMode : std::uint8_t
{
    Closed = 0,
    ThinOneSided = 1,
    ThinTwoSided = 2
};

enum class CollisionShape : std::uint8_t
{
    Box = 0,       // Oriented box: localRotationDeg makes this an OBB.
    Sphere = 1,
    Capsule = 2,
    ConvexHull = 3 // Reserved for a later compiler stage.
};

enum class AxisDirection : std::int8_t
{
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5
};

enum class JointType : std::uint8_t
{
    Fixed = 0,
    Revolute = 1
};

enum class MassPropertyMode : std::uint8_t
{
    Disabled = 0,
    AutoFromCollision = 1,
    Manual = 2
};

enum class LightType : std::uint8_t
{
    None = 0,
    Point = 1,
    Spot = 2
};

enum EdgeFlag : std::uint32_t
{
    EdgeBoundary = 1u << 0,
    EdgeCrease = 1u << 1,
    EdgePolygonBoundary = 1u << 2,
    EdgeTriangulationInternal = 1u << 3,
    EdgeNormalSeam = 1u << 4,
    EdgeMaterialSeam = 1u << 5,
    EdgeAuthored = 1u << 6,
    // Source edge is used by more than two triangles. Persisted as a generic
    // flag so Preflight can distinguish real author-topology non-manifold
    // edges from coincident-but-separate positional seams.
    EdgeNonManifold = 1u << 7,
    // This edge adjacency was rebuilt by the editor canonical authoring pass
    // and is authoritative geometric topology. Positional coincidence must not
    // reconnect unrelated boundary sheets once this marker is present.
    EdgeCanonicalTopology = 1u << 8
};

enum EdgeRenderMask : std::uint8_t
{
    EdgeRenderTechnical = 1u << 0,
    EdgeRenderElite = 1u << 1
};

// Import metadata records what the source asset meant before compilation.
// Runtime geometry is always stored in canonical game coordinates:
// +X right, +Y up, -Z forward/nose.
struct SourceBasis
{
    std::string preset = "game_current";
    AxisDirection right = AxisDirection::PositiveX;
    AxisDirection up = AxisDirection::PositiveY;
    AxisDirection forward = AxisDirection::NegativeZ;
    bool canonicalized = true;
};

struct MaterialDefinition
{
    std::string id;          // Stable semantic id, e.g. hull_outer / emit.nav.red.
    std::string sourceName;  // Original OBJ/MTL material name.
    glm::vec4 baseColor {0.65f, 0.68f, 0.72f, 1.0f};
    glm::vec3 emissiveColor {0.0f};
    float emissiveStrength = 0.0f;
    float metallic = 0.0f;
    float roughness = 0.65f;
    bool twoSided = false;
    std::string baseColorTexture;
    std::string emissiveTexture;
};

struct Vertex
{
    glm::vec3 position {0.0f};
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec2 uv {0.0f};
};

struct Triangle
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    std::int32_t sourcePolygonId = -1;
    std::int32_t materialIndex = NoIndex;
    std::uint32_t smoothingGroupId = 0;
};

struct Edge
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::int32_t triangleA = -1;
    std::int32_t triangleB = -1;
    std::uint32_t flags = 0;
    std::uint8_t renderMask = EdgeRenderTechnical | EdgeRenderElite;
};

struct MeshLod
{
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<Edge> edges;
    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};
};

// Legacy v2/v3 authoring representation. New v4 assets do not require one
// GeometryDefinition to exist in every LOD; this container is retained only for
// source import and migration of old packages.
struct GeometryDefinition
{
    std::string id;
    std::vector<std::string> sourceLods;
    SurfaceMode surfaceMode = SurfaceMode::Closed;
    std::vector<MeshLod> lods;
};

// One geometry inside one render LOD. Unlike legacy GeometryDefinition this has
// exactly one mesh representation and does not imply any relationship to a
// geometry in another LOD.
struct RenderGeometryDefinition
{
    std::string id;
    std::string sourcePath;
    SurfaceMode surfaceMode = SurfaceMode::Closed;
    MeshLod mesh;
};

// Resolved rigid-body properties are authored offline. Runtime must never need
// to integrate a render mesh to discover these after a part breaks off.
// inertiaDiagonal = (Ixx,Iyy,Izz), inertiaProducts = (Ixy,Ixz,Iyz), all in
// node-local coordinates about centerOfMass.
struct RigidBodyProperties
{
    MassPropertyMode mode = MassPropertyMode::Disabled;
    float densityKgM3 = 780.0f;
    float massKg = 0.0f;
    glm::vec3 centerOfMass {0.0f};
    glm::vec3 inertiaDiagonal {0.0f};
    glm::vec3 inertiaProducts {0.0f};
};

// Geometric kinematics/structural attachment belong to the asset. Simulation
// stores only current angle/rate and damage state.
struct NodeJoint
{
    JointType type = JointType::Fixed;
    glm::vec3 pivot {0.0f};
    glm::vec3 axis {0.0f, 1.0f, 0.0f};
    float defaultRateDegPerSec = 0.0f;
    float minAngleDeg = -360.0f;
    float maxAngleDeg = 360.0f;
    bool breakable = false;
    float breakForceN = 0.0f;
    float breakTorqueNm = 0.0f;
};

// A node is an assembly transform. Multiple nodes may reference the same
// geometryIndex; that is the canonical representation of model instancing.
struct Node
{
    std::string id;
    std::string moduleId;
    std::int32_t parentIndex = NoIndex;
    // Legacy v2/v3 render binding only. v4 render bindings live in RenderNode
    // inside each independent RenderLod.
    std::int32_t geometryIndex = NoIndex;
    std::string defaultStateId = "intact";

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 pivot {0.0f};

    NodeJoint joint;
    RigidBodyProperties physics;
    bool enabled = true;
};



// Optional semantic state of one assembly part. Simulation owns the current
// state id; the asset describes what changes when that state is active. A state
// transition is a single semantic switch: runtime must select state-scoped
// render bindings, collision, hit regions, openings, sockets and repair targets
// atomically on the same simulation tick. A state may override the part
// transform/pivot (for a bent or partially detached section) without forcing it
// to become a separate world actor. Full detach is represented explicitly by
// detached=true.
struct StateVariant
{
    std::string id;
    std::string displayName;
    std::int32_t nodeIndex = NoIndex;

    bool transformOverride = false;
    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 pivot {0.0f};

    bool physicsOverride = false;
    RigidBodyProperties physics;
    bool detached = false;
    bool enabled = true;
};

// Render graph node local to one LOD. Different LODs may have completely
// different node counts, hierarchy, geometry pools and instancing. A render
// node can optionally bind to a semantic Node and to one or more semantic state
// ids. Empty activeStates means the render node is state-independent.
struct RenderNode
{
    std::string id;
    std::int32_t parentIndex = NoIndex;
    std::int32_t geometryIndex = NoIndex;
    std::int32_t semanticNodeIndex = NoIndex;
    std::vector<std::string> activeStates;

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 pivot {0.0f};
    bool enabled = true;
};

// Each render LOD is an independent visual document. LOD0 may be a detailed
// assembly with instances, LOD1 a welded shell, and distant LODs may be a few
// coarse primitives. No G-index or topology dependency crosses LOD boundaries.
struct RenderLod
{
    std::uint32_t level = 0;
    std::string sourceKind = "source"; // source / generated / manual
    std::int32_t generatedFromLod = NoIndex;
    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};

    // v4 manifest diagnostics are available even while the independent .elmesh
    // payload is not resident. They are runtime metadata, not a new wire field:
    // the existing LODS chunk already stores these two counts. Keeping them
    // explicitly avoids abusing vector::capacity() as hidden manifest state.
    std::uint32_t declaredGeometryCount = 0;
    std::uint32_t declaredNodeCount = 0;

    std::vector<RenderGeometryDefinition> geometries;
    std::vector<RenderNode> nodes;
};

struct CollisionVolume
{
    std::string id;
    std::string moduleId;
    std::int32_t parentNodeIndex = NoIndex;
    CollisionShape shape = CollisionShape::Box;

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 halfSize {0.5f};
    float radius = 0.5f;
    float halfHeight = 0.5f; // Capsule cylindrical half-length; local +Y axis.

    // Empty means active in every semantic state. For destructive variants,
    // intact and breached collision sets can coexist and switch atomically.
    std::vector<std::string> activeStates;
    bool enabled = true;
};

struct LightProperties
{
    LightType type = LightType::None;
    glm::vec3 color {1.0f};
    float intensity = 1.0f;
    float rangeMeters = 10.0f;
    float outerConeDeg = 35.0f;
};

// General semantic attachment point shared by ships, stations and equipment.
// kind remains a stable string so new gameplay anchor kinds do not require a
// binary-format revision. Light anchors use the optional light payload; visible
// glowing surfaces remain ordinary emissive materials.
struct Socket
{
    std::string id;
    std::string kind;
    std::string moduleId;
    std::int32_t parentNodeIndex = NoIndex;

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 extent {0.0f};
    LightProperties light;
    std::vector<std::string> activeStates;

    bool enabled = true;
};



// State-scoped damage target. It is intentionally separate from collision so
// gameplay can map health/repair logic to a semantic region without making the
// renderer or physics own damage state.
struct HitRegion
{
    std::string id;
    std::int32_t parentNodeIndex = NoIndex;
    std::vector<std::string> activeStates;
    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 halfSize {0.5f};
    bool enabled = true;
};

// Explicit traversable/line-of-fire opening created by a state such as
// "breached". Collision still determines exact physical contact; this semantic
// portal lets navigation, sensors and gameplay reason about the new hole.
struct Opening
{
    std::string id;
    std::int32_t parentNodeIndex = NoIndex;
    std::vector<std::string> activeStates;
    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 halfSize {0.5f};
    bool traversable = true;
    bool lineOfFire = true;
    bool enabled = true;
};

struct RepairTarget
{
    std::string id;
    std::string kind;
    std::int32_t parentNodeIndex = NoIndex;
    std::vector<std::string> activeStates;
    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    std::string repairedStateId;
    bool enabled = true;
};

struct ModelAsset
{
    std::uint32_t formatVersion = ModelAssetFormatVersion;
    std::string assetId;
    std::string displayName;
    std::uint16_t sourceObjectType = 0;
    float lodSwitchDistance = 2500.0f;

    SourceBasis sourceBasis;
    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};

    std::vector<MaterialDefinition> materials;

    // Semantic/gameplay assembly shared by every visual representation.
    std::vector<Node> nodes;
    std::vector<StateVariant> stateVariants;
    std::vector<CollisionVolume> collisionVolumes;
    std::vector<Socket> sockets;
    std::vector<HitRegion> hitRegions;
    std::vector<Opening> openings;
    std::vector<RepairTarget> repairTargets;

    // Independent render documents. Their hierarchy/geometry/instances do not
    // need to correspond across LOD levels.
    std::vector<RenderLod> renderLods;

    // Migration-only v2/v3 source representation. New v4 saves ignore this.
    std::vector<GeometryDefinition> geometries;
};

} // namespace elite::model_asset
