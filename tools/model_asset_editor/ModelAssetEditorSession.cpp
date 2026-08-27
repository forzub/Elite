#include "tools/model_asset_editor/ModelAssetEditorSession.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "tools/model_asset_editor/RuntimeAssemblyImporter.h"
#include "tools/model_asset_editor/GeometryInstanceFitter.h"
#include "tools/model_asset_editor/EditorVersion.h"

namespace elite::model_asset::editor
{
namespace
{
using json = nlohmann::json;
constexpr float Pi = 3.14159265358979323846f;

json vec3Json(const glm::vec3& v) { return json::array({v.x, v.y, v.z}); }
json vec4Json(const glm::vec4& v) { return json::array({v.x, v.y, v.z, v.w}); }

glm::vec3 jsonVec3(const json& value, const glm::vec3& fallback)
{
    if (!value.is_array() || value.size() != 3) return fallback;
    return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
}

double diagnosticTriangleArea(const MeshLod& lod, const Triangle& triangle)
{
    if (triangle.a >= lod.vertices.size() ||
        triangle.b >= lod.vertices.size() ||
        triangle.c >= lod.vertices.size())
        return 0.0;
    const glm::dvec3 a(lod.vertices[triangle.a].position);
    const glm::dvec3 b(lod.vertices[triangle.b].position);
    const glm::dvec3 c(lod.vertices[triangle.c].position);
    return 0.5 * glm::length(glm::cross(b - a, c - a));
}

std::uint64_t estimatedGeometryBinaryBytes(const GeometryDefinition& geometry)
{
    // Estimates the heavy .elmesh payload contribution of one geometry across all LODs.
    // Manifest metadata and file headers are intentionally excluded.
    std::uint64_t bytes = 0;
    const auto stringBytes = [](const std::string& value) -> std::uint64_t {
        return sizeof(std::uint32_t) + static_cast<std::uint64_t>(value.size());
    };
    for (const auto& lod : geometry.lods)
    {
        bytes += sizeof(std::uint32_t); // geometry index
        bytes += stringBytes(geometry.id);
        bytes += 6u * sizeof(float); // min/max bounds
        bytes += sizeof(std::uint32_t);
        bytes += static_cast<std::uint64_t>(lod.vertices.size()) * 8u * sizeof(float);
        bytes += sizeof(std::uint32_t);
        bytes += static_cast<std::uint64_t>(lod.triangles.size()) *
            (5u * sizeof(std::uint32_t) + sizeof(std::int32_t));
        bytes += sizeof(std::uint32_t);
        bytes += static_cast<std::uint64_t>(lod.edges.size()) *
            (2u * sizeof(std::uint32_t) + 2u * sizeof(std::int32_t) +
             sizeof(std::uint32_t) + sizeof(std::uint8_t));
    }
    return bytes;
}

std::filesystem::path editorSourceFilePath(
    const std::filesystem::path& sourceRoot,
    const std::string& runtimePath)
{
    if (runtimePath.empty()) return {};
    std::filesystem::path path(runtimePath);
    if (path.is_absolute()) return path;
    const auto direct = sourceRoot / path;
    if (std::filesystem::exists(direct)) return direct;
    return sourceRoot / "src" / path;
}

std::uint64_t safeFileBytes(const std::filesystem::path& path)
{
    if (path.empty()) return 0;
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<std::uint64_t>(bytes);
}

std::size_t diagnosticUniquePositionCount(const MeshLod& lod)
{
    std::vector<glm::vec3> positions;
    positions.reserve(lod.vertices.size());
    for (const auto& vertex : lod.vertices)
        positions.push_back(vertex.position);
    std::sort(positions.begin(), positions.end(), [](const glm::vec3& a, const glm::vec3& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });
    return static_cast<std::size_t>(std::distance(
        positions.begin(),
        std::unique(positions.begin(), positions.end(), [](const glm::vec3& a, const glm::vec3& b) {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        })));
}

void appendInstanceFitDiagnostic(
    const ModelAsset& asset,
    std::size_t referenceNodeIndex,
    std::size_t targetNodeIndex,
    const GeometryInstanceFit& fit) noexcept
{
    try
    {
        if (referenceNodeIndex >= asset.nodes.size() || targetNodeIndex >= asset.nodes.size())
            return;
        const Node& referenceNode = asset.nodes[referenceNodeIndex];
        const Node& targetNode = asset.nodes[targetNodeIndex];
        if (referenceNode.geometryIndex < 0 || targetNode.geometryIndex < 0)
            return;
        const auto referenceGeometryIndex = static_cast<std::size_t>(referenceNode.geometryIndex);
        const auto targetGeometryIndex = static_cast<std::size_t>(targetNode.geometryIndex);
        if (referenceGeometryIndex >= asset.geometries.size() ||
            targetGeometryIndex >= asset.geometries.size())
            return;

        const std::filesystem::path logPath =
            std::filesystem::path("build") / "logs" / "model_asset_instance_fit.log";
        std::filesystem::create_directories(logPath.parent_path());
        std::ofstream out(logPath, std::ios::app);
        if (!out)
            return;

        const auto materialName = [&](std::int32_t index) -> std::string {
            if (index < 0 || static_cast<std::size_t>(index) >= asset.materials.size())
                return std::string("<none:") + std::to_string(index) + ">";
            const auto& material = asset.materials[static_cast<std::size_t>(index)];
            return material.id + " [" + material.sourceName + "]";
        };
        const auto dumpGeometry = [&](const char* label, const GeometryDefinition& geometry) {
            out << label << " id=" << geometry.id;
            for (std::size_t sourceIndex = 0; sourceIndex < geometry.sourceLods.size(); ++sourceIndex)
                out << " lod" << sourceIndex << '=' << geometry.sourceLods[sourceIndex];
            out << '\n';
            for (std::size_t li = 0; li < geometry.lods.size(); ++li)
            {
                const MeshLod& lod = geometry.lods[li];
                std::map<std::int32_t, double> materialArea;
                double totalArea = 0.0;
                for (const Triangle& triangle : lod.triangles)
                {
                    const double area = diagnosticTriangleArea(lod, triangle);
                    totalArea += area;
                    materialArea[triangle.materialIndex] += area;
                }
                out << "  LOD" << li
                    << " vertices=" << lod.vertices.size()
                    << " unique_positions=" << diagnosticUniquePositionCount(lod)
                    << " triangles=" << lod.triangles.size()
                    << " edges=" << lod.edges.size()
                    << " area=" << std::setprecision(12) << totalArea
                    << " bounds_min=(" << lod.minBounds.x << ',' << lod.minBounds.y << ',' << lod.minBounds.z << ')'
                    << " bounds_max=(" << lod.maxBounds.x << ',' << lod.maxBounds.y << ',' << lod.maxBounds.z << ")\n";
                for (const auto& [materialIndex, area] : materialArea)
                    out << "    material " << materialIndex << " " << materialName(materialIndex)
                        << " area=" << std::setprecision(12) << area << '\n';
            }
        };

        out << "\n=== INSTANCE FIT ===\n";
        out << "reference node=" << referenceNode.id << " G" << referenceGeometryIndex << '\n';
        out << "target    node=" << targetNode.id << " G" << targetGeometryIndex << '\n';
        out << "identity_basis=LOD0; lower_lods_are_independent_and_not_fit_gates\n";
        out << "result valid=" << fit.valid
            << " geometry_matched=" << fit.geometryMatched
            << " material_compatible=" << fit.materialCompatible
            << " message=" << fit.message << '\n';
        out << "rms_m=" << fit.rmsErrorMeters
            << " max_m=" << fit.maxErrorMeters
            << " tolerance_m=" << fit.toleranceMeters
            << " compared_vertices=" << fit.comparedVertices << '\n';
        out << "translation=(" << fit.translation.x << ',' << fit.translation.y << ',' << fit.translation.z << ")\n";
        out << "rotation_columns:\n";
        for (int column = 0; column < 3; ++column)
            out << "  (" << fit.rotation[column].x << ',' << fit.rotation[column].y << ',' << fit.rotation[column].z << ")\n";
        dumpGeometry("REFERENCE", asset.geometries[referenceGeometryIndex]);
        dumpGeometry("TARGET", asset.geometries[targetGeometryIndex]);
        out << "=== END INSTANCE FIT ===\n";
    }
    catch (...)
    {
        // Diagnostics must never alter editor behaviour.
    }
}


const char* surfaceModeName(SurfaceMode mode)
{
    switch (mode)
    {
        case SurfaceMode::Closed: return "closed";
        case SurfaceMode::ThinOneSided: return "thin_one_sided";
        case SurfaceMode::ThinTwoSided: return "thin_two_sided";
    }
    return "closed";
}
SurfaceMode surfaceModeFromName(const std::string& value)
{
    if (value == "thin_one_sided") return SurfaceMode::ThinOneSided;
    if (value == "thin_two_sided") return SurfaceMode::ThinTwoSided;
    return SurfaceMode::Closed;
}

const char* collisionShapeName(CollisionShape shape)
{
    switch (shape)
    {
        case CollisionShape::Box: return "box";
        case CollisionShape::Sphere: return "sphere";
        case CollisionShape::Capsule: return "capsule";
        case CollisionShape::ConvexHull: return "convex_hull";
    }
    return "box";
}
CollisionShape collisionShapeFromName(const std::string& value)
{
    if (value == "sphere") return CollisionShape::Sphere;
    if (value == "capsule") return CollisionShape::Capsule;
    if (value == "convex_hull") return CollisionShape::ConvexHull;
    return CollisionShape::Box;
}

const char* jointTypeName(JointType type)
{
    return type == JointType::Revolute ? "revolute" : "fixed";
}
JointType jointTypeFromName(const std::string& value)
{
    return value == "revolute" ? JointType::Revolute : JointType::Fixed;
}

const char* massModeName(MassPropertyMode mode)
{
    switch (mode)
    {
        case MassPropertyMode::Disabled: return "disabled";
        case MassPropertyMode::AutoFromCollision: return "auto_collision";
        case MassPropertyMode::Manual: return "manual";
    }
    return "disabled";
}
MassPropertyMode massModeFromName(const std::string& value)
{
    if (value == "auto_collision") return MassPropertyMode::AutoFromCollision;
    if (value == "manual") return MassPropertyMode::Manual;
    return MassPropertyMode::Disabled;
}

const char* lightTypeName(LightType type)
{
    switch (type)
    {
        case LightType::None: return "none";
        case LightType::Point: return "point";
        case LightType::Spot: return "spot";
    }
    return "none";
}
LightType lightTypeFromName(const std::string& value)
{
    if (value == "point") return LightType::Point;
    if (value == "spot") return LightType::Spot;
    return LightType::None;
}


glm::mat3 eulerRotation(const glm::vec3& deg)
{
    const glm::mat4 m = glm::eulerAngleXYZ(
        glm::radians(deg.x), glm::radians(deg.y), glm::radians(deg.z));
    return glm::mat3(m);
}

glm::vec3 eulerDegrees(const glm::quat& q)
{
    return glm::degrees(glm::eulerAngles(glm::normalize(q)));
}


struct RigidTransform
{
    glm::mat3 rotation {1.0f};
    glm::vec3 translation {0.0f};
};

RigidTransform nodeRigidTransform(const Node& node)
{
    RigidTransform out;
    out.rotation = eulerRotation(node.localRotationDeg);
    out.translation = node.localPosition + node.pivot - out.rotation * node.pivot;
    return out;
}

RigidTransform composeRigid(const RigidTransform& a, const RigidTransform& b)
{
    return {
        a.rotation * b.rotation,
        a.rotation * b.translation + a.translation
    };
}

RigidTransform inverseRigid(const RigidTransform& value)
{
    const glm::mat3 r = glm::transpose(value.rotation);
    return {r, -(r * value.translation)};
}

glm::vec3 transformPoint(const RigidTransform& transform, const glm::vec3& point)
{
    return transform.rotation * point + transform.translation;
}

glm::vec3 transformDirection(const RigidTransform& transform, const glm::vec3& direction)
{
    const glm::vec3 mapped = transform.rotation * direction;
    const float n2 = glm::dot(mapped, mapped);
    return n2 > 1.0e-12f ? mapped / std::sqrt(n2) : direction;
}

void setNodeRigidTransform(Node& node, const RigidTransform& transform, const glm::vec3& pivot)
{
    node.pivot = pivot;
    node.localRotationDeg = eulerDegrees(glm::quat_cast(transform.rotation));
    node.localPosition = transform.translation - pivot + transform.rotation * pivot;
}

glm::mat3 inertiaMatrix(const RigidBodyProperties& physics)
{
    glm::mat3 out(0.0f);
    out[0][0] = physics.inertiaDiagonal.x;
    out[1][1] = physics.inertiaDiagonal.y;
    out[2][2] = physics.inertiaDiagonal.z;
    out[0][1] = out[1][0] = physics.inertiaProducts.x;
    out[0][2] = out[2][0] = physics.inertiaProducts.y;
    out[1][2] = out[2][1] = physics.inertiaProducts.z;
    return out;
}

void storeInertiaMatrix(RigidBodyProperties& physics, const glm::mat3& inertia)
{
    physics.inertiaDiagonal = {
        inertia[0][0],
        inertia[1][1],
        inertia[2][2]
    };
    physics.inertiaProducts = {
        inertia[0][1],
        inertia[0][2],
        inertia[1][2]
    };
}

// Re-express node-local authoring data after a baked geometry transform F has
// been moved from the mesh into the node transform. World-space placement must
// remain unchanged: newLocal = inverse(F) * oldLocal.
void rebaseNodeLocalData(ModelAsset& asset, std::size_t nodeIndex, const RigidTransform& inverseFit)
{
    auto& node = asset.nodes[nodeIndex];

    node.joint.pivot = transformPoint(inverseFit, node.joint.pivot);
    node.joint.axis = transformDirection(inverseFit, node.joint.axis);

    node.physics.centerOfMass = transformPoint(inverseFit, node.physics.centerOfMass);
    const glm::mat3 oldInertia = inertiaMatrix(node.physics);
    const glm::mat3 newInertia =
        inverseFit.rotation * oldInertia * glm::transpose(inverseFit.rotation);
    storeInertiaMatrix(node.physics, newInertia);

    for (auto& collision : asset.collisionVolumes)
    {
        if (collision.parentNodeIndex != static_cast<std::int32_t>(nodeIndex))
            continue;
        collision.localPosition = transformPoint(inverseFit, collision.localPosition);
        const glm::mat3 oldRotation = eulerRotation(collision.localRotationDeg);
        collision.localRotationDeg = eulerDegrees(
            glm::quat_cast(inverseFit.rotation * oldRotation));
    }

    for (auto& socket : asset.sockets)
    {
        if (socket.parentNodeIndex != static_cast<std::int32_t>(nodeIndex))
            continue;
        socket.localPosition = transformPoint(inverseFit, socket.localPosition);
        const glm::mat3 oldRotation = eulerRotation(socket.localRotationDeg);
        socket.localRotationDeg = eulerDegrees(
            glm::quat_cast(inverseFit.rotation * oldRotation));
    }

    // Direct child transforms are expressed in this node's local coordinate
    // system. Rebase only their relationship to the parent; their own local
    // collision/socket/joint metadata remains in the child's unchanged frame.
    for (std::size_t childIndex = 0; childIndex < asset.nodes.size(); ++childIndex)
    {
        auto& child = asset.nodes[childIndex];
        if (child.parentIndex != static_cast<std::int32_t>(nodeIndex))
            continue;
        const RigidTransform rebased =
            composeRigid(inverseFit, nodeRigidTransform(child));
        setNodeRigidTransform(child, rebased, child.pivot);
    }
}

struct PrimitiveMass
{
    float mass = 0.0f;
    glm::vec3 center {0.0f};
    glm::mat3 inertia {0.0f};
};

PrimitiveMass primitiveMass(const CollisionVolume& c, float density)
{
    PrimitiveMass out;
    out.center = c.localPosition;
    glm::vec3 diag(0.0f);

    if (c.shape == CollisionShape::Sphere)
    {
        const float r = std::max(c.radius, 0.001f);
        const float volume = (4.0f / 3.0f) * Pi * r * r * r;
        out.mass = density * volume;
        diag = glm::vec3(0.4f * out.mass * r * r);
    }
    else if (c.shape == CollisionShape::Capsule)
    {
        const float r = std::max(c.radius, 0.001f);
        const float half = std::max(c.halfHeight, 0.0f);
        const float length = 2.0f * half;
        const float cylVolume = Pi * r * r * length;
        const float sphereVolume = (4.0f / 3.0f) * Pi * r * r * r;
        const float mc = density * cylVolume;
        const float ms = density * sphereVolume;
        out.mass = mc + ms;
        const float iAxis = 0.5f * mc * r * r + 0.4f * ms * r * r;
        const float iSide = mc * (3.0f * r * r + length * length) / 12.0f +
            0.4f * ms * r * r + ms * half * half;
        diag = glm::vec3(iSide, iAxis, iSide); // local capsule axis = +Y
    }
    else
    {
        const glm::vec3 h = glm::max(c.halfSize, glm::vec3(0.001f));
        out.mass = density * (8.0f * h.x * h.y * h.z);
        diag.x = (out.mass / 3.0f) * (h.y * h.y + h.z * h.z);
        diag.y = (out.mass / 3.0f) * (h.x * h.x + h.z * h.z);
        diag.z = (out.mass / 3.0f) * (h.x * h.x + h.y * h.y);
    }

    glm::mat3 local(0.0f);
    local[0][0] = diag.x; local[1][1] = diag.y; local[2][2] = diag.z;
    const glm::mat3 r = eulerRotation(c.localRotationDeg);
    out.inertia = r * local * glm::transpose(r);
    return out;
}

bool estimatePhysicsFromCollision(ModelAsset& asset, std::size_t nodeIndex, float density)
{
    if (nodeIndex >= asset.nodes.size()) return false;
    std::vector<PrimitiveMass> pieces;
    for (const auto& collision : asset.collisionVolumes)
    {
        if (collision.enabled && collision.parentNodeIndex == static_cast<std::int32_t>(nodeIndex))
            pieces.push_back(primitiveMass(collision, density));
    }
    if (pieces.empty()) return false;

    float totalMass = 0.0f;
    glm::vec3 com(0.0f);
    for (const auto& p : pieces) { totalMass += p.mass; com += p.center * p.mass; }
    if (totalMass <= 1.0e-6f) return false;
    com /= totalMass;

    glm::mat3 inertia(0.0f);
    const glm::mat3 identity(1.0f);
    for (const auto& p : pieces)
    {
        const glm::vec3 d = p.center - com;
        const float d2 = glm::dot(d, d);
        inertia += p.inertia + p.mass * (d2 * identity - glm::outerProduct(d, d));
    }

    auto& physics = asset.nodes[nodeIndex].physics;
    physics.mode = MassPropertyMode::AutoFromCollision;
    physics.densityKgM3 = density;
    physics.massKg = totalMass;
    physics.centerOfMass = com;
    physics.inertiaDiagonal = glm::vec3(inertia[0][0], inertia[1][1], inertia[2][2]);
    physics.inertiaProducts = glm::vec3(inertia[0][1], inertia[0][2], inertia[1][2]);
    return true;
}


glm::vec3 axisVector(AxisDirection axis)
{
    switch (axis)
    {
        case AxisDirection::PositiveX: return {1,0,0};
        case AxisDirection::NegativeX: return {-1,0,0};
        case AxisDirection::PositiveY: return {0,1,0};
        case AxisDirection::NegativeY: return {0,-1,0};
        case AxisDirection::PositiveZ: return {0,0,1};
        case AxisDirection::NegativeZ: return {0,0,-1};
    }
    return {1,0,0};
}

SourceBasis basisPreset(const std::string& preset)
{
    SourceBasis basis;
    basis.preset = preset;
    if (preset == "blender_model")
    {
        basis.right = AxisDirection::PositiveX;
        basis.up = AxisDirection::PositiveZ;
        basis.forward = AxisDirection::NegativeY;
    }
    else
    {
        basis.preset = "game_current";
        basis.right = AxisDirection::PositiveX;
        basis.up = AxisDirection::PositiveY;
        basis.forward = AxisDirection::NegativeZ;
    }
    basis.canonicalized = true;
    return basis;
}

glm::mat3 sourceToCanonical(const SourceBasis& source)
{
    const glm::mat3 sourceSemantic(
        axisVector(source.right),
        axisVector(source.up),
        axisVector(source.forward));
    const glm::mat3 canonicalSemantic(
        glm::vec3(1,0,0),
        glm::vec3(0,1,0),
        glm::vec3(0,0,-1));
    return canonicalSemantic * glm::transpose(sourceSemantic);
}

glm::vec3 convertEuler(const glm::vec3& deg, const glm::mat3& basis)
{
    const glm::mat3 source = eulerRotation(deg);
    const glm::mat3 converted = basis * source * glm::transpose(basis);
    return eulerDegrees(glm::quat_cast(converted));
}

void recomputeLodBounds(MeshLod& lod)
{
    if (lod.vertices.empty()) { lod.minBounds = glm::vec3(0); lod.maxBounds = glm::vec3(0); return; }
    lod.minBounds = glm::vec3(std::numeric_limits<float>::max());
    lod.maxBounds = glm::vec3(-std::numeric_limits<float>::max());
    for (const auto& v : lod.vertices)
    {
        lod.minBounds = glm::min(lod.minBounds, v.position);
        lod.maxBounds = glm::max(lod.maxBounds, v.position);
    }
}

void convertAssetBasisToCanonical(ModelAsset& asset, const SourceBasis& source)
{
    const glm::mat3 basis = sourceToCanonical(source);
    const bool flipWinding = glm::determinant(basis) < 0.0f;

    for (auto& geometry : asset.geometries)
    {
        for (auto& lod : geometry.lods)
        {
            for (auto& v : lod.vertices)
            {
                v.position = basis * v.position;
                const glm::vec3 n = basis * v.normal;
                if (glm::dot(n, n) > 1.0e-12f) v.normal = glm::normalize(n);
            }
            if (flipWinding)
                for (auto& t : lod.triangles) std::swap(t.b, t.c);
            recomputeLodBounds(lod);
        }
    }

    for (auto& n : asset.nodes)
    {
        n.localPosition = basis * n.localPosition;
        n.localRotationDeg = convertEuler(n.localRotationDeg, basis);
        n.pivot = basis * n.pivot;
        n.joint.pivot = basis * n.joint.pivot;
        n.joint.axis = glm::normalize(basis * n.joint.axis);
        n.physics.centerOfMass = basis * n.physics.centerOfMass;
        glm::mat3 inertia(0.0f);
        inertia[0][0] = n.physics.inertiaDiagonal.x;
        inertia[1][1] = n.physics.inertiaDiagonal.y;
        inertia[2][2] = n.physics.inertiaDiagonal.z;
        inertia[0][1] = inertia[1][0] = n.physics.inertiaProducts.x;
        inertia[0][2] = inertia[2][0] = n.physics.inertiaProducts.y;
        inertia[1][2] = inertia[2][1] = n.physics.inertiaProducts.z;
        inertia = basis * inertia * glm::transpose(basis);
        n.physics.inertiaDiagonal = {inertia[0][0], inertia[1][1], inertia[2][2]};
        n.physics.inertiaProducts = {inertia[0][1], inertia[0][2], inertia[1][2]};
    }
    for (auto& c : asset.collisionVolumes)
    {
        c.localPosition = basis * c.localPosition;
        c.localRotationDeg = convertEuler(c.localRotationDeg, basis);
    }
    for (auto& socket : asset.sockets)
    {
        socket.localPosition = basis * socket.localPosition;
        socket.localRotationDeg = convertEuler(socket.localRotationDeg, basis);
    }

    if (!asset.geometries.empty())
    {
        asset.minBounds = glm::vec3(std::numeric_limits<float>::max());
        asset.maxBounds = glm::vec3(-std::numeric_limits<float>::max());
        for (const auto& geometry : asset.geometries)
            if (!geometry.lods.empty())
            {
                asset.minBounds = glm::min(asset.minBounds, geometry.lods.front().minBounds);
                asset.maxBounds = glm::max(asset.maxBounds, geometry.lods.front().maxBounds);
            }
    }
    asset.sourceBasis = source;
    asset.sourceBasis.canonicalized = true;
}

void remapGeometryAfterErase(ModelAsset& asset, std::size_t erased)
{
    for (auto& node : asset.nodes)
    {
        if (node.geometryIndex == static_cast<std::int32_t>(erased)) node.geometryIndex = NoIndex;
        else if (node.geometryIndex > static_cast<std::int32_t>(erased)) --node.geometryIndex;
    }
}


std::string uniqueGeometryId(const ModelAsset& asset, std::string desired)
{
    if (desired.empty()) desired = "geometry";
    const auto exists = [&](const std::string& id) {
        return std::any_of(asset.geometries.begin(), asset.geometries.end(),
            [&](const GeometryDefinition& geometry) { return geometry.id == id; });
    };
    if (!exists(desired)) return desired;
    const std::string base = desired;
    for (std::size_t suffix = 2; ; ++suffix)
    {
        desired = base + "_" + std::to_string(suffix);
        if (!exists(desired)) return desired;
    }
}
}

ModelAssetEditorSession::ModelAssetEditorSession(
    std::filesystem::path sourceRoot,
    HtmlUiServer& server
)
    : m_sourceRoot(std::move(sourceRoot)), m_server(server)
{
    m_catalog = {
        {"cobra_mk1", "Cobra Mk.I", ObjectType::CobraMk1},
        {"station", "Orbital Station", ObjectType::Station},
        {"repair_drone_debug", "Repair Drone", ObjectType::RepairDroneDebug},
        {"guidance_dock_cube", "Guidance Dock Cube", ObjectType::GuidanceDockCube},
        {"guidance_dock_cylinder", "Guidance Dock Cylinder", ObjectType::GuidanceDockCylinder}
    };
}

std::filesystem::path ModelAssetEditorSession::compiledPath(const std::string& id) const
{
    return m_sourceRoot / "src" / "assets" / "compiled" / "models" / id / (id + ".elmodel");
}

std::filesystem::path ModelAssetEditorSession::legacyCompiledPath(const std::string& id) const
{
    return m_sourceRoot / "src" / "assets" / "compiled" / "models" / (id + ".elmodel");
}


std::size_t ModelAssetEditorSession::lodCount() const
{
    std::size_t count = 0;
    for (const auto& geometry : m_asset.geometries)
        count = std::max(count, geometry.lods.size());
    return count;
}

void ModelAssetEditorSession::syncDirty()
{
    m_dirty = m_manifestDirty || std::any_of(
        m_lodState.begin(), m_lodState.end(), [](const LodEditState& state) { return state.dirty; });
}

void ModelAssetEditorSession::resetLodState(bool loaded, bool dirty)
{
    m_lodState.assign(lodCount(), LodEditState{loaded, dirty});
    m_manifestDirty = dirty;
    syncDirty();
}

void ModelAssetEditorSession::markManifestDirty()
{
    m_manifestDirty = true;
    syncDirty();
}

void ModelAssetEditorSession::markLodDirty(std::size_t lodIndex)
{
    if (lodIndex >= m_lodState.size())
        m_lodState.resize(lodIndex + 1);
    m_lodState[lodIndex].loaded = true;
    m_lodState[lodIndex].dirty = true;
    syncDirty();
}

void ModelAssetEditorSession::markAllLoadedLodsDirty()
{
    for (auto& state : m_lodState)
        if (state.loaded)
            state.dirty = true;
    syncDirty();
}

bool ModelAssetEditorSession::loadLodOnly(std::size_t lodIndex, bool forceReload)
{
    if (m_selectedId.empty() || m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return false;
    }
    if (lodIndex >= m_lodState.size())
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " is not declared by this asset", true);
        return false;
    }
    auto& state = m_lodState[lodIndex];
    if (state.loaded && !forceReload)
    {
        sendStatus("NO CHANGES: LOD" + std::to_string(lodIndex) + " is already loaded");
        return true;
    }
    if (state.dirty && !forceReload)
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " has unsaved changes; use Reload LOD only after confirming discard", true);
        return false;
    }

    const auto manifest = compiledPath(m_selectedId);
    if (!std::filesystem::exists(manifest))
    {
        sendStatus("Cannot load independent LOD before the v3 package has been saved", true);
        return false;
    }
    std::string error;
    const auto lodPath = ModelAssetBinary::lodPayloadPath(manifest.string(), lodIndex);
    sendStatus((forceReload ? "Reloading " : "Loading ") + std::string("LOD") + std::to_string(lodIndex) + "...", false, "reading");
    sendProgress("reading", forceReload ? "RELOAD LOD" : "LOAD LOD", 0, 1, lodPath);
    if (!ModelAssetBinary::loadLod(manifest.string(), m_asset, lodIndex, &error))
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " load failed: " + error, true);
        return false;
    }
    state.loaded = true;
    state.dirty = false;
    syncDirty();
    sendProgress("reading", forceReload ? "RELOAD LOD" : "LOAD LOD", 1, 1, lodPath);
    sendAsset();
    sendStatus(std::string(forceReload ? "Reloaded " : "Loaded ") + "LOD" + std::to_string(lodIndex) + " from " + lodPath.filename().string());
    return true;
}

