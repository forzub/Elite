#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace elite::model_asset
{

constexpr std::uint32_t ModelAssetFormatVersion = 2;
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
    EdgeAuthored = 1u << 6
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

struct GeometryDefinition
{
    std::string id;
    std::string sourceLod0;
    std::string sourceLod1;
    SurfaceMode surfaceMode = SurfaceMode::Closed;
    std::vector<MeshLod> lods;
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
    std::int32_t geometryIndex = NoIndex;

    glm::vec3 localPosition {0.0f};
    glm::vec3 localRotationDeg {0.0f};
    glm::vec3 pivot {0.0f};

    NodeJoint joint;
    RigidBodyProperties physics;
    bool enabled = true;
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
    std::vector<GeometryDefinition> geometries;
    std::vector<Node> nodes;
    std::vector<CollisionVolume> collisionVolumes;
    std::vector<Socket> sockets;
};

} // namespace elite::model_asset
