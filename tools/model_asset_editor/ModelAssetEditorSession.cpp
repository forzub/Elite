#include "tools/model_asset_editor/ModelAssetEditorSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetMigration.h"
#include "src/model_asset/ModelAssetVariantNaming.h"
#include "tools/model_asset_editor/RuntimeAssemblyImporter.h"
#include "tools/model_asset_editor/NativeObjImporter.h"
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

std::vector<std::string> jsonStrings(const json& value)
{
    std::vector<std::string> out;
    if (!value.is_array()) return out;
    for (const auto& item : value) if (item.is_string()) out.push_back(item.get<std::string>());
    return out;
}

std::vector<std::size_t> jsonIndices(const json& value)
{
    std::vector<std::size_t> out;
    if (!value.is_array()) return out;
    for (const auto& item : value)
    {
        if (!item.is_number_integer() && !item.is_number_unsigned()) continue;
        const auto raw = item.get<std::int64_t>();
        if (raw >= 0) out.push_back(static_cast<std::size_t>(raw));
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
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

std::uint64_t estimatedLodBinaryBytes(const GeometryDefinition& geometry, const MeshLod& lod)
{
    // Estimates this geometry's contribution to one independent .lodN.elmesh payload.
    // File headers are intentionally excluded.
    const auto stringBytes = [](const std::string& value) -> std::uint64_t {
        return sizeof(std::uint32_t) + static_cast<std::uint64_t>(value.size());
    };
    std::uint64_t bytes = 0;
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
    return bytes;
}

std::uint64_t estimatedGeometryBinaryBytes(const GeometryDefinition& geometry)
{
    std::uint64_t bytes = 0;
    for (const auto& lod : geometry.lods)
        bytes += estimatedLodBinaryBytes(geometry, lod);
    return bytes;
}

std::uint64_t estimatedRenderGeometryBinaryBytes(const RenderGeometryDefinition& geometry)
{
    GeometryDefinition legacy;
    legacy.id = geometry.id;
    return estimatedLodBinaryBytes(legacy, geometry.mesh);
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

    const auto fromProjectRoot = sourceRoot / "src" / path;
    if (std::filesystem::exists(fromProjectRoot)) return fromProjectRoot;

    const std::string generic = path.generic_string();
    constexpr const char* AssetsPrefix = "assets/";
    constexpr const char* ModelsPrefix = "assets/models/";
    if (generic.rfind(ModelsPrefix, 0) == 0)
    {
        const auto fromModelsRoot = sourceRoot / generic.substr(std::char_traits<char>::length(ModelsPrefix));
        if (std::filesystem::exists(fromModelsRoot)) return fromModelsRoot;
    }
    if (generic.rfind(AssetsPrefix, 0) == 0)
    {
        const auto fromAssetsRoot = sourceRoot / generic.substr(std::char_traits<char>::length(AssetsPrefix));
        if (std::filesystem::exists(fromAssetsRoot)) return fromAssetsRoot;
    }
    return direct;
}

std::uint64_t safeFileBytes(const std::filesystem::path& path)
{
    if (path.empty()) return 0;
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<std::uint64_t>(bytes);
}

bool sameVec2Exact(const glm::vec2& a, const glm::vec2& b)
{
    return a.x == b.x && a.y == b.y;
}

bool sameVec3Exact(const glm::vec3& a, const glm::vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool sameMeshLodExact(const MeshLod& a, const MeshLod& b)
{
    if (a.vertices.size() != b.vertices.size() ||
        a.triangles.size() != b.triangles.size() ||
        a.edges.size() != b.edges.size() ||
        !sameVec3Exact(a.minBounds, b.minBounds) ||
        !sameVec3Exact(a.maxBounds, b.maxBounds))
        return false;

    for (std::size_t i = 0; i < a.vertices.size(); ++i)
    {
        const auto& av = a.vertices[i];
        const auto& bv = b.vertices[i];
        if (!sameVec3Exact(av.position, bv.position) ||
            !sameVec3Exact(av.normal, bv.normal) ||
            !sameVec2Exact(av.uv, bv.uv))
            return false;
    }
    for (std::size_t i = 0; i < a.triangles.size(); ++i)
    {
        const auto& at = a.triangles[i];
        const auto& bt = b.triangles[i];
        if (at.a != bt.a || at.b != bt.b || at.c != bt.c ||
            at.sourcePolygonId != bt.sourcePolygonId ||
            at.materialIndex != bt.materialIndex ||
            at.smoothingGroupId != bt.smoothingGroupId)
            return false;
    }
    for (std::size_t i = 0; i < a.edges.size(); ++i)
    {
        const auto& ae = a.edges[i];
        const auto& be = b.edges[i];
        if (ae.a != be.a || ae.b != be.b ||
            ae.triangleA != be.triangleA || ae.triangleB != be.triangleB ||
            ae.flags != be.flags || ae.renderMask != be.renderMask)
            return false;
    }
    return true;
}

std::map<std::size_t, std::filesystem::path> discoverSavedLodPayloads(
    const std::filesystem::path& manifestPath)
{
    std::map<std::size_t, std::filesystem::path> out;
    const auto directory = manifestPath.parent_path();
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec) return out;
    const std::string prefix = manifestPath.stem().string() + ".lod";
    constexpr const char* suffix = ".elmesh";
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
        const std::string name = it->path().filename().string();
        if (name.rfind(prefix, 0) != 0 || name.size() <= prefix.size() + std::char_traits<char>::length(suffix))
            continue;
        if (name.compare(name.size() - std::char_traits<char>::length(suffix), std::char_traits<char>::length(suffix), suffix) != 0)
            continue;
        const std::string digits = name.substr(prefix.size(), name.size() - prefix.size() - std::char_traits<char>::length(suffix));
        if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
            continue;
        try { out[static_cast<std::size_t>(std::stoull(digits))] = it->path(); } catch (...) {}
    }
    return out;
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

void appendRenderInstanceFitDiagnostic(
    const ModelAsset& asset,
    std::size_t lodIndex,
    std::size_t referenceRenderNodeIndex,
    std::size_t targetRenderNodeIndex,
    const GeometryInstanceFit& fit) noexcept
{
    try
    {
        if (lodIndex >= asset.renderLods.size()) return;
        const auto& lod = asset.renderLods[lodIndex];
        if (referenceRenderNodeIndex >= lod.nodes.size() || targetRenderNodeIndex >= lod.nodes.size()) return;
        const auto& referenceNode = lod.nodes[referenceRenderNodeIndex];
        const auto& targetNode = lod.nodes[targetRenderNodeIndex];
        if (referenceNode.geometryIndex < 0 || targetNode.geometryIndex < 0) return;
        const auto referenceGeometryIndex = static_cast<std::size_t>(referenceNode.geometryIndex);
        const auto targetGeometryIndex = static_cast<std::size_t>(targetNode.geometryIndex);
        if (referenceGeometryIndex >= lod.geometries.size() || targetGeometryIndex >= lod.geometries.size()) return;

        const std::filesystem::path logPath =
            std::filesystem::path("build") / "logs" / "model_asset_instance_fit.log";
        std::filesystem::create_directories(logPath.parent_path());
        std::ofstream out(logPath, std::ios::app);
        if (!out) return;

        const auto materialName = [&](std::int32_t index) -> std::string {
            if (index < 0 || static_cast<std::size_t>(index) >= asset.materials.size())
                return std::string("<none:") + std::to_string(index) + ">";
            const auto& material = asset.materials[static_cast<std::size_t>(index)];
            return material.id + " [" + material.sourceName + "]";
        };
        const auto dumpGeometry = [&](const char* label, const RenderGeometryDefinition& geometry) {
            const MeshLod& mesh = geometry.mesh;
            std::map<std::int32_t, double> materialArea;
            double totalArea = 0.0;
            for (const Triangle& triangle : mesh.triangles)
            {
                const double area = diagnosticTriangleArea(mesh, triangle);
                totalArea += area;
                materialArea[triangle.materialIndex] += area;
            }
            out << label << " id=" << geometry.id << " source=" << geometry.sourcePath << '\n';
            out << "  vertices=" << mesh.vertices.size()
                << " unique_positions=" << diagnosticUniquePositionCount(mesh)
                << " triangles=" << mesh.triangles.size()
                << " edges=" << mesh.edges.size()
                << " area=" << std::setprecision(12) << totalArea
                << " bounds_min=(" << mesh.minBounds.x << ',' << mesh.minBounds.y << ',' << mesh.minBounds.z << ')'
                << " bounds_max=(" << mesh.maxBounds.x << ',' << mesh.maxBounds.y << ',' << mesh.maxBounds.z << ")\n";
            for (const auto& [materialIndex, area] : materialArea)
                out << "    material " << materialIndex << ' ' << materialName(materialIndex)
                    << " area=" << std::setprecision(12) << area << '\n';
        };

        out << "\n=== RENDER INSTANCE FIT ===\n";
        out << "lod=" << lodIndex << '\n';
        out << "reference render node=" << referenceNode.id << " G" << referenceGeometryIndex << '\n';
        out << "target    render node=" << targetNode.id << " G" << targetGeometryIndex << '\n';
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
        dumpGeometry("REFERENCE", lod.geometries[referenceGeometryIndex]);
        dumpGeometry("TARGET", lod.geometries[targetGeometryIndex]);
        out << "=== END RENDER INSTANCE FIT ===\n";
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

RigidTransform renderNodeRigidTransform(const RenderNode& node)
{
    return {eulerRotation(node.localRotationDeg), node.localPosition + node.pivot - eulerRotation(node.localRotationDeg) * node.pivot};
}

void setRenderNodeRigidTransform(RenderNode& node, const RigidTransform& transform, const glm::vec3& pivot)
{
    node.pivot = pivot;
    node.localRotationDeg = eulerDegrees(glm::quat_cast(transform.rotation));
    node.localPosition = transform.translation - pivot + transform.rotation * pivot;
}

void applyRenderInstanceFit(
    RenderLod& lod,
    std::size_t renderNodeIndex,
    std::size_t referenceNodeIndex,
    const GeometryInstanceFit& fit)
{
    auto& node = lod.nodes.at(renderNodeIndex);
    const auto& referenceNode = lod.nodes.at(referenceNodeIndex);
    if (node.geometryIndex < 0 || referenceNode.geometryIndex < 0)
        throw std::runtime_error("both render nodes must have geometry");

    const auto referenceGeometryIndex = static_cast<std::size_t>(referenceNode.geometryIndex);
    const RigidTransform bakedFit {fit.rotation, fit.translation};
    const RigidTransform inverseFit = inverseRigid(bakedFit);
    const RigidTransform oldTransform = renderNodeRigidTransform(node);
    const glm::vec3 newPivot = transformPoint(inverseFit, node.pivot);

    for (std::size_t childIndex = 0; childIndex < lod.nodes.size(); ++childIndex)
    {
        auto& child = lod.nodes[childIndex];
        if (child.parentIndex != static_cast<std::int32_t>(renderNodeIndex)) continue;
        const RigidTransform rebased = composeRigid(inverseFit, renderNodeRigidTransform(child));
        setRenderNodeRigidTransform(child, rebased, child.pivot);
    }

    setRenderNodeRigidTransform(node, composeRigid(oldTransform, bakedFit), newPivot);
    node.geometryIndex = static_cast<std::int32_t>(referenceGeometryIndex);
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

void recomputeRenderLodBounds(RenderLod& lod)
{
    bool haveBounds = false;
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(-std::numeric_limits<float>::max());
    for (const auto& geometry : lod.geometries)
    {
        if (geometry.mesh.vertices.empty()) continue;
        if (!haveBounds)
        {
            minBounds = geometry.mesh.minBounds;
            maxBounds = geometry.mesh.maxBounds;
            haveBounds = true;
        }
        else
        {
            minBounds = glm::min(minBounds, geometry.mesh.minBounds);
            maxBounds = glm::max(maxBounds, geometry.mesh.maxBounds);
        }
    }
    lod.minBounds = haveBounds ? minBounds : glm::vec3(0.0f);
    lod.maxBounds = haveBounds ? maxBounds : glm::vec3(0.0f);
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

    for (auto& renderLod : asset.renderLods)
    {
        for (auto& geometry : renderLod.geometries)
        {
            auto& lod = geometry.mesh;
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
        for (auto& renderNode : renderLod.nodes)
        {
            renderNode.localPosition = basis * renderNode.localPosition;
            renderNode.localRotationDeg = convertEuler(renderNode.localRotationDeg, basis);
            renderNode.pivot = basis * renderNode.pivot;
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
    for (auto& state : asset.stateVariants)
    {
        if (state.transformOverride)
        {
            state.localPosition = basis * state.localPosition;
            state.localRotationDeg = convertEuler(state.localRotationDeg, basis);
            state.pivot = basis * state.pivot;
        }
        if (state.physicsOverride) state.physics.centerOfMass = basis * state.physics.centerOfMass;
    }
    for (auto& hit : asset.hitRegions)
    {
        hit.localPosition = basis * hit.localPosition;
        hit.localRotationDeg = convertEuler(hit.localRotationDeg, basis);
    }
    for (auto& opening : asset.openings)
    {
        opening.localPosition = basis * opening.localPosition;
        opening.localRotationDeg = convertEuler(opening.localRotationDeg, basis);
    }
    for (auto& repair : asset.repairTargets)
    {
        repair.localPosition = basis * repair.localPosition;
        repair.localRotationDeg = convertEuler(repair.localRotationDeg, basis);
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

void remapRenderGeometryAfterErase(RenderLod& lod, std::size_t erased)
{
    for (auto& node : lod.nodes)
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

std::string uniqueRenderGeometryId(const RenderLod& lod, std::string desired)
{
    if (desired.empty()) desired = "geometry";
    const auto exists = [&](const std::string& id) {
        return std::any_of(lod.geometries.begin(), lod.geometries.end(), [&](const RenderGeometryDefinition& geometry) { return geometry.id == id; });
    };
    if (!exists(desired)) return desired;
    const std::string base = desired;
    for (std::size_t suffix = 2; ; ++suffix)
    {
        desired = base + "_" + std::to_string(suffix);
        if (!exists(desired)) return desired;
    }
}

std::string uniqueRenderNodeId(const RenderLod& lod, std::string desired)
{
    if (desired.empty()) desired = "render_node";
    const auto exists = [&](const std::string& id) {
        return std::any_of(lod.nodes.begin(), lod.nodes.end(), [&](const RenderNode& node) { return node.id == id; });
    };
    if (!exists(desired)) return desired;
    const std::string base = desired;
    for (std::size_t suffix = 2; ; ++suffix)
    {
        desired = base + "_" + std::to_string(suffix);
        if (!exists(desired)) return desired;
    }
}


bool semanticStateDeclared(const ModelAsset& asset, std::int32_t nodeIndex, const std::string& stateId)
{
    if (stateId == "intact") return true;
    return std::any_of(asset.stateVariants.begin(), asset.stateVariants.end(), [&](const StateVariant& variant) {
        return variant.nodeIndex == nodeIndex && variant.id == stateId;
    });
}

void requireSemanticStates(
    const ModelAsset& asset,
    std::int32_t nodeIndex,
    const std::vector<std::string>& stateIds,
    const std::string& owner)
{
    if (stateIds.empty()) return;
    if (nodeIndex < 0 || nodeIndex >= static_cast<std::int32_t>(asset.nodes.size()))
        throw std::runtime_error(owner + " has state scope but no semantic node binding");
    for (const auto& stateId : stateIds)
        if (!semanticStateDeclared(asset, nodeIndex, stateId))
            throw std::runtime_error(owner + " references undeclared state '" + stateId + "'");
}

std::filesystem::path defaultEditorSourceAssetsRoot(const std::filesystem::path& projectRoot)
{
    std::error_code ec;
    const auto projectModels = projectRoot / "assets" / "models";
    if (std::filesystem::is_directory(projectModels, ec) && !ec)
        return projectModels;

    ec.clear();
    const auto legacySrcModels = projectRoot / "src" / "assets" / "models";
    if (std::filesystem::is_directory(legacySrcModels, ec) && !ec)
        return projectRoot / "src";

    // Prefer the current project layout even before the directory is populated.
    return projectModels;
}

std::uint32_t packageFormatVersion(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return 0;
    std::array<char, 8> magic {};
    std::uint32_t version = 0;
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    return in ? version : 0;
}

const std::array<const char*, 9>& wizardStageOrder()
{
    static const std::array<const char*, 9> stages {
        "source", "lods", "geometry", "surfaces", "semantics",
        "physics", "damage", "validate", "build"
    };
    return stages;
}

std::size_t wizardStageIndex(const std::string& stage)
{
    const auto& stages = wizardStageOrder();
    const auto it = std::find(stages.begin(), stages.end(), stage);
    return it == stages.end() ? stages.size() : static_cast<std::size_t>(std::distance(stages.begin(), it));
}

}

ModelAssetEditorSession::ModelAssetEditorSession(
    std::filesystem::path sourceRoot,
    HtmlUiServer& server
)
    : m_sourceRoot(std::move(sourceRoot)), m_server(server)
{
    m_sourceAssetsRoot = defaultEditorSourceAssetsRoot(m_sourceRoot);
    m_compiledModelsRoot = m_sourceRoot / "src" / "assets" / "compiled" / "models";
    loadSettings();
    installLocalizationBundle();

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
    return m_compiledModelsRoot / id / (id + ".elmodel");
}

std::filesystem::path ModelAssetEditorSession::legacyCompiledPath(const std::string& id) const
{
    return m_compiledModelsRoot / (id + ".elmodel");
}

std::filesystem::path ModelAssetEditorSession::settingsPath() const
{
    return m_sourceRoot / "build" / "tools" / "model_asset_editor" / "model_asset_editor.settings.json";
}

void ModelAssetEditorSession::loadSettings()
{
    const auto path = settingsPath();
    std::ifstream in(path);
    if (!in) return;
    try
    {
        json settings;
        in >> settings;
        const auto source = settings.value("sourceAssetsRoot", std::string());
        const auto compiled = settings.value("compiledModelsRoot", std::string());
        const auto locale = settings.value("locale", std::string("en"));
        if (!source.empty()) m_sourceAssetsRoot = std::filesystem::path(source);
        if (!compiled.empty()) m_compiledModelsRoot = std::filesystem::path(compiled);
        static const std::array<const char*, 5> supportedLocales {"en", "ru", "zh-Hans", "es", "ja"};
        if (std::find(supportedLocales.begin(), supportedLocales.end(), locale) != supportedLocales.end())
            m_locale = locale;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[ModelAssetEditor] settings ignored: " << ex.what() << '\n';
    }
}

std::filesystem::path ModelAssetEditorSession::wizardWorkspacePath() const
{
    if (m_selectedId.empty())
        return m_sourceRoot / "build" / "tools" / "model_asset_editor" / "workspaces" / "_none";
    return m_sourceRoot / "build" / "tools" / "model_asset_editor" / "workspaces" / m_selectedId;
}

std::filesystem::path ModelAssetEditorSession::wizardStatePath() const
{
    return wizardWorkspacePath() / "wizard_state.json";
}

std::filesystem::path ModelAssetEditorSession::wizardCheckpointPath(const std::string& stage) const
{
    return wizardWorkspacePath() / ("checkpoint-" + stage) / (m_selectedId + ".elmodel");
}

std::filesystem::path ModelAssetEditorSession::latestWizardCheckpoint(std::string* stage) const
{
    const auto& order = wizardStageOrder();
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        const auto state = m_wizardStages.find(*it);
        if (state == m_wizardStages.end() ||
            (state->second.status != "complete" && state->second.status != "stale"))
            continue;

        // Checkpoints are editor-owned files at canonical workspace paths. Do
        // not trust a persisted arbitrary path when deciding what to resume.
        const auto checkpoint = wizardCheckpointPath(*it);
        if (!std::filesystem::exists(checkpoint))
            continue;

        if (stage) *stage = *it;
        return checkpoint;
    }
    if (stage) stage->clear();
    return {};
}

void ModelAssetEditorSession::loadWizardState()
{
    m_wizardStages.clear();
    m_baseVisualIds.clear();
    m_sourceExtraMeshIds.clear();
    m_sourceVariantReplacements.clear();
    m_legacySourceVariantReplacements.clear();
    m_nextBaseVisualOrdinal = 1;
    m_nextSourceVariantOrdinal = 1;
    for (const char* id : wizardStageOrder())
        m_wizardStages.emplace(id, WizardStageState{});

    std::ifstream in(wizardStatePath());
    if (!in) return;
    try
    {
        json state;
        in >> state;
        const int schemaVersion = state.value("schemaVersion", 0);
        if (schemaVersion < 1 || schemaVersion > 3) return;
        const auto stages = state.value("stages", json::object());
        for (auto& [id, value] : m_wizardStages)
        {
            if (!stages.contains(id) || !stages[id].is_object()) continue;
            const auto status = stages[id].value("status", std::string("not_started"));
            if (status == "complete" || status == "stale" || status == "not_started")
                value.status = status;
            const auto checkpoint = stages[id].value("checkpoint", std::string());
            if (!checkpoint.empty()) value.checkpointManifest = std::filesystem::path(checkpoint);
        }

        if (schemaVersion >= 3)
        {
            m_nextBaseVisualOrdinal = std::max<std::size_t>(
                1, state.value("nextBaseVisualOrdinal", std::size_t(1)));
            m_nextSourceVariantOrdinal = std::max<std::size_t>(
                1, state.value("nextSourceVariantOrdinal", std::size_t(1)));

            for (const auto& item : state.value("baseVisuals", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto geometryId = item.value("geometryId", std::string());
                const auto id = item.value("id", std::string());
                if (lodIndex == std::size_t(-1) || geometryId.empty() || id.empty()) continue;
                m_baseVisualIds[lodIndex][geometryId] = id;
            }
            for (const auto& item : state.value("sourceExtraMeshes", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto sourcePath = item.value("sourcePath", std::string());
                const auto id = item.value("id", std::string());
                if (lodIndex == std::size_t(-1) || sourcePath.empty() || id.empty()) continue;
                m_sourceExtraMeshIds[lodIndex][sourcePath] = id;
            }
            for (const auto& item : state.value("sourceVariantReplacements", json::array()))
            {
                if (!item.is_object()) continue;
                const auto variantId = item.value("variantId", std::string());
                if (variantId.empty()) continue;
                std::vector<std::string> replaces;
                for (const auto& value : item.value("replacesBaseVisualIds", json::array()))
                    if (value.is_string() && !value.get<std::string>().empty())
                        replaces.push_back(value.get<std::string>());
                std::sort(replaces.begin(), replaces.end());
                replaces.erase(std::unique(replaces.begin(), replaces.end()), replaces.end());
                if (!replaces.empty())
                    m_sourceVariantReplacements[variantId] = std::move(replaces);
            }
            // Pending v0.9.5/v0.9.6 records can survive a v3 state write until
            // their LOD is actually loaded and can be migrated without guessing.
            for (const auto& item : state.value("legacySourceVariantReplacements", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto variantId = item.value("variantId", std::string());
                if (lodIndex == std::size_t(-1) || variantId.empty()) continue;
                std::vector<std::string> replaces;
                for (const auto& value : item.value("replaces", json::array()))
                    if (value.is_string() && !value.get<std::string>().empty())
                        replaces.push_back(value.get<std::string>());
                std::sort(replaces.begin(), replaces.end());
                replaces.erase(std::unique(replaces.begin(), replaces.end()), replaces.end());
                if (!replaces.empty())
                    m_legacySourceVariantReplacements[lodIndex][variantId] = std::move(replaces);
            }
        }
        else if (schemaVersion >= 2)
        {
            // v0.9.5/v0.9.6 stored filename-derived variant ids and LOD-local
            // geometry ids. Keep them only as migration input; once a LOD is
            // loaded reconcileAuthoringVisualRegistry() converts them to opaque
            // authoring ids and stable base visual ids.
            for (const auto& item : state.value("sourceVariantReplacements", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto variantId = item.value("variantId", std::string());
                if (lodIndex == std::size_t(-1) || variantId.empty()) continue;
                std::vector<std::string> replaces;
                for (const auto& value : item.value("replaces", json::array()))
                    if (value.is_string() && !value.get<std::string>().empty())
                        replaces.push_back(value.get<std::string>());
                std::sort(replaces.begin(), replaces.end());
                replaces.erase(std::unique(replaces.begin(), replaces.end()), replaces.end());
                if (!replaces.empty())
                    m_legacySourceVariantReplacements[lodIndex][variantId] = std::move(replaces);
            }
        }

        // The workspace owns checkpoint locations. Rebind persisted records to
        // the current canonical workspace path and discard records whose files
        // no longer exist (for example after linear-history pruning).
        for (auto& [id, value] : m_wizardStages)
        {
            const auto checkpoint = wizardCheckpointPath(id);
            if (std::filesystem::exists(checkpoint))
                value.checkpointManifest = checkpoint;
            else
                value.checkpointManifest.clear();
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[ModelAssetEditor] wizard state ignored: " << ex.what() << '\n';
    }
}

bool ModelAssetEditorSession::writeWizardState() const
{
    try
    {
        std::filesystem::create_directories(wizardWorkspacePath());
        json stages = json::object();
        for (const auto& [id, value] : m_wizardStages)
        {
            stages[id] = {
                {"status", value.status},
                {"checkpoint", value.checkpointManifest.empty() ? std::string() : value.checkpointManifest.generic_string()}
            };
        }

        json baseVisuals = json::array();
        for (const auto& [lodIndex, byGeometry] : m_baseVisualIds)
            for (const auto& [geometryId, id] : byGeometry)
                if (!geometryId.empty() && !id.empty())
                    baseVisuals.push_back({{"lod", lodIndex}, {"geometryId", geometryId}, {"id", id}});

        json sourceExtraMeshes = json::array();
        for (const auto& [lodIndex, byPath] : m_sourceExtraMeshIds)
            for (const auto& [sourcePath, id] : byPath)
                if (!sourcePath.empty() && !id.empty())
                    sourceExtraMeshes.push_back({{"lod", lodIndex}, {"sourcePath", sourcePath}, {"id", id}});

        json sourceVariantReplacements = json::array();
        for (const auto& [variantId, replaces] : m_sourceVariantReplacements)
            if (!variantId.empty() && !replaces.empty())
                sourceVariantReplacements.push_back({
                    {"variantId", variantId},
                    {"replacesBaseVisualIds", replaces}
                });

        json legacySourceVariantReplacements = json::array();
        for (const auto& [lodIndex, byVariant] : m_legacySourceVariantReplacements)
            for (const auto& [variantId, replaces] : byVariant)
                if (!variantId.empty() && !replaces.empty())
                    legacySourceVariantReplacements.push_back({
                        {"lod", lodIndex}, {"variantId", variantId}, {"replaces", replaces}
                    });

        json state = {
            {"schemaVersion", 3},
            {"assetId", m_selectedId},
            {"editorVersion", ModelAssetEditorVersion},
            {"stages", std::move(stages)},
            {"nextBaseVisualOrdinal", m_nextBaseVisualOrdinal},
            {"nextSourceVariantOrdinal", m_nextSourceVariantOrdinal},
            {"baseVisuals", std::move(baseVisuals)},
            {"sourceExtraMeshes", std::move(sourceExtraMeshes)},
            {"sourceVariantReplacements", std::move(sourceVariantReplacements)},
            {"legacySourceVariantReplacements", std::move(legacySourceVariantReplacements)}
        };
        std::ofstream out(wizardStatePath(), std::ios::trunc);
        if (!out) return false;
        out << std::setw(2) << state << '\n';
        return static_cast<bool>(out);
    }
    catch (...)
    {
        return false;
    }
}

std::string ModelAssetEditorSession::allocateBaseVisualId()
{
    for (;;)
    {
        std::ostringstream out;
        out << "base." << std::setw(6) << std::setfill('0') << m_nextBaseVisualOrdinal++;
        const std::string candidate = out.str();
        bool used = false;
        for (const auto& [lodIndex, byGeometry] : m_baseVisualIds)
        {
            (void)lodIndex;
            if (std::any_of(byGeometry.begin(), byGeometry.end(), [&](const auto& item) {
                    return item.second == candidate;
                }))
            {
                used = true;
                break;
            }
        }
        if (!used) return candidate;
    }
}

std::string ModelAssetEditorSession::allocateSourceVariantId()
{
    for (;;)
    {
        std::ostringstream out;
        out << "extra." << std::setw(6) << std::setfill('0') << m_nextSourceVariantOrdinal++;
        const std::string candidate = out.str();
        bool used = m_sourceVariantReplacements.find(candidate) != m_sourceVariantReplacements.end();
        if (!used)
        {
            for (const auto& [lodIndex, byPath] : m_sourceExtraMeshIds)
            {
                (void)lodIndex;
                if (std::any_of(byPath.begin(), byPath.end(), [&](const auto& item) {
                        return item.second == candidate;
                    }))
                {
                    used = true;
                    break;
                }
            }
        }
        if (!used) return candidate;
    }
}

std::string ModelAssetEditorSession::baseVisualId(
    std::size_t lodIndex,
    const std::string& geometryId) const
{
    const auto lodIt = m_baseVisualIds.find(lodIndex);
    if (lodIt == m_baseVisualIds.end()) return {};
    const auto geometryIt = lodIt->second.find(geometryId);
    return geometryIt == lodIt->second.end() ? std::string() : geometryIt->second;
}

std::string ModelAssetEditorSession::sourceVariantAuthoringId(
    std::size_t lodIndex,
    const RenderGeometryDefinition& geometry) const
{
    if (!geometry.sourcePath.empty())
    {
        const auto lodIt = m_sourceExtraMeshIds.find(lodIndex);
        if (lodIt != m_sourceExtraMeshIds.end())
        {
            const auto sourceIt = lodIt->second.find(geometry.sourcePath);
            if (sourceIt != lodIt->second.end()) return sourceIt->second;
        }
    }
    return renderVariantIdentity(geometry.id).variantId;
}

void ModelAssetEditorSession::reconcileAuthoringVisualRegistry()
{
    if (m_asset.assetId.empty()) return;
    bool changed = false;

    for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex)
    {
        if (lodIndex >= m_lodState.size() || !m_lodState[lodIndex].loaded)
            continue;
        const auto& lod = m_asset.renderLods[lodIndex];
        for (const auto& geometry : lod.geometries)
        {
            const auto identity = renderVariantIdentity(geometry.id);
            if (!identity.isVariant)
            {
                auto& id = m_baseVisualIds[lodIndex][geometry.id];
                if (id.empty())
                {
                    id = allocateBaseVisualId();
                    changed = true;
                }
                continue;
            }

            if (geometry.sourcePath.empty()) continue;
            auto& id = m_sourceExtraMeshIds[lodIndex][geometry.sourcePath];
            if (id.empty())
            {
                // Existing v0.9.4-v0.9.6 checkpoints encoded a filename stem in
                // geometry.id. Do not preserve that as semantic identity: assign
                // a new opaque authoring id while keeping sourcePath only as the
                // reload pointer.
                id = allocateSourceVariantId();
                changed = true;
            }
        }
    }

    if (!m_legacySourceVariantReplacements.empty())
    {
        // v0.9.5/v0.9.6 replacement records were LOD-local and referred to
        // filename-derived variant ids plus transient geometry ids. Migrate a
        // legacy record only while that LOD is actually loaded; otherwise keep
        // it pending so opening another LOD later cannot silently discard it.
        decltype(m_legacySourceVariantReplacements) pendingLegacy;
        for (const auto& [lodIndex, byVariant] : m_legacySourceVariantReplacements)
        {
            if (lodIndex >= m_asset.renderLods.size() ||
                lodIndex >= m_lodState.size() || !m_lodState[lodIndex].loaded)
            {
                pendingLegacy[lodIndex] = byVariant;
                continue;
            }

            const auto& lod = m_asset.renderLods[lodIndex];
            for (const auto& [legacyVariantId, baseGeometryIds] : byVariant)
            {
                std::string stableVariantId;
                for (const auto& geometry : lod.geometries)
                {
                    const auto identity = renderVariantIdentity(geometry.id);
                    if (identity.isVariant && identity.variantId == legacyVariantId)
                    {
                        stableVariantId = sourceVariantAuthoringId(lodIndex, geometry);
                        break;
                    }
                }

                if (stableVariantId.empty())
                {
                    // The source variant is not present in this loaded document
                    // anymore. Preserve the old record rather than inventing a
                    // new semantic identity from its former filename.
                    pendingLegacy[lodIndex][legacyVariantId] = baseGeometryIds;
                    continue;
                }

                auto& replacements = m_sourceVariantReplacements[stableVariantId];
                for (const auto& geometryId : baseGeometryIds)
                {
                    const auto stableBaseId = baseVisualId(lodIndex, geometryId);
                    if (!stableBaseId.empty() &&
                        std::find(replacements.begin(), replacements.end(), stableBaseId) == replacements.end())
                        replacements.push_back(stableBaseId);
                }
                std::sort(replacements.begin(), replacements.end());
                replacements.erase(std::unique(replacements.begin(), replacements.end()), replacements.end());
                if (replacements.empty()) m_sourceVariantReplacements.erase(stableVariantId);
                changed = true;
            }
        }
        m_legacySourceVariantReplacements = std::move(pendingLegacy);
    }

    if (changed && !writeWizardState())
        std::cerr << "[ModelAssetEditor] authoring visual registry could not be persisted\n";
}

void ModelAssetEditorSession::pruneWizardAfter(const std::string& stage)
{
    const auto first = wizardStageIndex(stage);
    const auto& order = wizardStageOrder();
    if (first >= order.size()) return;

    for (std::size_t i = first + 1; i < order.size(); ++i)
    {
        const std::string laterId = order[i];
        auto& later = m_wizardStages[laterId];
        std::error_code ec;
        std::filesystem::remove_all(wizardCheckpointPath(laterId).parent_path(), ec);
        if (ec)
        {
            std::cerr << "[ModelAssetEditor] cannot prune later wizard checkpoint "
                      << laterId << ": " << ec.message() << '\n';
        }
        later.status = "not_started";
        later.checkpointManifest.clear();
    }
}

void ModelAssetEditorSession::invalidateWizardFrom(const std::string& stage)
{
    const auto first = wizardStageIndex(stage);
    const auto& order = wizardStageOrder();
    if (first >= order.size()) return;

    auto& current = m_wizardStages[order[first]];
    if (current.status == "complete") current.status = "stale";
    else if (current.status != "stale") current.status = "not_started";

    // A linear wizard cannot keep checkpoints produced from a future state once
    // an earlier stage changes. Keep only the current stage's previous checkpoint
    // as an explicit rollback point; physically remove every later branch.
    pruneWizardAfter(stage);
    (void)writeWizardState();
}

nlohmann::json ModelAssetEditorSession::serializeWizard() const
{
    json stages = json::array();
    const auto& order = wizardStageOrder();
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        const std::string id = order[i];
        const auto it = m_wizardStages.find(id);
        const WizardStageState value = it == m_wizardStages.end() ? WizardStageState{} : it->second;
        const bool implemented = i < 3;
        const bool previousComplete = i == 0 ||
            (m_wizardStages.count(order[i - 1]) && m_wizardStages.at(order[i - 1]).status == "complete");
        stages.push_back({
            {"id", id}, {"index", i}, {"status", value.status},
            {"implemented", implemented}, {"unlocked", implemented && previousComplete},
            {"checkpointPath", value.checkpointManifest.empty() ? std::string() : value.checkpointManifest.generic_string()},
            {"checkpointExists", !value.checkpointManifest.empty() && std::filesystem::exists(value.checkpointManifest)}
        });
    }
    return {
        {"workspacePath", wizardWorkspacePath().generic_string()},
        {"statePath", wizardStatePath().generic_string()},
        {"stages", std::move(stages)}
    };
}

bool ModelAssetEditorSession::completeWizardStage(const std::string& stage)
{
    const auto stageIndex = wizardStageIndex(stage);
    if (stageIndex >= 3)
    {
        sendStatus("Wizard stage '" + stage + "' is visible but not implemented yet", true);
        return false;
    }
    if (stageIndex > 0)
    {
        const std::string previous = wizardStageOrder()[stageIndex - 1];
        if (m_wizardStages[previous].status != "complete")
        {
            sendStatus("Complete wizard stage '" + previous + "' first", true);
            return false;
        }
    }
    if (m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return false;
    }
    if (!ensureAllLodsLoaded())
        return false;

    if (stage == "source")
    {
        std::size_t sourceBacked = 0;
        std::size_t missing = 0;
        for (const auto& lod : m_asset.renderLods)
            for (const auto& geometry : lod.geometries)
                if (!geometry.sourcePath.empty())
                {
                    ++sourceBacked;
                    if (!std::filesystem::exists(editorSourceFilePath(m_sourceAssetsRoot, geometry.sourcePath))) ++missing;
                }
        if (sourceBacked == 0 || missing != 0)
        {
            sendStatus("SOURCE validation failed: source meshes=" + std::to_string(sourceBacked) +
                ", missing=" + std::to_string(missing), true);
            return false;
        }
    }
    else if (stage == "lods")
    {
        if (m_asset.renderLods.empty())
        {
            sendStatus("LOD validation failed: asset has no render LOD documents", true);
            return false;
        }
        for (const auto& lod : m_asset.renderLods)
            if (lod.nodes.empty() || lod.geometries.empty())
            {
                sendStatus("LOD validation failed: LOD" + std::to_string(lod.level) + " has an empty render graph", true);
                return false;
            }
        const auto savedPayloads = discoverSavedLodPayloads(compiledPath(m_selectedId));
        for (const auto& [savedIndex, savedPath] : savedPayloads)
            if (savedIndex >= m_asset.renderLods.size())
            {
                sendStatus("LOD validation failed: saved " + savedPath.filename().string() +
                    " exists, but the current asset declares only " + std::to_string(m_asset.renderLods.size()) +
                    " render LOD document(s). Reimport/migration lost an LOD or the saved payload is stale.", true);
                return false;
            }
    }
    else if (stage == "geometry")
    {
        for (const auto& lod : m_asset.renderLods)
            for (const auto& node : lod.nodes)
                if (node.geometryIndex >= static_cast<std::int32_t>(lod.geometries.size()))
                {
                    sendStatus("GEOMETRY validation failed: invalid render-node geometry binding", true);
                    return false;
                }
    }

    std::string error;
    if (!ModelAssetBinary::validate(m_asset, &error))
    {
        sendStatus("Wizard " + stage + " preflight failed: " + error, true);
        return false;
    }

    const auto checkpoint = wizardCheckpointPath(stage);
    std::filesystem::create_directories(checkpoint.parent_path());
    sendStatus("Writing wizard checkpoint for " + stage + "...", false, "writing");
    sendProgress("writing", "CHECKPOINT " + stage, 0, 1, checkpoint);
    if (!ModelAssetBinary::save(checkpoint.string(), m_asset, &error))
    {
        sendStatus("Cannot write wizard checkpoint: " + error, true);
        return false;
    }
    auto& value = m_wizardStages[stage];
    value.status = "complete";
    value.checkpointManifest = checkpoint;
    pruneWizardAfter(stage);
    if (!writeWizardState())
    {
        sendStatus("Checkpoint was written, but wizard_state.json could not be saved", true);
        return false;
    }
    sendProgress("writing", "CHECKPOINT " + stage, 1, 1, checkpoint);
    sendAssetMetadata();
    const std::string next = stageIndex + 1 < 3 ? wizardStageOrder()[stageIndex + 1] : std::string();
    m_server.broadcastText(json({{"type", "wizard_stage_completed"}, {"stage", stage}, {"nextStage", next}, {"checkpoint", checkpoint.generic_string()}}).dump());
    sendStatus("Wizard stage complete: " + stage + "; checkpoint saved outside the production package");
    return true;
}

bool ModelAssetEditorSession::restoreWizardCheckpoint(const std::string& stage)
{
    const auto it = m_wizardStages.find(stage);
    if (it == m_wizardStages.end() || it->second.checkpointManifest.empty() ||
        !std::filesystem::exists(it->second.checkpointManifest))
    {
        sendStatus("No checkpoint exists for wizard stage '" + stage + "'", true);
        return false;
    }
    ModelAsset restored;
    std::string error;
    sendStatus("Restoring wizard checkpoint " + stage + "...", false, "reading");
    sendProgress("reading", "RESTORE CHECKPOINT", 0, 1, it->second.checkpointManifest);
    if (!ModelAssetBinary::load(it->second.checkpointManifest.string(), restored, &error))
    {
        sendStatus("Cannot restore wizard checkpoint: " + error, true);
        return false;
    }
    m_asset = std::move(restored);
    resetLodState(true, true); // restored work is intentionally unsaved relative to production output
    auto& restoredStage = m_wizardStages[stage];
    restoredStage.status = "complete";
    pruneWizardAfter(stage);
    (void)writeWizardState();
    sendProgress("reading", "RESTORE CHECKPOINT", 1, 1, it->second.checkpointManifest);
    sendAsset();
    m_server.broadcastText(json({{"type", "wizard_checkpoint_restored"}, {"stage", stage}}).dump());
    sendStatus("Restored " + stage + " checkpoint into editor memory; production package and source OBJ are unchanged");
    return true;
}

bool ModelAssetEditorSession::scanRenderDuplicates(
    std::size_t lodIndex,
    std::size_t referenceRenderNodeIndex,
    const std::vector<std::size_t>& targetRenderNodeIndices)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    const auto& lod = m_asset.renderLods.at(lodIndex);

    // Interactive editor mode: compare one explicit reference against only the
    // checked rows. This keeps the UI deterministic and also returns negative
    // results, so the table can show both matches and non-matches.
    if (referenceRenderNodeIndex != std::size_t(-1))
    {
        if (referenceRenderNodeIndex >= lod.nodes.size())
        {
            sendStatus("Duplicate comparison reference is outside LOD" + std::to_string(lodIndex), true);
            return false;
        }
        const auto& referenceNode = lod.nodes[referenceRenderNodeIndex];
        if (referenceNode.geometryIndex < 0 ||
            static_cast<std::size_t>(referenceNode.geometryIndex) >= lod.geometries.size())
        {
            sendStatus("Duplicate comparison reference has no geometry: " + referenceNode.id, true);
            return false;
        }

        std::vector<std::size_t> targets;
        targets.reserve(targetRenderNodeIndices.size());
        for (const auto target : targetRenderNodeIndices)
        {
            if (target == referenceRenderNodeIndex || target >= lod.nodes.size()) continue;
            if (lod.nodes[target].geometryIndex < 0) continue;
            targets.push_back(target);
        }
        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

        const auto referenceGi = static_cast<std::size_t>(referenceNode.geometryIndex);
        GeometryDefinition referenceGeometry;
        referenceGeometry.id = lod.geometries[referenceGi].id;
        referenceGeometry.lods.push_back(lod.geometries[referenceGi].mesh);

        json results = json::array();
        json candidates = json::array();
        std::size_t matches = 0;
        std::size_t done = 0;
        sendStatus(
            "Comparing selected LOD" + std::to_string(lodIndex) +
            " geometry against reference " + referenceNode.id + "...",
            false,
            "working");

        for (const auto targetNodeIndex : targets)
        {
            sendProgress("working", "COMPARE GEOMETRY", done, targets.size());
            const auto& targetNode = lod.nodes[targetNodeIndex];
            const auto targetGi = static_cast<std::size_t>(targetNode.geometryIndex);
            const bool alreadyInstance = targetGi == referenceGi;

            GeometryInstanceFit fit;
            if (alreadyInstance)
            {
                fit.valid = true;
                fit.geometryMatched = true;
                fit.materialCompatible = true;
                fit.message = "already shares reference geometry";
            }
            else
            {
                GeometryDefinition targetGeometry;
                targetGeometry.id = lod.geometries[targetGi].id;
                targetGeometry.lods.push_back(lod.geometries[targetGi].mesh);
                fit = fitGeometryAsRigidInstance(referenceGeometry, targetGeometry);
            }

            json row = {
                {"referenceGeometryIndex", referenceGi},
                {"targetGeometryIndex", targetGi},
                {"referenceRenderNodeIndex", referenceRenderNodeIndex},
                {"targetRenderNodeIndex", targetNodeIndex},
                {"referenceGeometryId", lod.geometries[referenceGi].id},
                {"targetGeometryId", lod.geometries[targetGi].id},
                {"referenceRenderNodeId", referenceNode.id},
                {"targetRenderNodeId", targetNode.id},
                {"match", fit.valid},
                {"alreadyInstance", alreadyInstance},
                {"rmsErrorMeters", fit.rmsErrorMeters},
                {"maxErrorMeters", fit.maxErrorMeters},
                {"toleranceMeters", fit.toleranceMeters},
                {"comparedVertices", fit.comparedVertices},
                {"message", fit.message}
            };
            results.push_back(row);
            if (fit.valid)
            {
                ++matches;
                if (!alreadyInstance) candidates.push_back(row);
            }
            ++done;
        }

        sendProgress("working", "COMPARE GEOMETRY", targets.size(), targets.size());
        m_server.broadcastText(json({
            {"type", "geometry_scan_result"},
            {"mode", "reference"},
            {"lodIndex", lodIndex},
            {"referenceRenderNodeIndex", referenceRenderNodeIndex},
            {"testedPairs", targets.size()},
            {"matches", matches},
            {"results", std::move(results)},
            {"candidates", std::move(candidates)}
        }).dump());
        sendStatus(
            "Geometry comparison complete: " + std::to_string(matches) +
            " of " + std::to_string(targets.size()) + " selected elements match " + referenceNode.id);
        return true;
    }

    // Compatibility mode retained for capability/tests and older UI clients.
    // New UI intentionally avoids this all-pairs O(n^2) workflow.
    std::vector<std::size_t> representative(lod.geometries.size(), std::size_t(-1));
    for (std::size_t ni = 0; ni < lod.nodes.size(); ++ni)
    {
        const auto gi = lod.nodes[ni].geometryIndex;
        if (gi >= 0 && static_cast<std::size_t>(gi) < representative.size() && representative[static_cast<std::size_t>(gi)] == std::size_t(-1))
            representative[static_cast<std::size_t>(gi)] = ni;
    }
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    for (std::size_t a = 0; a < representative.size(); ++a)
        if (representative[a] != std::size_t(-1))
            for (std::size_t b = a + 1; b < representative.size(); ++b)
                if (representative[b] != std::size_t(-1)) pairs.emplace_back(a, b);

    json candidates = json::array();
    std::size_t done = 0;
    sendStatus("Scanning LOD" + std::to_string(lodIndex) + " for exact rigid duplicate geometry...", false, "working");
    for (const auto& [referenceGi, targetGi] : pairs)
    {
        sendProgress("working", "SCAN DUPLICATE GEOMETRY", done, pairs.size());
        GeometryDefinition referenceGeometry, targetGeometry;
        referenceGeometry.id = lod.geometries[referenceGi].id;
        referenceGeometry.lods.push_back(lod.geometries[referenceGi].mesh);
        targetGeometry.id = lod.geometries[targetGi].id;
        targetGeometry.lods.push_back(lod.geometries[targetGi].mesh);
        const GeometryInstanceFit fit = fitGeometryAsRigidInstance(referenceGeometry, targetGeometry);
        if (fit.valid)
        {
            candidates.push_back({
                {"referenceGeometryIndex", referenceGi}, {"targetGeometryIndex", targetGi},
                {"referenceRenderNodeIndex", representative[referenceGi]}, {"targetRenderNodeIndex", representative[targetGi]},
                {"referenceGeometryId", lod.geometries[referenceGi].id}, {"targetGeometryId", lod.geometries[targetGi].id},
                {"referenceRenderNodeId", lod.nodes[representative[referenceGi]].id}, {"targetRenderNodeId", lod.nodes[representative[targetGi]].id},
                {"rmsErrorMeters", fit.rmsErrorMeters}, {"maxErrorMeters", fit.maxErrorMeters},
                {"toleranceMeters", fit.toleranceMeters}, {"comparedVertices", fit.comparedVertices}
            });
        }
        ++done;
    }
    sendProgress("working", "SCAN DUPLICATE GEOMETRY", pairs.size(), pairs.size());
    m_server.broadcastText(json({
        {"type", "geometry_scan_result"}, {"mode", "all_pairs"}, {"lodIndex", lodIndex},
        {"testedPairs", pairs.size()}, {"candidates", std::move(candidates)}
    }).dump());
    sendStatus("Duplicate scan complete for LOD" + std::to_string(lodIndex));
    return true;
}

bool ModelAssetEditorSession::saveSettings(
    const std::filesystem::path& sourceAssetsRoot,
    const std::filesystem::path& compiledModelsRoot,
    const std::string& locale)
{
    static const std::array<const char*, 5> supportedLocales {"en", "ru", "zh-Hans", "es", "ja"};
    if (std::find(supportedLocales.begin(), supportedLocales.end(), locale) == supportedLocales.end())
    {
        sendStatus("Unsupported editor locale: " + locale, true);
        return false;
    }
    if (sourceAssetsRoot.empty() || compiledModelsRoot.empty())
    {
        sendStatus("Settings paths cannot be empty", true);
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(sourceAssetsRoot, ec) || ec)
    {
        sendStatus("Source model root does not exist or is not a directory: " + sourceAssetsRoot.generic_string(), true);
        return false;
    }
    ec.clear();
    std::filesystem::create_directories(compiledModelsRoot, ec);
    if (ec)
    {
        sendStatus("Cannot create compiled model output directory: " + compiledModelsRoot.generic_string(), true);
        return false;
    }

    m_sourceAssetsRoot = sourceAssetsRoot.lexically_normal();
    m_compiledModelsRoot = compiledModelsRoot.lexically_normal();
    m_locale = locale;

    if (!writeSettingsFile())
        return false;

    // Acknowledge the write first. Catalog/asset refreshes can be comparatively
    // expensive in the Web UI and must never hide the result of pressing Save.
    m_server.broadcastText(json({
        {"type", "settings_saved"},
        {"sourceAssetsRoot", m_sourceAssetsRoot.generic_string()},
        {"compiledModelsRoot", m_compiledModelsRoot.generic_string()},
        {"settingsPath", settingsPath().generic_string()},
        {"locale", m_locale}
    }).dump());
    std::cerr << "[ModelAssetEditor] settings saved: "
              << settingsPath().generic_string()
              << " source=" << m_sourceAssetsRoot.generic_string()
              << " compiled=" << m_compiledModelsRoot.generic_string()
              << " locale=" << m_locale << '\n';
    sendStatus("Editor settings saved. Paths affect subsequent reimport/load/save operations; existing files are not moved.");
    sendSettings();
    sendCatalog();
    if (!m_asset.assetId.empty()) sendAssetMetadata();
    return true;
}


std::size_t ModelAssetEditorSession::lodCount() const
{
    return m_asset.renderLods.size();
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
        sendStatus("Cannot load independent LOD before the v4 package has been saved", true);
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
    if (lodIndex < m_asset.renderLods.size())
    {
        auto& lod = m_asset.renderLods[lodIndex];
        lod.geometries.clear();
        lod.nodes.clear();
    }
    state.loaded = false;
    syncDirty();
    sendAssetMetadata();
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
    sendAssetMetadata();
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
    sendAssetMetadata();
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

bool ModelAssetEditorSession::writeSettingsFile()
{
    const auto path = settingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        sendStatus("Cannot create editor settings directory: " + path.parent_path().generic_string(), true);
        return false;
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        sendStatus("Cannot write editor settings: " + path.generic_string(), true);
        return false;
    }
    out << json({
        {"version", 2},
        {"sourceAssetsRoot", m_sourceAssetsRoot.generic_string()},
        {"compiledModelsRoot", m_compiledModelsRoot.generic_string()},
        {"locale", m_locale}
    }).dump(2) << '\n';
    if (!out)
    {
        sendStatus("Cannot finish writing editor settings: " + path.generic_string(), true);
        return false;
    }
    return true;
}

bool ModelAssetEditorSession::setLocale(const std::string& locale)
{
    static const std::array<const char*, 5> supportedLocales {"en", "ru", "zh-Hans", "es", "ja"};
    if (std::find(supportedLocales.begin(), supportedLocales.end(), locale) == supportedLocales.end())
    {
        sendStatus("Unsupported editor locale: " + locale, true);
        return false;
    }
    m_locale = locale;
    if (!writeSettingsFile())
        return false;
    sendSettings();
    sendStatus("Editor locale: " + m_locale);
    return true;
}

void ModelAssetEditorSession::installLocalizationBundle()
{
    const auto path = m_sourceRoot / "src" / "assets" / "localization" / "ui" / "tools" / "model_asset_editor.json";
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::cerr << "[ModelAssetEditor] localization file not found: " << path << '\n';
        return;
    }
    std::ostringstream content;
    content << in.rdbuf();
    m_server.setVirtualFile(
        "/model_asset_editor_i18n.json",
        content.str(),
        "application/json; charset=utf-8");
}

void ModelAssetEditorSession::sendSettings()
{
    m_server.broadcastText(json({
        {"type", "settings"},
        {"editorVersion", ModelAssetEditorVersion},
        {"projectRoot", m_sourceRoot.generic_string()},
        {"sourceAssetsRoot", m_sourceAssetsRoot.generic_string()},
        {"compiledModelsRoot", m_compiledModelsRoot.generic_string()},
        {"defaultSourceAssetsRoot", defaultEditorSourceAssetsRoot(m_sourceRoot).generic_string()},
        {"defaultCompiledModelsRoot", (m_sourceRoot / "src" / "assets" / "compiled" / "models").generic_string()},
        {"settingsPath", settingsPath().generic_string()},
        {"locale", m_locale}
    }).dump());
}

void ModelAssetEditorSession::sendCatalog()
{
    json items = json::array();
    for (const auto& entry : m_catalog)
    {
        const auto packagePath = compiledPath(entry.id);
        const auto legacyPath = legacyCompiledPath(entry.id);
        const bool havePackage = std::filesystem::exists(packagePath);
        const bool haveLegacyV2 = std::filesystem::exists(legacyPath);
        const std::uint32_t packageVersion = havePackage ? packageFormatVersion(packagePath) : 0u;
        const std::uint32_t legacyVersion = havePackage ? packageVersion : (haveLegacyV2 ? packageFormatVersion(legacyPath) : 0u);
        items.push_back({
            {"id", entry.id},
            {"displayName", entry.displayName},
            {"compiled", havePackage || haveLegacyV2},
            {"compiledV4", havePackage && packageVersion == ModelAssetFormatVersion},
            {"legacyPackage", (havePackage && packageVersion > 0u && packageVersion < ModelAssetFormatVersion) || (!havePackage && haveLegacyV2)},
            {"legacyVersion", legacyVersion}
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
    loadWizardState();
    ModelAsset loaded;
    std::string error;
    std::string warning;
    const auto binary = compiledPath(id);
    const auto legacyBinary = legacyCompiledPath(id);
    const bool havePackage = std::filesystem::exists(binary);
    const bool haveLegacyV2 = !havePackage && std::filesystem::exists(legacyBinary);
    const auto readPath = havePackage ? binary : legacyBinary;
    const auto importProgress = [&](const ImportProgress& update)
    {
        sendProgress("reading", update.stage, update.completed, update.total, update.path);
    };

    std::string resumedCheckpointStage;
    const auto resumeCheckpoint = forceReimport ? std::filesystem::path{} :
        latestWizardCheckpoint(&resumedCheckpointStage);
    const bool resumeWorkspace = !resumeCheckpoint.empty();

    if (resumeWorkspace)
    {
        sendStatus("Resuming editor workspace from " + resumedCheckpointStage + " checkpoint...", false, "reading");
        sendProgress("reading", "RESUME CHECKPOINT", 0, 1, resumeCheckpoint);
        if (!ModelAssetBinary::load(resumeCheckpoint.string(), loaded, &error))
        {
            // Never silently fall back to an older production package: that
            // would make saved editor work appear to have vanished.
            sendStatus("Cannot resume latest wizard checkpoint '" + resumedCheckpointStage +
                "': " + error + ". Production package was not loaded instead.", true);
            return false;
        }
        if (!loaded.assetId.empty() && loaded.assetId != id)
        {
            sendStatus("Cannot resume wizard checkpoint: checkpoint asset id '" + loaded.assetId +
                "' does not match selected asset '" + id + "'", true);
            return false;
        }
        m_asset = std::move(loaded);
        // Checkpoints are complete authoring snapshots. They are current with
        // respect to the wizard checkpoint, but intentionally dirty relative
        // to the production package until the user explicitly saves output.
        resetLodState(true, true);
        sendProgress("reading", "RESUME CHECKPOINT", 1, 1, resumeCheckpoint);
    }
    else if (!forceReimport && (havePackage || haveLegacyV2))
    {
        sendStatus("Reading compiled model asset " + readPath.filename().string() + "...", false, "reading");
        sendProgress("reading", "READ MANIFEST", 0, 1, readPath);
        bool legacyPackage = false;
        if (!ModelAssetBinary::loadManifest(readPath.string(), loaded, &legacyPackage, &error))
        {
            if (error == "unsupported model asset version")
            {
                sendStatus("Compiled asset format is obsolete; reimporting source...", false, "reading");
                if (!importRuntimeAssembly(
                        m_sourceAssetsRoot, it->type, it->id, it->displayName, loaded,
                        &error, &warning, importProgress))
                {
                    sendStatus("Cannot reimport obsolete compiled asset: " + error, true);
                    return false;
                }
                buildIndependentRenderLodsFromLegacy(loaded);
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
        else if (legacyPackage)
        {
            const std::uint32_t oldVersion = loaded.formatVersion;
            sendProgress("reading", "MIGRATE LEGACY PACKAGE", 0, 1, readPath);
            if (!ModelAssetBinary::load(readPath.string(), loaded, &error))
            {
                sendStatus("Cannot migrate legacy compiled asset: " + error, true);
                return false;
            }
            loaded.formatVersion = ModelAssetFormatVersion;
            m_asset = std::move(loaded);
            resetLodState(true, true);
            if (oldVersion == 3u && havePackage)
            {
                warning = "Legacy asset v3 loaded and converted in memory to asset v4. Save all upgrades the compiled package in place to independent render LOD graphs; source OBJ/assembly files remain unchanged.";
            }
            else
            {
                warning = "Legacy asset v" + std::to_string(oldVersion) +
                    " loaded and converted in memory to asset v4. Save all writes the v4 package beside the untouched legacy monolithic binary; source OBJ/assembly files remain unchanged.";
            }
            sendProgress("reading", "MIGRATE LEGACY PACKAGE", 1, 1, readPath);
        }
        else
        {
            sendProgress("reading", "READ V4 MANIFEST", 1, 1, readPath);
            m_asset = std::move(loaded);
            resetLodState(false, false);
            if (!m_lodState.empty())
            {
                const auto lod0Path = ModelAssetBinary::lodPayloadPath(binary.string(), 0);
                sendProgress("reading", "LOAD RENDER LOD0", 0, 1, lod0Path);
                if (!ModelAssetBinary::loadLod(binary.string(), m_asset, 0, &error))
                {
                    sendStatus("Cannot load LOD0 render graph: " + error, true);
                    return false;
                }
                m_lodState[0].loaded = true;
                sendProgress("reading", "LOAD RENDER LOD0", 1, 1, lod0Path);
            }
            syncDirty();
        }
    }
    else
    {
        sendStatus("Importing source OBJ/assembly...", false, "reading");
        if (!importRuntimeAssembly(
                m_sourceAssetsRoot, it->type, it->id, it->displayName, loaded,
                &error, &warning, importProgress))
        {
            sendStatus("Cannot import source assembly: " + error, true);
            return false;
        }
        buildIndependentRenderLodsFromLegacy(loaded);
        loaded.formatVersion = ModelAssetFormatVersion;
        m_asset = std::move(loaded);
        resetLodState(true, true);
    }

    if (forceReimport)
    {
        invalidateWizardFrom("source");
        (void)writeWizardState();
    }
    sendProgress("reading", "LOAD VIEW", 0, 1, readPath.empty() ? binary : readPath);
    sendAsset();
    if (!warning.empty()) sendStatus(warning);
    else if (resumeWorkspace)
        sendStatus("Workspace resumed from the latest " + resumedCheckpointStage +
            " checkpoint; production package and source OBJ are unchanged");
    else sendStatus(forceReimport ? "Source assembly imported into independent render LODs" :
        "Asset loaded; semantic state is shared, render LOD graphs are independent");
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
    sendStatus("Saving dirty v4 package members...", false, "writing");

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
    sendAssetMetadata();
    sendStatus("Saved dirty package members only; clean LOD files and source OBJ/assembly were not rewritten");
    sendCatalog();
    return true;
}

nlohmann::json ModelAssetEditorSession::serializeAsset(bool includeGeometryPayload) const
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
    out["geometryPayloadIncluded"] = includeGeometryPayload;
    out["wizard"] = serializeWizard();

    out["materials"] = json::array();
    for (std::size_t i = 0; i < m_asset.materials.size(); ++i)
    {
        const auto& m = m_asset.materials[i];
        out["materials"].push_back({{"index", i}, {"id", m.id}, {"sourceName", m.sourceName}, {"baseColor", vec4Json(m.baseColor)}, {"emissiveColor", vec3Json(m.emissiveColor)}, {"emissiveStrength", m.emissiveStrength}, {"metallic", m.metallic}, {"roughness", m.roughness}, {"twoSided", m.twoSided}, {"baseColorTexture", m.baseColorTexture}, {"emissiveTexture", m.emissiveTexture}});
    }

    // Semantic assembly is deliberately render-LOD agnostic.
    out["nodes"] = json::array();
    for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
    {
        const auto& n = m_asset.nodes[ni];
        std::size_t variantCount = 0;
        for (const auto& variant : m_asset.stateVariants)
            if (variant.nodeIndex == static_cast<std::int32_t>(ni)) ++variantCount;
        out["nodes"].push_back({
            {"index", ni}, {"id", n.id}, {"moduleId", n.moduleId}, {"parentIndex", n.parentIndex},
            {"defaultStateId", n.defaultStateId}, {"stateVariantCount", variantCount},
            {"localPosition", vec3Json(n.localPosition)}, {"localRotationDeg", vec3Json(n.localRotationDeg)}, {"pivot", vec3Json(n.pivot)}, {"enabled", n.enabled},
            {"joint", {{"type", jointTypeName(n.joint.type)}, {"pivot", vec3Json(n.joint.pivot)}, {"axis", vec3Json(n.joint.axis)}, {"defaultRateDegPerSec", n.joint.defaultRateDegPerSec}, {"minAngleDeg", n.joint.minAngleDeg}, {"maxAngleDeg", n.joint.maxAngleDeg}, {"breakable", n.joint.breakable}, {"breakForceN", n.joint.breakForceN}, {"breakTorqueNm", n.joint.breakTorqueNm}}},
            {"physics", {{"mode", massModeName(n.physics.mode)}, {"densityKgM3", n.physics.densityKgM3}, {"massKg", n.physics.massKg}, {"centerOfMass", vec3Json(n.physics.centerOfMass)}, {"inertiaDiagonal", vec3Json(n.physics.inertiaDiagonal)}, {"inertiaProducts", vec3Json(n.physics.inertiaProducts)}}}
        });
    }

    out["stateVariants"] = json::array();
    for (std::size_t i = 0; i < m_asset.stateVariants.size(); ++i)
    {
        const auto& v = m_asset.stateVariants[i];
        out["stateVariants"].push_back({
            {"index", i}, {"id", v.id}, {"displayName", v.displayName}, {"nodeIndex", v.nodeIndex},
            {"transformOverride", v.transformOverride}, {"localPosition", vec3Json(v.localPosition)},
            {"localRotationDeg", vec3Json(v.localRotationDeg)}, {"pivot", vec3Json(v.pivot)},
            {"physicsOverride", v.physicsOverride}, {"detached", v.detached}, {"enabled", v.enabled},
            {"physics", {{"mode", massModeName(v.physics.mode)}, {"densityKgM3", v.physics.densityKgM3}, {"massKg", v.physics.massKg}, {"centerOfMass", vec3Json(v.physics.centerOfMass)}, {"inertiaDiagonal", vec3Json(v.physics.inertiaDiagonal)}, {"inertiaProducts", vec3Json(v.physics.inertiaProducts)}}}
        });
    }

    out["renderLods"] = json::array();
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        const auto& lod = m_asset.renderLods[li];
        json jl = {
            {"index", li}, {"level", lod.level}, {"sourceKind", lod.sourceKind}, {"generatedFromLod", lod.generatedFromLod},
            {"minBounds", vec3Json(lod.minBounds)}, {"maxBounds", vec3Json(lod.maxBounds)},
            {"loaded", li < m_lodState.size() && m_lodState[li].loaded},
            {"dirty", li < m_lodState.size() && m_lodState[li].dirty}
        };
        jl["geometries"] = json::array();
        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            const auto& geometry = lod.geometries[gi];
            json usedBy = json::array();
            for (const auto& renderNode : lod.nodes)
                if (renderNode.geometryIndex == static_cast<std::int32_t>(gi)) usedBy.push_back(renderNode.id);
            const auto& mesh = geometry.mesh;
            const auto variantIdentity = renderVariantIdentity(geometry.id);
            const std::string authoringVariantId = variantIdentity.isVariant
                ? sourceVariantAuthoringId(li, geometry) : std::string();
            const std::string stableBaseVisualId = variantIdentity.isVariant
                ? std::string() : baseVisualId(li, geometry.id);
            const auto replacementIds = variantIdentity.isVariant
                ? sourceVariantReplacementIds(authoringVariantId) : std::vector<std::string>{};
            json g = {
                {"index", gi}, {"id", geometry.id}, {"sourcePath", geometry.sourcePath},
                {"sourceFileName", std::filesystem::path(geometry.sourcePath).filename().string()},
                {"surfaceMode", surfaceModeName(geometry.surfaceMode)}, {"usageCount", usedBy.size()}, {"usedBy", std::move(usedBy)},
                {"isSourceVariant", variantIdentity.isVariant},
                {"variantId", authoringVariantId},
                {"baseVisualId", stableBaseVisualId},
                {"replacesBaseVisualIds", replacementIds},
                {"minBounds", vec3Json(mesh.minBounds)}, {"maxBounds", vec3Json(mesh.maxBounds)},
                {"vertexCount", mesh.vertices.size()}, {"triangleCount", mesh.triangles.size()}, {"edgeCount", mesh.edges.size()},
                {"estimatedBinaryBytes", estimatedRenderGeometryBinaryBytes(geometry)}
            };
            if (includeGeometryPayload)
            {
                g["positions"] = json::array(); g["normals"] = json::array(); g["indices"] = json::array(); g["triangleMaterials"] = json::array(); g["smoothingGroups"] = json::array(); g["edges"] = json::array();
                for (const auto& vertex : mesh.vertices)
                {
                    g["positions"].push_back(vertex.position.x); g["positions"].push_back(vertex.position.y); g["positions"].push_back(vertex.position.z);
                    g["normals"].push_back(vertex.normal.x); g["normals"].push_back(vertex.normal.y); g["normals"].push_back(vertex.normal.z);
                }
                for (const auto& triangle : mesh.triangles)
                {
                    g["indices"].push_back(triangle.a); g["indices"].push_back(triangle.b); g["indices"].push_back(triangle.c);
                    g["triangleMaterials"].push_back(triangle.materialIndex); g["smoothingGroups"].push_back(triangle.smoothingGroupId);
                }
                for (std::size_t ei = 0; ei < mesh.edges.size(); ++ei)
                {
                    const auto& edge = mesh.edges[ei];
                    g["edges"].push_back({{"index", ei}, {"a", edge.a}, {"b", edge.b}, {"triangleA", edge.triangleA}, {"triangleB", edge.triangleB}, {"flags", edge.flags}, {"renderMask", edge.renderMask}});
                }
            }
            jl["geometries"].push_back(std::move(g));
        }
        jl["nodes"] = json::array();
        for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
        {
            const auto& node = lod.nodes[ri];
            jl["nodes"].push_back({
                {"index", ri}, {"id", node.id}, {"parentIndex", node.parentIndex}, {"geometryIndex", node.geometryIndex},
                {"semanticNodeIndex", node.semanticNodeIndex}, {"activeStates", node.activeStates},
                {"localPosition", vec3Json(node.localPosition)}, {"localRotationDeg", vec3Json(node.localRotationDeg)},
                {"pivot", vec3Json(node.pivot)}, {"enabled", node.enabled}
            });
        }
        out["renderLods"].push_back(std::move(jl));
    }

    out["collisionVolumes"] = json::array();
    for (std::size_t ci = 0; ci < m_asset.collisionVolumes.size(); ++ci)
    {
        const auto& c = m_asset.collisionVolumes[ci];
        out["collisionVolumes"].push_back({{"index", ci}, {"id", c.id}, {"moduleId", c.moduleId}, {"parentNodeIndex", c.parentNodeIndex}, {"shape", collisionShapeName(c.shape)}, {"activeStates", c.activeStates}, {"localPosition", vec3Json(c.localPosition)}, {"localRotationDeg", vec3Json(c.localRotationDeg)}, {"halfSize", vec3Json(c.halfSize)}, {"radius", c.radius}, {"halfHeight", c.halfHeight}, {"enabled", c.enabled}});
    }
    out["sockets"] = json::array();
    for (std::size_t si = 0; si < m_asset.sockets.size(); ++si)
    {
        const auto& socket = m_asset.sockets[si];
        out["sockets"].push_back({{"index", si}, {"id", socket.id}, {"kind", socket.kind}, {"moduleId", socket.moduleId}, {"parentNodeIndex", socket.parentNodeIndex}, {"activeStates", socket.activeStates}, {"localPosition", vec3Json(socket.localPosition)}, {"localRotationDeg", vec3Json(socket.localRotationDeg)}, {"extent", vec3Json(socket.extent)}, {"enabled", socket.enabled}, {"light", {{"type", lightTypeName(socket.light.type)}, {"color", vec3Json(socket.light.color)}, {"intensity", socket.light.intensity}, {"rangeMeters", socket.light.rangeMeters}, {"outerConeDeg", socket.light.outerConeDeg}}}});
    }

    out["hitRegions"] = json::array();
    for (std::size_t i = 0; i < m_asset.hitRegions.size(); ++i)
    {
        const auto& h = m_asset.hitRegions[i];
        out["hitRegions"].push_back({{"index", i}, {"id", h.id}, {"parentNodeIndex", h.parentNodeIndex}, {"activeStates", h.activeStates}, {"localPosition", vec3Json(h.localPosition)}, {"localRotationDeg", vec3Json(h.localRotationDeg)}, {"halfSize", vec3Json(h.halfSize)}, {"enabled", h.enabled}});
    }
    out["openings"] = json::array();
    for (std::size_t i = 0; i < m_asset.openings.size(); ++i)
    {
        const auto& o = m_asset.openings[i];
        out["openings"].push_back({{"index", i}, {"id", o.id}, {"parentNodeIndex", o.parentNodeIndex}, {"activeStates", o.activeStates}, {"localPosition", vec3Json(o.localPosition)}, {"localRotationDeg", vec3Json(o.localRotationDeg)}, {"halfSize", vec3Json(o.halfSize)}, {"traversable", o.traversable}, {"lineOfFire", o.lineOfFire}, {"enabled", o.enabled}});
    }
    out["repairTargets"] = json::array();
    for (std::size_t i = 0; i < m_asset.repairTargets.size(); ++i)
    {
        const auto& t = m_asset.repairTargets[i];
        out["repairTargets"].push_back({{"index", i}, {"id", t.id}, {"kind", t.kind}, {"parentNodeIndex", t.parentNodeIndex}, {"activeStates", t.activeStates}, {"localPosition", vec3Json(t.localPosition)}, {"localRotationDeg", vec3Json(t.localRotationDeg)}, {"repairedStateId", t.repairedStateId}, {"enabled", t.enabled}});
    }

    std::uint64_t sourceBytes = 0, estimatedGeometryBytes = 0, estimatedUnusedGeometryBytes = 0;
    std::size_t unusedGeometryCount = 0, sourceVariantGeometryCount = 0;
    std::set<std::string> countedSourceFiles;
    for (const auto& lod : m_asset.renderLods)
    {
        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            const auto& geometry = lod.geometries[gi];
            const auto geometryBytes = estimatedRenderGeometryBinaryBytes(geometry);
            estimatedGeometryBytes += geometryBytes;
            const bool used = std::any_of(lod.nodes.begin(), lod.nodes.end(), [gi](const RenderNode& node) { return node.geometryIndex == static_cast<std::int32_t>(gi); });
            const bool sourceVariant = isRenderVariantGeometryId(geometry.id);
            if (sourceVariant) ++sourceVariantGeometryCount;
            if (!used && !sourceVariant) { ++unusedGeometryCount; estimatedUnusedGeometryBytes += geometryBytes; }
            if (!geometry.sourcePath.empty() && countedSourceFiles.insert(geometry.sourcePath).second)
                sourceBytes += safeFileBytes(editorSourceFilePath(m_sourceAssetsRoot, geometry.sourcePath));
        }
    }

    const auto binary = compiledPath(m_asset.assetId);
    const auto legacyBinary = legacyCompiledPath(m_asset.assetId);
    json lodPayloads = json::array();
    std::uint64_t savedPackageBytes = safeFileBytes(binary);
    const auto savedLodPayloads = discoverSavedLodPayloads(binary);
    std::set<std::size_t> lodIndices;
    for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex) lodIndices.insert(lodIndex);
    for (const auto& [lodIndex, unusedPath] : savedLodPayloads) { (void)unusedPath; lodIndices.insert(lodIndex); }
    for (const std::size_t lodIndex : lodIndices)
    {
        const auto saved = savedLodPayloads.find(lodIndex);
        const auto lodPath = saved != savedLodPayloads.end() ? saved->second : ModelAssetBinary::lodPayloadPath(binary.string(), lodIndex);
        const auto bytes = safeFileBytes(lodPath);
        if (saved != savedLodPayloads.end()) savedPackageBytes += bytes;
        lodPayloads.push_back({
            {"lod", lodIndex}, {"path", lodPath.generic_string()}, {"bytes", bytes},
            {"declared", lodIndex < m_asset.renderLods.size()},
            {"loaded", lodIndex < m_lodState.size() && m_lodState[lodIndex].loaded},
            {"dirty", lodIndex < m_lodState.size() && m_lodState[lodIndex].dirty}
        });
    }
    out["storage"] = {
        {"binaryPath", binary.generic_string()}, {"manifestBytes", safeFileBytes(binary)}, {"manifestDirty", m_manifestDirty},
        {"savedPackageBytes", savedPackageBytes}, {"lodPayloads", std::move(lodPayloads)},
        {"legacyBinaryPath", legacyBinary.generic_string()}, {"legacyBinaryBytes", safeFileBytes(legacyBinary)},
        {"sourceMeshBytes", sourceBytes}, {"estimatedGeometryPayloadBytes", estimatedGeometryBytes},
        {"estimatedUnusedGeometryBytes", estimatedUnusedGeometryBytes}, {"unusedGeometryCount", unusedGeometryCount},
        {"sourceVariantGeometryCount", sourceVariantGeometryCount}, {"sourceFilesReadOnly", true}
    };
    return out;
}

void ModelAssetEditorSession::sendAsset()
{
    reconcileAuthoringVisualRegistry();
    m_server.broadcastText(json({{"type", "asset"}, {"dirty", m_dirty}, {"asset", serializeAsset(true)}}).dump());
}

void ModelAssetEditorSession::sendAssetMetadata(const nlohmann::json& hints)
{
    reconcileAuthoringVisualRegistry();
    json payload = {{"type", "asset_metadata"}, {"dirty", m_dirty}, {"asset", serializeAsset(false)}};
    if (hints.is_object())
        for (const auto& [key, value] : hints.items())
            payload[key] = value;
    m_server.broadcastText(payload.dump());
}

std::vector<std::string> ModelAssetEditorSession::sourceVariantReplacementIds(
    const std::string& variantId) const
{
    const auto variantIt = m_sourceVariantReplacements.find(variantId);
    return variantIt == m_sourceVariantReplacements.end()
        ? std::vector<std::string>{} : variantIt->second;
}

bool ModelAssetEditorSession::setSourceVariantReplacement(
    std::size_t lodIndex,
    const std::string& variantId,
    const std::string& requestedBaseVisualId,
    bool allowed)
{
    if (lodIndex >= m_asset.renderLods.size())
        throw std::runtime_error("invalid LOD index");
    if (variantId.empty() || requestedBaseVisualId.empty())
        throw std::runtime_error("variant/base visual id cannot be empty");

    reconcileAuthoringVisualRegistry();
    const auto& lod = m_asset.renderLods[lodIndex];
    const bool haveVariant = std::any_of(
        lod.geometries.begin(), lod.geometries.end(), [&](const RenderGeometryDefinition& geometry)
        {
            return isRenderVariantGeometryId(geometry.id) &&
                sourceVariantAuthoringId(lodIndex, geometry) == variantId;
        });
    if (!haveVariant)
        throw std::runtime_error("additional mesh is not loaded in LOD" + std::to_string(lodIndex));

    const bool haveBase = std::any_of(
        lod.geometries.begin(), lod.geometries.end(), [&](const RenderGeometryDefinition& geometry)
        {
            return !isRenderVariantGeometryId(geometry.id) &&
                baseVisualId(lodIndex, geometry.id) == requestedBaseVisualId;
        });
    if (!haveBase)
        throw std::runtime_error("replacement target base visual is not loaded in LOD" + std::to_string(lodIndex));

    auto& values = m_sourceVariantReplacements[variantId];
    const auto it = std::find(values.begin(), values.end(), requestedBaseVisualId);
    if (allowed)
    {
        if (it == values.end()) values.push_back(requestedBaseVisualId);
        std::sort(values.begin(), values.end());
    }
    else if (it != values.end())
    {
        values.erase(it);
    }
    if (values.empty()) m_sourceVariantReplacements.erase(variantId);

    // Compatibility is authoring data. DAMAGE later decides which compatible
    // visual is chosen for a concrete hit/state; GEOMETRY only defines the set.
    invalidateWizardFrom("geometry");
    sendAssetMetadata();
    sendStatus(
        std::string(allowed ? "Allowed " : "Disallowed ") + variantId +
        " as replacement for " + requestedBaseVisualId +
        " in LOD" + std::to_string(lodIndex));
    return true;
}

bool ModelAssetEditorSession::refreshSourceVariants()
{
    if (m_asset.assetId.empty())
    {
        sendStatus("No asset loaded", true);
        return false;
    }

    reconcileAuthoringVisualRegistry();
    struct VariantJob
    {
        std::size_t lodIndex = 0;
        SourceAdditionalMesh source;
        std::string variantId;
    };

    std::map<std::pair<std::size_t, std::string>, VariantJob> discovered;
    std::vector<std::string> discoveryWarnings;
    std::vector<std::string> scannedLodRoots;
    bool registryChanged = false;
    for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex)
    {
        if (lodIndex >= m_lodState.size() || !m_lodState[lodIndex].loaded)
            continue;
        const auto& lod = m_asset.renderLods[lodIndex];
        std::vector<std::string> knownRuntimePaths = runtimeAssemblyLodSourcePaths(
            static_cast<ObjectType>(m_asset.sourceObjectType), lodIndex);
        if (knownRuntimePaths.empty())
        {
            for (const auto& geometry : lod.geometries)
            {
                if (!geometry.sourcePath.empty() && !isRenderVariantGeometryId(geometry.id))
                    knownRuntimePaths.push_back(geometry.sourcePath);
            }
        }
        if (knownRuntimePaths.empty()) continue;

        std::vector<std::string> lodWarnings;
        const auto additional = discoverAdditionalLodMeshes(
            m_sourceAssetsRoot, knownRuntimePaths, &lodWarnings);
        discoveryWarnings.insert(
            discoveryWarnings.end(), lodWarnings.begin(), lodWarnings.end());

        std::set<std::string> roots;
        for (const auto& runtimePath : knownRuntimePaths)
        {
            const auto resolved = editorSourceFilePath(m_sourceAssetsRoot, runtimePath);
            auto directory = resolved.parent_path();
            while (!directory.empty())
            {
                std::string name = directory.filename().string();
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                const bool isLod = name.size() > 3 && name.rfind("lod", 0) == 0 &&
                    std::all_of(name.begin() + 3, name.end(), [](unsigned char c) {
                        return std::isdigit(c) != 0;
                    });
                if (isLod)
                {
                    roots.insert(directory.generic_string());
                    break;
                }
                const auto parent = directory.parent_path();
                if (parent == directory) break;
                directory = parent;
            }
        }
        for (const auto& root : roots)
            scannedLodRoots.push_back("LOD" + std::to_string(lodIndex) + "=" + root);

        for (const auto& source : additional)
        {
            auto& id = m_sourceExtraMeshIds[lodIndex][source.runtimePath];
            if (id.empty())
            {
                id = allocateSourceVariantId();
                registryChanged = true;
            }
            const auto key = std::make_pair(lodIndex, source.runtimePath);
            discovered.emplace(key, VariantJob{lodIndex, source, id});
        }
    }

    const auto joinedRoots = [&]()
    {
        std::ostringstream out;
        for (std::size_t i = 0; i < scannedLodRoots.size(); ++i)
        {
            if (i) out << ", ";
            out << scannedLodRoots[i];
        }
        return out.str();
    };

    std::vector<VariantJob> jobs;
    jobs.reserve(discovered.size());
    for (auto& [key, job] : discovered)
    {
        (void)key;
        jobs.push_back(std::move(job));
    }

    if (jobs.empty())
    {
        if (registryChanged) (void)writeWizardState();
        sendStatus(
            "NO CHANGES: no additional OBJ files found recursively under loaded LOD directories" +
            (scannedLodRoots.empty() ? std::string() : "; scanned " + joinedRoots()));
        return true;
    }

    std::size_t added = 0, updated = 0, unchanged = 0, failed = 0, completed = 0;
    bool manifestChanged = false;
    std::set<std::size_t> changedLods;
    std::vector<std::string> failures = discoveryWarnings;
    for (const auto& job : jobs)
    {
        if (job.lodIndex >= m_asset.renderLods.size()) continue;
        auto& lod = m_asset.renderLods[job.lodIndex];
        const std::string variantGeometryId = makeRenderVariantGeometryId(job.variantId);

        sendProgress(
            "reading", "REFRESH EXTRA LOD MESHES", completed, jobs.size(),
            job.source.file);
        MeshLod mesh;
        std::string importError;
        const std::size_t materialsBefore = m_asset.materials.size();
        const bool imported = importObjNative(
            job.source.file, m_asset, mesh, &importError);
        manifestChanged = manifestChanged || m_asset.materials.size() != materialsBefore;
        if (!imported)
        {
            ++failed;
            ++completed;
            failures.push_back(job.variantId + ": " + importError);
            sendProgress(
                "reading", "REFRESH EXTRA LOD MESHES", completed, jobs.size(),
                job.source.file);
            continue;
        }

        auto existing = std::find_if(
            lod.geometries.begin(), lod.geometries.end(),
            [&](const RenderGeometryDefinition& geometry)
            {
                if (!isRenderVariantGeometryId(geometry.id)) return false;
                if (!geometry.sourcePath.empty() && geometry.sourcePath == job.source.runtimePath)
                    return true;
                return sourceVariantAuthoringId(job.lodIndex, geometry) == job.variantId;
            });
        if (existing == lod.geometries.end())
        {
            RenderGeometryDefinition variant;
            variant.id = variantGeometryId;
            variant.sourcePath = job.source.runtimePath;
            variant.mesh = std::move(mesh);
            lod.geometries.push_back(std::move(variant));
            changedLods.insert(job.lodIndex);
            ++added;
        }
        else if (existing->id == variantGeometryId &&
                 existing->sourcePath == job.source.runtimePath &&
                 sameMeshLodExact(existing->mesh, mesh))
        {
            ++unchanged;
        }
        else
        {
            existing->id = variantGeometryId;
            existing->sourcePath = job.source.runtimePath;
            existing->mesh = std::move(mesh);
            changedLods.insert(job.lodIndex);
            ++updated;
        }
        ++completed;
        sendProgress(
            "reading", "REFRESH EXTRA LOD MESHES", completed, jobs.size(),
            job.source.file);
    }

    for (const auto lodIndex : changedLods)
    {
        recomputeRenderLodBounds(m_asset.renderLods[lodIndex]);
        markLodDirty(lodIndex);
    }
    if (manifestChanged) markManifestDirty();
    if (registryChanged && !writeWizardState())
    {
        sendStatus("Extra meshes were refreshed, but their stable authoring ids could not be saved", true);
        return false;
    }
    if (!changedLods.empty() || manifestChanged)
        invalidateWizardFrom("geometry");
    if (!changedLods.empty())
        sendAsset(); // Geometry payload really changed; refresh browser cache once.
    else if (manifestChanged || registryChanged)
        sendAssetMetadata();

    std::string message =
        "Additional LOD meshes refreshed: " +
        std::to_string(added) + " added, " + std::to_string(updated) +
        " updated, " + std::to_string(unchanged) + " unchanged, " +
        std::to_string(failed) + " failed across " +
        std::to_string(changedLods.size()) + " loaded LOD(s)";
    if (!failures.empty()) message += "; first note: " + failures.front();
    sendStatus(message, failed != 0);
    return failed == 0;
}

void ModelAssetEditorSession::handleMessage(const std::string& payload)
{
    try
    {
        const json message = json::parse(payload);
        const std::string command = message.value("command", "");

        if (command == "request_catalog") { sendCatalog(); if (!m_selectedId.empty()) sendAsset(); return; }
        if (command == "request_settings") { sendSettings(); return; }
        if (command == "save_settings")
        {
            const auto source = std::filesystem::path(message.value("sourceAssetsRoot", std::string()));
            const auto compiled = std::filesystem::path(message.value("compiledModelsRoot", std::string()));
            const auto locale = message.value("locale", m_locale);
            std::cerr << "[ModelAssetEditor] save_settings requested: source="
                      << source.generic_string()
                      << " compiled=" << compiled.generic_string()
                      << " locale=" << locale << '\n';
            saveSettings(source, compiled, locale);
            return;
        }
        if (command == "set_locale") { setLocale(message.value("locale", m_locale)); return; }
        if (command == "select_asset") { selectAsset(message.value("assetId", ""), false); return; }
        if (command == "reimport_asset") { selectAsset(m_selectedId, true); return; }
        if (command == "save_asset") { saveAsset(); return; }
        if (command == "save_manifest") { saveManifestOnly(); return; }
        if (command == "save_lod") { saveLodOnly(message.value("lodIndex", std::size_t(-1))); return; }
        if (command == "load_lod") { loadLodOnly(message.value("lodIndex", std::size_t(-1)), false); return; }
        if (command == "reload_lod") { loadLodOnly(message.value("lodIndex", std::size_t(-1)), true); return; }
        if (command == "unload_lod") { unloadLod(message.value("lodIndex", std::size_t(-1))); return; }
        if (command == "complete_wizard_stage") { completeWizardStage(message.value("stage", std::string())); return; }
        if (command == "restore_wizard_checkpoint") { restoreWizardCheckpoint(message.value("stage", std::string())); return; }
        if (command == "scan_render_duplicates")
        {
            scanRenderDuplicates(
                message.value("lodIndex", std::size_t(-1)),
                message.value("referenceRenderNodeIndex", std::size_t(-1)),
                jsonIndices(message.value("targetRenderNodeIndices", json::array())));
            return;
        }

        if (m_asset.assetId.empty()) { sendStatus("No asset loaded", true); return; }
        if (command == "refresh_source_variants") { refreshSourceVariants(); return; }
        if (command == "set_source_variant_replacement")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            std::string requestedBaseId = message.value("baseVisualId", std::string());
            if (requestedBaseId.empty())
                requestedBaseId = baseVisualId(
                    lodIndex, message.value("baseGeometryId", std::string()));
            setSourceVariantReplacement(
                lodIndex,
                message.value("variantId", std::string()),
                requestedBaseId,
                message.value("allowed", false));
            return;
        }

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
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated node transform: " + n.id); return;
        }
        if (command == "set_node_default_state")
        {
            const auto nodeIndex = message.value("nodeIndex", std::size_t(-1));
            const std::string stateId = message.value("stateId", std::string("intact"));
            if (nodeIndex >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            if (stateId != "intact")
            {
                const bool exists = std::any_of(m_asset.stateVariants.begin(), m_asset.stateVariants.end(), [&](const StateVariant& variant) {
                    return variant.nodeIndex == static_cast<std::int32_t>(nodeIndex) && variant.id == stateId;
                });
                if (!exists) throw std::runtime_error("default state is not declared for this semantic node");
            }
            m_asset.nodes[nodeIndex].defaultStateId = stateId;
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated default semantic state: " + m_asset.nodes[nodeIndex].id + " / " + stateId); return;
        }
        if (command == "add_state_variant")
        {
            const auto nodeIndex = message.value("nodeIndex", std::size_t(-1));
            if (nodeIndex >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            StateVariant variant;
            variant.nodeIndex = static_cast<std::int32_t>(nodeIndex);
            variant.id = message.value("id", std::string());
            if (variant.id.empty()) throw std::runtime_error("state id cannot be empty");
            if (variant.id == "intact") throw std::runtime_error("intact is the implicit base state and cannot be added as a variant");
            for (const auto& existing : m_asset.stateVariants)
                if (existing.nodeIndex == variant.nodeIndex && existing.id == variant.id)
                    throw std::runtime_error("state id already exists for this semantic node");
            variant.displayName = message.value("displayName", variant.id);
            variant.transformOverride = message.value("transformOverride", false);
            variant.localPosition = jsonVec3(message.value("position", json::array()), m_asset.nodes[nodeIndex].localPosition);
            variant.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), m_asset.nodes[nodeIndex].localRotationDeg);
            variant.pivot = jsonVec3(message.value("pivot", json::array()), m_asset.nodes[nodeIndex].pivot);
            variant.physicsOverride = message.value("physicsOverride", false);
            variant.physics = m_asset.nodes[nodeIndex].physics;
            variant.detached = message.value("detached", false);
            m_asset.stateVariants.push_back(std::move(variant));
            markManifestDirty(); sendAssetMetadata(); sendStatus("Added semantic state variant: " + m_asset.nodes[nodeIndex].id + " / " + m_asset.stateVariants.back().id); return;
        }
        if (command == "set_state_variant")
        {
            const auto index = message.value("variantIndex", std::size_t(-1));
            if (index >= m_asset.stateVariants.size()) throw std::runtime_error("invalid state variant index");
            auto& variant = m_asset.stateVariants[index];
            if (message.contains("displayName")) variant.displayName = message.value("displayName", variant.displayName);
            if (message.contains("transformOverride")) variant.transformOverride = message.value("transformOverride", variant.transformOverride);
            if (message.contains("position")) variant.localPosition = jsonVec3(message["position"], variant.localPosition);
            if (message.contains("rotationDeg")) variant.localRotationDeg = jsonVec3(message["rotationDeg"], variant.localRotationDeg);
            if (message.contains("pivot")) variant.pivot = jsonVec3(message["pivot"], variant.pivot);
            if (message.contains("physicsOverride")) variant.physicsOverride = message.value("physicsOverride", variant.physicsOverride);
            if (message.contains("massMode")) variant.physics.mode = massModeFromName(message.value("massMode", std::string(massModeName(variant.physics.mode))));
            variant.physics.densityKgM3 = message.value("densityKgM3", variant.physics.densityKgM3);
            variant.physics.massKg = message.value("massKg", variant.physics.massKg);
            if (message.contains("centerOfMass")) variant.physics.centerOfMass = jsonVec3(message["centerOfMass"], variant.physics.centerOfMass);
            if (message.contains("inertiaDiagonal")) variant.physics.inertiaDiagonal = jsonVec3(message["inertiaDiagonal"], variant.physics.inertiaDiagonal);
            if (message.contains("inertiaProducts")) variant.physics.inertiaProducts = jsonVec3(message["inertiaProducts"], variant.physics.inertiaProducts);
            if (message.contains("detached")) variant.detached = message.value("detached", variant.detached);
            if (message.contains("enabled")) variant.enabled = message.value("enabled", variant.enabled);
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated semantic state variant: " + variant.id); return;
        }
        if (command == "delete_state_variant")
        {
            if (!ensureAllLodsLoaded()) return;
            const auto index = message.value("variantIndex", std::size_t(-1));
            if (index >= m_asset.stateVariants.size()) throw std::runtime_error("invalid state variant index");
            const auto variant = m_asset.stateVariants[index];
            const auto referencesState = [&](std::int32_t parentNodeIndex, const std::vector<std::string>& activeStates) {
                return parentNodeIndex == variant.nodeIndex && std::find(activeStates.begin(), activeStates.end(), variant.id) != activeStates.end();
            };
            for (const auto& lod : m_asset.renderLods)
                for (const auto& renderNode : lod.nodes)
                    if (referencesState(renderNode.semanticNodeIndex, renderNode.activeStates))
                        throw std::runtime_error("state variant is still referenced by a render node");
            for (const auto& c : m_asset.collisionVolumes) if (referencesState(c.parentNodeIndex, c.activeStates)) throw std::runtime_error("state variant is still referenced by collision");
            for (const auto& socket : m_asset.sockets) if (referencesState(socket.parentNodeIndex, socket.activeStates)) throw std::runtime_error("state variant is still referenced by socket");
            for (const auto& hit : m_asset.hitRegions) if (referencesState(hit.parentNodeIndex, hit.activeStates)) throw std::runtime_error("state variant is still referenced by hit region");
            for (const auto& opening : m_asset.openings) if (referencesState(opening.parentNodeIndex, opening.activeStates)) throw std::runtime_error("state variant is still referenced by opening");
            for (const auto& repair : m_asset.repairTargets)
            {
                if (referencesState(repair.parentNodeIndex, repair.activeStates))
                    throw std::runtime_error("state variant is still referenced by repair target");
                if (repair.parentNodeIndex == variant.nodeIndex && repair.repairedStateId == variant.id)
                    throw std::runtime_error("state variant is still a repair target result state");
            }
            if (variant.nodeIndex >= 0 && static_cast<std::size_t>(variant.nodeIndex) < m_asset.nodes.size() && m_asset.nodes[static_cast<std::size_t>(variant.nodeIndex)].defaultStateId == variant.id)
                throw std::runtime_error("state variant is the semantic node default state");
            m_asset.stateVariants.erase(m_asset.stateVariants.begin() + static_cast<std::ptrdiff_t>(index));
            markManifestDirty(); sendAssetMetadata(); sendStatus("Deleted semantic state variant: " + variant.id); return;
        }
        if (command == "set_render_node_transform")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            auto& node = lod.nodes[renderNodeIndex];
            if (message.contains("position")) node.localPosition = jsonVec3(message["position"], node.localPosition);
            if (message.contains("rotationDeg")) node.localRotationDeg = jsonVec3(message["rotationDeg"], node.localRotationDeg);
            if (message.contains("pivot")) node.pivot = jsonVec3(message["pivot"], node.pivot);
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata(); sendStatus("Updated LOD" + std::to_string(lodIndex) + " placement: " + node.id); return;
        }
        if (command == "set_render_node_geometry")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            const auto geometryIndex = message.value("geometryIndex", std::int32_t(NoIndex));
            if (!ensureLodLoaded(lodIndex)) return;
            if (lodIndex >= m_asset.renderLods.size()) throw std::runtime_error("invalid render LOD index");
            auto& lod = m_asset.renderLods[lodIndex];
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            if (geometryIndex < NoIndex || geometryIndex >= static_cast<std::int32_t>(lod.geometries.size())) throw std::runtime_error("invalid render geometry index");
            lod.nodes[renderNodeIndex].geometryIndex = geometryIndex;
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata(); sendStatus("Updated LOD" + std::to_string(lodIndex) + " geometry assignment: " + lod.nodes[renderNodeIndex].id); return;
        }
        if (command == "set_render_node_states")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            const auto stateIds = jsonStrings(message.value("activeStates", json::array()));
            requireSemanticStates(m_asset, lod.nodes[renderNodeIndex].semanticNodeIndex, stateIds, "render node " + lod.nodes[renderNodeIndex].id);
            lod.nodes[renderNodeIndex].activeStates = stateIds;
            markLodDirty(lodIndex); sendAssetMetadata(); sendStatus("Updated render state selector: " + lod.nodes[renderNodeIndex].id); return;
        }
        if (command == "fit_render_node_as_instance")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            const auto referenceNodeIndex = message.value("referenceRenderNodeIndex", std::size_t(-1));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size() || referenceNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            auto& node = lod.nodes[renderNodeIndex]; const auto& referenceNode = lod.nodes[referenceNodeIndex];
            if (node.geometryIndex < 0 || referenceNode.geometryIndex < 0) throw std::runtime_error("both render nodes must have geometry");
            if (node.geometryIndex == referenceNode.geometryIndex) throw std::runtime_error("render nodes already share one geometry");
            const auto targetGi = static_cast<std::size_t>(node.geometryIndex), referenceGi = static_cast<std::size_t>(referenceNode.geometryIndex);
            GeometryDefinition referenceGeometry, targetGeometry;
            referenceGeometry.id = lod.geometries[referenceGi].id; referenceGeometry.lods.push_back(lod.geometries[referenceGi].mesh);
            targetGeometry.id = lod.geometries[targetGi].id; targetGeometry.lods.push_back(lod.geometries[targetGi].mesh);
            const GeometryInstanceFit fit = fitGeometryAsRigidInstance(referenceGeometry, targetGeometry);
            appendRenderInstanceFitDiagnostic(m_asset, lodIndex, referenceNodeIndex, renderNodeIndex, fit);
            if (!fit.valid) throw std::runtime_error(
                "cannot consolidate render instance: " + fit.message +
                "; see build/logs/model_asset_instance_fit.log");
            applyRenderInstanceFit(lod, renderNodeIndex, referenceNodeIndex, fit);
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus("Consolidated LOD" + std::to_string(lodIndex) + " element " + node.id + " as an instance of " + referenceNode.id + "; RMS=" + std::to_string(fit.rmsErrorMeters) + " m"); return;
        }
        if (command == "consolidate_render_duplicates")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto referenceNodeIndex = message.value("referenceRenderNodeIndex", std::size_t(-1));
            auto targets = jsonIndices(message.value("targetRenderNodeIndices", json::array()));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (referenceNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid reference render node index");
            const auto& referenceNode = lod.nodes[referenceNodeIndex];
            if (referenceNode.geometryIndex < 0) throw std::runtime_error("reference render node has no geometry");
            const auto referenceGi = static_cast<std::size_t>(referenceNode.geometryIndex);

            struct PendingFit { std::size_t nodeIndex; GeometryInstanceFit fit; };
            std::vector<PendingFit> pending;
            GeometryDefinition referenceGeometry;
            referenceGeometry.id = lod.geometries[referenceGi].id;
            referenceGeometry.lods.push_back(lod.geometries[referenceGi].mesh);

            for (const auto targetNodeIndex : targets)
            {
                if (targetNodeIndex == referenceNodeIndex || targetNodeIndex >= lod.nodes.size()) continue;
                const auto& targetNode = lod.nodes[targetNodeIndex];
                if (targetNode.geometryIndex < 0) continue;
                const auto targetGi = static_cast<std::size_t>(targetNode.geometryIndex);
                if (targetGi == referenceGi) continue; // already an instance of the selected geometry

                GeometryDefinition targetGeometry;
                targetGeometry.id = lod.geometries[targetGi].id;
                targetGeometry.lods.push_back(lod.geometries[targetGi].mesh);
                const GeometryInstanceFit fit = fitGeometryAsRigidInstance(referenceGeometry, targetGeometry);
                appendRenderInstanceFitDiagnostic(m_asset, lodIndex, referenceNodeIndex, targetNodeIndex, fit);
                if (!fit.valid)
                {
                    throw std::runtime_error(
                        "cannot consolidate selected element '" + targetNode.id + "': " + fit.message +
                        "; comparison is stale or geometry changed");
                }
                pending.push_back({targetNodeIndex, fit});
            }

            if (pending.empty())
            {
                sendStatus("No selected elements need consolidation; they already use the reference geometry");
                return;
            }

            for (const auto& item : pending)
                applyRenderInstanceFit(lod, item.nodeIndex, referenceNodeIndex, item.fit);

            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus(
                "Consolidated " + std::to_string(pending.size()) + " selected LOD" + std::to_string(lodIndex) +
                " elements as instances of " + referenceNode.id);
            return;
        }
        if (command == "break_render_node_instance")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            auto& node = lod.nodes[renderNodeIndex];
            if (node.geometryIndex < 0) throw std::runtime_error("render node has no geometry");
            const auto sourceGeometryIndex = static_cast<std::size_t>(node.geometryIndex);
            auto geometry = lod.geometries[sourceGeometryIndex];
            geometry.id = uniqueRenderGeometryId(lod, message.value("geometryId", geometry.id + "_unique"));
            const auto newGeometryIndex = lod.geometries.size();
            node.geometryIndex = static_cast<std::int32_t>(newGeometryIndex);
            lod.geometries.push_back(std::move(geometry));
            markLodDirty(lodIndex);
            invalidateWizardFrom("geometry");
            sendAssetMetadata({
                {"geometryClones", json::array({{
                    {"lodIndex", lodIndex},
                    {"sourceGeometryIndex", sourceGeometryIndex},
                    {"newGeometryIndex", newGeometryIndex}
                }})}
            });
            sendStatus("Broke LOD" + std::to_string(lodIndex) + " render instance into unique geometry");
            return;
        }
        if (command == "set_render_node_semantic")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            const auto semanticNodeIndex = message.value("semanticNodeIndex", std::int32_t(NoIndex));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            if (semanticNodeIndex < NoIndex || semanticNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                throw std::runtime_error("invalid semantic node index");
            requireSemanticStates(m_asset, semanticNodeIndex, lod.nodes[renderNodeIndex].activeStates, "render node " + lod.nodes[renderNodeIndex].id);
            lod.nodes[renderNodeIndex].semanticNodeIndex = semanticNodeIndex;
            markLodDirty(lodIndex); sendAssetMetadata();
            sendStatus("Updated LOD" + std::to_string(lodIndex) + " semantic binding: " + lod.nodes[renderNodeIndex].id); return;
        }
        if (command == "duplicate_render_node_instance")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            RenderNode clone = lod.nodes[renderNodeIndex];
            clone.id = uniqueRenderNodeId(lod, message.value("id", clone.id + "_instance"));
            lod.nodes.push_back(std::move(clone));
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus("Created LOD" + std::to_string(lodIndex) + " render instance: " + lod.nodes.back().id); return;
        }
        if (command == "create_radial_render_instances")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            const int count = std::clamp(message.value("count", 3), 2, 64);
            const float totalAngle = message.value("totalAngleDeg", 360.0f);
            const std::string axisName = message.value("axis", std::string("y"));
            const glm::vec3 pivot = jsonVec3(message.value("pivot", json::array()), glm::vec3(0.0f));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            const RenderNode base = lod.nodes[renderNodeIndex];
            const glm::vec3 axis = axisName == "x" ? glm::vec3(1,0,0) : axisName == "z" ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
            for (int i = 1; i < count; ++i)
            {
                const float angleDeg = totalAngle * static_cast<float>(i) / static_cast<float>(count);
                const glm::quat q = glm::angleAxis(glm::radians(angleDeg), axis);
                const RigidTransform orbit {glm::mat3_cast(q), pivot - glm::mat3_cast(q) * pivot};
                RenderNode clone = base;
                clone.id = uniqueRenderNodeId(lod, base.id + "_radial_" + std::to_string(i + 1));
                const RigidTransform transformed = composeRigid(orbit, renderNodeRigidTransform(base));
                setRenderNodeRigidTransform(clone, transformed, base.pivot);
                lod.nodes.push_back(std::move(clone));
            }
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus("Created " + std::to_string(count - 1) + " LOD" + std::to_string(lodIndex) + " radial render instances"); return;
        }
        if (command == "delete_render_node")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size()) throw std::runtime_error("invalid render node index");
            for (const auto& child : lod.nodes)
                if (child.parentIndex == static_cast<std::int32_t>(renderNodeIndex))
                    throw std::runtime_error("cannot delete render node with children");
            const std::string id = lod.nodes[renderNodeIndex].id;
            lod.nodes.erase(lod.nodes.begin() + static_cast<std::ptrdiff_t>(renderNodeIndex));
            for (auto& node : lod.nodes)
                if (node.parentIndex > static_cast<std::int32_t>(renderNodeIndex)) --node.parentIndex;
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata(); sendStatus("Deleted LOD" + std::to_string(lodIndex) + " render node: " + id); return;
        }

        if (command == "delete_node")
        {
            if (!ensureAllLodsLoaded()) return;
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            const std::string deletedId = m_asset.nodes[index].id;
            for (const auto& n : m_asset.nodes) if (n.parentIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("cannot delete node with children");
            for (const auto& c : m_asset.collisionVolumes) if (c.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent collision volumes first");
            for (const auto& s : m_asset.sockets) if (s.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent sockets first");
            for (const auto& v : m_asset.stateVariants) if (v.nodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete semantic state variants first");
            for (const auto& h : m_asset.hitRegions) if (h.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent hit regions first");
            for (const auto& o : m_asset.openings) if (o.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent openings first");
            for (const auto& r : m_asset.repairTargets) if (r.parentNodeIndex == static_cast<std::int32_t>(index)) throw std::runtime_error("delete/reparent repair targets first");
            m_asset.nodes.erase(m_asset.nodes.begin() + static_cast<std::ptrdiff_t>(index));
            for (auto& n : m_asset.nodes) if (n.parentIndex > static_cast<std::int32_t>(index)) --n.parentIndex;
            for (auto& c : m_asset.collisionVolumes) if (c.parentNodeIndex > static_cast<std::int32_t>(index)) --c.parentNodeIndex;
            for (auto& s : m_asset.sockets) if (s.parentNodeIndex > static_cast<std::int32_t>(index)) --s.parentNodeIndex;
            for (auto& v : m_asset.stateVariants) if (v.nodeIndex > static_cast<std::int32_t>(index)) --v.nodeIndex;
            for (auto& h : m_asset.hitRegions) if (h.parentNodeIndex > static_cast<std::int32_t>(index)) --h.parentNodeIndex;
            for (auto& o : m_asset.openings) if (o.parentNodeIndex > static_cast<std::int32_t>(index)) --o.parentNodeIndex;
            for (auto& r : m_asset.repairTargets) if (r.parentNodeIndex > static_cast<std::int32_t>(index)) --r.parentNodeIndex;
            for (auto& lod : m_asset.renderLods) for (auto& rn : lod.nodes) { if (rn.semanticNodeIndex == static_cast<std::int32_t>(index)) rn.semanticNodeIndex = NoIndex; else if (rn.semanticNodeIndex > static_cast<std::int32_t>(index)) --rn.semanticNodeIndex; }
            markManifestDirty(); markAllLoadedLodsDirty(); sendAssetMetadata(); sendStatus("Deleted semantic node: " + deletedId + "; all render LOD bindings were remapped"); return;
        }
        if (command == "delete_unused_geometries")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(0));
            if (!ensureLodLoaded(lodIndex)) return;
            if (lodIndex >= m_asset.renderLods.size()) throw std::runtime_error("invalid render LOD index");
            auto& lod = m_asset.renderLods[lodIndex];
            std::vector<std::string> deleted;
            std::uint64_t estimatedBytes = 0;
            for (std::size_t gi = lod.geometries.size(); gi-- > 0; )
            {
                const bool used = std::any_of(lod.nodes.begin(), lod.nodes.end(), [&](const RenderNode& n) { return n.geometryIndex == static_cast<std::int32_t>(gi); });
                const bool protectedVariant = isRenderVariantGeometryId(lod.geometries[gi].id);
                if (!used && !protectedVariant)
                {
                    deleted.push_back("G" + std::to_string(gi) + " " + lod.geometries[gi].id);
                    estimatedBytes += estimatedRenderGeometryBinaryBytes(lod.geometries[gi]);
                    lod.geometries.erase(lod.geometries.begin() + static_cast<std::ptrdiff_t>(gi));
                    remapRenderGeometryAfterErase(lod, gi);
                }
            }
            if (deleted.empty())
            {
                sendStatus("NO CHANGES: no unused geometry definitions in LOD" + std::to_string(lodIndex));
                return;
            }
            std::reverse(deleted.begin(), deleted.end());
            std::string names;
            for (std::size_t i = 0; i < deleted.size(); ++i) { if (i) names += ", "; names += deleted[i]; }
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus("Deleted " + std::to_string(deleted.size()) + " unused LOD" + std::to_string(lodIndex) +
                       " render geometries: " + names + "; estimated payload removed " + std::to_string(estimatedBytes) +
                       " B. Other LODs and source OBJ files were not changed.");
            return;
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
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated joint: " + m_asset.nodes[index].id); return;
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
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated rigid-body properties: " + m_asset.nodes[index].id); return;
        }
        if (command == "estimate_physics")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            const float density = std::max(0.001f, message.value("densityKgM3", 780.0f));
            if (!estimatePhysicsFromCollision(m_asset, index, density)) throw std::runtime_error("node has no enabled local collision volumes");
            markManifestDirty(); sendAssetMetadata(); sendStatus("Estimated rigid-body properties from collision: " + m_asset.nodes[index].id); return;
        }
        if (command == "set_surface_mode")
        {
            const auto li = message.value("lodIndex", std::size_t(0));
            const auto gi = message.value("geometryIndex", std::size_t(-1));
            if (!ensureLodLoaded(li)) return;
            if (li >= m_asset.renderLods.size() || gi >= m_asset.renderLods[li].geometries.size()) throw std::runtime_error("invalid render geometry index");
            auto& geometry = m_asset.renderLods[li].geometries[gi];
            geometry.surfaceMode = surfaceModeFromName(message.value("surfaceMode", "closed")); markLodDirty(li); sendAssetMetadata();
            sendStatus("Set LOD" + std::to_string(li) + " G" + std::to_string(gi) + " surface mode to " + std::string(surfaceModeName(geometry.surfaceMode))); return;
        }
        if (command == "set_edge_render_mask")
        {
            const auto gi = message.value("geometryIndex", std::size_t(-1)), li = message.value("lodIndex", std::size_t(0)), ei = message.value("edgeIndex", std::size_t(-1));
            if (!ensureLodLoaded(li)) return;
            if (li >= m_asset.renderLods.size() || gi >= m_asset.renderLods[li].geometries.size() || ei >= m_asset.renderLods[li].geometries[gi].mesh.edges.size()) throw std::runtime_error("invalid edge index");
            const auto renderMask = static_cast<std::uint8_t>(message.value("renderMask", 0) & 0xff);
            m_asset.renderLods[li].geometries[gi].mesh.edges[ei].renderMask = renderMask;
            markLodDirty(li);
            sendAssetMetadata({
                {"edgePatches", json::array({{
                    {"lodIndex", li}, {"geometryIndex", gi}, {"edgeIndex", ei}, {"renderMask", renderMask}
                }})}
            });
            sendStatus("Updated LOD" + std::to_string(li) + " G" + std::to_string(gi) + " edge " + std::to_string(ei)); return;
        }
        if (command == "add_collision")
        {
            CollisionVolume c; c.id = message.value("id", std::string("hit.new")); c.moduleId = message.value("moduleId", std::string()); c.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (c.parentNodeIndex < NoIndex || c.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid collision parent node");
            c.shape = collisionShapeFromName(message.value("shape", std::string("box")));
            c.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f)); c.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            c.halfSize = glm::max(jsonVec3(message.value("halfSize", json::array()), glm::vec3(1.0f)), glm::vec3(0.001f)); c.radius = std::max(0.001f, message.value("radius", 1.0f)); c.halfHeight = std::max(0.0f, message.value("halfHeight", 1.0f));
            c.activeStates = jsonStrings(message.value("activeStates", json::array()));
            const std::string createdId = c.id;
            m_asset.collisionVolumes.push_back(std::move(c)); markManifestDirty(); sendAssetMetadata(); sendStatus("Added collision volume: " + createdId); return;
        }
        if (command == "delete_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            const std::string deletedId = m_asset.collisionVolumes[index].id;
            m_asset.collisionVolumes.erase(m_asset.collisionVolumes.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAssetMetadata(); sendStatus("Deleted collision volume: " + deletedId); return;
        }
        if (command == "duplicate_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            auto c = m_asset.collisionVolumes[index]; c.id += "_copy"; const std::string createdId = c.id; m_asset.collisionVolumes.push_back(std::move(c)); markManifestDirty(); sendAssetMetadata(); sendStatus("Duplicated collision volume: " + createdId); return;
        }
        if (command == "set_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            auto& c = m_asset.collisionVolumes[index]; c.shape = collisionShapeFromName(message.value("shape", std::string(collisionShapeName(c.shape))));
            if (message.contains("position")) c.localPosition = jsonVec3(message["position"], c.localPosition); if (message.contains("rotationDeg")) c.localRotationDeg = jsonVec3(message["rotationDeg"], c.localRotationDeg); if (message.contains("halfSize")) c.halfSize = glm::max(jsonVec3(message["halfSize"], c.halfSize), glm::vec3(0.001f));
            c.radius = std::max(0.001f, message.value("radius", c.radius)); c.halfHeight = std::max(0.0f, message.value("halfHeight", c.halfHeight)); if (message.contains("enabled")) c.enabled = message["enabled"].get<bool>();
            if (message.contains("activeStates"))
            {
                const auto states = jsonStrings(message["activeStates"]);
                requireSemanticStates(m_asset, c.parentNodeIndex, states, "collision " + c.id);
                c.activeStates = states;
            }
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated collision volume: " + c.id); return;
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
            (void)ringAxis; markManifestDirty(); sendAssetMetadata(); sendStatus("Generated " + std::to_string(count) + " radial collision capsules for " + m_asset.nodes[nodeIndex].id); return;
        }
        if (command == "add_socket")
        {
            Socket s; s.id = message.value("id", std::string("socket.new")); s.kind = message.value("kind", std::string("generic")); s.moduleId = message.value("moduleId", std::string()); s.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (s.parentNodeIndex < NoIndex || s.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid socket parent node");
            s.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f)); s.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            s.activeStates = jsonStrings(message.value("activeStates", json::array()));
            if (s.kind == "light" || s.kind == "light_point") s.light.type = LightType::Point; else if (s.kind == "light_spot") s.light.type = LightType::Spot;
            const std::string createdId = s.id;
            m_asset.sockets.push_back(std::move(s)); markManifestDirty(); sendAssetMetadata(); sendStatus("Added socket: " + createdId); return;
        }
        if (command == "delete_socket")
        {
            const auto index = message.value("socketIndex", std::size_t(-1)); if (index >= m_asset.sockets.size()) throw std::runtime_error("invalid socket index");
            const std::string deletedId = m_asset.sockets[index].id;
            m_asset.sockets.erase(m_asset.sockets.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAssetMetadata(); sendStatus("Deleted socket: " + deletedId); return;
        }
        if (command == "set_socket")
        {
            const auto index = message.value("socketIndex", std::size_t(-1)); if (index >= m_asset.sockets.size()) throw std::runtime_error("invalid socket index");
            auto& s = m_asset.sockets[index]; if (message.contains("position")) s.localPosition = jsonVec3(message["position"], s.localPosition); if (message.contains("rotationDeg")) s.localRotationDeg = jsonVec3(message["rotationDeg"], s.localRotationDeg); if (message.contains("enabled")) s.enabled = message["enabled"].get<bool>(); if (message.contains("activeStates")) { const auto states = jsonStrings(message["activeStates"]); requireSemanticStates(m_asset, s.parentNodeIndex, states, "socket " + s.id); s.activeStates = states; }
            if (message.contains("lightType")) s.light.type = lightTypeFromName(message["lightType"].get<std::string>()); if (message.contains("lightColor")) s.light.color = jsonVec3(message["lightColor"], s.light.color); s.light.intensity = message.value("lightIntensity", s.light.intensity); s.light.rangeMeters = message.value("lightRangeMeters", s.light.rangeMeters); s.light.outerConeDeg = message.value("lightOuterConeDeg", s.light.outerConeDeg);
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated socket: " + s.id); return;
        }
        if (command == "add_hit_region")
        {
            HitRegion hit;
            hit.id = message.value("id", std::string("hit_region.new"));
            hit.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (hit.parentNodeIndex < 0 || hit.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid hit-region parent node");
            hit.activeStates = jsonStrings(message.value("activeStates", json::array()));
            requireSemanticStates(m_asset, hit.parentNodeIndex, hit.activeStates, "hit region " + hit.id);
            hit.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f));
            hit.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            hit.halfSize = glm::max(jsonVec3(message.value("halfSize", json::array()), glm::vec3(0.5f)), glm::vec3(0.001f));
            m_asset.hitRegions.push_back(std::move(hit)); markManifestDirty(); sendAssetMetadata(); sendStatus("Added state-scoped hit region"); return;
        }
        if (command == "set_hit_region")
        {
            const auto index = message.value("hitRegionIndex", std::size_t(-1)); if (index >= m_asset.hitRegions.size()) throw std::runtime_error("invalid hit-region index");
            auto& hit = m_asset.hitRegions[index];
            if (message.contains("activeStates")) { const auto states = jsonStrings(message["activeStates"]); requireSemanticStates(m_asset, hit.parentNodeIndex, states, "hit region " + hit.id); hit.activeStates = states; }
            if (message.contains("position")) hit.localPosition = jsonVec3(message["position"], hit.localPosition);
            if (message.contains("rotationDeg")) hit.localRotationDeg = jsonVec3(message["rotationDeg"], hit.localRotationDeg);
            if (message.contains("halfSize")) hit.halfSize = glm::max(jsonVec3(message["halfSize"], hit.halfSize), glm::vec3(0.001f));
            if (message.contains("enabled")) hit.enabled = message.value("enabled", hit.enabled);
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated hit region: " + hit.id); return;
        }
        if (command == "delete_hit_region")
        {
            const auto index = message.value("hitRegionIndex", std::size_t(-1)); if (index >= m_asset.hitRegions.size()) throw std::runtime_error("invalid hit-region index");
            const auto id = m_asset.hitRegions[index].id; m_asset.hitRegions.erase(m_asset.hitRegions.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAssetMetadata(); sendStatus("Deleted hit region: " + id); return;
        }
        if (command == "add_opening")
        {
            Opening opening;
            opening.id = message.value("id", std::string("opening.new"));
            opening.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (opening.parentNodeIndex < 0 || opening.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid opening parent node");
            opening.activeStates = jsonStrings(message.value("activeStates", json::array()));
            requireSemanticStates(m_asset, opening.parentNodeIndex, opening.activeStates, "opening " + opening.id);
            opening.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f));
            opening.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            opening.halfSize = glm::max(jsonVec3(message.value("halfSize", json::array()), glm::vec3(0.5f)), glm::vec3(0.001f));
            opening.traversable = message.value("traversable", true); opening.lineOfFire = message.value("lineOfFire", true);
            m_asset.openings.push_back(std::move(opening)); markManifestDirty(); sendAssetMetadata(); sendStatus("Added state-scoped opening"); return;
        }
        if (command == "set_opening")
        {
            const auto index = message.value("openingIndex", std::size_t(-1)); if (index >= m_asset.openings.size()) throw std::runtime_error("invalid opening index");
            auto& opening = m_asset.openings[index];
            if (message.contains("activeStates")) { const auto states = jsonStrings(message["activeStates"]); requireSemanticStates(m_asset, opening.parentNodeIndex, states, "opening " + opening.id); opening.activeStates = states; }
            if (message.contains("position")) opening.localPosition = jsonVec3(message["position"], opening.localPosition);
            if (message.contains("rotationDeg")) opening.localRotationDeg = jsonVec3(message["rotationDeg"], opening.localRotationDeg);
            if (message.contains("halfSize")) opening.halfSize = glm::max(jsonVec3(message["halfSize"], opening.halfSize), glm::vec3(0.001f));
            if (message.contains("traversable")) opening.traversable = message.value("traversable", opening.traversable);
            if (message.contains("lineOfFire")) opening.lineOfFire = message.value("lineOfFire", opening.lineOfFire);
            if (message.contains("enabled")) opening.enabled = message.value("enabled", opening.enabled);
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated opening: " + opening.id); return;
        }
        if (command == "delete_opening")
        {
            const auto index = message.value("openingIndex", std::size_t(-1)); if (index >= m_asset.openings.size()) throw std::runtime_error("invalid opening index");
            const auto id = m_asset.openings[index].id; m_asset.openings.erase(m_asset.openings.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAssetMetadata(); sendStatus("Deleted opening: " + id); return;
        }
        if (command == "add_repair_target")
        {
            RepairTarget target; target.id = message.value("id", std::string("repair.new")); target.kind = message.value("kind", std::string("hull_patch")); target.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (target.parentNodeIndex < 0 || target.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid repair target parent node");
            target.activeStates = jsonStrings(message.value("activeStates", json::array()));
            requireSemanticStates(m_asset, target.parentNodeIndex, target.activeStates, "repair target " + target.id);
            target.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f));
            target.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            target.repairedStateId = message.value("repairedStateId", std::string("intact"));
            if (!target.repairedStateId.empty() && !semanticStateDeclared(m_asset, target.parentNodeIndex, target.repairedStateId))
                throw std::runtime_error("repair target result state is not declared for this semantic node");
            m_asset.repairTargets.push_back(std::move(target)); markManifestDirty(); sendAssetMetadata(); sendStatus("Added repair target"); return;
        }
        if (command == "set_repair_target")
        {
            const auto index = message.value("repairTargetIndex", std::size_t(-1)); if (index >= m_asset.repairTargets.size()) throw std::runtime_error("invalid repair target index");
            auto& target = m_asset.repairTargets[index];
            if (message.contains("kind")) target.kind = message.value("kind", target.kind);
            if (message.contains("activeStates")) { const auto states = jsonStrings(message["activeStates"]); requireSemanticStates(m_asset, target.parentNodeIndex, states, "repair target " + target.id); target.activeStates = states; }
            if (message.contains("position")) target.localPosition = jsonVec3(message["position"], target.localPosition);
            if (message.contains("rotationDeg")) target.localRotationDeg = jsonVec3(message["rotationDeg"], target.localRotationDeg);
            if (message.contains("repairedStateId"))
            {
                const auto repairedStateId = message.value("repairedStateId", target.repairedStateId);
                if (!repairedStateId.empty() && !semanticStateDeclared(m_asset, target.parentNodeIndex, repairedStateId))
                    throw std::runtime_error("repair target result state is not declared for this semantic node");
                target.repairedStateId = repairedStateId;
            }
            if (message.contains("enabled")) target.enabled = message.value("enabled", target.enabled);
            markManifestDirty(); sendAssetMetadata(); sendStatus("Updated repair target: " + target.id); return;
        }
        if (command == "delete_repair_target")
        {
            const auto index = message.value("repairTargetIndex", std::size_t(-1)); if (index >= m_asset.repairTargets.size()) throw std::runtime_error("invalid repair target index");
            const auto id = m_asset.repairTargets[index].id; m_asset.repairTargets.erase(m_asset.repairTargets.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); sendAssetMetadata(); sendStatus("Deleted repair target: " + id); return;
        }

        sendStatus("Unknown editor command: " + command, true);
    }
    catch (const std::exception& ex)
    {
        sendStatus(std::string("Editor command failed: ") + ex.what(), true);
    }
}

} // namespace elite::model_asset::editor