bool ModelAssetEditorSession::ensureLodLoaded(std::size_t lodIndex)
{
    if (lodIndex >= m_lodState.size())
        return false;
    if (m_lodState[lodIndex].loaded)
        return true;
    return loadLodOnly(lodIndex, false);
}

bool ModelAssetEditorSession::ensureAllLodsLoaded()
{
    for (std::size_t lodIndex = 0; lodIndex < m_lodState.size(); ++lodIndex)
        if (!m_lodState[lodIndex].loaded && !loadLodOnly(lodIndex, false))
            return false;
    return true;
}

bool ModelAssetEditorSession::unloadLod(std::size_t lodIndex)
{
    if (lodIndex >= m_lodState.size())
    {
        sendStatus("Invalid LOD index", true);
        return false;
    }
    auto& state = m_lodState[lodIndex];
    if (!state.loaded)
    {
        sendStatus("NO CHANGES: LOD" + std::to_string(lodIndex) + " is already unloaded");
        return true;
    }
    if (state.dirty)
    {
        sendStatus("Cannot unload dirty LOD" + std::to_string(lodIndex) + "; save or reload it first", true);
        return false;
    }
    for (auto& geometry : m_asset.geometries)
    {
        if (lodIndex >= geometry.lods.size()) continue;
        auto& lod = geometry.lods[lodIndex];
        lod.vertices.clear();
        lod.triangles.clear();
        lod.edges.clear();
    }
    state.loaded = false;
    syncDirty();
    sendAsset();
    sendStatus("Unloaded LOD" + std::to_string(lodIndex) + " from editor memory; file on disk unchanged");
    return true;
}

bool ModelAssetEditorSession::saveManifestOnly()
{
    if (m_selectedId.empty() || m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return false;
    }
    const auto path = compiledPath(m_selectedId);
    const bool fileExists = std::filesystem::exists(path);
    const bool anyLodDirty = std::any_of(
        m_lodState.begin(), m_lodState.end(), [](const LodEditState& state) { return state.dirty; });
    if (anyLodDirty)
    {
        sendStatus("Cannot save manifest alone while a LOD payload is DIRTY; use Save all to keep the package coherent", true);
        return false;
    }
    if (!m_manifestDirty && fileExists)
    {
        sendStatus("NO CHANGES: manifest is clean");
        return true;
    }
    std::string error;
    m_asset.formatVersion = ModelAssetFormatVersion;
    sendStatus("Writing semantic manifest " + path.filename().string() + "...", false, "writing");
    sendProgress("writing", "SAVE MANIFEST", 0, 1, path);
    if (!ModelAssetBinary::saveManifest(path.string(), m_asset, &error))
    {
        sendStatus("Manifest save failed: " + error, true);
        return false;
    }
    m_manifestDirty = false;
    syncDirty();
    sendProgress("writing", "SAVE MANIFEST", 1, 1, path);
    sendAsset();
    sendStatus("Saved manifest only; LOD payload files and source OBJ/assembly unchanged");
    sendCatalog();
    return true;
}

bool ModelAssetEditorSession::saveLodOnly(std::size_t lodIndex)
{
    if (lodIndex >= m_lodState.size())
    {
        sendStatus("Invalid LOD index", true);
        return false;
    }
    auto& state = m_lodState[lodIndex];
    if (m_manifestDirty)
    {
        sendStatus("Cannot save one LOD while the semantic manifest is DIRTY; use Save all to keep cross-file identity coherent", true);
        return false;
    }
    if (!state.loaded)
    {
        sendStatus("Cannot save unloaded LOD" + std::to_string(lodIndex), true);
        return false;
    }
    const auto manifest = compiledPath(m_selectedId);
    const auto lodPath = ModelAssetBinary::lodPayloadPath(manifest.string(), lodIndex);
    if (!state.dirty && std::filesystem::exists(lodPath))
    {
        sendStatus("NO CHANGES: LOD" + std::to_string(lodIndex) + " is clean");
        return true;
    }
    std::string error;
    sendStatus("Writing LOD" + std::to_string(lodIndex) + " " + lodPath.filename().string() + "...", false, "writing");
    sendProgress("writing", "SAVE LOD" + std::to_string(lodIndex), 0, 1, lodPath);
    if (!ModelAssetBinary::saveLod(manifest.string(), m_asset, lodIndex, &error))
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " save failed: " + error, true);
        return false;
    }
    state.dirty = false;
    syncDirty();
    sendProgress("writing", "SAVE LOD" + std::to_string(lodIndex), 1, 1, lodPath);
    sendAsset();
    sendStatus("Saved LOD" + std::to_string(lodIndex) + " only; manifest, other LOD files and source OBJ/assembly unchanged");
    return true;
}

void ModelAssetEditorSession::sendStatus(
    const std::string& message,
    bool error,
    const std::string& activity
)
{
    json payload = {
        {"type", "status"}, {"message", message}, {"error", error},
        {"dirty", m_dirty}, {"activity", error ? "error" : activity}
    };
    if (!m_selectedId.empty())
    {
        const auto path = compiledPath(m_selectedId);
        payload["path"] = path.generic_string();
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
            payload["bytes"] = std::filesystem::file_size(path, ec);
    }
    m_server.broadcastText(payload.dump());
}


void ModelAssetEditorSession::sendProgress(
    const std::string& activity,
    const std::string& stage,
    std::size_t completed,
    std::size_t total,
    const std::filesystem::path& path
)
{
    m_server.broadcastText(json({
        {"type", "progress"},
        {"activity", activity},
        {"stage", stage},
        {"completed", completed},
        {"total", total},
        {"path", path.empty() ? std::string() : path.generic_string()},
        {"dirty", m_dirty}
    }).dump());
}

void ModelAssetEditorSession::sendCatalog()
{
    json items = json::array();
    for (const auto& entry : m_catalog)
    {
        const bool splitV3 = std::filesystem::exists(compiledPath(entry.id));
        const bool legacyV2 = std::filesystem::exists(legacyCompiledPath(entry.id));
        items.push_back({
            {"id", entry.id},
            {"displayName", entry.displayName},
            {"compiled", splitV3 || legacyV2},
            {"compiledV3", splitV3},
            {"legacyV2", !splitV3 && legacyV2}
        });
    }
    m_server.broadcastText(json({
        {"type", "catalog"},
        {"items", items},
        {"editorVersion", ModelAssetEditorVersion},
        {"assetFormatVersion", ModelAssetFormatVersion}
    }).dump());
}

bool ModelAssetEditorSession::selectAsset(const std::string& id, bool forceReimport)
{
    const auto it = std::find_if(m_catalog.begin(), m_catalog.end(), [&](const CatalogEntry& e) { return e.id == id; });
    if (it == m_catalog.end()) { sendStatus("Unknown asset id: " + id, true); return false; }

    m_selectedId = id;
    ModelAsset loaded;
    std::string error;
    std::string warning;
    const auto binary = compiledPath(id);
    const auto legacyBinary = legacyCompiledPath(id);
    const bool haveV3 = std::filesystem::exists(binary);
    const bool haveLegacyV2 = !haveV3 && std::filesystem::exists(legacyBinary);
    const auto readPath = haveV3 ? binary : legacyBinary;
    const auto importProgress = [&](const ImportProgress& update)
    {
        sendProgress("reading", update.stage, update.completed, update.total, update.path);
    };

    if (!forceReimport && (haveV3 || haveLegacyV2))
    {
        sendStatus(
            std::string(haveLegacyV2 ? "Reading legacy v2 " : "Reading v3 manifest ") +
                readPath.filename().string() + "...",
            false, "reading");
        sendProgress("reading", haveLegacyV2 ? "READ LEGACY V2" : "READ V3 MANIFEST", 0, 1, readPath);
        bool loadedLegacyV2 = false;
        if (!ModelAssetBinary::loadManifest(readPath.string(), loaded, &loadedLegacyV2, &error))
        {
            if (error == "unsupported model asset version")
            {
                sendStatus("Compiled asset format is obsolete; reimporting source...", false, "reading");
                if (!importRuntimeAssembly(
                        m_sourceRoot, it->type, it->id, it->displayName, loaded,
                        &error, &warning, importProgress))
                {
                    sendStatus("Cannot reimport obsolete compiled asset: " + error, true);
                    return false;
                }
                loaded.formatVersion = ModelAssetFormatVersion;
                m_asset = std::move(loaded);
                resetLodState(true, true);
            }
            else
            {
                sendStatus("Cannot load compiled asset: " + error, true);
                return false;
            }
        }
        else
        {
            sendProgress("reading", haveLegacyV2 ? "READ LEGACY V2" : "READ V3 MANIFEST", 1, 1, readPath);
            m_asset = std::move(loaded);
            if (loadedLegacyV2)
            {
                // v2 is monolithic, therefore every imported LOD is resident.
                // The first v3 Save all writes independent files and leaves v2 untouched.
                m_asset.formatVersion = ModelAssetFormatVersion;
                resetLodState(true, true);
                warning = "Legacy .elmodel v2 loaded for migration. Save all writes a v3 manifest + independent LOD files; the legacy v2 file is left unchanged.";
            }
            else
            {
                // v3 opens the semantic manifest first. Only LOD0 is pulled into
                // memory by default; every other .elmesh remains independently loadable.
                resetLodState(false, false);
                if (!m_lodState.empty())
                {
                    const auto lod0Path = ModelAssetBinary::lodPayloadPath(binary.string(), 0);
                    sendProgress("reading", "LOAD LOD0", 0, 1, lod0Path);
                    if (!ModelAssetBinary::loadLod(binary.string(), m_asset, 0, &error))
                    {
                        sendStatus("Cannot load LOD0 payload: " + error, true);
                        return false;
                    }
                    m_lodState[0].loaded = true;
                    sendProgress("reading", "LOAD LOD0", 1, 1, lod0Path);
                }
                syncDirty();
            }
        }
    }
    else
    {
        sendStatus("Importing source OBJ/assembly...", false, "reading");
        if (!importRuntimeAssembly(
                m_sourceRoot, it->type, it->id, it->displayName, loaded,
                &error, &warning, importProgress))
        {
            sendStatus("Cannot import source assembly: " + error, true);
            return false;
        }
        loaded.formatVersion = ModelAssetFormatVersion;
        m_asset = std::move(loaded);
        // Source import has no v3 package yet: all imported representations are
        // resident and dirty until explicitly saved.
        resetLodState(true, true);
    }

    sendProgress("reading", "LOAD VIEW", 0, 1, readPath.empty() ? binary : readPath);
    sendAsset();
    if (!warning.empty())
        sendStatus(warning);
    else
        sendStatus(forceReimport ? "Source assembly imported" : "Asset loaded; LOD files can be loaded/saved independently");
    return true;
}

bool ModelAssetEditorSession::saveAsset()
{
    if (m_selectedId.empty() || m_asset.assetId.empty()) { sendStatus("No asset selected", true); return false; }
    syncDirty();
    const auto path = compiledPath(m_selectedId);
    const bool manifestExists = std::filesystem::exists(path);
    bool anyLodFileMissing = false;
    for (std::size_t i = 0; i < m_lodState.size(); ++i)
        if (!std::filesystem::exists(ModelAssetBinary::lodPayloadPath(path.string(), i)))
            anyLodFileMissing = true;
    if (!m_dirty && manifestExists && !anyLodFileMissing)
    {
        sendStatus("NO CHANGES: manifest and LOD payloads are clean");
        return true;
    }

    // A missing payload can only be created from resident geometry. Existing
    // clean/unloaded payloads are deliberately not read or rewritten.
    for (std::size_t i = 0; i < m_lodState.size(); ++i)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(path.string(), i);
        if (!std::filesystem::exists(lodPath) && !m_lodState[i].loaded)
        {
            sendStatus("Save all cannot create missing unloaded LOD" + std::to_string(i) + "; load or reimport it first", true);
            return false;
        }
    }

    m_asset.formatVersion = ModelAssetFormatVersion;
    std::size_t work = (m_manifestDirty || !manifestExists) ? 1u : 0u;
    for (std::size_t i = 0; i < m_lodState.size(); ++i)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(path.string(), i);
        if (m_lodState[i].dirty || !std::filesystem::exists(lodPath)) ++work;
    }
    std::size_t completed = 0;
    std::string error;
    sendStatus("Saving dirty v3 package members...", false, "writing");

    for (std::size_t i = 0; i < m_lodState.size(); ++i)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(path.string(), i);
        if (!m_lodState[i].dirty && std::filesystem::exists(lodPath)) continue;
        sendProgress("writing", "SAVE LOD" + std::to_string(i), completed, work, lodPath);
        if (!ModelAssetBinary::saveLod(path.string(), m_asset, i, &error))
        {
            sendStatus("Save all failed at LOD" + std::to_string(i) + ": " + error, true);
            return false;
        }
        m_lodState[i].dirty = false;
        ++completed;
    }

    // Manifest is committed after payload edits so its descriptors never get
    // ahead of newly-written geometry files.
    if (m_manifestDirty || !manifestExists)
    {
        sendProgress("writing", "SAVE MANIFEST", completed, work, path);
        if (!ModelAssetBinary::saveManifest(path.string(), m_asset, &error))
        {
            sendStatus("Save all failed at manifest: " + error, true);
            return false;
        }
        m_manifestDirty = false;
        ++completed;
    }
    if (!ModelAssetBinary::pruneStaleLods(path.string(), m_asset, &error))
    {
        sendStatus("Package saved but stale-LOD cleanup failed: " + error, true);
        return false;
    }
    syncDirty();
    sendProgress("writing", "SAVE ALL", completed, std::max<std::size_t>(work, completed), path);
    sendAsset();
    sendStatus("Saved dirty package members only; clean LOD files and source OBJ/assembly were not rewritten");
    sendCatalog();
    return true;
}

nlohmann::json ModelAssetEditorSession::serializeAsset() const
{
    json out;
    out["assetId"] = m_asset.assetId;
    out["displayName"] = m_asset.displayName;
    out["formatVersion"] = m_asset.formatVersion;
    out["sourceObjectType"] = m_asset.sourceObjectType;
    out["lodSwitchDistance"] = m_asset.lodSwitchDistance;
    out["minBounds"] = vec3Json(m_asset.minBounds);
    out["maxBounds"] = vec3Json(m_asset.maxBounds);
    out["binaryPath"] = compiledPath(m_asset.assetId).generic_string();
    out["sourceBasis"] = {{"preset", m_asset.sourceBasis.preset}, {"right", static_cast<int>(m_asset.sourceBasis.right)}, {"up", static_cast<int>(m_asset.sourceBasis.up)}, {"forward", static_cast<int>(m_asset.sourceBasis.forward)}, {"canonicalized", m_asset.sourceBasis.canonicalized}};
    out["manifestDirty"] = m_manifestDirty;

    out["materials"] = json::array();
    for (std::size_t i = 0; i < m_asset.materials.size(); ++i)
    {
        const auto& m = m_asset.materials[i];
        out["materials"].push_back({{"index", i}, {"id", m.id}, {"sourceName", m.sourceName}, {"baseColor", vec4Json(m.baseColor)}, {"emissiveColor", vec3Json(m.emissiveColor)}, {"emissiveStrength", m.emissiveStrength}, {"metallic", m.metallic}, {"roughness", m.roughness}, {"twoSided", m.twoSided}, {"baseColorTexture", m.baseColorTexture}, {"emissiveTexture", m.emissiveTexture}});
    }

    out["geometries"] = json::array();
    for (std::size_t gi = 0; gi < m_asset.geometries.size(); ++gi)
    {
        const auto& geometry = m_asset.geometries[gi];
        json usedBy = json::array();
        for (const auto& node : m_asset.nodes)
            if (node.geometryIndex == static_cast<std::int32_t>(gi))
                usedBy.push_back(node.id);
        json g = {{"index", gi}, {"id", geometry.id}, {"sourceLods", geometry.sourceLods}, {"surfaceMode", surfaceModeName(geometry.surfaceMode)}, {"usageCount", usedBy.size()}, {"usedBy", std::move(usedBy)}, {"estimatedBinaryBytes", estimatedGeometryBinaryBytes(geometry)}};
        g["lods"] = json::array();
        for (std::size_t li = 0; li < geometry.lods.size(); ++li)
        {
            const auto& lod = geometry.lods[li];
            json l = {{"index", li}, {"minBounds", vec3Json(lod.minBounds)}, {"maxBounds", vec3Json(lod.maxBounds)}, {"loaded", li < m_lodState.size() && m_lodState[li].loaded}, {"dirty", li < m_lodState.size() && m_lodState[li].dirty}};
            l["positions"] = json::array(); l["normals"] = json::array(); l["indices"] = json::array(); l["triangleMaterials"] = json::array(); l["smoothingGroups"] = json::array(); l["edges"] = json::array();
            for (const auto& v : lod.vertices)
            {
                l["positions"].push_back(v.position.x); l["positions"].push_back(v.position.y); l["positions"].push_back(v.position.z);
                l["normals"].push_back(v.normal.x); l["normals"].push_back(v.normal.y); l["normals"].push_back(v.normal.z);
            }
            for (const auto& t : lod.triangles)
            {
                l["indices"].push_back(t.a); l["indices"].push_back(t.b); l["indices"].push_back(t.c);
                l["triangleMaterials"].push_back(t.materialIndex); l["smoothingGroups"].push_back(t.smoothingGroupId);
            }
            for (std::size_t ei = 0; ei < lod.edges.size(); ++ei)
            {
                const auto& e = lod.edges[ei];
                l["edges"].push_back({{"index", ei}, {"a", e.a}, {"b", e.b}, {"triangleA", e.triangleA}, {"triangleB", e.triangleB}, {"flags", e.flags}, {"renderMask", e.renderMask}});
            }
            g["lods"].push_back(std::move(l));
        }
        out["geometries"].push_back(std::move(g));
    }

    out["nodes"] = json::array();
    for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
    {
        const auto& n = m_asset.nodes[ni];
        out["nodes"].push_back({
            {"index", ni}, {"id", n.id}, {"moduleId", n.moduleId}, {"parentIndex", n.parentIndex}, {"geometryIndex", n.geometryIndex},
            {"localPosition", vec3Json(n.localPosition)}, {"localRotationDeg", vec3Json(n.localRotationDeg)}, {"pivot", vec3Json(n.pivot)}, {"enabled", n.enabled},
            {"joint", {{"type", jointTypeName(n.joint.type)}, {"pivot", vec3Json(n.joint.pivot)}, {"axis", vec3Json(n.joint.axis)}, {"defaultRateDegPerSec", n.joint.defaultRateDegPerSec}, {"minAngleDeg", n.joint.minAngleDeg}, {"maxAngleDeg", n.joint.maxAngleDeg}, {"breakable", n.joint.breakable}, {"breakForceN", n.joint.breakForceN}, {"breakTorqueNm", n.joint.breakTorqueNm}}},
            {"physics", {{"mode", massModeName(n.physics.mode)}, {"densityKgM3", n.physics.densityKgM3}, {"massKg", n.physics.massKg}, {"centerOfMass", vec3Json(n.physics.centerOfMass)}, {"inertiaDiagonal", vec3Json(n.physics.inertiaDiagonal)}, {"inertiaProducts", vec3Json(n.physics.inertiaProducts)}}}
        });
    }

    out["collisionVolumes"] = json::array();
    for (std::size_t ci = 0; ci < m_asset.collisionVolumes.size(); ++ci)
    {
        const auto& c = m_asset.collisionVolumes[ci];
        out["collisionVolumes"].push_back({{"index", ci}, {"id", c.id}, {"moduleId", c.moduleId}, {"parentNodeIndex", c.parentNodeIndex}, {"shape", collisionShapeName(c.shape)}, {"localPosition", vec3Json(c.localPosition)}, {"localRotationDeg", vec3Json(c.localRotationDeg)}, {"halfSize", vec3Json(c.halfSize)}, {"radius", c.radius}, {"halfHeight", c.halfHeight}, {"enabled", c.enabled}});
    }

    out["sockets"] = json::array();
    for (std::size_t si = 0; si < m_asset.sockets.size(); ++si)
    {
        const auto& s = m_asset.sockets[si];
        out["sockets"].push_back({{"index", si}, {"id", s.id}, {"kind", s.kind}, {"moduleId", s.moduleId}, {"parentNodeIndex", s.parentNodeIndex}, {"localPosition", vec3Json(s.localPosition)}, {"localRotationDeg", vec3Json(s.localRotationDeg)}, {"extent", vec3Json(s.extent)}, {"enabled", s.enabled}, {"light", {{"type", lightTypeName(s.light.type)}, {"color", vec3Json(s.light.color)}, {"intensity", s.light.intensity}, {"rangeMeters", s.light.rangeMeters}, {"outerConeDeg", s.light.outerConeDeg}}}});
    }

    std::uint64_t sourceBytes = 0;
    std::uint64_t estimatedGeometryBytes = 0;
    std::uint64_t estimatedUnusedGeometryBytes = 0;
    std::size_t unusedGeometryCount = 0;
    std::set<std::string> countedSourceFiles;
    for (std::size_t gi = 0; gi < m_asset.geometries.size(); ++gi)
    {
        const auto& geometry = m_asset.geometries[gi];
        const auto geometryBytes = estimatedGeometryBinaryBytes(geometry);
        estimatedGeometryBytes += geometryBytes;
        const bool used = std::any_of(
            m_asset.nodes.begin(), m_asset.nodes.end(),
            [gi](const Node& node) { return node.geometryIndex == static_cast<std::int32_t>(gi); });
        if (!used)
        {
            ++unusedGeometryCount;
            estimatedUnusedGeometryBytes += geometryBytes;
        }
        for (const auto& source : geometry.sourceLods)
        {
            if (source.empty() || !countedSourceFiles.insert(source).second) continue;
            sourceBytes += safeFileBytes(editorSourceFilePath(m_sourceRoot, source));
        }
    }
    const auto binary = compiledPath(m_asset.assetId);
    const auto legacyBinary = legacyCompiledPath(m_asset.assetId);
    json lodPayloads = json::array();
    std::uint64_t savedPackageBytes = safeFileBytes(binary);
    std::size_t maxLodCount = 0;
    for (const auto& geometry : m_asset.geometries)
        maxLodCount = std::max(maxLodCount, geometry.lods.size());
    for (std::size_t lodIndex = 0; lodIndex < maxLodCount; ++lodIndex)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(binary.string(), lodIndex);
        const auto bytes = safeFileBytes(lodPath);
        savedPackageBytes += bytes;
        lodPayloads.push_back({
            {"lod", lodIndex},
            {"path", lodPath.generic_string()},
            {"bytes", bytes},
            {"loaded", lodIndex < m_lodState.size() && m_lodState[lodIndex].loaded},
            {"dirty", lodIndex < m_lodState.size() && m_lodState[lodIndex].dirty}
        });
    }
    out["storage"] = {
        {"binaryPath", binary.generic_string()},
        {"manifestBytes", safeFileBytes(binary)},
        {"manifestDirty", m_manifestDirty},
        {"savedPackageBytes", savedPackageBytes},
        {"lodPayloads", std::move(lodPayloads)},
        {"legacyBinaryPath", legacyBinary.generic_string()},
        {"legacyBinaryBytes", safeFileBytes(legacyBinary)},
        {"sourceMeshBytes", sourceBytes},
        {"estimatedGeometryPayloadBytes", estimatedGeometryBytes},
        {"estimatedUnusedGeometryBytes", estimatedUnusedGeometryBytes},
        {"unusedGeometryCount", unusedGeometryCount},
        {"sourceFilesReadOnly", true}
    };
    return out;
}

void ModelAssetEditorSession::sendAsset()
{
    m_server.broadcastText(json({{"type", "asset"}, {"dirty", m_dirty}, {"asset", serializeAsset()}}).dump());
}

void ModelAssetEditorSession::handleMessage(const std::string& payload)
{
    try
    {
        const json message = json::parse(payload);
        const std::string command = message.value("command", "");

        if (command == "request_catalog") { sendCatalog(); if (!m_selectedId.empty()) sendAsset(); return; }
        if (command == "select_asset") { selectAsset(message.value("assetId", ""), false); return; }
        if (command == "reimport_asset") { selectAsset(m_selectedId, true); return; }
        if (command == "save_asset") { saveAsset(); return; }
        if (command == "save_manifest") { saveManifestOnly(); return; }
        if (command == "save_lod") { saveLodOnly(message.value("lodIndex", std::size_t(-1))); return; }
        if (command == "load_lod") { loadLodOnly(message.value("lodIndex", std::size_t(-1)), false); return; }
        if (command == "reload_lod") { loadLodOnly(message.value("lodIndex", std::size_t(-1)), true); return; }
        if (command == "unload_lod") { unloadLod(message.value("lodIndex", std::size_t(-1))); return; }
        if (command == "quit_editor") { m_quitRequested.store(true); return; }

        if (m_asset.assetId.empty()) { sendStatus("No asset loaded", true); return; }

        if (command == "convert_source_basis")
        {
            const std::string preset = message.value("preset", std::string("game_current"));
            if (preset == "game_current") { sendStatus("Asset is already in canonical game basis"); return; }
            if (m_asset.sourceBasis.preset != "game_current")
                throw std::runtime_error("basis conversion already applied; reimport source before applying another preset");
            if (!ensureAllLodsLoaded()) return;
            convertAssetBasisToCanonical(m_asset, basisPreset(preset));
            markManifestDirty(); markAllLoadedLodsDirty(); sendAsset(); sendStatus("Converted source basis to canonical +X/+Y/-Z"); return;
        }
        if (command == "set_node_transform")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            auto& n = m_asset.nodes[index];
            if (message.contains("position")) n.localPosition = jsonVec3(message["position"], n.localPosition);
            if (message.contains("rotationDeg")) n.localRotationDeg = jsonVec3(message["rotationDeg"], n.localRotationDeg);
            if (message.contains("pivot")) n.pivot = jsonVec3(message["pivot"], n.pivot);
            markManifestDirty(); sendAsset(); sendStatus("Updated node transform: " + n.id); return;
        }
        if (command == "set_node_geometry")
        {
            const auto nodeIndex = message.value("nodeIndex", std::size_t(-1));
            const auto geometryIndex = message.value("geometryIndex", std::int32_t(-1));
            if (nodeIndex >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            if (geometryIndex < NoIndex || geometryIndex >= static_cast<std::int32_t>(m_asset.geometries.size())) throw std::runtime_error("invalid geometry index");
            m_asset.nodes[nodeIndex].geometryIndex = geometryIndex; markManifestDirty(); sendAsset();
            sendStatus("Node " + m_asset.nodes[nodeIndex].id + " now references " + (geometryIndex < 0 ? std::string("no geometry") : std::string("G") + std::to_string(geometryIndex))); return;
        }
        if (command == "fit_node_as_instance")
        {
            const auto nodeIndex = message.value("nodeIndex", std::size_t(-1));
            const auto referenceNodeIndex = message.value("referenceNodeIndex", std::size_t(-1));
            if (nodeIndex >= m_asset.nodes.size() || referenceNodeIndex >= m_asset.nodes.size())
                throw std::runtime_error("invalid node/reference index");
            if (nodeIndex == referenceNodeIndex)
                throw std::runtime_error("select a different reference node");

            auto& node = m_asset.nodes[nodeIndex];
            const auto& referenceNode = m_asset.nodes[referenceNodeIndex];
            if (node.geometryIndex < 0 || referenceNode.geometryIndex < 0)
                throw std::runtime_error("both nodes must have geometry");
            if (node.geometryIndex == referenceNode.geometryIndex)
                throw std::runtime_error("nodes already share one geometry definition");

            if (!ensureLodLoaded(0)) return;
            const auto targetGeometryIndex = static_cast<std::size_t>(node.geometryIndex);
            const auto referenceGeometryIndex = static_cast<std::size_t>(referenceNode.geometryIndex);
            const GeometryInstanceFit fit = fitGeometryAsRigidInstance(
                m_asset.geometries[referenceGeometryIndex],
                m_asset.geometries[targetGeometryIndex]);
            appendInstanceFitDiagnostic(m_asset, referenceNodeIndex, nodeIndex, fit);
            if (!fit.valid)
                throw std::runtime_error(
                    "cannot consolidate as instance: " + fit.message +
                    "; see build/logs/model_asset_instance_fit.log");

            const RigidTransform bakedFit {fit.rotation, fit.translation};
            const RigidTransform inverseFit = inverseRigid(bakedFit);
            const RigidTransform oldNodeTransform = nodeRigidTransform(node);
            const glm::vec3 newPivot = transformPoint(inverseFit, node.pivot);

            // All node-local semantic data must be expressed in the reference
            // geometry frame before F moves from vertex data into Node.
            rebaseNodeLocalData(m_asset, nodeIndex, inverseFit);
            setNodeRigidTransform(
                m_asset.nodes[nodeIndex],
                composeRigid(oldNodeTransform, bakedFit),
                newPivot);
            m_asset.nodes[nodeIndex].geometryIndex =
                static_cast<std::int32_t>(referenceGeometryIndex);

            markManifestDirty();
            sendAsset();
            sendStatus(
                "Consolidated " + m_asset.nodes[nodeIndex].id +
                " as instance of " + referenceNode.id +
                "; RMS=" + std::to_string(fit.rmsErrorMeters) +
                " m, max=" + std::to_string(fit.maxErrorMeters) +
                " m; old G" + std::to_string(targetGeometryIndex) +
                " retained for review");
            return;
        }
        if (command == "duplicate_node_instance")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            Node clone = m_asset.nodes[index];
            clone.id = message.value("id", clone.id + "_instance");
            const std::string createdId = clone.id;
            m_asset.nodes.push_back(std::move(clone)); markManifestDirty(); sendAsset();
            sendStatus("Created node instance: " + createdId); return;
        }
        if (command == "break_node_instance")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            auto& node = m_asset.nodes[index];
            if (node.geometryIndex < 0) throw std::runtime_error("node has no geometry");
            if (!ensureAllLodsLoaded()) return;
            auto geometry = m_asset.geometries[static_cast<std::size_t>(node.geometryIndex)];
            geometry.id = uniqueGeometryId(m_asset, message.value("geometryId", geometry.id + "_unique"));
            node.geometryIndex = static_cast<std::int32_t>(m_asset.geometries.size());
            const auto newGeometryIndex = node.geometryIndex;
            m_asset.geometries.push_back(std::move(geometry)); markManifestDirty(); markAllLoadedLodsDirty(); sendAsset();
            sendStatus("Broke instance " + node.id + " into unique G" + std::to_string(newGeometryIndex)); return;
        }
        if (command == "delete_node")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            const std::string deletedId = m_asset.nodes[index].id;
            for (const auto& n : m_asset.nodes) if (n.parentIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("cannot delete node with children");
            for (const auto& c : m_asset.collisionVolumes) if (c.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent collision volumes first");
            for (const auto& s : m_asset.sockets) if (s.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent sockets first");
            m_asset.nodes.erase(m_asset.nodes.begin() + static_cast<std::ptrdiff_t>(index));
            for (auto& n : m_asset.nodes) if (n.parentIndex > static_cast<std::int32_t>(index)) --n.parentIndex;
            for (auto& c : m_asset.collisionVolumes) if (c.parentNodeIndex > static_cast<std::int32_t>(index)) --c.parentNodeIndex;
            for (auto& s : m_asset.sockets) if (s.parentNodeIndex > static_cast<std::int32_t>(index)) --s.parentNodeIndex;
            markManifestDirty(); sendAsset(); sendStatus("Deleted node: " + deletedId); return;
        }
        if (command == "delete_unused_geometries")
        {
            if (!ensureAllLodsLoaded()) return;
            std::vector<std::string> deleted;
            std::uint64_t estimatedBytes = 0;
            for (std::size_t gi = m_asset.geometries.size(); gi-- > 0; )
            {
                const bool used = std::any_of(m_asset.nodes.begin(), m_asset.nodes.end(), [&](const Node& n) { return n.geometryIndex == static_cast<std::int32_t>(gi); });
                if (!used)
                {
                    deleted.push_back("G" + std::to_string(gi) + " " + m_asset.geometries[gi].id);
                    estimatedBytes += estimatedGeometryBinaryBytes(m_asset.geometries[gi]);
                    m_asset.geometries.erase(m_asset.geometries.begin() + static_cast<std::ptrdiff_t>(gi));
                    remapGeometryAfterErase(m_asset, gi);
                }
            }
            if (deleted.empty())
            {
                sendStatus("NO CHANGES: no unused geometry definitions");
                return;
            }
            std::reverse(deleted.begin(), deleted.end());
            std::string names;
            for (std::size_t i = 0; i < deleted.size(); ++i)
            {
                if (i) names += ", ";
                names += deleted[i];
            }
            markManifestDirty(); markAllLoadedLodsDirty(); sendAsset();
            sendStatus("Deleted " + std::to_string(deleted.size()) + " unused geometries: " + names +
                       "; estimated .elmesh payload removed " + std::to_string(estimatedBytes) + " B; all loaded LOD payloads are now DIRTY; use Save all to update the package on disk");
            return;
        }
        if (command == "create_radial_instances")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            const int count = std::clamp(message.value("count", 3), 2, 64);
            const float totalAngle = message.value("totalAngleDeg", 360.0f);
            const std::string axisName = message.value("axis", std::string("y"));
            const glm::vec3 pivot = jsonVec3(message.value("pivot", json::array()), glm::vec3(0.0f));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            const Node base = m_asset.nodes[index];
            const glm::vec3 axis = axisName == "x" ? glm::vec3(1,0,0) : axisName == "z" ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
            for (int i = 1; i < count; ++i)
            {
                const float angleDeg = totalAngle * static_cast<float>(i) / static_cast<float>(count);
                const glm::quat q = glm::angleAxis(glm::radians(angleDeg), axis);
                Node clone = base;
                clone.id = base.id + "_" + std::to_string(i + 1);
                clone.localPosition = pivot + q * (base.localPosition - pivot);
                clone.localRotationDeg = eulerDegrees(q * glm::quat(glm::radians(base.localRotationDeg)));
                m_asset.nodes.push_back(std::move(clone));
            }
            markManifestDirty(); sendAsset();
            sendStatus("Created " + std::to_string(count - 1) + " radial instances from " + base.id); return;
        }
        if (command == "set_joint")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            auto& j = m_asset.nodes[index].joint;
            j.type = jointTypeFromName(message.value("type", std::string(jointTypeName(j.type))));
            if (message.contains("pivot")) j.pivot = jsonVec3(message["pivot"], j.pivot);
            if (message.contains("axis")) j.axis = glm::normalize(jsonVec3(message["axis"], j.axis));
            j.defaultRateDegPerSec = message.value("defaultRateDegPerSec", j.defaultRateDegPerSec);
            j.minAngleDeg = message.value("minAngleDeg", j.minAngleDeg); j.maxAngleDeg = message.value("maxAngleDeg", j.maxAngleDeg);
            j.breakable = message.value("breakable", j.breakable); j.breakForceN = message.value("breakForceN", j.breakForceN); j.breakTorqueNm = message.value("breakTorqueNm", j.breakTorqueNm);
            markManifestDirty(); sendAsset(); sendStatus("Updated joint: " + m_asset.nodes[index].id); return;
        }
        if (command == "set_physics")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            auto& p = m_asset.nodes[index].physics;
            p.mode = massModeFromName(message.value("mode", std::string(massModeName(p.mode))));
            p.densityKgM3 = std::max(0.0f, message.value("densityKgM3", p.densityKgM3));
            p.massKg = std::max(0.0f, message.value("massKg", p.massKg));
            if (message.contains("centerOfMass")) p.centerOfMass = jsonVec3(message["centerOfMass"], p.centerOfMass);
            if (message.contains("inertiaDiagonal")) p.inertiaDiagonal = glm::max(jsonVec3(message["inertiaDiagonal"], p.inertiaDiagonal), glm::vec3(0.0f));
            if (message.contains("inertiaProducts")) p.inertiaProducts = jsonVec3(message["inertiaProducts"], p.inertiaProducts);
            markManifestDirty(); sendAsset(); sendStatus("Updated rigid-body properties: " + m_asset.nodes[index].id); return;
        }
        if (command == "estimate_physics")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            const float density = std::max(0.001f, message.value("densityKgM3", 780.0f));
            if (!estimatePhysicsFromCollision(m_asset, index, density)) throw std::runtime_error("node has no enabled local collision volumes");
            markManifestDirty(); sendAsset(); sendStatus("Estimated rigid-body properties from collision: " + m_asset.nodes[index].id); return;
        }
        if (command == "set_surface_mode")
        {
            const auto gi = message.value("geometryIndex", std::size_t(-1));
            if (gi >= m_asset.geometries.size()) throw std::runtime_error("invalid geometry index");
            m_asset.geometries[gi].surfaceMode = surfaceModeFromName(message.value("surfaceMode", "closed")); markManifestDirty(); sendAsset();
            sendStatus("Set G" + std::to_string(gi) + " surface mode to " + std::string(surfaceModeName(m_asset.geometries[gi].surfaceMode))); return;
        }
        if (command == "set_edge_render_mask")
        {
            const auto gi = message.value("geometryIndex", std::size_t(-1)), li = message.value("lodIndex", std::size_t(0)), ei = message.value("edgeIndex", std::size_t(-1));
            if (gi >= m_asset.geometries.size() || li >= m_asset.geometries[gi].lods.size() || ei >= m_asset.geometries[gi].lods[li].edges.size()) throw std::runtime_error("invalid edge index");
            if (!ensureLodLoaded(li)) return;
            m_asset.geometries[gi].lods[li].edges[ei].renderMask = static_cast<std::uint8_t>(message.value("renderMask", 0) & 0xff); markLodDirty(li); sendAsset();
            sendStatus("Updated G" + std::to_string(gi) + " LOD" + std::to_string(li) + " edge " + std::to_string(ei)); return;
        }
        if (command == "add_collision")
        {
            CollisionVolume c; c.id = message.value("id", std::string("hit.new")); c.moduleId = message.value("moduleId", std::string()); c.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (c.parentNodeIndex < NoIndex || c.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid collision parent node");
            c.shape = collisionShapeFromName(message.value("shape", std::string("box")));
            c.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f)); c.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            c.halfSize = glm::max(jsonVec3(message.value("halfSize", json::array()), glm::vec3(1.0f)), glm::vec3(0.001f)); c.radius = std::max(0.001f, message.value("radius", 1.0f)); c.halfHeight = std::max(0.0f, message.value("halfHeight", 1.0f));
            const std::string createdId = c.id;
            m_asset.collisionVolumes.push_back(std::move(c)); markManifestDirty(); sendAsset(); sendStatus("Added collision volume: " + createdId); return;
        }
        if (command == "delete_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            const std::string deletedId = m_asset.collisionVolumes[index].id;
            m_asset.collisionVolumes.erase(m_asset.collisionVolumes.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAsset(); sendStatus("Deleted collision volume: " + deletedId); return;
        }
        if (command == "duplicate_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            auto c = m_asset.collisionVolumes[index]; c.id += "_copy"; const std::string createdId = c.id; m_asset.collisionVolumes.push_back(std::move(c)); markManifestDirty(); sendAsset(); sendStatus("Duplicated collision volume: " + createdId); return;
        }
        if (command == "set_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            auto& c = m_asset.collisionVolumes[index]; c.shape = collisionShapeFromName(message.value("shape", std::string(collisionShapeName(c.shape))));
            if (message.contains("position")) c.localPosition = jsonVec3(message["position"], c.localPosition); if (message.contains("rotationDeg")) c.localRotationDeg = jsonVec3(message["rotationDeg"], c.localRotationDeg); if (message.contains("halfSize")) c.halfSize = glm::max(jsonVec3(message["halfSize"], c.halfSize), glm::vec3(0.001f));
            c.radius = std::max(0.001f, message.value("radius", c.radius)); c.halfHeight = std::max(0.0f, message.value("halfHeight", c.halfHeight)); if (message.contains("enabled")) c.enabled = message["enabled"].get<bool>();
            markManifestDirty(); sendAsset(); sendStatus("Updated collision volume: " + c.id); return;
        }
        if (command == "generate_radial_capsules")
        {
            const auto nodeIndex = message.value("nodeIndex", std::size_t(-1)); if (nodeIndex >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            const int count = std::clamp(message.value("count", 16), 3, 128); const float ringRadius = std::max(0.001f, message.value("ringRadius", 10.0f)); const float capsuleRadius = std::max(0.001f, message.value("capsuleRadius", 1.0f));
            const float halfHeight = std::max(0.001f, message.value("halfHeight", (Pi * ringRadius / count) * 0.85f)); const std::string axisName = message.value("axis", std::string("y")); const glm::vec3 center = jsonVec3(message.value("center", json::array()), glm::vec3(0.0f));
            const glm::vec3 ringAxis = axisName == "x" ? glm::vec3(1,0,0) : axisName == "z" ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
            for (int i = 0; i < count; ++i)
            {
                const float a = 2.0f * Pi * static_cast<float>(i) / static_cast<float>(count);
                glm::vec3 radial, tangent;
                if (axisName == "x") { radial = {0, std::cos(a), std::sin(a)}; tangent = {0, -std::sin(a), std::cos(a)}; }
                else if (axisName == "z") { radial = {std::cos(a), std::sin(a), 0}; tangent = {-std::sin(a), std::cos(a), 0}; }
                else { radial = {std::cos(a), 0, std::sin(a)}; tangent = {-std::sin(a), 0, std::cos(a)}; }
                CollisionVolume c; c.id = "hit." + m_asset.nodes[nodeIndex].id + ".ring." + std::to_string(i + 1); c.moduleId = m_asset.nodes[nodeIndex].moduleId; c.parentNodeIndex = static_cast<std::int32_t>(nodeIndex); c.shape = CollisionShape::Capsule; c.localPosition = center + radial * ringRadius; c.radius = capsuleRadius; c.halfHeight = halfHeight;
                c.localRotationDeg = eulerDegrees(glm::rotation(glm::vec3(0,1,0), glm::normalize(tangent))); m_asset.collisionVolumes.push_back(std::move(c));
            }
            (void)ringAxis; markManifestDirty(); sendAsset(); sendStatus("Generated " + std::to_string(count) + " radial collision capsules for " + m_asset.nodes[nodeIndex].id); return;
        }
        if (command == "add_socket")
        {
            Socket s; s.id = message.value("id", std::string("socket.new")); s.kind = message.value("kind", std::string("generic")); s.moduleId = message.value("moduleId", std::string()); s.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (s.parentNodeIndex < NoIndex || s.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid socket parent node");
            s.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f)); s.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            if (s.kind == "light" || s.kind == "light_point") s.light.type = LightType::Point; else if (s.kind == "light_spot") s.light.type = LightType::Spot;
            const std::string createdId = s.id;
            m_asset.sockets.push_back(std::move(s)); markManifestDirty(); sendAsset(); sendStatus("Added socket: " + createdId); return;
        }
        if (command == "delete_socket")
        {
            const auto index = message.value("socketIndex", std::size_t(-1)); if (index >= m_asset.sockets.size()) throw std::runtime_error("invalid socket index");
            const std::string deletedId = m_asset.sockets[index].id;
            m_asset.sockets.erase(m_asset.sockets.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAsset(); sendStatus("Deleted socket: " + deletedId); return;
        }
        if (command == "set_socket")
        {
            const auto index = message.value("socketIndex", std::size_t(-1)); if (index >= m_asset.sockets.size()) throw std::runtime_error("invalid socket index");
            auto& s = m_asset.sockets[index]; if (message.contains("position")) s.localPosition = jsonVec3(message["position"], s.localPosition); if (message.contains("rotationDeg")) s.localRotationDeg = jsonVec3(message["rotationDeg"], s.localRotationDeg); if (message.contains("enabled")) s.enabled = message["enabled"].get<bool>();
            if (message.contains("lightType")) s.light.type = lightTypeFromName(message["lightType"].get<std::string>()); if (message.contains("lightColor")) s.light.color = jsonVec3(message["lightColor"], s.light.color); s.light.intensity = message.value("lightIntensity", s.light.intensity); s.light.rangeMeters = message.value("lightRangeMeters", s.light.rangeMeters); s.light.outerConeDeg = message.value("lightOuterConeDeg", s.light.outerConeDeg);
            markManifestDirty(); sendAsset(); sendStatus("Updated socket: " + s.id); return;
        }

        sendStatus("Unknown editor command: " + command, true);
    }
    catch (const std::exception& ex)
    {
        sendStatus(std::string("Editor command failed: ") + ex.what(), true);
    }
}

} // namespace elite::model_asset::editor
