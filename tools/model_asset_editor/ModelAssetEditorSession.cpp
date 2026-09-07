#include "tools/model_asset_editor/ModelAssetEditorSession.h"
#include "tools/model_asset_editor/ModelAssetEditorWire.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <sstream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetIdentity.h"
#include "src/model_asset/ModelAssetMigration.h"
#include "src/model_asset/ModelAssetLodSelection.h"
#include "src/model_asset/ModelAssetSemantics.h"
#include "src/model_asset/ModelAssetVariantNaming.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "src/render/core/earcut.hpp"
#include "tools/model_asset_editor/RuntimeAssemblyImporter.h"
#include "tools/model_asset_editor/SourceFolderImporter.h"
#include "tools/model_asset_editor/CanonicalMeshBuilder.h"
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

glm::vec4 jsonVec4(const json& value, const glm::vec4& fallback)
{
    if (!value.is_array() || value.size() != 4) return fallback;
    return glm::vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
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

std::string lowerText(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}


std::string utcTimestampNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
#if defined(_WIN32)
    gmtime_s(&utc, &raw);
#else
    gmtime_r(&raw, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

bool pathIsWithin(const std::filesystem::path& file, const std::filesystem::path& root)
{
    if (file.empty() || root.empty()) return false;
    std::error_code ec;
    const auto relative = std::filesystem::relative(file, root, ec);
    if (ec || relative.empty()) return false;
    const auto generic = relative.lexically_normal().generic_string();
    return generic != ".." && generic.rfind("../", 0) != 0;
}

std::uint64_t appendFileFingerprint(
    const std::filesystem::path& path,
    std::uint64_t hash)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return 0;
    std::array<char, 64 * 1024> buffer {};
    while (in)
    {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = in.gcount();
        for (std::streamsize i = 0; i < count; ++i)
        {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

std::uint64_t sourceFileFingerprint(const std::filesystem::path& path)
{
    if (path.empty()) return 0;
    // FNV-1a 64-bit is sufficient here: this is editor provenance/change
    // detection, not a cryptographic identity or runtime content address.
    // Keep the byte-for-byte hash contract stable because accepted fingerprints
    // are persisted in editor_state.json. For OBJ we collect mtllib declarations
    // during the same binary pass instead of reading every large OBJ twice.
    std::ifstream in(path, std::ios::binary);
    if (!in) return 0;
    std::uint64_t hash = 1469598103934665603ull;
    std::set<std::filesystem::path> materialFiles;
    std::string line;
    const bool objSource = lowerText(path.extension().string()) == ".obj";
    const auto consumeLine = [&](std::string raw)
    {
        if (!objSource) return;
        const auto first = raw.find_first_not_of(" \t");
        if (first == std::string::npos || raw.compare(first, 6, "mtllib") != 0) return;
        auto name = raw.substr(first + 6);
        const auto nameFirst = name.find_first_not_of(" \t");
        if (nameFirst == std::string::npos) return;
        name.erase(0, nameFirst);
        const auto nameLast = name.find_last_not_of(" \t\r\n");
        if (nameLast != std::string::npos) name.erase(nameLast + 1);
        if (name.empty()) return;
        std::error_code ec;
        const auto referenced = (path.parent_path() / std::filesystem::path(name)).lexically_normal();
        if (std::filesystem::is_regular_file(referenced, ec) && !ec)
            materialFiles.insert(referenced);
    };

    std::array<char, 64 * 1024> buffer {};
    while (in)
    {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = in.gcount();
        for (std::streamsize i = 0; i < count; ++i)
        {
            const unsigned char byte = static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash ^= byte;
            hash *= 1099511628211ull;
            if (objSource)
            {
                const char c = static_cast<char>(byte);
                if (c == '\n')
                {
                    consumeLine(line);
                    line.clear();
                }
                else line.push_back(c);
            }
        }
    }
    if (objSource && !line.empty()) consumeLine(line);

    // OBJ is only half of an authored source revision. A Blender material edit
    // often changes only the sibling MTL; include referenced/same-stem MTL bytes
    // so SOURCE CHANGE SCAN also notices a newly authored material like window.
    if (objSource)
    {
        auto companion = path;
        companion.replace_extension(".mtl");
        std::error_code ec;
        if (std::filesystem::is_regular_file(companion, ec) && !ec)
            materialFiles.insert(companion.lexically_normal());
        for (const auto& material : materialFiles)
        {
            hash ^= 0xffu; // file-boundary marker
            hash *= 1099511628211ull;
            const auto combined = appendFileFingerprint(material, hash);
            if (combined != 0) hash = combined;
        }
    }
    return hash;
}


std::string sourceHashHex(std::uint64_t hash)
{
    if (hash == 0) return {};
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
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


constexpr double LodVisibilityCutoffPx = 2.0;
constexpr std::size_t LodMaximumRecommendedLevels = 5; // including LOD0
constexpr double LodFeatureBandFactor = 4.0;
constexpr const char* GeneratedLodComponentCullAlgorithmId = "generated_lod_component_cull_v1";

struct LodDisjointSet
{
    explicit LodDisjointSet(std::size_t count) : parent(count), rank(count, 0)
    {
        for (std::size_t i = 0; i < count; ++i) parent[i] = i;
    }

    std::size_t find(std::size_t value)
    {
        while (parent[value] != value)
        {
            parent[value] = parent[parent[value]];
            value = parent[value];
        }
        return value;
    }

    void unite(std::size_t a, std::size_t b)
    {
        a = find(a);
        b = find(b);
        if (a == b) return;
        if (rank[a] < rank[b]) std::swap(a, b);
        parent[b] = a;
        if (rank[a] == rank[b]) ++rank[a];
    }

    std::vector<std::size_t> parent;
    std::vector<std::uint8_t> rank;
};

struct LodComponentInfo
{
    std::vector<std::size_t> triangleIndices;
    glm::dvec3 principalExtents {0.0}; // sorted ascending
    double featureMeters = 0.0;        // middle principal extent
    double longMeters = 0.0;
    bool protectedStructure = false;
};

struct LodGeometryAnalysis
{
    std::size_t geometryIndex = 0;
    std::size_t usageCount = 0;
    std::size_t totalTriangles = 0;
    std::vector<LodComponentInfo> components;
};

void jacobiEigenvectors3x3(const double input[3][3], double vectors[3][3])
{
    double a[3][3] = {
        {input[0][0], input[0][1], input[0][2]},
        {input[1][0], input[1][1], input[1][2]},
        {input[2][0], input[2][1], input[2][2]}
    };
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            vectors[r][c] = r == c ? 1.0 : 0.0;

    for (int iteration = 0; iteration < 18; ++iteration)
    {
        int p = 0, q = 1;
        double largest = std::abs(a[0][1]);
        if (std::abs(a[0][2]) > largest) { p = 0; q = 2; largest = std::abs(a[0][2]); }
        if (std::abs(a[1][2]) > largest) { p = 1; q = 2; largest = std::abs(a[1][2]); }
        if (largest < 1.0e-12) break;

        const double angle = 0.5 * std::atan2(2.0 * a[p][q], a[q][q] - a[p][p]);
        const double c = std::cos(angle);
        const double s = std::sin(angle);

        for (int k = 0; k < 3; ++k)
        {
            const double apk = a[p][k];
            const double aqk = a[q][k];
            a[p][k] = c * apk - s * aqk;
            a[q][k] = s * apk + c * aqk;
        }
        for (int k = 0; k < 3; ++k)
        {
            const double akp = a[k][p];
            const double akq = a[k][q];
            a[k][p] = c * akp - s * akq;
            a[k][q] = s * akp + c * akq;
        }
        a[p][q] = a[q][p] = 0.0;

        for (int k = 0; k < 3; ++k)
        {
            const double vkp = vectors[k][p];
            const double vkq = vectors[k][q];
            vectors[k][p] = c * vkp - s * vkq;
            vectors[k][q] = s * vkp + c * vkq;
        }
    }
}

glm::dvec3 componentPrincipalExtents(
    const MeshLod& mesh,
    const std::vector<std::size_t>& triangleIndices)
{
    std::vector<std::uint32_t> vertexIndices;
    vertexIndices.reserve(triangleIndices.size() * 3u);
    for (const auto triangleIndex : triangleIndices)
    {
        if (triangleIndex >= mesh.triangles.size()) continue;
        const auto& triangle = mesh.triangles[triangleIndex];
        if (triangle.a < mesh.vertices.size()) vertexIndices.push_back(triangle.a);
        if (triangle.b < mesh.vertices.size()) vertexIndices.push_back(triangle.b);
        if (triangle.c < mesh.vertices.size()) vertexIndices.push_back(triangle.c);
    }
    std::sort(vertexIndices.begin(), vertexIndices.end());
    vertexIndices.erase(std::unique(vertexIndices.begin(), vertexIndices.end()), vertexIndices.end());
    if (vertexIndices.empty()) return glm::dvec3(0.0);

    glm::dvec3 centroid(0.0);
    for (const auto index : vertexIndices) centroid += glm::dvec3(mesh.vertices[index].position);
    centroid /= static_cast<double>(vertexIndices.size());

    double covariance[3][3] = {};
    for (const auto index : vertexIndices)
    {
        const glm::dvec3 d = glm::dvec3(mesh.vertices[index].position) - centroid;
        covariance[0][0] += d.x * d.x; covariance[0][1] += d.x * d.y; covariance[0][2] += d.x * d.z;
        covariance[1][0] += d.y * d.x; covariance[1][1] += d.y * d.y; covariance[1][2] += d.y * d.z;
        covariance[2][0] += d.z * d.x; covariance[2][1] += d.z * d.y; covariance[2][2] += d.z * d.z;
    }
    const double invCount = 1.0 / static_cast<double>(vertexIndices.size());
    for (auto& row : covariance) for (double& value : row) value *= invCount;

    double axes[3][3];
    jacobiEigenvectors3x3(covariance, axes);
    double minimum[3] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    double maximum[3] = {-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (const auto index : vertexIndices)
    {
        const glm::dvec3 p(mesh.vertices[index].position);
        for (int axis = 0; axis < 3; ++axis)
        {
            const double projected = p.x * axes[0][axis] + p.y * axes[1][axis] + p.z * axes[2][axis];
            minimum[axis] = std::min(minimum[axis], projected);
            maximum[axis] = std::max(maximum[axis], projected);
        }
    }
    std::array<double, 3> extents = {
        std::max(0.0, maximum[0] - minimum[0]),
        std::max(0.0, maximum[1] - minimum[1]),
        std::max(0.0, maximum[2] - minimum[2])
    };
    std::sort(extents.begin(), extents.end());
    return glm::dvec3(extents[0], extents[1], extents[2]);
}

std::vector<LodComponentInfo> analyzeConnectedComponents(const MeshLod& mesh)
{
    std::vector<LodComponentInfo> out;
    if (mesh.vertices.empty() || mesh.triangles.empty()) return out;

    LodDisjointSet sets(mesh.vertices.size());

    // OBJ import duplicates final vertices at UV/normal/material seams. Those
    // duplicates still belong to one physical surface and must not become fake
    // disconnected components. Weld only for analysis, with a very small
    // scale-relative tolerance; authored geometry itself remains untouched.
    const glm::dvec3 meshExtent = glm::dvec3(mesh.maxBounds) - glm::dvec3(mesh.minBounds);
    const double weldEpsilon = std::max(1.0e-6, glm::length(meshExtent) * 1.0e-7);
    std::map<std::array<std::int64_t, 3>, std::size_t> representativeByPosition;
    for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex)
    {
        const glm::dvec3 p(mesh.vertices[vertexIndex].position);
        const std::array<std::int64_t, 3> key = {
            static_cast<std::int64_t>(std::llround(p.x / weldEpsilon)),
            static_cast<std::int64_t>(std::llround(p.y / weldEpsilon)),
            static_cast<std::int64_t>(std::llround(p.z / weldEpsilon))
        };
        const auto [it, inserted] = representativeByPosition.emplace(key, vertexIndex);
        if (!inserted) sets.unite(vertexIndex, it->second);
    }

    for (const auto& triangle : mesh.triangles)
    {
        if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() || triangle.c >= mesh.vertices.size())
            continue;
        sets.unite(triangle.a, triangle.b);
        sets.unite(triangle.a, triangle.c);
    }

    std::map<std::size_t, std::vector<std::size_t>> trianglesByRoot;
    for (std::size_t triangleIndex = 0; triangleIndex < mesh.triangles.size(); ++triangleIndex)
    {
        const auto& triangle = mesh.triangles[triangleIndex];
        if (triangle.a >= mesh.vertices.size()) continue;
        trianglesByRoot[sets.find(triangle.a)].push_back(triangleIndex);
    }
    if (trianglesByRoot.empty()) return out;

    std::size_t largestTriangleCount = 0;
    for (const auto& [root, triangles] : trianglesByRoot)
    {
        (void)root;
        largestTriangleCount = std::max(largestTriangleCount, triangles.size());
    }

    out.reserve(trianglesByRoot.size());
    for (auto& [root, triangles] : trianglesByRoot)
    {
        (void)root;
        LodComponentInfo component;
        component.triangleIndices = std::move(triangles);
        component.principalExtents = componentPrincipalExtents(mesh, component.triangleIndices);
        // The middle principal extent is a useful orientation-independent visual
        // thickness: tube -> diameter, sheet -> smaller in-plane dimension.
        component.featureMeters = component.principalExtents.y;
        component.longMeters = component.principalExtents.z;
        const double share = static_cast<double>(component.triangleIndices.size()) /
            static_cast<double>(std::max<std::size_t>(1, mesh.triangles.size()));
        component.protectedStructure =
            component.triangleIndices.size() == largestTriangleCount || share >= 0.25;
        out.push_back(std::move(component));
    }
    return out;
}


enum class PreflightTopologyClass : std::uint8_t
{
    Auto = 0,
    ClosedVolume,
    ThinOneSided,
    ThinTwoSided,
    BreachedVolume,
    Mixed,
    Invalid
};

const char* preflightTopologyClassName(PreflightTopologyClass value)
{
    switch (value)
    {
        case PreflightTopologyClass::ClosedVolume: return "closed_volume";
        case PreflightTopologyClass::ThinOneSided: return "thin_one_sided";
        case PreflightTopologyClass::ThinTwoSided: return "thin_two_sided";
        case PreflightTopologyClass::BreachedVolume: return "breached_volume";
        case PreflightTopologyClass::Mixed: return "mixed";
        case PreflightTopologyClass::Invalid: return "invalid";
        default: return "auto";
    }
}

PreflightTopologyClass preflightTopologyClassFromName(const std::string& value)
{
    if (value == "closed_volume") return PreflightTopologyClass::ClosedVolume;
    if (value == "thin_one_sided") return PreflightTopologyClass::ThinOneSided;
    if (value == "thin_two_sided") return PreflightTopologyClass::ThinTwoSided;
    if (value == "breached_volume") return PreflightTopologyClass::BreachedVolume;
    if (value == "mixed") return PreflightTopologyClass::Mixed;
    if (value == "invalid") return PreflightTopologyClass::Invalid;
    return PreflightTopologyClass::Auto;
}

using PreflightEdgeKey = std::uint64_t;

PreflightEdgeKey preflightEdgeKey(std::size_t a, std::size_t b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(b));
}

struct PreflightEdgeUse
{
    std::size_t triangleIndex = 0;
    // +1 follows the stored edge direction, -1 opposes it, 0 means the
    // persisted source-edge metadata could not be matched back to the triangle.
    int direction = 0;
};

struct PreflightComponentAudit
{
    std::vector<std::size_t> triangleIndices;
    std::size_t boundaryEdges = 0;
    std::size_t nonManifoldEdges = 0;
    std::size_t windingConflicts = 0;
    std::size_t degenerateTriangles = 0;
    bool closed = false;
    bool insideOut = false;
    glm::dvec3 principalExtents {0.0};
    PreflightTopologyClass suggestedClass = PreflightTopologyClass::Auto;
    double confidence = 0.0;
};

struct PreflightGeometryAudit
{
    std::vector<PreflightComponentAudit> components;
    std::size_t boundaryEdges = 0;
    std::size_t nonManifoldEdges = 0;
    std::size_t windingConflicts = 0;
    std::size_t degenerateTriangles = 0;
    std::size_t closedComponents = 0;
    std::size_t openComponents = 0;
    std::size_t insideOutComponents = 0;
    PreflightTopologyClass suggestedClass = PreflightTopologyClass::Auto;
    double confidence = 0.0;
};

struct PreflightCanonicalTopology
{
    std::vector<std::size_t> canonicalVertex;
    std::map<PreflightEdgeKey, std::vector<PreflightEdgeUse>> edgeUses;
};

PreflightCanonicalTopology buildPreflightCanonicalTopology(const MeshLod& mesh)
{
    PreflightCanonicalTopology out;
    out.canonicalVertex.resize(mesh.vertices.size());
    if (mesh.vertices.empty()) return out;

    // Match game::ship::geometry::ObjLoader: source-position weld at 1e-4.
    constexpr double RuntimeWeldEpsilon = 1.0e-4;
    std::map<std::array<std::int64_t, 3>, std::size_t> canonicalByPosition;
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi)
    {
        const glm::dvec3 p(mesh.vertices[vi].position);
        const std::array<std::int64_t, 3> key = {
            static_cast<std::int64_t>(std::llround(p.x / RuntimeWeldEpsilon)),
            static_cast<std::int64_t>(std::llround(p.y / RuntimeWeldEpsilon)),
            static_cast<std::int64_t>(std::llround(p.z / RuntimeWeldEpsilon))
        };
        const auto [it, inserted] = canonicalByPosition.emplace(key, canonicalByPosition.size());
        (void)inserted;
        out.canonicalVertex[vi] = it->second;
    }

    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
    {
        const auto& triangle = mesh.triangles[ti];
        if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() || triangle.c >= mesh.vertices.size()) continue;
        const std::array<std::size_t, 3> cv = {
            out.canonicalVertex[triangle.a], out.canonicalVertex[triangle.b], out.canonicalVertex[triangle.c]
        };
        for (int edge = 0; edge < 3; ++edge)
        {
            const auto a = cv[edge], b = cv[(edge + 1) % 3];
            if (a == b) continue;
            out.edgeUses[preflightEdgeKey(a, b)].push_back({ti, a < b ? 1 : -1});
        }
    }
    return out;
}

PreflightGeometryAudit auditPreflightGeometry(const MeshLod& mesh)
{
    PreflightGeometryAudit out;
    if (mesh.triangles.empty()) return out;

    // Canonical authoring topology intentionally follows the proven runtime
    // loader policy: positional weld at 1e-4. Render-vertex splits for UVs are
    // preserved in MeshLod; the weld is the geometric identity used for
    // topology/orientation/normal preparation and later LOD analysis.
    const auto topology = buildPreflightCanonicalTopology(mesh);
    LodDisjointSet triangleSets(mesh.triangles.size());
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        // Exactly-two adjacency defines one orientable surface. Canonical
        // multi-use edges can be produced by coincident/touching shells after
        // the positional weld and are reported as warnings, not used to fuse
        // otherwise independent components. Real source non-manifold topology
        // is rejected earlier by CanonicalMeshBuilder.
        if (uses.size() == 2)
            triangleSets.unite(uses[0].triangleIndex, uses[1].triangleIndex);
    }

    std::map<std::size_t, std::vector<std::size_t>> trianglesByRoot;
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
        trianglesByRoot[triangleSets.find(ti)].push_back(ti);

    std::map<std::size_t, std::vector<const std::vector<PreflightEdgeUse>*>> componentEdges;
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        std::set<std::size_t> touchedRoots;
        for (const auto& use : uses) touchedRoots.insert(triangleSets.find(use.triangleIndex));
        for (const auto root : touchedRoots) componentEdges[root].push_back(&uses);
    }

    for (const auto& [root, triangles] : trianglesByRoot)
    {
        PreflightComponentAudit component;
        component.triangleIndices = triangles;
        component.principalExtents = componentPrincipalExtents(mesh, triangles);
        const auto edgesIt = componentEdges.find(root);
        if (edgesIt != componentEdges.end())
        {
            for (const auto* usesPtr : edgesIt->second)
            {
                const auto& uses = *usesPtr;
                if (uses.size() > 2) ++component.nonManifoldEdges;
                else if (uses.size() == 1) ++component.boundaryEdges;
                else if (uses.size() == 2 && uses[0].direction == uses[1].direction)
                    ++component.windingConflicts;
            }
        }

        double signedVolume6 = 0.0;
        for (const auto ti : triangles)
        {
            const auto& triangle = mesh.triangles[ti];
            if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() || triangle.c >= mesh.vertices.size())
            {
                ++component.degenerateTriangles;
                continue;
            }
            const auto ca = topology.canonicalVertex[triangle.a];
            const auto cb = topology.canonicalVertex[triangle.b];
            const auto cc = topology.canonicalVertex[triangle.c];
            if (ca == cb || cb == cc || cc == ca)
            {
                ++component.degenerateTriangles;
                continue;
            }
            const glm::dvec3 a(mesh.vertices[triangle.a].position);
            const glm::dvec3 b(mesh.vertices[triangle.b].position);
            const glm::dvec3 c(mesh.vertices[triangle.c].position);
            const glm::dvec3 cross = glm::cross(b - a, c - a);
            const double crossLength = glm::length(cross);
            if (crossLength <= 1.0e-12)
            {
                ++component.degenerateTriangles;
                continue;
            }
            signedVolume6 += glm::dot(a, glm::cross(b, c));
        }

        component.closed = component.boundaryEdges == 0;
        component.insideOut = component.closed && signedVolume6 < -1.0e-10;
        if (component.closed)
        {
            component.suggestedClass = PreflightTopologyClass::ClosedVolume;
            component.confidence = 1.0;
        }
        else
        {
            const double middle = std::max(1.0e-9, component.principalExtents.y);
            const double thinRatio = component.principalExtents.x / middle;
            if (thinRatio <= 0.03)
            {
                component.suggestedClass = PreflightTopologyClass::ThinTwoSided;
                component.confidence = thinRatio <= 0.01 ? 0.97 : 0.88;
            }
            else
            {
                component.suggestedClass = PreflightTopologyClass::BreachedVolume;
                component.confidence = 0.72;
            }
        }

        out.boundaryEdges += component.boundaryEdges;
        out.nonManifoldEdges += component.nonManifoldEdges;
        out.windingConflicts += component.windingConflicts;
        out.degenerateTriangles += component.degenerateTriangles;
        if (component.closed) ++out.closedComponents; else ++out.openComponents;
        if (component.insideOut) ++out.insideOutComponents;
        out.components.push_back(std::move(component));
    }

    if (out.components.empty())
    {
        out.suggestedClass = PreflightTopologyClass::Invalid;
        out.confidence = 1.0;
        return out;
    }
    if (out.closedComponents == out.components.size())
    {
        out.suggestedClass = PreflightTopologyClass::ClosedVolume;
        out.confidence = 1.0;
        return out;
    }
    if (out.openComponents == out.components.size())
    {
        const bool allThin = std::all_of(out.components.begin(), out.components.end(), [](const auto& component) {
            return component.suggestedClass == PreflightTopologyClass::ThinTwoSided;
        });
        const bool allBreached = std::all_of(out.components.begin(), out.components.end(), [](const auto& component) {
            return component.suggestedClass == PreflightTopologyClass::BreachedVolume;
        });
        if (allThin)
        {
            out.suggestedClass = PreflightTopologyClass::ThinTwoSided;
            out.confidence = std::min_element(out.components.begin(), out.components.end(), [](const auto& a, const auto& b) {
                return a.confidence < b.confidence;
            })->confidence;
        }
        else if (allBreached)
        {
            out.suggestedClass = PreflightTopologyClass::BreachedVolume;
            out.confidence = 0.72;
        }
        else
        {
            out.suggestedClass = PreflightTopologyClass::Mixed;
            out.confidence = 0.55;
        }
        return out;
    }
    out.suggestedClass = PreflightTopologyClass::Mixed;
    out.confidence = 0.55;
    return out;
}


struct LodCoplanarPreviewGeometry
{
    std::vector<std::size_t> removedTriangleIndices;
    std::vector<std::uint32_t> addedTriangleIndices;
    std::size_t candidateRegions = 0;
    std::size_t collapsedRegions = 0;
};

struct LodCoplanarTriangleInfo
{
    bool valid = false;
    glm::dvec3 normal {0.0};
    double planeD = 0.0;
    std::array<std::size_t, 3> canonicalVertices {0, 0, 0};
};

using LodRenderVertexKey = std::array<std::int64_t, 8>;

using LodEdgeKey = std::uint64_t;

LodEdgeKey lodEdgeKey(std::size_t a, std::size_t b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(b));
}

std::array<std::size_t, 2> lodEdgeVertices(LodEdgeKey key)
{
    return {
        static_cast<std::size_t>(static_cast<std::uint32_t>(key >> 32u)),
        static_cast<std::size_t>(static_cast<std::uint32_t>(key & 0xffffffffu))
    };
}

struct LodEdgeAdjacency
{
    std::size_t triangleA = std::numeric_limits<std::size_t>::max();
    std::size_t triangleB = std::numeric_limits<std::size_t>::max();
    std::uint8_t count = 0;
};

LodRenderVertexKey lodRenderVertexKey(
    const Vertex& vertex,
    double positionEpsilon)
{
    const double p = std::max(positionEpsilon, 1.0e-9);
    constexpr double NormalEpsilon = 1.0e-5;
    constexpr double UvEpsilon = 1.0e-6;
    return {
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.position.x) / p)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.position.y) / p)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.position.z) / p)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.normal.x) / NormalEpsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.normal.y) / NormalEpsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.normal.z) / NormalEpsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.uv.x) / UvEpsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(vertex.uv.y) / UvEpsilon))
    };
}

bool triangulateSimpleCoplanarLoop(
    const MeshLod& mesh,
    const std::vector<std::uint32_t>& boundary,
    const glm::dvec3& normal,
    std::vector<std::uint32_t>& triangles)
{
    triangles.clear();
    if (boundary.size() < 3) return false;

    glm::dvec3 helper = std::abs(normal.z) < 0.85
        ? glm::dvec3(0.0, 0.0, 1.0)
        : glm::dvec3(0.0, 1.0, 0.0);
    glm::dvec3 axisU = glm::cross(helper, normal);
    const double axisULength = glm::length(axisU);
    if (axisULength <= 1.0e-12) return false;
    axisU /= axisULength;
    const glm::dvec3 axisV = glm::cross(normal, axisU);

    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> polygon(1);
    polygon.front().reserve(boundary.size());
    for (const auto vertexIndex : boundary)
    {
        if (vertexIndex >= mesh.vertices.size()) return false;
        const glm::dvec3 p(mesh.vertices[vertexIndex].position);
        polygon.front().push_back({glm::dot(p, axisU), glm::dot(p, axisV)});
    }

    const auto localIndices = mapbox::earcut<std::uint32_t>(polygon);
    if (localIndices.size() < 3 || localIndices.size() % 3 != 0) return false;
    triangles.reserve(localIndices.size());
    for (std::size_t i = 0; i < localIndices.size(); i += 3)
    {
        const auto la = localIndices[i];
        const auto lb = localIndices[i + 1];
        const auto lc = localIndices[i + 2];
        if (la >= boundary.size() || lb >= boundary.size() || lc >= boundary.size()) return false;
        std::uint32_t a = boundary[la];
        std::uint32_t b = boundary[lb];
        std::uint32_t c = boundary[lc];
        const glm::dvec3 pa(mesh.vertices[a].position);
        const glm::dvec3 pb(mesh.vertices[b].position);
        const glm::dvec3 pc(mesh.vertices[c].position);
        if (glm::dot(glm::cross(pb - pa, pc - pa), normal) < 0.0)
            std::swap(b, c);
        triangles.push_back(a);
        triangles.push_back(b);
        triangles.push_back(c);
    }
    return true;
}

LodCoplanarPreviewGeometry analyzeCoplanarCollapse(const MeshLod& mesh)
{
    LodCoplanarPreviewGeometry out;
    if (mesh.vertices.empty() || mesh.triangles.empty()) return out;

    const glm::dvec3 meshExtent = glm::dvec3(mesh.maxBounds) - glm::dvec3(mesh.minBounds);
    const double diagonal = std::max(1.0e-6, glm::length(meshExtent));
    const double positionEpsilon = std::max(1.0e-7, diagonal * 1.0e-8);
    const double planeEpsilon = std::max(1.0e-6, diagonal * 1.0e-7);
    const double normalCosine = std::cos(0.20 * Pi / 180.0);

    std::map<LodRenderVertexKey, std::size_t> canonicalByKey;
    std::vector<std::size_t> canonicalVertex(mesh.vertices.size(), 0);
    std::vector<std::uint32_t> representativeVertex;
    representativeVertex.reserve(mesh.vertices.size());
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi)
    {
        const auto key = lodRenderVertexKey(mesh.vertices[vi], positionEpsilon);
        const auto [it, inserted] = canonicalByKey.emplace(key, canonicalByKey.size());
        canonicalVertex[vi] = it->second;
        if (inserted) representativeVertex.push_back(static_cast<std::uint32_t>(vi));
    }

    std::vector<LodCoplanarTriangleInfo> info(mesh.triangles.size());
    std::unordered_map<LodEdgeKey, LodEdgeAdjacency> trianglesByEdge;
    trianglesByEdge.reserve(mesh.triangles.size() * 2u);
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
    {
        const auto& triangle = mesh.triangles[ti];
        if (triangle.a >= mesh.vertices.size() ||
            triangle.b >= mesh.vertices.size() ||
            triangle.c >= mesh.vertices.size())
            continue;
        const glm::dvec3 a(mesh.vertices[triangle.a].position);
        const glm::dvec3 b(mesh.vertices[triangle.b].position);
        const glm::dvec3 c(mesh.vertices[triangle.c].position);
        glm::dvec3 normal = glm::cross(b - a, c - a);
        const double length = glm::length(normal);
        if (length <= 1.0e-12) continue;
        normal /= length;
        auto& row = info[ti];
        row.valid = true;
        row.normal = normal;
        row.planeD = -glm::dot(normal, a);
        row.canonicalVertices = {
            canonicalVertex[triangle.a],
            canonicalVertex[triangle.b],
            canonicalVertex[triangle.c]
        };
        for (int edge = 0; edge < 3; ++edge)
        {
            const LodEdgeKey key = lodEdgeKey(
                row.canonicalVertices[edge],
                row.canonicalVertices[(edge + 1) % 3]);
            auto& adjacency = trianglesByEdge[key];
            if (adjacency.count == 0) adjacency.triangleA = ti;
            else if (adjacency.count == 1) adjacency.triangleB = ti;
            if (adjacency.count < std::numeric_limits<std::uint8_t>::max()) ++adjacency.count;
        }
    }

    LodDisjointSet sets(mesh.triangles.size());
    for (const auto& [edge, adjacent] : trianglesByEdge)
    {
        (void)edge;
        if (adjacent.count != 2) continue;
        const auto a = adjacent.triangleA;
        const auto b = adjacent.triangleB;
        if (!info[a].valid || !info[b].valid) continue;
        const auto& ta = mesh.triangles[a];
        const auto& tb = mesh.triangles[b];
        if (ta.materialIndex != tb.materialIndex || ta.smoothingGroupId != tb.smoothingGroupId) continue;
        if (glm::dot(info[a].normal, info[b].normal) < normalCosine) continue;
        bool coplanar = true;
        for (const auto vertexIndex : {tb.a, tb.b, tb.c})
        {
            const glm::dvec3 p(mesh.vertices[vertexIndex].position);
            if (std::abs(glm::dot(info[a].normal, p) + info[a].planeD) > planeEpsilon)
            {
                coplanar = false;
                break;
            }
        }
        if (coplanar) sets.unite(a, b);
    }

    std::map<std::size_t, std::vector<std::size_t>> regions;
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
        if (info[ti].valid) regions[sets.find(ti)].push_back(ti);

    for (const auto& [root, regionTriangles] : regions)
    {
        (void)root;
        if (regionTriangles.size() < 3) continue;
        ++out.candidateRegions;

        std::unordered_map<LodEdgeKey, std::uint8_t> edgeCount;
        edgeCount.reserve(regionTriangles.size() * 2u);
        for (const auto ti : regionTriangles)
        {
            const auto& cv = info[ti].canonicalVertices;
            for (int edge = 0; edge < 3; ++edge)
            {
                auto& count = edgeCount[lodEdgeKey(cv[edge], cv[(edge + 1) % 3])];
                if (count < std::numeric_limits<std::uint8_t>::max()) ++count;
            }
        }

        std::map<std::size_t, std::vector<std::size_t>> boundaryNeighbors;
        std::size_t boundaryEdges = 0;
        bool nonManifold = false;
        for (const auto& [edge, count] : edgeCount)
        {
            if (count > 2) { nonManifold = true; break; }
            if (count != 1) continue;
            const auto vertices = lodEdgeVertices(edge);
            boundaryNeighbors[vertices[0]].push_back(vertices[1]);
            boundaryNeighbors[vertices[1]].push_back(vertices[0]);
            ++boundaryEdges;
        }
        if (nonManifold || boundaryEdges < 3 || boundaryNeighbors.size() < 3) continue;
        bool simpleBoundary = true;
        for (const auto& [vertex, neighbors] : boundaryNeighbors)
        {
            (void)vertex;
            if (neighbors.size() != 2) { simpleBoundary = false; break; }
        }
        if (!simpleBoundary) continue;

        std::vector<std::size_t> canonicalLoop;
        canonicalLoop.reserve(boundaryNeighbors.size());
        const std::size_t start = boundaryNeighbors.begin()->first;
        std::size_t previous = std::numeric_limits<std::size_t>::max();
        std::size_t current = start;
        for (std::size_t guard = 0; guard <= boundaryNeighbors.size(); ++guard)
        {
            canonicalLoop.push_back(current);
            const auto& neighbors = boundaryNeighbors[current];
            const std::size_t next = neighbors[0] == previous ? neighbors[1] : neighbors[0];
            previous = current;
            current = next;
            if (current == start) break;
        }
        if (current != start || canonicalLoop.size() != boundaryNeighbors.size() ||
            canonicalLoop.size() != boundaryEdges)
            continue; // holes or more than one boundary loop are intentionally skipped.

        std::vector<std::uint32_t> boundary;
        boundary.reserve(canonicalLoop.size());
        bool validBoundary = true;
        for (const auto canonical : canonicalLoop)
        {
            if (canonical >= representativeVertex.size()) { validBoundary = false; break; }
            boundary.push_back(representativeVertex[canonical]);
        }
        if (!validBoundary) continue;

        std::vector<std::uint32_t> replacement;
        if (!triangulateSimpleCoplanarLoop(mesh, boundary, info[regionTriangles.front()].normal, replacement))
            continue;
        const std::size_t replacementTriangles = replacement.size() / 3u;
        if (replacementTriangles >= regionTriangles.size()) continue;

        out.removedTriangleIndices.insert(
            out.removedTriangleIndices.end(), regionTriangles.begin(), regionTriangles.end());
        out.addedTriangleIndices.insert(
            out.addedTriangleIndices.end(), replacement.begin(), replacement.end());
        ++out.collapsedRegions;
    }
    return out;
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = std::clamp(fraction, 0.0, 1.0) * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double t = position - static_cast<double>(lower);
    return values[lower] * (1.0 - t) + values[upper] * t;
}

std::vector<std::array<std::size_t, 2>> compressTriangleRanges(std::vector<std::size_t> values)
{
    std::vector<std::array<std::size_t, 2>> ranges;
    if (values.empty()) return ranges;
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    std::size_t begin = values.front();
    std::size_t previous = begin;
    for (std::size_t i = 1; i < values.size(); ++i)
    {
        if (values[i] == previous + 1)
        {
            previous = values[i];
            continue;
        }
        ranges.push_back({begin, previous - begin + 1});
        begin = previous = values[i];
    }
    ranges.push_back({begin, previous - begin + 1});
    return ranges;
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
    const std::filesystem::path& logPath,
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


void resetMeshRepairDiagnostic(
    const std::filesystem::path& logPath,
    const ModelAsset& asset) noexcept
{
    try
    {
        std::filesystem::create_directories(logPath.parent_path());
        std::ofstream out(logPath, std::ios::trunc);
        if (!out) return;
        out << "MODEL ASSET EDITOR MESH PREPARATION\n";
        out << "editor_version=" << ModelAssetEditorVersion << '\n';
        out << "algorithm=" << CanonicalMeshAlgorithmId << '\n';
        out << "asset=" << asset.assetId << '\n';
        out << "scope=one PREPARE MESHES operation; file is replaced on every run\n";
    }
    catch (...)
    {
        // Diagnostics must never alter editor behaviour.
    }
}

void appendMeshRepairDiagnostic(
    const std::filesystem::path& logPath,
    const ModelAsset& asset,
    std::size_t lodIndex,
    const RenderGeometryDefinition& geometry,
    const CanonicalMeshBuildResult& result) noexcept
{
    try
    {
        std::filesystem::create_directories(logPath.parent_path());
        std::ofstream out(logPath, std::ios::app);
        if (!out) return;

        out << "\n=== MESH REPAIR ===\n";
        out << "asset=" << asset.assetId
            << " lod=" << lodIndex
            << " geometry=" << geometry.id
            << " source=" << geometry.sourcePath << '\n';
        out << "algorithm=" << CanonicalMeshAlgorithmId
            << " success=" << result.success
            << " status=" << (result.repairStatus.empty() ? (result.success ? "GOOD_ENOUGH" : "FAILED") : result.repairStatus)
            << " changed=" << result.changed << '\n';
        out << "input render_vertices=" << result.before.renderVertices
            << " geometric_points=" << result.before.geometricPoints
            << " triangles=" << result.before.triangles
            << " degenerate=" << result.before.degenerateTriangles
            << " duplicate=" << result.before.duplicateTriangles
            << " boundary_edges=" << result.before.boundaryEdges
            << " nonmanifold_edges=" << result.before.canonicalMultiUseEdges
            << " winding_flips=" << result.before.windingFlipsRequired
            << " winding_conflicts=" << result.before.windingConflicts << '\n';

        out << "libigl split_topology_vertices=" << result.splitTopologyVertices
            << " raycast_patches=" << result.raycastPatches
            << " raycast_flipped_triangles=" << result.raycastFlippedTriangles << '\n';

        out << "output render_vertices=" << result.after.renderVertices
            << " geometric_points=" << result.after.geometricPoints
            << " triangles=" << result.after.triangles
            << " boundary_edges=" << result.after.boundaryEdges
            << " nonmanifold_edges=" << result.after.canonicalMultiUseEdges
            << " winding_flips=" << result.after.windingFlipsRequired
            << " winding_conflicts=" << result.after.windingConflicts
            << " inward_closed=" << result.after.insideOutClosedComponents << '\n';
        out << "removed_degenerate=" << result.removedDegenerateTriangles
            << " removed_duplicate=" << result.removedDuplicateTriangles
            << " flipped_triangles=" << result.flippedTriangles
            << " topology_stabilization_passes=" << result.topologyStabilizationPasses
            << " normal_islands=" << result.normalIslands
            << " rebuilt_edges=" << result.rebuiltEdges << '\n';
        if (!result.error.empty()) out << "error=" << result.error << '\n';
        out << "=== END MESH REPAIR ===\n";
    }
    catch (...)
    {
        // Repair diagnostics must never alter editor behaviour.
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

const char* physicalSizeAxisName(PhysicalSizeAxis axis)
{
    switch (axis) { case PhysicalSizeAxis::X: return "x"; case PhysicalSizeAxis::Y: return "y"; case PhysicalSizeAxis::Z: return "z"; }
    return "z";
}
PhysicalSizeAxis physicalSizeAxisFromName(const std::string& value)
{
    if (value == "x") return PhysicalSizeAxis::X;
    if (value == "y") return PhysicalSizeAxis::Y;
    return PhysicalSizeAxis::Z;
}
const char* structuralLinkKindName(StructuralLinkKind kind)
{
    switch (kind) {
        case StructuralLinkKind::WeldSeam: return "weld_seam";
        case StructuralLinkKind::FixedMount: return "fixed_mount";
        case StructuralLinkKind::EquipmentMount: return "equipment_mount";
        case StructuralLinkKind::ControlledLock: return "controlled_lock";
    }
    return "fixed_mount";
}
StructuralLinkKind structuralLinkKindFromName(const std::string& value)
{
    if (value == "weld_seam") return StructuralLinkKind::WeldSeam;
    if (value == "equipment_mount") return StructuralLinkKind::EquipmentMount;
    if (value == "controlled_lock") return StructuralLinkKind::ControlledLock;
    return StructuralLinkKind::FixedMount;
}
const char* structuralDamageShapeName(StructuralDamageShape shape)
{
    return shape == StructuralDamageShape::Capsule ? "capsule" : "box";
}


glm::mat3 eulerRotation(const glm::vec3& deg)
{
    const glm::mat4 m = glm::eulerAngleXYZ(
        glm::radians(deg.x), glm::radians(deg.y), glm::radians(deg.z));
    return glm::mat3(m);
}

glm::vec3 eulerDegrees(const glm::mat3& rotation)
{
    // ModelAsset stores Euler angles using an explicit XYZ convention, and both
    // the editor renderer (THREE.Euler(..., "XYZ")) and eulerRotation() decode
    // them as XYZ. the generic quaternion Euler extractor uses a different convention
    // for general multi-axis rotations, so matrix -> quat -> eulerAngles was not
    // the inverse of eulerAngleXYZ(). That could corrupt one radial/consolidated
    // instance while the fit matrix itself was correct. Extract with the matching
    // XYZ routine instead.
    float x = 0.0f, y = 0.0f, z = 0.0f;
    glm::extractEulerAngleXYZ(glm::mat4(rotation), x, y, z);
    const glm::vec3 deg = glm::degrees(glm::vec3(x, y, z));

    // Guard the persistence representation itself: any encoded rotation must
    // reconstruct the same matrix under the asset's XYZ decoder.
    const glm::mat3 roundTrip = eulerRotation(deg);
    float maxError = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r)
            maxError = std::max(maxError, std::abs(roundTrip[c][r] - rotation[c][r]));
    if (!std::isfinite(maxError) || maxError > 2.0e-4f)
        throw std::runtime_error("cannot encode rigid rotation as ModelAsset XYZ Euler angles");
    return deg;
}

glm::vec3 eulerDegrees(const glm::quat& q)
{
    return eulerDegrees(glm::mat3_cast(glm::normalize(q)));
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

RigidTransform semanticNodeWorldTransform(const ModelAsset& asset, std::size_t index)
{
    if (index >= asset.nodes.size())
        throw std::runtime_error("invalid semantic node index");
    std::vector<RigidTransform> cache(asset.nodes.size());
    std::vector<std::uint8_t> state(asset.nodes.size(), 0);
    std::function<RigidTransform(std::size_t)> resolve = [&](std::size_t i) -> RigidTransform
    {
        if (i >= asset.nodes.size()) throw std::runtime_error("invalid semantic parent index");
        if (state[i] == 2) return cache[i];
        if (state[i] == 1) throw std::runtime_error("semantic hierarchy cycle");
        state[i] = 1;
        const auto& node = asset.nodes[i];
        const RigidTransform local = nodeRigidTransform(node);
        if (node.parentIndex >= 0)
            cache[i] = composeRigid(resolve(static_cast<std::size_t>(node.parentIndex)), local);
        else
            cache[i] = local;
        state[i] = 2;
        return cache[i];
    };
    return resolve(index);
}

bool semanticNodeHasDynamicTransformState(const ModelAsset& asset, std::size_t nodeIndex)
{
    return std::any_of(asset.stateVariants.begin(), asset.stateVariants.end(), [&](const StateVariant& state) {
        return state.nodeIndex == static_cast<std::int32_t>(nodeIndex) && (state.transformOverride || state.detached);
    });
}

bool semanticNodeHasDynamicIncoming(const ModelAsset& asset, std::size_t nodeIndex)
{
    if (nodeIndex >= asset.nodes.size()) return false;
    const auto& node = asset.nodes[nodeIndex];
    return node.joint.type != JointType::Fixed || node.joint.breakable ||
        semanticNodeHasDynamicTransformState(asset, nodeIndex);
}

bool semanticAncestorHasDynamicTransform(const ModelAsset& asset, std::size_t nodeIndex)
{
    if (nodeIndex >= asset.nodes.size()) return false;
    std::set<std::size_t> visited;
    std::int32_t parent = asset.nodes[nodeIndex].parentIndex;
    while (parent >= 0)
    {
        const auto parentIndex = static_cast<std::size_t>(parent);
        if (parentIndex >= asset.nodes.size() || !visited.insert(parentIndex).second)
            return true; // Invalid/cyclic trees are never safe to flatten.
        if (semanticNodeHasDynamicIncoming(asset, parentIndex)) return true;
        parent = asset.nodes[parentIndex].parentIndex;
    }
    return false;
}

std::vector<std::size_t> staticSemanticFlattenCandidates(const ModelAsset& asset)
{
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < asset.nodes.size(); ++i)
    {
        const auto& node = asset.nodes[i];
        if (node.parentIndex < 0) continue;
        if (semanticNodeHasDynamicIncoming(asset, i)) continue;
        if (semanticAncestorHasDynamicTransform(asset, i)) continue;
        result.push_back(i);
    }
    return result;
}

std::size_t semanticNodeDepth(const ModelAsset& asset, std::size_t nodeIndex)
{
    std::size_t depth = 0;
    std::set<std::size_t> visited;
    std::int32_t parent = nodeIndex < asset.nodes.size() ? asset.nodes[nodeIndex].parentIndex : NoIndex;
    while (parent >= 0)
    {
        const auto parentIndex = static_cast<std::size_t>(parent);
        if (parentIndex >= asset.nodes.size() || !visited.insert(parentIndex).second) break;
        ++depth;
        parent = asset.nodes[parentIndex].parentIndex;
    }
    return depth;
}

void reparentSemanticNodesPreserveWorld(
    ModelAsset& asset,
    std::vector<std::size_t> nodeIndices,
    std::int32_t parentIndex)
{
    if (parentIndex < NoIndex || parentIndex >= static_cast<std::int32_t>(asset.nodes.size()))
        throw std::runtime_error("invalid semantic parent index");
    std::sort(nodeIndices.begin(), nodeIndices.end());
    nodeIndices.erase(std::unique(nodeIndices.begin(), nodeIndices.end()), nodeIndices.end());
    if (nodeIndices.empty()) throw std::runtime_error("no semantic nodes selected for reparent");
    const std::set<std::size_t> moving(nodeIndices.begin(), nodeIndices.end());
    if (parentIndex >= 0 && moving.count(static_cast<std::size_t>(parentIndex)))
        throw std::runtime_error("semantic parent cannot be inside the moved selection");

    // A batch represents sibling/subtree roots. Passing both an ancestor and one
    // of its descendants would flatten the descendant, so reject that ambiguous
    // request instead of silently changing the authored tree shape.
    for (const auto index : nodeIndices)
    {
        if (index >= asset.nodes.size()) throw std::runtime_error("invalid semantic node index");
        std::int32_t cursor = asset.nodes[index].parentIndex;
        while (cursor >= 0)
        {
            if (moving.count(static_cast<std::size_t>(cursor)))
                throw std::runtime_error("semantic batch reparent contains both ancestor and descendant");
            cursor = asset.nodes[static_cast<std::size_t>(cursor)].parentIndex;
        }
    }

    // Validate cycles against the original hierarchy before mutating anything.
    for (const auto index : nodeIndices)
    {
        std::int32_t cursor = parentIndex;
        while (cursor >= 0)
        {
            if (cursor == static_cast<std::int32_t>(index))
                throw std::runtime_error("semantic reparent would create a hierarchy cycle");
            cursor = asset.nodes[static_cast<std::size_t>(cursor)].parentIndex;
        }
    }

    std::vector<RigidTransform> oldWorld;
    oldWorld.reserve(nodeIndices.size());
    for (const auto index : nodeIndices)
        oldWorld.push_back(semanticNodeWorldTransform(asset, index));
    const RigidTransform parentWorld = parentIndex >= 0
        ? semanticNodeWorldTransform(asset, static_cast<std::size_t>(parentIndex))
        : RigidTransform{};
    const RigidTransform inverseParent = inverseRigid(parentWorld);

    for (std::size_t i = 0; i < nodeIndices.size(); ++i)
    {
        auto& node = asset.nodes[nodeIndices[i]];
        node.parentIndex = parentIndex;
        const RigidTransform local = composeRigid(inverseParent, oldWorld[i]);
        setNodeRigidTransform(node, local, node.pivot);
    }
}

RigidTransform renderNodeRigidTransform(const RenderNode& node)
{
    return {eulerRotation(node.localRotationDeg), node.localPosition + node.pivot - eulerRotation(node.localRotationDeg) * node.pivot};
}

double renderLodPlacedCharacteristicSize(const RenderLod& lod)
{
    glm::vec3 worldMin(std::numeric_limits<float>::max());
    glm::vec3 worldMax(-std::numeric_limits<float>::max());
    bool haveWorldBounds = false;
    std::vector<RigidTransform> worldTransforms(lod.nodes.size());
    std::vector<std::uint8_t> worldState(lod.nodes.size(), 0);
    std::function<RigidTransform(std::size_t)> resolveWorld = [&](std::size_t nodeIndex) -> RigidTransform
    {
        if (nodeIndex >= lod.nodes.size()) return {};
        if (worldState[nodeIndex] == 2) return worldTransforms[nodeIndex];
        if (worldState[nodeIndex] == 1) return renderNodeRigidTransform(lod.nodes[nodeIndex]);
        worldState[nodeIndex] = 1;
        const auto local = renderNodeRigidTransform(lod.nodes[nodeIndex]);
        const auto parent = lod.nodes[nodeIndex].parentIndex;
        worldTransforms[nodeIndex] = parent >= 0 && static_cast<std::size_t>(parent) < lod.nodes.size()
            ? composeRigid(resolveWorld(static_cast<std::size_t>(parent)), local)
            : local;
        worldState[nodeIndex] = 2;
        return worldTransforms[nodeIndex];
    };

    for (std::size_t nodeIndex = 0; nodeIndex < lod.nodes.size(); ++nodeIndex)
    {
        const auto& node = lod.nodes[nodeIndex];
        if (!node.enabled || node.geometryIndex < 0 ||
            static_cast<std::size_t>(node.geometryIndex) >= lod.geometries.size())
            continue;
        const auto& mesh = lod.geometries[static_cast<std::size_t>(node.geometryIndex)].mesh;
        const auto transform = resolveWorld(nodeIndex);
        const glm::vec3 mn = mesh.minBounds;
        const glm::vec3 mx = mesh.maxBounds;
        for (int mask = 0; mask < 8; ++mask)
        {
            const glm::vec3 corner(
                (mask & 1) ? mx.x : mn.x,
                (mask & 2) ? mx.y : mn.y,
                (mask & 4) ? mx.z : mn.z);
            const glm::vec3 point = transformPoint(transform, corner);
            if (!haveWorldBounds)
            {
                worldMin = worldMax = point;
                haveWorldBounds = true;
            }
            else
            {
                worldMin = glm::min(worldMin, point);
                worldMax = glm::max(worldMax, point);
            }
        }
    }
    if (!haveWorldBounds) return 1.0;
    return static_cast<double>(lodCharacteristicSize(glm::max(worldMax - worldMin, glm::vec3(0.0f))));
}


void scaleAuthoringDistancesUniform(ModelAsset& asset, float scale)
{
    // Authoring -> meter conversion used only on a temporary BUILD copy (or the
    // inverse when a metric production package is adopted into the editor).
    // Physical values already expressed in SI units -- kg, kg*m^2, N, Nm and
    // LightProperties::rangeMeters -- are deliberately NOT multiplied again.
    if (!std::isfinite(scale) || scale <= 0.0f) throw std::runtime_error("invalid authoring-to-meter scale");
    if (std::abs(scale - 1.0f) <= 1.0e-7f) return;
    const auto scalePhysicsPosition = [&](RigidBodyProperties& p) { p.centerOfMass *= scale; };
    asset.minBounds *= scale;
    asset.maxBounds *= scale;
    for (auto& node : asset.nodes)
    {
        node.localPosition *= scale; node.pivot *= scale; node.joint.pivot *= scale;
        scalePhysicsPosition(node.physics);
    }
    for (auto& state : asset.stateVariants)
    {
        state.localPosition *= scale; state.pivot *= scale; scalePhysicsPosition(state.physics);
    }
    for (auto& c : asset.collisionVolumes)
    {
        c.localPosition *= scale; c.halfSize *= scale; c.radius *= scale; c.halfHeight *= scale;
    }
    for (auto& socket : asset.sockets)
    {
        socket.localPosition *= scale; socket.extent *= scale;
    }
    for (auto& hit : asset.hitRegions) { hit.localPosition *= scale; hit.halfSize *= scale; }
    for (auto& opening : asset.openings) { opening.localPosition *= scale; opening.halfSize *= scale; }
    for (auto& repair : asset.repairTargets) repair.localPosition *= scale;
    for (auto& link : asset.structuralLinks)
        for (auto& proxy : link.damageProxies)
        {
            proxy.localPosition *= scale; proxy.halfSize *= scale; proxy.radius *= scale; proxy.halfHeight *= scale;
        }
    for (auto& lod : asset.renderLods)
    {
        lod.minBounds *= scale; lod.maxBounds *= scale;
        for (auto& node : lod.nodes) { node.localPosition *= scale; node.pivot *= scale; }
        for (auto& geometry : lod.geometries)
        {
            geometry.mesh.minBounds *= scale; geometry.mesh.maxBounds *= scale;
            for (auto& vertex : geometry.mesh.vertices) vertex.position *= scale;
        }
    }
    for (auto& geometry : asset.geometries)
        for (auto& lod : geometry.lods)
        {
            lod.minBounds *= scale; lod.maxBounds *= scale;
            for (auto& vertex : lod.vertices) vertex.position *= scale;
        }
}

const char* physicalGeometrySpaceName(PhysicalGeometrySpace space)
{
    switch (space)
    {
        case PhysicalGeometrySpace::Authoring: return "authoring";
        case PhysicalGeometrySpace::Meters: return "meters";
        case PhysicalGeometrySpace::LegacyUnknown: return "legacy_unknown";
    }
    return "legacy_unknown";
}

float authoringToMetersScale(const ModelAsset& asset)
{
    return asset.physicalSize.enabled && std::isfinite(asset.physicalSize.sourceToMeters) &&
        asset.physicalSize.sourceToMeters > 0.0f
        ? asset.physicalSize.sourceToMeters
        : 1.0f;
}

std::vector<RigidTransform> renderWorldTransforms(const RenderLod& lod)
{
    std::vector<RigidTransform> out(lod.nodes.size());
    std::vector<std::uint8_t> state(lod.nodes.size(), 0);
    std::function<RigidTransform(std::size_t)> resolve = [&](std::size_t i) -> RigidTransform {
        if (i >= lod.nodes.size()) return {};
        if (state[i] == 2) return out[i];
        if (state[i] == 1) return renderNodeRigidTransform(lod.nodes[i]);
        state[i] = 1;
        const auto local = renderNodeRigidTransform(lod.nodes[i]);
        const auto p = lod.nodes[i].parentIndex;
        out[i] = p >= 0 && static_cast<std::size_t>(p) < lod.nodes.size()
            ? composeRigid(resolve(static_cast<std::size_t>(p)), local) : local;
        state[i] = 2;
        return out[i];
    };
    for (std::size_t i = 0; i < lod.nodes.size(); ++i) resolve(i);
    return out;
}

glm::vec3 authoringPlacedExtents(const ModelAsset& asset)
{
    const glm::vec3 fallback = glm::max(asset.maxBounds - asset.minBounds, glm::vec3(0.0f));
    if (asset.renderLods.empty()) return fallback;

    const auto& lod = asset.renderLods.front();
    if (lod.nodes.empty() || lod.geometries.empty()) return fallback;
    const auto worlds = renderWorldTransforms(lod);
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    bool have = false;
    for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
    {
        const auto& rn = lod.nodes[ri];
        if (!rn.enabled || rn.geometryIndex < 0 || static_cast<std::size_t>(rn.geometryIndex) >= lod.geometries.size()) continue;
        const auto& mesh = lod.geometries[static_cast<std::size_t>(rn.geometryIndex)].mesh;
        const glm::vec3 b0 = mesh.minBounds;
        const glm::vec3 b1 = mesh.maxBounds;
        for (int mask = 0; mask < 8; ++mask)
        {
            const glm::vec3 corner(
                (mask & 1) ? b1.x : b0.x,
                (mask & 2) ? b1.y : b0.y,
                (mask & 4) ? b1.z : b0.z);
            const glm::vec3 world = transformPoint(worlds[ri], corner);
            mn = glm::min(mn, world);
            mx = glm::max(mx, world);
            have = true;
        }
    }
    return have ? glm::max(mx - mn, glm::vec3(0.0f)) : fallback;
}

float authoringAxisExtent(const ModelAsset& asset, PhysicalSizeAxis axis)
{
    const glm::vec3 size = authoringPlacedExtents(asset);
    switch (axis) { case PhysicalSizeAxis::X: return size.x; case PhysicalSizeAxis::Y: return size.y; case PhysicalSizeAxis::Z: return size.z; }
    return size.z;
}

struct WorldSegment
{
    glm::vec3 a {0.0f};
    glm::vec3 b {0.0f};
};

bool semanticNodeIsDescendantOrSelf(const ModelAsset& asset, std::int32_t candidate, std::int32_t ancestor)
{
    std::size_t guard = 0;
    while (candidate >= 0 && candidate < static_cast<std::int32_t>(asset.nodes.size()) && guard++ <= asset.nodes.size())
    {
        if (candidate == ancestor) return true;
        candidate = asset.nodes[static_cast<std::size_t>(candidate)].parentIndex;
    }
    return false;
}

std::vector<WorldSegment> semanticBoundarySegments(const ModelAsset& asset, const RenderLod& lod, std::int32_t semanticNodeIndex)
{
    std::vector<WorldSegment> out;
    const auto worlds = renderWorldTransforms(lod);
    for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
    {
        const auto& rn = lod.nodes[ri];
        if (!rn.enabled || !semanticNodeIsDescendantOrSelf(asset, rn.semanticNodeIndex, semanticNodeIndex) || rn.geometryIndex < 0 ||
            static_cast<std::size_t>(rn.geometryIndex) >= lod.geometries.size()) continue;
        const auto& mesh = lod.geometries[static_cast<std::size_t>(rn.geometryIndex)].mesh;
        for (const auto& edge : mesh.edges)
        {
            if ((edge.flags & EdgeBoundary) == 0 && edge.triangleB >= 0) continue;
            if (edge.a >= mesh.vertices.size() || edge.b >= mesh.vertices.size()) continue;
            out.push_back({transformPoint(worlds[ri], mesh.vertices[edge.a].position),
                           transformPoint(worlds[ri], mesh.vertices[edge.b].position)});
        }
    }
    return out;
}

StructuralDamageProxy makeStructuralProxySeed(
    const ModelAsset& asset,
    const RenderLod& lod,
    const StructuralLinkDefinition& link)
{
    StructuralDamageProxy proxy;
    proxy.id = link.id + ".hit.1";
    proxy.parentNodeIndex = link.nodeAIndex;
    if (link.nodeAIndex < 0 || link.nodeBIndex < 0 ||
        link.nodeAIndex >= static_cast<std::int32_t>(asset.nodes.size()) ||
        link.nodeBIndex >= static_cast<std::int32_t>(asset.nodes.size())) return proxy;

    const auto aWorld = semanticNodeWorldTransform(asset, static_cast<std::size_t>(link.nodeAIndex));
    const auto invA = inverseRigid(aWorld);
    if (link.kind == StructuralLinkKind::WeldSeam)
    {
        const auto aSegs = semanticBoundarySegments(asset, lod, link.nodeAIndex);
        const auto bSegs = semanticBoundarySegments(asset, lod, link.nodeBIndex);
        float bestScore = std::numeric_limits<float>::max();
        WorldSegment bestA, bestB;
        bool have = false;
        for (const auto& sa : aSegs)
        {
            const glm::vec3 da0 = sa.b - sa.a;
            const float la = glm::length(da0);
            if (la <= 1.0e-5f) continue;
            const glm::vec3 da = da0 / la;
            const glm::vec3 ma = (sa.a + sa.b) * 0.5f;
            for (const auto& sb : bSegs)
            {
                const glm::vec3 db0 = sb.b - sb.a;
                const float lb = glm::length(db0);
                if (lb <= 1.0e-5f) continue;
                const glm::vec3 db = db0 / lb;
                const float parallel = std::abs(glm::dot(da, db));
                if (parallel < 0.80f) continue;
                const glm::vec3 mb = (sb.a + sb.b) * 0.5f;
                const float distance = glm::length(ma - mb);
                const float score = distance + (1.0f - parallel) * std::min(la, lb);
                if (score < bestScore) { bestScore = score; bestA = sa; bestB = sb; have = true; }
            }
        }
        if (have)
        {
            const glm::vec3 midA = (bestA.a + bestA.b) * 0.5f;
            const glm::vec3 midB = (bestB.a + bestB.b) * 0.5f;
            glm::vec3 axis = (bestA.b - bestA.a) + (bestB.b - bestB.a);
            if (glm::dot(axis, axis) <= 1.0e-10f) axis = bestA.b - bestA.a;
            axis = glm::normalize(axis);
            const float length = std::max(0.02f, std::min(glm::length(bestA.b-bestA.a), glm::length(bestB.b-bestB.a)));
            const glm::vec3 centerWorld = (midA + midB) * 0.5f;
            const glm::mat3 worldRot = glm::mat3_cast(glm::rotation(glm::vec3(0,1,0), axis));
            proxy.shape = StructuralDamageShape::Capsule;
            proxy.localPosition = transformPoint(invA, centerWorld);
            proxy.localRotationDeg = eulerDegrees(invA.rotation * worldRot);
            proxy.radius = std::max(0.01f, glm::length(midA-midB) * 0.5f + 0.01f);
            proxy.halfHeight = length * 0.5f;
            return proxy;
        }
    }

    const glm::vec3 aOrigin = aWorld.translation;
    const glm::vec3 bOrigin = semanticNodeWorldTransform(asset, static_cast<std::size_t>(link.nodeBIndex)).translation;
    if (link.kind == StructuralLinkKind::WeldSeam)
    {
        // No compatible boundary pair was found. Keep the authored intent as a
        // capsule (never silently change WELD into a box) and provide a small
        // editable seed in canonical space. The user can align it in GRAPH.
        glm::vec3 axis = bOrigin - aOrigin;
        const float distance = glm::length(axis);
        if (distance <= 1.0e-5f) axis = glm::vec3(0.0f, 1.0f, 0.0f);
        else axis /= distance;
        proxy.shape = StructuralDamageShape::Capsule;
        proxy.localPosition = transformPoint(invA, (aOrigin + bOrigin) * 0.5f);
        const glm::mat3 worldRot = glm::mat3_cast(glm::rotation(glm::vec3(0,1,0), axis));
        proxy.localRotationDeg = eulerDegrees(invA.rotation * worldRot);
        proxy.radius = std::clamp(std::max(distance, 0.1f) * 0.01f, 0.02f, 0.5f);
        proxy.halfHeight = std::clamp(std::max(distance, 0.1f) * 0.10f, 0.10f, 2.0f);
        return proxy;
    }

    // Generic mount/lock seed: place an editable OBB midway between semantic
    // origins. This is deliberately a seed, not inferred physical truth.
    proxy.shape = StructuralDamageShape::Box;
    proxy.localPosition = transformPoint(invA, (aOrigin + bOrigin) * 0.5f);
    const float d = std::max(0.1f, glm::length(bOrigin - aOrigin));
    proxy.halfSize = glm::vec3(std::clamp(d * 0.025f, 0.05f, 1.0f));
    return proxy;
}

void setRenderNodeRigidTransform(RenderNode& node, const RigidTransform& transform, const glm::vec3& pivot)
{
    // Encode into a temporary node first. Instance/array authoring must be
    // placement preserving: a serialization convention mismatch is an error,
    // never a reason to silently move one copy onto another.
    RenderNode encoded = node;
    encoded.pivot = pivot;
    encoded.localRotationDeg = eulerDegrees(transform.rotation);
    encoded.localPosition = transform.translation - pivot + transform.rotation * pivot;

    const RigidTransform roundTrip = renderNodeRigidTransform(encoded);
    float maxRotationError = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r)
            maxRotationError = std::max(
                maxRotationError,
                std::abs(roundTrip.rotation[c][r] - transform.rotation[c][r]));
    const float translationError = glm::length(roundTrip.translation - transform.translation);
    if (!std::isfinite(maxRotationError) || !std::isfinite(translationError) ||
        maxRotationError > 2.0e-4f || translationError > 2.0e-4f)
    {
        throw std::runtime_error(
            "render-node transform XYZ round-trip changed placement");
    }

    node.pivot = encoded.pivot;
    node.localRotationDeg = encoded.localRotationDeg;
    node.localPosition = encoded.localPosition;
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

PrimitiveMass primitiveMass(const CollisionVolume& c, float density, float sourceToMeters)
{
    PrimitiveMass out;
    const float scale = std::max(sourceToMeters, 1.0e-9f);
    out.center = c.localPosition * scale;
    glm::vec3 diag(0.0f);

    if (c.shape == CollisionShape::Sphere)
    {
        const float r = std::max(c.radius * scale, 0.001f);
        const float volume = (4.0f / 3.0f) * Pi * r * r * r;
        out.mass = density * volume;
        diag = glm::vec3(0.4f * out.mass * r * r);
    }
    else if (c.shape == CollisionShape::Capsule)
    {
        const float r = std::max(c.radius * scale, 0.001f);
        const float half = std::max(c.halfHeight * scale, 0.0f);
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
        const glm::vec3 h = glm::max(c.halfSize * scale, glm::vec3(0.001f));
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
    if (nodeIndex >= asset.nodes.size() || !asset.physicalSize.enabled ||
        asset.physicalSize.geometrySpace != PhysicalGeometrySpace::Authoring ||
        !std::isfinite(asset.physicalSize.sourceToMeters) || asset.physicalSize.sourceToMeters <= 0.0f)
        return false;
    const float metricScale = asset.physicalSize.sourceToMeters;
    std::vector<PrimitiveMass> pieces;
    for (const auto& collision : asset.collisionVolumes)
    {
        if (collision.enabled && collision.parentNodeIndex == static_cast<std::int32_t>(nodeIndex))
            pieces.push_back(primitiveMass(collision, density, metricScale));
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
    physics.centerOfMass = com / metricScale;
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

struct LodCullBuildStats
{
    std::size_t removedTriangles = 0;
    std::size_t removedComponents = 0;
};

MeshLod buildComponentCullMesh(
    const MeshLod& source,
    double thresholdMeters,
    LodCullBuildStats* stats = nullptr)
{
    LodCullBuildStats local;
    if (source.triangles.empty() || thresholdMeters <= 0.0)
    {
        if (stats) *stats = local;
        return source;
    }

    std::vector<std::uint8_t> removed(source.triangles.size(), 0);
    for (const auto& component : analyzeConnectedComponents(source))
    {
        if (component.protectedStructure || component.featureMeters >= thresholdMeters)
            continue;
        ++local.removedComponents;
        for (const auto triangleIndex : component.triangleIndices)
        {
            if (triangleIndex >= removed.size() || removed[triangleIndex]) continue;
            removed[triangleIndex] = 1;
            ++local.removedTriangles;
        }
    }

    if (local.removedTriangles == 0)
    {
        if (stats) *stats = local;
        return source;
    }

    MeshLod out;
    std::vector<std::int32_t> triangleMap(source.triangles.size(), -1);
    std::vector<std::uint8_t> usedVertex(source.vertices.size(), 0);
    out.triangles.reserve(source.triangles.size() - local.removedTriangles);
    for (std::size_t ti = 0; ti < source.triangles.size(); ++ti)
    {
        if (removed[ti]) continue;
        const auto& triangle = source.triangles[ti];
        if (triangle.a >= source.vertices.size() ||
            triangle.b >= source.vertices.size() ||
            triangle.c >= source.vertices.size())
            continue;
        triangleMap[ti] = static_cast<std::int32_t>(out.triangles.size());
        out.triangles.push_back(triangle);
        usedVertex[triangle.a] = usedVertex[triangle.b] = usedVertex[triangle.c] = 1;
    }

    std::vector<std::int32_t> vertexMap(source.vertices.size(), -1);
    out.vertices.reserve(source.vertices.size());
    for (std::size_t vi = 0; vi < source.vertices.size(); ++vi)
    {
        if (!usedVertex[vi]) continue;
        vertexMap[vi] = static_cast<std::int32_t>(out.vertices.size());
        out.vertices.push_back(source.vertices[vi]);
    }
    for (auto& triangle : out.triangles)
    {
        triangle.a = static_cast<std::uint32_t>(vertexMap[triangle.a]);
        triangle.b = static_cast<std::uint32_t>(vertexMap[triangle.b]);
        triangle.c = static_cast<std::uint32_t>(vertexMap[triangle.c]);
    }

    out.edges.reserve(source.edges.size());
    for (const auto& edge : source.edges)
    {
        if (edge.a >= vertexMap.size() || edge.b >= vertexMap.size()) continue;
        if (vertexMap[edge.a] < 0 || vertexMap[edge.b] < 0) continue;
        const auto remapTriangle = [&](std::int32_t oldIndex) -> std::int32_t
        {
            if (oldIndex < 0 || static_cast<std::size_t>(oldIndex) >= triangleMap.size()) return -1;
            return triangleMap[static_cast<std::size_t>(oldIndex)];
        };
        const std::int32_t triangleA = remapTriangle(edge.triangleA);
        const std::int32_t triangleB = remapTriangle(edge.triangleB);
        const bool standaloneAuthoredEdge = edge.triangleA < 0 && edge.triangleB < 0;
        if (triangleA < 0 && triangleB < 0 && !standaloneAuthoredEdge) continue;

        Edge mapped = edge;
        mapped.a = static_cast<std::uint32_t>(vertexMap[edge.a]);
        mapped.b = static_cast<std::uint32_t>(vertexMap[edge.b]);
        mapped.triangleA = triangleA;
        mapped.triangleB = triangleB;
        out.edges.push_back(mapped);
    }
    recomputeLodBounds(out);
    if (stats) *stats = local;
    return out;
}

RenderLod buildGeneratedComponentCullLod(
    const RenderLod& source,
    std::size_t targetLevel,
    double thresholdMeters,
    std::size_t* removedTriangles = nullptr,
    std::size_t* removedComponents = nullptr)
{
    RenderLod generated = source;
    generated.level = static_cast<std::uint32_t>(targetLevel);
    generated.sourceKind = "generated";
    generated.generatedFromLod = static_cast<std::int32_t>(source.level);

    std::size_t totalRemovedTriangles = 0;
    std::size_t totalRemovedComponents = 0;
    for (auto& geometry : generated.geometries)
    {
        LodCullBuildStats stats;
        geometry.mesh = buildComponentCullMesh(geometry.mesh, thresholdMeters, &stats);
        totalRemovedTriangles += stats.removedTriangles;
        totalRemovedComponents += stats.removedComponents;
    }
    recomputeRenderLodBounds(generated);
    if (removedTriangles) *removedTriangles = totalRemovedTriangles;
    if (removedComponents) *removedComponents = totalRemovedComponents;
    return generated;
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

std::filesystem::path runtimeAssemblySourceDirectory(ObjectType type)
{
    try
    {
        game::ship::geometry::ObjectAssemblyRegistry::ensureInitialized();
        if (!game::ship::geometry::ObjectAssemblyRegistry::has(type)) return {};
        const auto& assembly = game::ship::geometry::ObjectAssemblyRegistry::get(type);

        std::set<std::filesystem::path> roots;
        const auto collect = [&](const std::string& raw)
        {
            if (raw.empty()) return;
            std::filesystem::path path(raw);
            const auto generic = path.generic_string();
            constexpr const char* prefix = "assets/models/";
            if (generic.rfind(prefix, 0) != 0) return;
            std::filesystem::path relative(generic.substr(std::char_traits<char>::length(prefix)));
            auto parent = relative.parent_path();
            while (!parent.empty())
            {
                const auto name = lowerText(parent.filename().string());
                if (name.size() > 3 && name.rfind("lod", 0) == 0 &&
                    std::all_of(name.begin() + 3, name.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
                {
                    parent = parent.parent_path();
                    break;
                }
                parent = parent.parent_path();
            }
            if (!parent.empty()) roots.insert(parent.lexically_normal());
        };

        collect(assembly.wholeShipProxyPath);
        for (const auto& module : assembly.modules)
            for (const auto& mesh : module.meshes)
            {
                collect(mesh.lod0Path);
                collect(mesh.lod1Path);
            }

        return roots.size() == 1 ? *roots.begin() : std::filesystem::path{};
    }
    catch (...)
    {
        return {};
    }
}

std::vector<std::filesystem::path> discoverShipSourceDirectories(const std::filesystem::path& sourceRoot)
{
    std::vector<std::filesystem::path> result;
    const auto shipsRoot = resolveSourceFolderAssetRoot(sourceRoot, "ships");
    if (shipsRoot.empty()) return result;

    std::error_code ec;
    std::filesystem::directory_iterator it(
        shipsRoot, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::directory_iterator end;
    for (; !ec && it != end; it.increment(ec))
    {
        if (!it->is_directory(ec)) continue;
        const auto relative = std::filesystem::path("ships") / it->path().filename();
        if (sourceFolderAssetAvailable(sourceRoot, relative))
            result.push_back(relative.lexically_normal());
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return lowerText(a.filename().string()) < lowerText(b.filename().string());
    });
    return result;
}

std::string catalogIdForShipFolder(const std::filesystem::path& directory)
{
    std::string id = "cobra_mk1_source_" + lowerText(directory.filename().string());
    for (char& c : id)
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    return id;
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
    m_workingFilesRoot = m_sourceRoot / "build" / "tools" / "model_asset_editor" / "workspaces";
    loadSettings();
    installLocalizationBundle();

    // Ship source folders are first-class catalog choices. The folder currently
    // referenced by ObjectAssemblyRegistry keeps the canonical cobra_mk1 id and
    // runtime-assembly import so legacy descriptor semantics are preserved. The
    // other folders are raw folder-authoritative source candidates.
    const auto runtimeCobraDirectory = runtimeAssemblySourceDirectory(ObjectType::CobraMk1);
    bool haveCanonicalCobra = false;
    for (const auto& directory : discoverShipSourceDirectories(m_sourceAssetsRoot))
    {
        const bool runtime = !runtimeCobraDirectory.empty() &&
            lowerText(directory.generic_string()) == lowerText(runtimeCobraDirectory.generic_string());
        m_catalog.push_back({
            runtime ? "cobra_mk1" : catalogIdForShipFolder(directory),
            std::string("Cobra Mk.I — ") + directory.filename().string(),
            ObjectType::CobraMk1,
            directory,
            // Geometry is folder-authoritative even when the legacy runtime
            // assembly is still useful as a bootstrap for semantic identity.
            CatalogSourceAuthority::Folder,
            runtime ? CatalogBootstrapMode::RuntimeAssembly : CatalogBootstrapMode::Folder
        });
        haveCanonicalCobra = haveCanonicalCobra || runtime;
    }
    if (!haveCanonicalCobra)
    {
        const std::string runtimeLabel = runtimeCobraDirectory.empty()
            ? "Cobra Mk.I — runtime assembly"
            : std::string("Cobra Mk.I — ") + runtimeCobraDirectory.filename().string() + " [runtime]";
        m_catalog.push_back({
            "cobra_mk1", runtimeLabel, ObjectType::CobraMk1,
            runtimeCobraDirectory, CatalogSourceAuthority::RuntimeAssembly, CatalogBootstrapMode::RuntimeAssembly
        });
    }

    // Station SOURCE is filesystem-authoritative: every OBJ directly in
    // stations/LOD<N>/ is a normal mesh; variants/**/*.obj are alternatives.
    m_catalog.push_back({"station", "Orbital Station", ObjectType::Station, "stations", CatalogSourceAuthority::Folder, CatalogBootstrapMode::Folder});
    m_catalog.push_back({"repair_drone_debug", "Repair Drone", ObjectType::RepairDroneDebug, {}, CatalogSourceAuthority::RuntimeAssembly, CatalogBootstrapMode::RuntimeAssembly});
    m_catalog.push_back({"guidance_dock_cube", "Guidance Dock Cube", ObjectType::GuidanceDockCube, {}, CatalogSourceAuthority::RuntimeAssembly, CatalogBootstrapMode::RuntimeAssembly});
    m_catalog.push_back({"guidance_dock_cylinder", "Guidance Dock Cylinder", ObjectType::GuidanceDockCylinder, {}, CatalogSourceAuthority::RuntimeAssembly, CatalogBootstrapMode::RuntimeAssembly});
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
        const auto working = settings.value("workingFilesRoot", std::string());
        const auto locale = settings.value("locale", std::string("en"));
        if (!source.empty()) m_sourceAssetsRoot = std::filesystem::path(source);
        if (!compiled.empty()) m_compiledModelsRoot = std::filesystem::path(compiled);
        if (!working.empty()) m_workingFilesRoot = std::filesystem::path(working);
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
    const auto root = m_workingFilesRoot.empty()
        ? (m_sourceRoot / "build" / "tools" / "model_asset_editor" / "workspaces")
        : m_workingFilesRoot;
    return root / (m_selectedId.empty() ? std::string("_none") : m_selectedId);
}

std::filesystem::path ModelAssetEditorSession::workingAssetPath() const
{
    return wizardWorkspacePath() / "working" / (m_selectedId + ".elmodel");
}

std::filesystem::path ModelAssetEditorSession::workingEditorStatePath() const
{
    return wizardWorkspacePath() / "working" / "editor_state.json";
}

std::filesystem::path ModelAssetEditorSession::productionEditorStatePath() const
{
    return wizardWorkspacePath() / "production_state.json";
}

std::filesystem::path ModelAssetEditorSession::wizardLogPath(const std::string& fileName) const
{
    return wizardWorkspacePath() / "logs" / fileName;
}


std::filesystem::path ModelAssetEditorSession::selectedSourceAssetRoot() const
{
    const auto catalog = std::find_if(
        m_catalog.begin(), m_catalog.end(), [&](const CatalogEntry& entry) {
            return entry.id == m_selectedId;
        });
    if (catalog == m_catalog.end() || catalog->sourceAuthority != CatalogSourceAuthority::Folder)
        return {};
    const auto directory = m_loadedSourceAssetDirectory.empty()
        ? catalog->sourceDirectory : m_loadedSourceAssetDirectory;
    return resolveSourceFolderAssetRoot(m_sourceAssetsRoot, directory);
}

std::filesystem::path ModelAssetEditorSession::selectedSourceFilePath(
    const std::string& sourcePath,
    std::string* error) const
{
    if (error) error->clear();
    if (sourcePath.empty())
    {
        if (error) *error = "source path is empty";
        return {};
    }

    const auto catalog = std::find_if(
        m_catalog.begin(), m_catalog.end(), [&](const CatalogEntry& entry) {
            return entry.id == m_selectedId;
        });
    if (catalog == m_catalog.end() || catalog->sourceAuthority != CatalogSourceAuthority::Folder)
        return editorSourceFilePath(m_sourceAssetsRoot, sourcePath); // legacy runtime-registry assets only

    const auto directory = m_loadedSourceAssetDirectory.empty()
        ? catalog->sourceDirectory : m_loadedSourceAssetDirectory;
    const auto assetRoot = resolveSourceFolderAssetRoot(m_sourceAssetsRoot, directory);
    if (assetRoot.empty())
    {
        if (error) *error = "configured SOURCE asset root is unavailable";
        return {};
    }

    std::filesystem::path candidate(sourcePath);
    if (!candidate.is_absolute()) candidate = m_sourceAssetsRoot / candidate;
    candidate = candidate.lexically_normal();
    if (!pathIsWithin(candidate, assetRoot))
    {
        if (error) *error = "linked SOURCE path escapes the selected asset root: " + candidate.generic_string();
        return {};
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(candidate, ec) || ec)
    {
        if (error) *error = "linked SOURCE file does not exist: " + candidate.generic_string();
        return {};
    }
    return candidate;
}

ModelAssetEditorSession::EditorAuthoringState ModelAssetEditorSession::captureEditorAuthoringState() const
{
    EditorAuthoringState state;
    state.baseVisualIds = m_baseVisualIds;
    state.sourceExtraMeshIds = m_sourceExtraMeshIds;
    state.sourceVariantReplacements = m_sourceVariantReplacements;
    state.geometryTopologyClasses = m_geometryTopologyClasses;
    state.meshPreparationRecords = m_meshPreparationRecords;
    state.meshOrientationOverrides = m_meshOrientationOverrides;
    state.legacySourceVariantReplacements = m_legacySourceVariantReplacements;
    state.sourceMeshFingerprints = m_sourceMeshFingerprints;
    state.sourceMeshQuickStamps = m_sourceMeshQuickStamps;
    state.meshSourceRecords = m_meshSourceRecords;
    state.componentMaintenanceIssues = m_componentMaintenanceIssues;
    state.semanticChildOrder = m_semanticChildOrder;
    state.nextBaseVisualOrdinal = m_nextBaseVisualOrdinal;
    state.nextSourceVariantOrdinal = m_nextSourceVariantOrdinal;
    return state;
}

ModelAssetEditorSession::StageValidityState ModelAssetEditorSession::captureStageValidity() const
{
    StageValidityState state;
    for (const char* id : wizardStageOrder())
    {
        const auto it = m_wizardStages.find(id);
        state[id] = it == m_wizardStages.end() ? "not_started" : it->second.status;
    }
    return state;
}

void ModelAssetEditorSession::applyStageValidity(const StageValidityState& state)
{
    for (const char* id : wizardStageOrder())
    {
        auto& target = m_wizardStages[id];
        const auto it = state.find(id);
        const std::string status = it == state.end() ? "not_started" : it->second;
        target.status = status == "complete" || status == "stale" || status == "needs_fix" ? status : "not_started";
    }
}

nlohmann::json ModelAssetEditorSession::serializeStageValidity(const StageValidityState& state) const
{
    json stages = json::object();
    for (const char* id : wizardStageOrder())
    {
        const auto it = state.find(id);
        const std::string status = it == state.end() ? "not_started" : it->second;
        stages[id] = status == "complete" || status == "stale" || status == "needs_fix" ? status : "not_started";
    }
    return stages;
}

bool ModelAssetEditorSession::parseStageValidity(
    const nlohmann::json& state,
    StageValidityState& parsed,
    std::string* error) const
{
    try
    {
        StageValidityState next;
        const auto stages = state.value("stages", json::object());
        for (const char* id : wizardStageOrder())
        {
            const std::string status = stages.is_object()
                ? stages.value(id, std::string("not_started"))
                : std::string("not_started");
            if (status != "complete" && status != "stale" && status != "needs_fix" && status != "not_started")
            {
                if (error) *error = "invalid stage validity for " + std::string(id);
                return false;
            }
            next[id] = status;
        }
        parsed = std::move(next);
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        if (error) *error = ex.what();
        return false;
    }
}

void ModelAssetEditorSession::applyEditorAuthoringState(EditorAuthoringState state)
{
    m_baseVisualIds = std::move(state.baseVisualIds);
    m_sourceExtraMeshIds = std::move(state.sourceExtraMeshIds);
    m_sourceVariantReplacements = std::move(state.sourceVariantReplacements);
    m_geometryTopologyClasses = std::move(state.geometryTopologyClasses);
    m_meshPreparationRecords = std::move(state.meshPreparationRecords);
    m_meshOrientationOverrides = std::move(state.meshOrientationOverrides);
    m_legacySourceVariantReplacements = std::move(state.legacySourceVariantReplacements);
    m_sourceMeshFingerprints = std::move(state.sourceMeshFingerprints);
    m_sourceMeshQuickStamps = std::move(state.sourceMeshQuickStamps);
    m_meshSourceRecords = std::move(state.meshSourceRecords);
    // schema-13 records are canonical. Rebuild the legacy path->hash index so
    // older PREPARE/orientation code has one synchronized view instead of a
    // second persisted source of truth.
    for (const auto& [lodIndex, byGeometry] : m_meshSourceRecords)
        for (const auto& [geometryId, record] : byGeometry)
        {
            (void)geometryId;
            if (!record.sourcePath.empty() && record.sourceHash != 0)
                m_sourceMeshFingerprints[lodIndex][record.sourcePath] = record.sourceHash;
        }
    m_componentMaintenanceIssues = std::move(state.componentMaintenanceIssues);
    m_semanticChildOrder = std::move(state.semanticChildOrder);
    m_nextBaseVisualOrdinal = std::max<std::size_t>(1, state.nextBaseVisualOrdinal);
    m_nextSourceVariantOrdinal = std::max<std::size_t>(1, state.nextSourceVariantOrdinal);
    // RAW source snapshots are deliberately session-only. RESTORE reloads the saved
    // WORKING ASSET; raw source data is requested/rebuilt explicitly when needed.
    m_rawMeshSnapshots.clear();
}

nlohmann::json ModelAssetEditorSession::serializeEditorAuthoringState(const EditorAuthoringState& state) const
{
    json baseVisuals = json::array();
    for (const auto& [lodIndex, byGeometry] : state.baseVisualIds)
        for (const auto& [geometryId, id] : byGeometry)
            if (!geometryId.empty() && !id.empty())
                baseVisuals.push_back({{"lod", lodIndex}, {"geometryId", geometryId}, {"id", id}});

    json sourceExtraMeshes = json::array();
    for (const auto& [lodIndex, byPath] : state.sourceExtraMeshIds)
        for (const auto& [sourcePath, id] : byPath)
            if (!sourcePath.empty() && !id.empty())
                sourceExtraMeshes.push_back({{"lod", lodIndex}, {"sourcePath", sourcePath}, {"id", id}});

    json sourceVariantReplacements = json::array();
    for (const auto& [variantId, replaces] : state.sourceVariantReplacements)
        if (!variantId.empty() && !replaces.empty())
            sourceVariantReplacements.push_back({
                {"variantId", variantId},
                {"replacesBaseVisualIds", replaces}
            });

    json geometryTopologyClasses = json::array();
    for (const auto& [lodIndex, byGeometry] : state.geometryTopologyClasses)
        for (const auto& [geometryId, topologyClass] : byGeometry)
            if (!geometryId.empty() && !topologyClass.empty())
                geometryTopologyClasses.push_back({
                    {"lod", lodIndex}, {"geometryId", geometryId}, {"class", topologyClass}
                });

    json meshPreparationRecords = json::array();
    for (const auto& [lodIndex, byGeometry] : state.meshPreparationRecords)
        for (const auto& [geometryId, record] : byGeometry)
            if (!geometryId.empty() && !record.algorithm.empty() && record.outputFingerprint != 0)
                meshPreparationRecords.push_back({
                    {"lod", lodIndex}, {"geometryId", geometryId}, {"algorithm", record.algorithm},
                    {"sourceRenderVertices", record.sourceRenderVertices}, {"sourceTriangles", record.sourceTriangles},
                    {"geometricPoints", record.geometricPoints},
                    {"outputRenderVertices", record.outputRenderVertices}, {"outputTriangles", record.outputTriangles},
                    {"removedDegenerateTriangles", record.removedDegenerateTriangles},
                    {"removedDuplicateTriangles", record.removedDuplicateTriangles},
                    {"sourceNonManifoldEdges", record.sourceNonManifoldEdges},
                    {"normalIslands", record.normalIslands}, {"rebuiltEdges", record.rebuiltEdges},
                    {"splitTopologyVertices", record.splitTopologyVertices},
                    {"raycastPatches", record.raycastPatches},
                    {"raycastFlippedTriangles", record.raycastFlippedTriangles},
                    {"outputFingerprint", record.outputFingerprint}
                });

    json meshOrientationOverrides = json::array();
    for (const auto& [lodIndex, byGeometry] : state.meshOrientationOverrides)
        for (const auto& [geometryId, record] : byGeometry)
            if (!geometryId.empty() && record.mode == "flipped")
                meshOrientationOverrides.push_back({
                    {"lod", lodIndex}, {"geometryId", geometryId}, {"mode", record.mode},
                    {"sourceFingerprint", record.sourceFingerprint}
                });

    json legacySourceVariantReplacements = json::array();
    for (const auto& [lodIndex, byVariant] : state.legacySourceVariantReplacements)
        for (const auto& [variantId, replaces] : byVariant)
            if (!variantId.empty() && !replaces.empty())
                legacySourceVariantReplacements.push_back({
                    {"lod", lodIndex}, {"variantId", variantId}, {"replaces", replaces}
                });

    json sourceMeshFingerprints = json::array();
    for (const auto& [lodIndex, byPath] : state.sourceMeshFingerprints)
        for (const auto& [sourcePath, fingerprint] : byPath)
            if (!sourcePath.empty() && fingerprint != 0)
                sourceMeshFingerprints.push_back({
                    {"lod", lodIndex}, {"sourcePath", sourcePath}, {"fingerprint", fingerprint}
                });

    json sourceMeshQuickStamps = json::array();
    for (const auto& [lodIndex, byPath] : state.sourceMeshQuickStamps)
        for (const auto& [sourcePath, quickStamp] : byPath)
            if (!sourcePath.empty() && quickStamp != 0)
                sourceMeshQuickStamps.push_back({
                    {"lod", lodIndex}, {"sourcePath", sourcePath}, {"quickStamp", quickStamp}
                });

    json meshSourceRecords = json::array();
    for (const auto& [lodIndex, byGeometry] : state.meshSourceRecords)
        for (const auto& [geometryId, record] : byGeometry)
        {
            if (geometryId.empty() || record.sourcePath.empty()) continue;
            json checks = json::object();
            for (const char* stage : wizardStageOrder())
            {
                const auto it = record.stageChecks.find(stage);
                const std::string value = it == record.stageChecks.end() ? "not_checked" : it->second;
                checks[stage] = value == "passed" || value == "failed" ? value : "not_checked";
            }
            meshSourceRecords.push_back({
                {"lod", lodIndex}, {"geometryId", geometryId},
                {"sourceFileName", record.sourceFileName}, {"sourcePath", record.sourcePath},
                {"sourceHash", record.sourceHash}, {"sourceMissing", record.sourceMissing},
                {"stageChecks", std::move(checks)}
            });
        }

    json componentMaintenance = json::array();
    for (const auto& [componentId, issues] : state.componentMaintenanceIssues)
        if (!componentId.empty() && !issues.empty())
            componentMaintenance.push_back({
                {"componentId", componentId},
                {"issues", std::vector<std::string>(issues.begin(), issues.end())}
            });

    json semanticTreeOrder = json::object();
    for (const auto& [parentId, childIds] : state.semanticChildOrder)
        if (!parentId.empty() && !childIds.empty()) semanticTreeOrder[parentId] = childIds;

    return {
        {"nextBaseVisualOrdinal", state.nextBaseVisualOrdinal},
        {"nextSourceVariantOrdinal", state.nextSourceVariantOrdinal},
        {"baseVisuals", std::move(baseVisuals)},
        {"sourceExtraMeshes", std::move(sourceExtraMeshes)},
        {"sourceVariantReplacements", std::move(sourceVariantReplacements)},
        {"geometryTopologyClasses", std::move(geometryTopologyClasses)},
        {"meshPreparationRecords", std::move(meshPreparationRecords)},
        {"meshOrientationOverrides", std::move(meshOrientationOverrides)},
        {"legacySourceVariantReplacements", std::move(legacySourceVariantReplacements)},
        {"sourceMeshFingerprints", std::move(sourceMeshFingerprints)},
        {"sourceMeshQuickStamps", std::move(sourceMeshQuickStamps)},
        {"meshSourceRecords", std::move(meshSourceRecords)},
        {"componentMaintenance", std::move(componentMaintenance)},
        {"semanticTreeOrder", std::move(semanticTreeOrder)}
    };
}

bool ModelAssetEditorSession::parseEditorAuthoringState(
    const nlohmann::json& state,
    int schemaVersion,
    EditorAuthoringState& parsed,
    std::string* error) const
{
    try
    {
        EditorAuthoringState next;
        if (schemaVersion >= 3)
        {
            next.nextBaseVisualOrdinal = std::max<std::size_t>(
                1, state.value("nextBaseVisualOrdinal", std::size_t(1)));
            next.nextSourceVariantOrdinal = std::max<std::size_t>(
                1, state.value("nextSourceVariantOrdinal", std::size_t(1)));

            for (const auto& item : state.value("baseVisuals", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto geometryId = item.value("geometryId", std::string());
                const auto id = item.value("id", std::string());
                if (lodIndex == std::size_t(-1) || geometryId.empty() || id.empty()) continue;
                next.baseVisualIds[lodIndex][geometryId] = id;
            }
            for (const auto& item : state.value("sourceExtraMeshes", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto sourcePath = item.value("sourcePath", std::string());
                const auto id = item.value("id", std::string());
                if (lodIndex == std::size_t(-1) || sourcePath.empty() || id.empty()) continue;
                next.sourceExtraMeshIds[lodIndex][sourcePath] = id;
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
                if (!replaces.empty()) next.sourceVariantReplacements[variantId] = std::move(replaces);
            }
            if (schemaVersion >= 4)
            {
                for (const auto& item : state.value("geometryTopologyClasses", json::array()))
                {
                    if (!item.is_object()) continue;
                    const auto lodIndex = item.value("lod", std::size_t(-1));
                    const auto geometryId = item.value("geometryId", std::string());
                    const auto topologyClass = item.value("class", std::string());
                    if (lodIndex == std::size_t(-1) || geometryId.empty() || topologyClass.empty()) continue;
                    const auto topology = preflightTopologyClassFromName(topologyClass);
                    if (topology == PreflightTopologyClass::ClosedVolume ||
                        topology == PreflightTopologyClass::ThinOneSided ||
                        topology == PreflightTopologyClass::ThinTwoSided ||
                        topology == PreflightTopologyClass::BreachedVolume)
                        next.geometryTopologyClasses[lodIndex][geometryId] = topologyClass;
                }
            }
            if (schemaVersion >= 5)
            {
                for (const auto& item : state.value("meshPreparationRecords", json::array()))
                {
                    if (!item.is_object()) continue;
                    const auto lodIndex = item.value("lod", std::size_t(-1));
                    const auto geometryId = item.value("geometryId", std::string());
                    if (lodIndex == std::size_t(-1) || geometryId.empty()) continue;
                    MeshPreparationRecord record;
                    record.algorithm = item.value("algorithm", std::string());
                    record.sourceRenderVertices = item.value("sourceRenderVertices", std::size_t(0));
                    record.sourceTriangles = item.value("sourceTriangles", std::size_t(0));
                    record.geometricPoints = item.value("geometricPoints", std::size_t(0));
                    record.outputRenderVertices = item.value("outputRenderVertices", std::size_t(0));
                    record.outputTriangles = item.value("outputTriangles", std::size_t(0));
                    record.removedDegenerateTriangles = item.value("removedDegenerateTriangles", std::size_t(0));
                    record.removedDuplicateTriangles = item.value("removedDuplicateTriangles", std::size_t(0));
                    record.sourceNonManifoldEdges = item.value("sourceNonManifoldEdges", std::size_t(0));
                    record.normalIslands = item.value("normalIslands", std::size_t(0));
                    record.rebuiltEdges = item.value("rebuiltEdges", std::size_t(0));
                    record.splitTopologyVertices = item.value("splitTopologyVertices", std::size_t(0));
                    record.raycastPatches = item.value("raycastPatches", std::size_t(0));
                    record.raycastFlippedTriangles = item.value("raycastFlippedTriangles", std::size_t(0));
                    record.outputFingerprint = item.value("outputFingerprint", std::uint64_t(0));
                    if ((record.algorithm == CanonicalMeshAlgorithmId ||
                         record.algorithm == GeneratedLodComponentCullAlgorithmId) &&
                        record.outputFingerprint != 0)
                        next.meshPreparationRecords[lodIndex][geometryId] = std::move(record);
                }
            }
            // Whole-mesh orientation overrides are additive editor metadata.
            // They are intentionally valid in both current working/prod sidecar schemas.
            for (const auto& item : state.value("meshOrientationOverrides", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto geometryId = item.value("geometryId", std::string());
                const auto mode = item.value("mode", std::string());
                if (lodIndex == std::size_t(-1) || geometryId.empty() || mode != "flipped") continue;
                MeshOrientationOverrideRecord record;
                record.mode = mode;
                record.sourceFingerprint = item.value("sourceFingerprint", std::uint64_t(0));
                next.meshOrientationOverrides[lodIndex][geometryId] = std::move(record);
            }

            // Pending v0.9.5/v0.9.6 records can survive until their LOD is
            // resident and can be migrated without guessing.
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
                    next.legacySourceVariantReplacements[lodIndex][variantId] = std::move(replaces);
            }
            for (const auto& item : state.value("sourceMeshFingerprints", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto sourcePath = item.value("sourcePath", std::string());
                const auto fingerprint = item.value("fingerprint", std::uint64_t(0));
                if (lodIndex == std::size_t(-1) || sourcePath.empty() || fingerprint == 0) continue;
                next.sourceMeshFingerprints[lodIndex][sourcePath] = fingerprint;
            }
            for (const auto& item : state.value("sourceMeshQuickStamps", json::array()))
            {
                if (!item.is_object()) continue;
                const auto lodIndex = item.value("lod", std::size_t(-1));
                const auto sourcePath = item.value("sourcePath", std::string());
                const auto quickStamp = item.value("quickStamp", std::uint64_t(0));
                if (lodIndex == std::size_t(-1) || sourcePath.empty() || quickStamp == 0) continue;
                next.sourceMeshQuickStamps[lodIndex][sourcePath] = quickStamp;
            }
            if (schemaVersion >= 13)
            {
                for (const auto& item : state.value("meshSourceRecords", json::array()))
                {
                    if (!item.is_object()) continue;
                    const auto lodIndex = item.value("lod", std::size_t(-1));
                    const auto geometryId = item.value("geometryId", std::string());
                    if (lodIndex == std::size_t(-1) || geometryId.empty()) continue;
                    MeshSourceRecord record;
                    record.sourceFileName = item.value("sourceFileName", std::string());
                    record.sourcePath = item.value("sourcePath", std::string());
                    record.sourceHash = item.value("sourceHash", std::uint64_t(0));
                    record.sourceMissing = schemaVersion >= 14 && item.value("sourceMissing", false);
                    const auto checks = item.value("stageChecks", json::object());
                    for (const char* stage : wizardStageOrder())
                    {
                        const std::string value = checks.is_object()
                            ? checks.value(stage, std::string("not_checked"))
                            : std::string("not_checked");
                        record.stageChecks[stage] = value == "passed" || value == "failed"
                            ? value : "not_checked";
                    }
                    if (record.sourceFileName.empty() && !record.sourcePath.empty())
                        record.sourceFileName = std::filesystem::path(record.sourcePath).filename().string();
                    next.meshSourceRecords[lodIndex][geometryId] = std::move(record);
                }
            }
            for (const auto& item : state.value("componentMaintenance", json::array()))
            {
                if (!item.is_object()) continue;
                const auto componentId = item.value("componentId", std::string());
                if (componentId.empty()) continue;
                std::set<std::string> issues;
                for (const auto& value : item.value("issues", json::array()))
                    if (value.is_string() && !value.get<std::string>().empty())
                        issues.insert(value.get<std::string>());
                if (!issues.empty()) next.componentMaintenanceIssues[componentId] = std::move(issues);
            }
            const auto treeOrder = state.value("semanticTreeOrder", json::object());
            if (treeOrder.is_object())
                for (auto it = treeOrder.begin(); it != treeOrder.end(); ++it)
                {
                    if (!it.value().is_array() || it.key().empty()) continue;
                    std::vector<std::string> ids;
                    std::set<std::string> seenIds;
                    for (const auto& value : it.value())
                        if (value.is_string())
                        {
                            const auto id = value.get<std::string>();
                            if (!id.empty() && seenIds.insert(id).second) ids.push_back(id);
                        }
                    if (!ids.empty()) next.semanticChildOrder[it.key()] = std::move(ids);
                }

        }
        else if (schemaVersion >= 2)
        {
            // Legacy state stored filename-derived variants. Preserve it only as
            // migration input; no new semantic identity is invented here.
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
                    next.legacySourceVariantReplacements[lodIndex][variantId] = std::move(replaces);
            }
        }
        parsed = std::move(next);
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        if (error) *error = ex.what();
        return false;
    }
}

nlohmann::json ModelAssetEditorSession::packageStampFor(const std::filesystem::path& manifest) const
{
    json stamp = {
        {"assetId", m_selectedId},
        {"formatVersion", m_asset.formatVersion},
        {"members", json::array()}
    };
    const auto append = [&](const std::filesystem::path& path)
    {
        std::error_code ec;
        json member = {{"name", path.filename().generic_string()}};
        const bool exists = std::filesystem::exists(path, ec) && !ec;
        member["exists"] = exists;
        if (exists)
        {
            ec.clear();
            member["size"] = std::filesystem::file_size(path, ec);
            if (ec) member["size"] = 0;
            ec.clear();
            const auto time = std::filesystem::last_write_time(path, ec);
            member["mtime"] = ec ? 0 : static_cast<std::int64_t>(time.time_since_epoch().count());
        }
        stamp["members"].push_back(std::move(member));
    };
    append(manifest);
    for (std::size_t i = 0; i < m_asset.renderLods.size(); ++i)
        append(ModelAssetBinary::lodPayloadPath(manifest.string(), i));
    return stamp;
}

nlohmann::json ModelAssetEditorSession::productionPackageStamp() const
{
    return packageStampFor(compiledPath(m_selectedId));
}

bool ModelAssetEditorSession::productionPackageStampMatches(const nlohmann::json& expected) const
{
    return expected.is_object() && expected == productionPackageStamp();
}

bool ModelAssetEditorSession::writeWorkingEditorState(
    const std::string& savedAtUtc,
    std::uint64_t saveRevision,
    std::string* error) const
{
    try
    {
        std::filesystem::create_directories(workingEditorStatePath().parent_path());
        json state = serializeEditorAuthoringState(captureEditorAuthoringState());
        state["schemaVersion"] = 15;
        state["snapshotKind"] = "model_asset_editor_working_state";
        state["assetId"] = m_selectedId;
        state["editorVersion"] = ModelAssetEditorVersion;
        state["saveRevision"] = saveRevision;
        state["savedAtUtc"] = savedAtUtc;
        state["sourceAssetDirectory"] = m_loadedSourceAssetDirectory.generic_string();
        state["sourceAssetRoot"] = selectedSourceAssetRoot().generic_string();
        state["stages"] = serializeStageValidity(captureStageValidity());
        state["aggregateStageChecks"] = aggregateStageChecksJson();
        state["physicalScaleGraph"] = {
            {"enabled", m_asset.physicalSize.enabled},
            {"axis", physicalSizeAxisName(m_asset.physicalSize.axis)},
            {"sourceExtent", m_asset.physicalSize.sourceExtent},
            {"targetMeters", m_asset.physicalSize.targetMeters},
            {"sourceToMeters", m_asset.physicalSize.sourceToMeters},
            {"geometrySpace", physicalGeometrySpaceName(m_asset.physicalSize.geometrySpace)},
            {"gameLinked", m_asset.physicalSize.gameLinked},
            {"gameDimensionsMeters", vec3Json(m_asset.physicalSize.gameDimensionsMeters)}
        };
        state["packageStamp"] = packageStampFor(workingAssetPath());
        const auto path = workingEditorStatePath();
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        {
            if (error) *error = "cannot open " + path.generic_string();
            return false;
        }
        out << std::setw(2) << state << '\n';
        if (!out)
        {
            if (error) *error = "cannot write " + path.generic_string();
            return false;
        }
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        if (error) *error = ex.what();
        return false;
    }
}

bool ModelAssetEditorSession::loadWorkingEditorState(
    EditorAuthoringState& state,
    StageValidityState& validity,
    std::string* savedAtUtc,
    std::uint64_t* saveRevision,
    std::filesystem::path* sourceAssetDirectory,
    std::string* error) const
{
    const auto path = workingEditorStatePath();
    std::ifstream in(path);
    if (!in)
    {
        if (error) *error = "working editor_state.json does not exist";
        return false;
    }
    try
    {
        json snapshot;
        in >> snapshot;
        const int schemaVersion = snapshot.value("schemaVersion", 0);
        if ((schemaVersion != 11 && schemaVersion != 12 && schemaVersion != 13 && schemaVersion != 14 && schemaVersion != 15) ||
            snapshot.value("snapshotKind", std::string()) != "model_asset_editor_working_state")
        {
            if (error) *error = "unsupported working editor-state schema";
            return false;
        }
        if (snapshot.value("assetId", std::string()) != m_selectedId)
        {
            if (error) *error = "working editor-state asset id does not match selected asset";
            return false;
        }
        if (snapshot.value("packageStamp", json::object()) != packageStampFor(workingAssetPath()))
        {
            if (error) *error = "working editor-state belongs to different working package bytes";
            return false;
        }
        if (!parseEditorAuthoringState(snapshot, schemaVersion, state, error)) return false;
        if (!parseStageValidity(snapshot, validity, error)) return false;
        if (savedAtUtc) *savedAtUtc = snapshot.value("savedAtUtc", std::string());
        if (saveRevision) *saveRevision = snapshot.value("saveRevision", std::uint64_t(0));
        if (sourceAssetDirectory)
            *sourceAssetDirectory = std::filesystem::path(snapshot.value("sourceAssetDirectory", std::string()));
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        if (error) *error = ex.what();
        return false;
    }
}

bool ModelAssetEditorSession::writeProductionEditorState(std::string* error) const
{
    try
    {
        std::filesystem::create_directories(wizardWorkspacePath());
        json state = serializeEditorAuthoringState(captureEditorAuthoringState());
        state["schemaVersion"] = 15;
        state["snapshotKind"] = "model_asset_editor_production_state";
        state["assetId"] = m_selectedId;
        state["editorVersion"] = ModelAssetEditorVersion;
        state["saveRevision"] = m_workingSaveRevision;
        state["savedAtUtc"] = m_workingSavedAtUtc;
        state["sourceAssetDirectory"] = m_loadedSourceAssetDirectory.generic_string();
        state["sourceAssetRoot"] = selectedSourceAssetRoot().generic_string();
        state["stages"] = serializeStageValidity(captureStageValidity());
        state["aggregateStageChecks"] = aggregateStageChecksJson();
        state["physicalScaleGraph"] = {
            {"enabled", m_asset.physicalSize.enabled},
            {"axis", physicalSizeAxisName(m_asset.physicalSize.axis)},
            {"sourceExtent", m_asset.physicalSize.sourceExtent},
            {"targetMeters", m_asset.physicalSize.targetMeters},
            {"sourceToMeters", m_asset.physicalSize.sourceToMeters},
            {"geometrySpace", "meters"},
            {"gameLinked", m_asset.physicalSize.gameLinked},
            {"gameDimensionsMeters", vec3Json(m_asset.physicalSize.gameDimensionsMeters)}
        };
        state["packageStamp"] = productionPackageStamp();
        const auto path = productionEditorStatePath();
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        {
            if (error) *error = "cannot open " + path.generic_string();
            return false;
        }
        out << std::setw(2) << state << '\n';
        if (!out)
        {
            if (error) *error = "cannot write " + path.generic_string();
            return false;
        }
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        if (error) *error = ex.what();
        return false;
    }
}

bool ModelAssetEditorSession::loadProductionEditorState(
    EditorAuthoringState& state,
    StageValidityState& validity,
    std::uint64_t* saveRevision,
    std::filesystem::path* sourceAssetDirectory,
    std::string* error) const
{
    const auto path = productionEditorStatePath();
    std::ifstream in(path);
    if (!in)
    {
        if (error) *error = "production_state.json does not exist";
        return false;
    }
    try
    {
        json snapshot;
        in >> snapshot;
        const int schemaVersion = snapshot.value("schemaVersion", 0);
        if ((schemaVersion != 8 && schemaVersion != 13 && schemaVersion != 14 && schemaVersion != 15) ||
            snapshot.value("snapshotKind", std::string()) != "model_asset_editor_production_state")
        {
            if (error) *error = "unsupported production editor-state schema";
            return false;
        }
        if (snapshot.value("assetId", std::string()) != m_selectedId)
        {
            if (error) *error = "production editor-state asset id does not match selected asset";
            return false;
        }
        if (!productionPackageStampMatches(snapshot.value("packageStamp", json::object())))
        {
            if (error) *error = "production editor-state belongs to different package bytes";
            return false;
        }
        if (!parseEditorAuthoringState(snapshot, schemaVersion, state, error)) return false;
        if (!parseStageValidity(snapshot, validity, error)) return false;
        if (saveRevision) *saveRevision = snapshot.value("saveRevision", std::uint64_t(0));
        if (sourceAssetDirectory)
            *sourceAssetDirectory = std::filesystem::path(snapshot.value("sourceAssetDirectory", std::string()));
        if (error) error->clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        if (error) *error = ex.what();
        return false;
    }
}

void ModelAssetEditorSession::loadWizardState()
{
    // Stage validity belongs only to the current in-memory/WORKING editor state.
    // There is no separate wizard-state authority; stage validity is part of the saved WORKING ASSET.
    m_wizardStages.clear();
    applyEditorAuthoringState(EditorAuthoringState{});
    for (const char* id : wizardStageOrder())
        m_wizardStages.emplace(id, WizardStageState{});
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
                // Legacy editor states encoded a filename stem in geometry.id. Do not
                // preserve that as semantic identity: assign
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

    (void)changed; // authoring registry persists only when the user presses SAVE
}

void ModelAssetEditorSession::invalidateWizardFrom(const std::string& stage)
{
    const auto first = wizardStageIndex(stage);
    const auto& order = wizardStageOrder();
    if (first >= order.size()) return;

    bool changed = false;
    for (std::size_t i = first; i < order.size(); ++i)
    {
        auto& value = m_wizardStages[order[i]];
        const std::string previous = value.status;
        if (value.status == "complete" || value.status == "stale") value.status = "stale";
        else if (value.status == "needs_fix") value.status = "needs_fix";
        else value.status = "not_started";
        changed = changed || value.status != previous;
    }
    if (changed) markEditorStateDirty();
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
        const bool implemented = true;
        const bool previousComplete = i == 0 ||
            (m_wizardStages.count(order[i - 1]) && m_wizardStages.at(order[i - 1]).status == "complete");
        const bool maintenanceMode = !m_selectedId.empty() && std::filesystem::exists(compiledPath(m_selectedId));
        stages.push_back({
            {"id", id}, {"index", i}, {"status", value.status},
            {"implemented", implemented}, {"unlocked", implemented && (maintenanceMode || previousComplete)}
        });
    }
    return {
        {"workspacePath", wizardWorkspacePath().generic_string()},
        {"workingAssetPath", workingAssetPath().generic_string()},
        {"workingEditorStatePath", workingEditorStatePath().generic_string()},
        {"maintenanceMode", !m_selectedId.empty() && std::filesystem::exists(compiledPath(m_selectedId))},
        {"stages", std::move(stages)}
    };
}

bool ModelAssetEditorSession::validateWizardStage(const std::string& stage, std::string* error)
{
    const auto fail = [&](const std::string& message)
    {
        if (error) *error = message;
        return false;
    };

    if (stage == "source")
    {
        std::size_t sourceBacked = 0;
        std::size_t missing = 0;
        for (const auto& lod : m_asset.renderLods)
            for (const auto& geometry : lod.geometries)
                if (!geometry.sourcePath.empty())
                {
                    ++sourceBacked;
                    if (!std::filesystem::exists(selectedSourceFilePath(geometry.sourcePath))) ++missing;
                }
        if (sourceBacked == 0 || missing != 0)
            return fail("SOURCE validation failed: source meshes=" + std::to_string(sourceBacked) +
                ", missing=" + std::to_string(missing));
    }
    else if (stage == "lods")
    {
        std::string canonicalReason;
        if (!verifyLoadedWorkingSetCanonical(&canonicalReason))
            return fail("LODS validation failed: prepare meshes first: " + canonicalReason);
        if (m_asset.renderLods.empty())
            return fail("LOD validation failed: asset has no render LOD documents");
        for (const auto& lod : m_asset.renderLods)
            if (lod.nodes.empty() || lod.geometries.empty())
                return fail("LOD validation failed: LOD" + std::to_string(lod.level) + " has an empty render graph");
        // Saved payloads are not part of current authoring validity. A source
        // reimport may intentionally declare a different LOD set (for example,
        // only a fresh LOD0 before regenerated LODs are authored). Stale members
        // in WORKING are pruned by SAVE; stale production
        // members remain until BUILD. They are diagnostics, never LODS blockers.
        // LODS owns current render-document readiness, not saved package debris
        // and not SURFACES semantics.
    }
    else if (stage == "geometry")
    {
        for (const auto& lod : m_asset.renderLods)
            for (const auto& node : lod.nodes)
                if (node.geometryIndex >= static_cast<std::int32_t>(lod.geometries.size()))
                    return fail("GEOMETRY validation failed: invalid render-node geometry binding");
    }
    else if (stage == "surfaces")
    {
        for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
        {
            const auto& lod = m_asset.renderLods[li];
            std::vector<std::size_t> usage(lod.geometries.size(), 0);
            for (const auto& node : lod.nodes)
                if (node.enabled && node.geometryIndex >= 0 &&
                    static_cast<std::size_t>(node.geometryIndex) < usage.size())
                    ++usage[static_cast<std::size_t>(node.geometryIndex)];

            for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
            {
                const auto& geometry = lod.geometries[gi];
                const bool relevant = usage[gi] != 0 || isRenderVariantGeometryId(geometry.id);
                if (!relevant) continue;

                std::string explicitClass;
                const auto classLodIt = m_geometryTopologyClasses.find(li);
                if (classLodIt != m_geometryTopologyClasses.end())
                {
                    const auto classIt = classLodIt->second.find(geometry.id);
                    if (classIt != classLodIt->second.end()) explicitClass = classIt->second;
                }
                const auto explicitParsed = preflightTopologyClassFromName(explicitClass);
                bool autoClosed = false;
                if (explicitParsed == PreflightTopologyClass::Auto)
                {
                    // AUTO is the only SURFACES validation path that needs
                    // topology evidence. Explicit author intent is metadata and
                    // must not pay for an audit merely to validate unrelated metadata.
                    const auto audit = auditPreflightGeometry(geometry.mesh);
                    autoClosed = audit.suggestedClass == PreflightTopologyClass::ClosedVolume;
                    if (audit.openComponents != 0)
                        return fail("SURFACES validation failed: LOD" + std::to_string(li) + " G" +
                            std::to_string(gi) + " " + geometry.id +
                            " is open and needs an explicit surface intent");
                }
                const auto effective = autoClosed ? PreflightTopologyClass::ClosedVolume : explicitParsed;
                const bool validIntent = effective == PreflightTopologyClass::ClosedVolume ||
                    effective == PreflightTopologyClass::ThinOneSided ||
                    effective == PreflightTopologyClass::ThinTwoSided ||
                    effective == PreflightTopologyClass::BreachedVolume;
                if (!validIntent)
                    return fail("SURFACES validation failed: LOD" + std::to_string(li) + " G" +
                        std::to_string(gi) + " " + geometry.id + " has no valid surface intent");
                const SurfaceMode expectedMode = effective == PreflightTopologyClass::ThinOneSided
                    ? SurfaceMode::ThinOneSided
                    : effective == PreflightTopologyClass::ThinTwoSided
                        ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
                if (geometry.surfaceMode != expectedMode)
                    return fail("SURFACES validation failed: LOD" + std::to_string(li) + " G" +
                        std::to_string(gi) + " surface mode does not match authored intent");
                for (const auto& triangle : geometry.mesh.triangles)
                {
                    // NoIndex is the implicit default visual surface. Most triangles
                    // do not need an explicit material merely to render in the game's
                    // style families; explicit materials are sparse overrides for
                    // colour/emission/other authored visual roles.
                    if (triangle.materialIndex == NoIndex)
                        continue;
                    if (triangle.materialIndex < 0 ||
                        static_cast<std::size_t>(triangle.materialIndex) >= m_asset.materials.size())
                        return fail("SURFACES validation failed: LOD" + std::to_string(li) + " G" +
                            std::to_string(gi) + " references an invalid explicit material index");
                }
            }
        }
        for (std::size_t mi = 0; mi < m_asset.materials.size(); ++mi)
        {
            const auto& material = m_asset.materials[mi];
            if (material.id.empty())
                return fail("SURFACES validation failed: material M" + std::to_string(mi) + " has an empty id");
            if (!std::isfinite(material.emissiveStrength) || material.emissiveStrength < 0.0f ||
                !std::isfinite(material.metallic) || material.metallic < 0.0f || material.metallic > 1.0f ||
                !std::isfinite(material.roughness) || material.roughness < 0.0f || material.roughness > 1.0f)
                return fail("SURFACES validation failed: material " + material.id + " has invalid PBR values");
        }
    }
    else if (stage == "semantics")
    {
        if (m_asset.nodes.empty())
            return fail("SEMANTICS validation failed: asset has no semantic nodes");
        // Semantic transforms form a forest in asset space. Multiple top-level
        // parts are valid: parentIndex == NoIndex means the part is authored
        // directly in the asset coordinate frame. A unique semantic ROOT is
        // neither required nor inferred as structural/lifecycle authority.
        std::vector<std::string> semanticEntryProblems;
        std::vector<std::string> orphanIds;
        for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
            if (inspectSemanticNodeUsage(m_asset, ni).isOrphanCandidate()) orphanIds.push_back(m_asset.nodes[ni].id);
        if (!orphanIds.empty())
        {
            std::string names;
            constexpr std::size_t MaxNamedOrphans = 8;
            for (std::size_t i = 0; i < std::min(orphanIds.size(), MaxNamedOrphans); ++i)
            {
                if (!names.empty()) names += ", ";
                names += orphanIds[i];
            }
            if (orphanIds.size() > MaxNamedOrphans) names += ", ...";
            semanticEntryProblems.push_back("orphan semantic parts=" + std::to_string(orphanIds.size()) + " [" + names +
                "]; delete them or give them visual/children/gameplay ownership");
        }
        if (!semanticEntryProblems.empty())
        {
            std::string message = "SEMANTICS validation failed: ";
            for (std::size_t i = 0; i < semanticEntryProblems.size(); ++i)
            {
                if (i) message += "; ";
                message += semanticEntryProblems[i];
            }
            return fail(message);
        }

        std::set<std::string> ids;
        for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
        {
            const auto& node = m_asset.nodes[ni];
            if (node.id.empty())
                return fail("SEMANTICS validation failed: semantic node N" + std::to_string(ni) + " has an empty id");
            if (!ids.insert(node.id).second)
                return fail("SEMANTICS validation failed: duplicate semantic node id " + node.id);
            if (node.parentIndex >= static_cast<std::int32_t>(m_asset.nodes.size()) ||
                node.parentIndex == static_cast<std::int32_t>(ni))
                return fail("SEMANTICS validation failed: invalid parent for semantic node " + node.id);

            std::set<std::size_t> chain;
            std::int32_t parent = node.parentIndex;
            while (parent >= 0)
            {
                const auto pi = static_cast<std::size_t>(parent);
                if (pi >= m_asset.nodes.size() || !chain.insert(pi).second)
                    return fail("SEMANTICS validation failed: semantic hierarchy cycle at " + node.id);
                parent = m_asset.nodes[pi].parentIndex;
            }
        }

        for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
        {
            const auto& lod = m_asset.renderLods[li];
            for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
            {
                const auto& renderNode = lod.nodes[ri];
                if (!renderNode.enabled || renderNode.geometryIndex < 0) continue;
                if (renderNode.semanticNodeIndex < 0)
                    return fail("SEMANTICS validation failed: LOD" + std::to_string(li) + " render node " +
                        renderNode.id + " has geometry but no semantic binding");
                if (renderNode.semanticNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                    return fail("SEMANTICS validation failed: LOD" + std::to_string(li) + " render node " +
                        renderNode.id + " references an invalid semantic node");
            }
        }
        const auto finiteVec3 = [](const glm::vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        for (const auto& node : m_asset.nodes)
        {
            const auto& joint = node.joint;
            if (!finiteVec3(joint.pivot) || !finiteVec3(joint.axis) ||
                !std::isfinite(joint.defaultRateDegPerSec) || !std::isfinite(joint.minAngleDeg) ||
                !std::isfinite(joint.maxAngleDeg) || joint.minAngleDeg > joint.maxAngleDeg ||
                !std::isfinite(joint.breakForceN) || !std::isfinite(joint.breakTorqueNm) ||
                joint.breakForceN < 0.0f || joint.breakTorqueNm < 0.0f)
                return fail("SEMANTICS validation failed: joint on " + node.id + " contains invalid data");
            if (joint.type == JointType::Revolute && glm::dot(joint.axis, joint.axis) <= 1.0e-8f)
                return fail("SEMANTICS validation failed: revolute joint on " + node.id + " has a zero axis");
        }

        std::set<std::string> socketIds;
        for (std::size_t si = 0; si < m_asset.sockets.size(); ++si)
        {
            const auto& socket = m_asset.sockets[si];
            if (socket.id.empty() || socket.kind.empty())
                return fail("SEMANTICS validation failed: socket S" + std::to_string(si) + " needs id and kind");
            if (!socketIds.insert(socket.id).second)
                return fail("SEMANTICS validation failed: duplicate socket id " + socket.id);
            if (socket.parentNodeIndex < NoIndex ||
                socket.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                return fail("SEMANTICS validation failed: socket " + socket.id + " has an invalid parent");
            if (!finiteVec3(socket.localPosition) || !finiteVec3(socket.localRotationDeg) ||
                !finiteVec3(socket.extent) || socket.extent.x < 0.0f || socket.extent.y < 0.0f || socket.extent.z < 0.0f)
                return fail("SEMANTICS validation failed: socket " + socket.id + " contains invalid transform/extent data");
            const auto& light = socket.light;
            if (!finiteVec3(light.color) || !std::isfinite(light.intensity) || !std::isfinite(light.rangeMeters) ||
                !std::isfinite(light.outerConeDeg) || light.intensity < 0.0f || light.rangeMeters < 0.0f ||
                light.outerConeDeg <= 0.0f || light.outerConeDeg > 180.0f)
                return fail("SEMANTICS validation failed: socket " + socket.id + " has invalid light data");
        }
    }
    else if (stage == "physics")
    {
        const auto finiteVec3 = [](const glm::vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        std::set<std::string> collisionIds;
        for (std::size_t ci = 0; ci < m_asset.collisionVolumes.size(); ++ci)
        {
            const auto& collision = m_asset.collisionVolumes[ci];
            if (collision.id.empty())
                return fail("PHYSICS validation failed: collision C" + std::to_string(ci) + " has an empty id");
            if (!collisionIds.insert(collision.id).second)
                return fail("PHYSICS validation failed: duplicate collision id " + collision.id);
            if (collision.parentNodeIndex < NoIndex ||
                collision.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                return fail("PHYSICS validation failed: collision " + collision.id + " has an invalid parent");
            if (!finiteVec3(collision.localPosition) || !finiteVec3(collision.localRotationDeg) ||
                !finiteVec3(collision.halfSize) || !std::isfinite(collision.radius) ||
                !std::isfinite(collision.halfHeight))
                return fail("PHYSICS validation failed: collision " + collision.id + " contains non-finite data");
            if (collision.shape == CollisionShape::Box &&
                (collision.halfSize.x <= 0.0f || collision.halfSize.y <= 0.0f || collision.halfSize.z <= 0.0f))
                return fail("PHYSICS validation failed: box collision " + collision.id + " has non-positive half size");
            if (collision.shape == CollisionShape::Sphere && collision.radius <= 0.0f)
                return fail("PHYSICS validation failed: sphere collision " + collision.id + " has non-positive radius");
            if (collision.shape == CollisionShape::Capsule &&
                (collision.radius <= 0.0f || collision.halfHeight < 0.0f))
                return fail("PHYSICS validation failed: capsule collision " + collision.id + " has invalid dimensions");
        }

        for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
        {
            const auto& node = m_asset.nodes[ni];
            const auto& physics = node.physics;
            if (!std::isfinite(physics.densityKgM3) || !std::isfinite(physics.massKg) ||
                !finiteVec3(physics.centerOfMass) || !finiteVec3(physics.inertiaDiagonal) ||
                !finiteVec3(physics.inertiaProducts) || physics.densityKgM3 < 0.0f || physics.massKg < 0.0f ||
                physics.inertiaDiagonal.x < 0.0f || physics.inertiaDiagonal.y < 0.0f || physics.inertiaDiagonal.z < 0.0f)
                return fail("PHYSICS validation failed: rigid body " + node.id + " contains invalid mass/inertia data");
            if (physics.mode == MassPropertyMode::Disabled) continue;
            if (physics.densityKgM3 <= 0.0f)
                return fail("PHYSICS validation failed: rigid body " + node.id + " has non-positive density");
            if (physics.massKg <= 0.0f)
                return fail("PHYSICS validation failed: rigid body " + node.id + " has non-positive resolved mass");
            if (physics.mode == MassPropertyMode::AutoFromCollision)
            {
                const bool hasCollision = std::any_of(
                    m_asset.collisionVolumes.begin(), m_asset.collisionVolumes.end(),
                    [ni](const CollisionVolume& collision) {
                        return collision.enabled && collision.parentNodeIndex == static_cast<std::int32_t>(ni);
                    });
                if (!hasCollision)
                    return fail("PHYSICS validation failed: auto rigid body " + node.id + " has no enabled collision volume");
            }
        }
    }
    else if (stage == "damage")
    {
        const auto finiteVec3 = [](const glm::vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        std::set<std::pair<std::int32_t, std::string>> stateIds;
        for (std::size_t vi = 0; vi < m_asset.stateVariants.size(); ++vi)
        {
            const auto& variant = m_asset.stateVariants[vi];
            if (variant.nodeIndex < 0 || variant.nodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size()) ||
                variant.id.empty() || variant.id == "intact")
                return fail("DAMAGE validation failed: invalid state variant V" + std::to_string(vi));
            if (!stateIds.emplace(variant.nodeIndex, variant.id).second)
                return fail("DAMAGE validation failed: duplicate state variant " + variant.id);
            if (variant.transformOverride &&
                (!finiteVec3(variant.localPosition) || !finiteVec3(variant.localRotationDeg) || !finiteVec3(variant.pivot)))
                return fail("DAMAGE validation failed: state " + variant.id + " has invalid transform override");
            if (variant.physicsOverride)
            {
                const auto& physics = variant.physics;
                if (!std::isfinite(physics.densityKgM3) || !std::isfinite(physics.massKg) ||
                    !finiteVec3(physics.centerOfMass) || !finiteVec3(physics.inertiaDiagonal) ||
                    !finiteVec3(physics.inertiaProducts) || physics.densityKgM3 < 0.0f || physics.massKg < 0.0f ||
                    physics.inertiaDiagonal.x < 0.0f || physics.inertiaDiagonal.y < 0.0f || physics.inertiaDiagonal.z < 0.0f)
                    return fail("DAMAGE validation failed: state " + variant.id + " has invalid physics override");
            }
        }
        const auto stateDeclared = [&](std::int32_t nodeIndex, const std::string& stateId) {
            return stateId == "intact" || stateIds.count({nodeIndex, stateId}) != 0;
        };
        for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
            if (!stateDeclared(static_cast<std::int32_t>(ni), m_asset.nodes[ni].defaultStateId))
                return fail("DAMAGE validation failed: default state is not declared for " + m_asset.nodes[ni].id);

        const auto validateScope = [&](std::int32_t parent, const std::vector<std::string>& states, const std::string& label) {
            if (parent < NoIndex || parent >= static_cast<std::int32_t>(m_asset.nodes.size()))
                return fail("DAMAGE validation failed: " + label + " has an invalid semantic parent");
            if (!states.empty() && parent == NoIndex)
                return fail("DAMAGE validation failed: " + label + " is state-scoped but has no semantic parent");
            for (const auto& stateId : states)
                if (!stateDeclared(parent, stateId))
                    return fail("DAMAGE validation failed: " + label + " references undeclared state " + stateId);
            return true;
        };

        for (const auto& lod : m_asset.renderLods)
            for (const auto& renderNode : lod.nodes)
            {
                if (renderNode.activeStates.empty()) continue;
                if (!validateScope(renderNode.semanticNodeIndex, renderNode.activeStates,
                    "LOD" + std::to_string(lod.level) + " render node " + renderNode.id)) return false;
            }
        for (const auto& collision : m_asset.collisionVolumes)
            if (!validateScope(collision.parentNodeIndex, collision.activeStates, "collision " + collision.id)) return false;
        for (const auto& socket : m_asset.sockets)
            if (!validateScope(socket.parentNodeIndex, socket.activeStates, "socket " + socket.id)) return false;

        std::set<std::string> hitIds, openingIds, repairIds;
        for (const auto& hit : m_asset.hitRegions)
        {
            if (hit.id.empty() || !hitIds.insert(hit.id).second)
                return fail("DAMAGE validation failed: hit-region ids must be non-empty and unique");
            if (!validateScope(hit.parentNodeIndex, hit.activeStates, "hit region " + hit.id)) return false;
            if (!finiteVec3(hit.localPosition) || !finiteVec3(hit.localRotationDeg) || !finiteVec3(hit.halfSize) ||
                hit.halfSize.x <= 0.0f || hit.halfSize.y <= 0.0f || hit.halfSize.z <= 0.0f)
                return fail("DAMAGE validation failed: hit region " + hit.id + " has invalid volume data");
        }
        for (const auto& opening : m_asset.openings)
        {
            if (opening.id.empty() || !openingIds.insert(opening.id).second)
                return fail("DAMAGE validation failed: opening ids must be non-empty and unique");
            if (!validateScope(opening.parentNodeIndex, opening.activeStates, "opening " + opening.id)) return false;
            if (!finiteVec3(opening.localPosition) || !finiteVec3(opening.localRotationDeg) || !finiteVec3(opening.halfSize) ||
                opening.halfSize.x <= 0.0f || opening.halfSize.y <= 0.0f || opening.halfSize.z <= 0.0f)
                return fail("DAMAGE validation failed: opening " + opening.id + " has invalid volume data");
        }
        for (const auto& repair : m_asset.repairTargets)
        {
            if (repair.id.empty() || !repairIds.insert(repair.id).second)
                return fail("DAMAGE validation failed: repair-target ids must be non-empty and unique");
            if (!validateScope(repair.parentNodeIndex, repair.activeStates, "repair target " + repair.id)) return false;
            if (!finiteVec3(repair.localPosition) || !finiteVec3(repair.localRotationDeg))
                return fail("DAMAGE validation failed: repair target " + repair.id + " has invalid transform data");
            if (!repair.repairedStateId.empty() && !stateDeclared(repair.parentNodeIndex, repair.repairedStateId))
                return fail("DAMAGE validation failed: repair target " + repair.id + " references undeclared repaired state " + repair.repairedStateId);
        }

        std::string binaryError;
        if (!ModelAssetBinary::validate(m_asset, &binaryError))
            return fail("DAMAGE validation failed: shared v4 contract: " + binaryError);
    }
    else if (stage == "validate" || stage == "build")
    {
        if (!m_asset.physicalSize.enabled ||
            m_asset.physicalSize.geometrySpace != PhysicalGeometrySpace::Authoring ||
            !std::isfinite(m_asset.physicalSize.sourceToMeters) || m_asset.physicalSize.sourceToMeters <= 0.0f)
            return fail(std::string(stage == "build" ? "BUILD" : "VALIDATE") +
                " requires an explicit SOURCE/WORKING authoring-space -> meter calibration");
        if (!m_componentMaintenanceIssues.empty())
        {
            const auto& first = *m_componentMaintenanceIssues.begin();
            std::ostringstream issues;
            for (const auto& issue : first.second)
            {
                if (issues.tellp() > 0) issues << ", ";
                issues << issue;
            }
            return fail(std::string(stage == "build" ? "BUILD" : "VALIDATE") +
                " blocked by pending maintenance for " + first.first + ": " + issues.str() +
                " (" + std::to_string(m_componentMaintenanceIssues.size()) + " component(s) pending)");
        }
        static const std::array<const char*, 7> productionStages {
            "source", "lods", "geometry", "surfaces", "semantics", "physics", "damage"
        };
        for (const char* upstream : productionStages)
        {
            std::string upstreamError;
            if (!validateWizardStage(upstream, &upstreamError))
                return fail(std::string(stage == "build" ? "BUILD" : "VALIDATE") +
                    " failed at " + upstream + ": " + upstreamError);
        }
        std::string binaryError;
        if (!ModelAssetBinary::validate(m_asset, &binaryError))
            return fail(std::string(stage == "build" ? "BUILD" : "VALIDATE") +
                " failed: shared v4 contract: " + binaryError);
    }

    if (error) error->clear();
    return true;
}

void ModelAssetEditorSession::sendWizardValidationReport()
{
    if (m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return;
    }
    if (!ensureAllLodsLoaded()) return;

    static const std::array<const char*, 7> productionStages {
        "source", "lods", "geometry", "surfaces", "semantics", "physics", "damage"
    };
    json rows = json::array();
    bool passed = true;
    for (const char* stage : productionStages)
    {
        std::string stageError;
        const bool stagePassed = validateWizardStage(stage, &stageError);
        passed = passed && stagePassed;
        rows.push_back({
            {"stage", stage},
            {"passed", stagePassed},
            {"message", stagePassed ? std::string("OK") : stageError}
        });
    }
    const bool maintenancePassed = m_componentMaintenanceIssues.empty();
    passed = passed && maintenancePassed;
    rows.push_back({
        {"stage", "maintenance"},
        {"passed", maintenancePassed},
        {"message", maintenancePassed
            ? std::string("OK")
            : std::to_string(m_componentMaintenanceIssues.size()) + " component(s) have pending local maintenance"}
    });

    std::string binaryError;
    const bool binaryPassed = ModelAssetBinary::validate(m_asset, &binaryError);
    passed = passed && binaryPassed;
    rows.push_back({
        {"stage", "v4_contract"},
        {"passed", binaryPassed},
        {"message", binaryPassed ? std::string("OK") : binaryError}
    });
    m_server.broadcastText(json({
        {"type", "wizard_validation_report"},
        {"passed", passed},
        {"rows", std::move(rows)}
    }).dump());
    sendStatus(passed ? "Full wizard validation passed" : "Full wizard validation found blockers", !passed);
}

bool ModelAssetEditorSession::checkWizardStage(const std::string& stage)
{
    const auto stageIndex = wizardStageIndex(stage);
    if (stageIndex >= wizardStageOrder().size())
    {
        sendStatus("Unknown wizard stage: " + stage, true);
        return false;
    }
    if (m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return false;
    }

    // Initial authoring follows the ordered pipeline. Production assets are
    // maintenance-mode and can enter any stage directly; local debt determines
    // readiness. No stage check saves anything.
    const bool maintenanceMode = std::filesystem::exists(compiledPath(m_selectedId));
    if (!maintenanceMode && stageIndex > 0)
    {
        const std::string previous = wizardStageOrder()[stageIndex - 1];
        if (m_wizardStages[previous].status != "complete")
        {
            sendStatus("Check wizard stage '" + previous + "' first", true);
            return false;
        }
    }
    if (!ensureAllLodsLoaded()) return false;

    std::string validationError;
    bool passed = validateWizardStage(stage, &validationError);
    auto& value = m_wizardStages[stage];
    const std::string previousStatus = value.status;
    value.status = passed ? "complete" : "needs_fix";
    if (value.status != previousStatus) markEditorStateDirty();

    // VALIDATE uses the same CHECK action but also publishes the per-contract
    // report consumed by the validation panel. This remains read-only.
    if (stage == "validate") sendWizardValidationReport();

    // BUILD is the only exception to the read-only CHECK rule: it is the
    // explicit terminal production write. Require the working state to be saved
    // first so RESTORE and BUILD can never point at two different revisions.
    if (passed && stage == "build")
    {
        syncDirty();
        if (m_dirty)
        {
            passed = false;
            validationError = "save the current WORKING ASSET before BUILD";
            value.status = "needs_fix";
            markEditorStateDirty();
        }
        else if (!buildProductionAsset())
        {
            passed = false;
            validationError = "BUILD production package save failed";
            value.status = "needs_fix";
            markEditorStateDirty();
        }
    }

    // Non-terminal stage evidence is an unsaved WORKING edit and therefore
    // makes SAVE available. BUILD certification belongs to the final production
    // state: it must not make the already-built WORKING revision dirty.
    recordMeshStageResult(stage, passed, stage != "build");
    if (passed && stage == "build")
    {
        std::string finalStateError;
        if (!writeProductionEditorState(&finalStateError))
        {
            passed = false;
            value.status = "needs_fix";
            validationError = "BUILD production sidecar finalization failed: " + finalStateError;
            for (auto& [lodIndex, byGeometry] : m_meshSourceRecords)
                for (auto& [geometryId, record] : byGeometry)
                {
                    (void)lodIndex;
                    (void)geometryId;
                    record.stageChecks[stage] = "failed";
                }
        }
    }

    // CHECK updates the per-mesh stage graph. Publish lightweight metadata so
    // every mesh table immediately reflects the current stage evidence; this
    // is not a SAVE and does not serialize any geometry payload.
    sendAssetMetadata();

    m_server.broadcastText(json({
        {"type", "wizard_state_patch"},
        {"dirty", m_dirty},
        {"wizard", serializeWizard()}
    }).dump());

    if (passed)
    {
        const std::string next = stageIndex + 1 < wizardStageOrder().size()
            ? wizardStageOrder()[stageIndex + 1] : std::string();
        m_server.broadcastText(json({
            {"type", "wizard_stage_checked"},
            {"stage", stage},
            {"nextStage", maintenanceMode ? std::string() : next}
        }).dump());
        sendStatus(stage == "build"
            ? "BUILD complete: production package written from the saved WORKING ASSET."
            : "Stage check passed: " + stage + ". Nothing was saved; the next stage is now available.");
    }
    else
    {
        sendStatus("Stage check failed: " + stage + ": " + validationError + ". Nothing was saved.", true);
    }
    return passed;
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



bool ModelAssetEditorSession::modelPreflightReadyForLod(std::string* reason) const
{
    if (m_asset.renderLods.empty() || m_lodState.empty() || !m_lodState[0].loaded)
    {
        if (reason) *reason = "LOD0 is not loaded";
        return false;
    }

    // LOD analysis is a geometric operation. Its gate is deliberately narrower
    // than the later SURFACES authoring contract: every used base LOD0 mesh
    // must be the current canonical PREPARE result and must satisfy technical
    // topology/orientation invariants. Whether an open mesh is authored as
    // ThinTwoSided or BreachedVolume is a separate semantic decision and must
    // not disable LOD analysis.
    const auto& lod = m_asset.renderLods[0];
    std::vector<std::size_t> usage(lod.geometries.size(), 0);
    for (const auto& node : lod.nodes)
        if (node.enabled && node.geometryIndex >= 0 && static_cast<std::size_t>(node.geometryIndex) < usage.size())
            ++usage[static_cast<std::size_t>(node.geometryIndex)];

    for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
    {
        const auto& geometry = lod.geometries[gi];
        if (usage[gi] == 0 || isRenderVariantGeometryId(geometry.id)) continue;

        const auto canonical = analyzeCanonicalMesh(geometry.mesh);
        if (canonical.structuralInvalid)
        {
            if (reason) *reason = "LOD0 G" + std::to_string(gi) + " is Invalid: " + canonical.invalidReason;
            return false;
        }

        bool canonicalCurrent = false;
        const auto prepLodIt = m_meshPreparationRecords.find(0);
        if (prepLodIt != m_meshPreparationRecords.end())
        {
            const auto prepIt = prepLodIt->second.find(geometry.id);
            canonicalCurrent = prepIt != prepLodIt->second.end() &&
                prepIt->second.algorithm == CanonicalMeshAlgorithmId &&
                prepIt->second.outputFingerprint == canonicalMeshFingerprint(geometry.mesh);
        }
        if (!canonicalCurrent)
        {
            if (reason) *reason = "LOD0 G" + std::to_string(gi) + " is not canonical at the current SOURCE boundary";
            return false;
        }
        if (canonical.degenerateTriangles != 0 || canonical.duplicateTriangles != 0 ||
            canonical.windingConflicts != 0 || canonical.insideOutClosedComponents != 0)
        {
            if (reason) *reason = "LOD0 G" + std::to_string(gi) + " canonical SOURCE record is stale";
            return false;
        }
    }
    if (reason) reason->clear();
    return true;
}

bool ModelAssetEditorSession::modelPreflightAllLoadedReady(std::string* reason) const
{
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        if (li >= m_lodState.size() || !m_lodState[li].loaded)
        {
            if (reason) *reason = "LOD" + std::to_string(li) + " is not loaded";
            return false;
        }
        const auto& lod = m_asset.renderLods[li];
        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            const auto& geometry = lod.geometries[gi];
            const auto canonical = analyzeCanonicalMesh(geometry.mesh);
            if (canonical.structuralInvalid)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " is Invalid: " + canonical.invalidReason;
                return false;
            }

            bool canonicalCurrent = false;
            const auto prepLodIt = m_meshPreparationRecords.find(li);
            if (prepLodIt != m_meshPreparationRecords.end())
            {
                const auto prepIt = prepLodIt->second.find(geometry.id);
                canonicalCurrent = prepIt != prepLodIt->second.end() &&
                    prepIt->second.algorithm == CanonicalMeshAlgorithmId &&
                    prepIt->second.outputFingerprint == canonicalMeshFingerprint(geometry.mesh);
            }
            if (!canonicalCurrent)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " is not canonical at the current SOURCE boundary";
                return false;
            }
            if (canonical.degenerateTriangles != 0 || canonical.duplicateTriangles != 0 ||
                canonical.windingConflicts != 0 || canonical.insideOutClosedComponents != 0)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " canonical SOURCE record is stale";
                return false;
            }

            const auto audit = auditPreflightGeometry(geometry.mesh);
            std::string explicitClass;
            const auto classLodIt = m_geometryTopologyClasses.find(li);
            if (classLodIt != m_geometryTopologyClasses.end())
            {
                const auto explicitIt = classLodIt->second.find(geometry.id);
                if (explicitIt != classLodIt->second.end()) explicitClass = explicitIt->second;
            }
            const auto explicitParsed = preflightTopologyClassFromName(explicitClass);
            const bool autoResolved = audit.suggestedClass == PreflightTopologyClass::ClosedVolume ||
                (audit.suggestedClass == PreflightTopologyClass::ThinTwoSided && audit.confidence >= 0.90);
            if (explicitParsed == PreflightTopologyClass::Auto && !autoResolved)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " needs an explicit target geometry class";
                return false;
            }
            const auto effectiveClass = explicitParsed == PreflightTopologyClass::Auto
                ? audit.suggestedClass : explicitParsed;
            if (effectiveClass != PreflightTopologyClass::ClosedVolume &&
                effectiveClass != PreflightTopologyClass::ThinOneSided &&
                effectiveClass != PreflightTopologyClass::ThinTwoSided &&
                effectiveClass != PreflightTopologyClass::BreachedVolume)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " has no valid geometry contract";
                return false;
            }
            const auto desiredSurface = effectiveClass == PreflightTopologyClass::ThinOneSided
                ? SurfaceMode::ThinOneSided
                : effectiveClass == PreflightTopologyClass::ThinTwoSided
                    ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
            if (geometry.surfaceMode != desiredSurface)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " surface mode does not match geometry class";
                return false;
            }
        }
    }
    if (reason) reason->clear();
    return true;
}

bool ModelAssetEditorSession::analyzeModelPreflight()
{
    if (!ensureAllLodsLoaded()) return false;

    // Preflight is an explicit audit, not a load-time gate. It intentionally
    // accepts a mixed/raw working set so the UI can tell the author which
    // meshes still need preparation instead of refusing to show the model.
    json rows = json::array();
    std::size_t geometryCount = 0;
    std::size_t componentCount = 0;
    std::size_t closedComponents = 0;
    std::size_t openComponents = 0;
    std::size_t boundaryEdges = 0;
    std::size_t canonicalMultiUseEdges = 0;
    std::size_t sourceNonManifoldEdges = 0;
    std::size_t windingConflicts = 0;
    std::size_t insideOutComponents = 0;
    std::size_t degenerateTriangles = 0;
    std::size_t duplicateTriangles = 0;
    std::size_t removedDegenerateTrianglesTotal = 0;
    std::size_t removedDuplicateTrianglesTotal = 0;
    std::size_t invalidTriangles = 0;
    std::size_t sourceReloadCount = 0;
    std::size_t canonicalCount = 0;
    std::size_t reviewCount = 0;
    std::size_t blockerCount = 0;
    std::size_t structuralBlockerCount = 0;
    std::size_t unresolvedCount = 0;
    std::size_t sourceRenderVertices = 0;
    std::size_t canonicalGeometricPoints = 0;
    std::size_t outputRenderVertices = 0;

    sendStatus("Checking canonical meshes and geometry contracts...", false, "working");
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        if (li >= m_lodState.size() || !m_lodState[li].loaded) continue;
        const auto& lod = m_asset.renderLods[li];
        std::vector<std::size_t> usage(lod.geometries.size(), 0);
        for (const auto& node : lod.nodes)
            if (node.enabled && node.geometryIndex >= 0 && static_cast<std::size_t>(node.geometryIndex) < usage.size())
                ++usage[static_cast<std::size_t>(node.geometryIndex)];

        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            const auto& geometry = lod.geometries[gi];
            const auto canonical = analyzeCanonicalMesh(geometry.mesh);
            const auto audit = auditPreflightGeometry(geometry.mesh);
            ++geometryCount;
            componentCount += audit.components.size();
            closedComponents += audit.closedComponents;
            openComponents += audit.openComponents;
            boundaryEdges += canonical.boundaryEdges;
            canonicalMultiUseEdges += canonical.canonicalMultiUseEdges;
            // Canonical edges are rebuilt and never inherit RAW EdgeNonManifold.
            // The preparation record keeps source evidence for diagnostics.
            windingConflicts += canonical.windingConflicts;
            insideOutComponents += canonical.insideOutClosedComponents;
            degenerateTriangles += canonical.degenerateTriangles;
            duplicateTriangles += canonical.duplicateTriangles;
            invalidTriangles += canonical.invalidTriangles;

            const MeshPreparationRecord* record = nullptr;
            const auto prepLodIt = m_meshPreparationRecords.find(li);
            if (prepLodIt != m_meshPreparationRecords.end())
            {
                const auto prepIt = prepLodIt->second.find(geometry.id);
                if (prepIt != prepLodIt->second.end()) record = &prepIt->second;
            }
            const auto currentFingerprint = canonicalMeshFingerprint(geometry.mesh);
            const bool canonicalCurrent = record && record->algorithm == CanonicalMeshAlgorithmId &&
                record->outputFingerprint == currentFingerprint;
            const bool canonicalRecordStale = record && !canonicalCurrent;
            std::string orientationOverride = "auto";
            bool orientationOverrideStale = false;
            bool orientationOverrideActive = false;
            const auto overrideLodIt = m_meshOrientationOverrides.find(li);
            if (overrideLodIt != m_meshOrientationOverrides.end())
            {
                const auto overrideIt = overrideLodIt->second.find(geometry.id);
                if (overrideIt != overrideLodIt->second.end() && overrideIt->second.mode == "flipped")
                {
                    orientationOverride = "flipped";
                    const auto currentSourceFingerprint = geometry.sourcePath.empty()
                        ? std::uint64_t(0)
                        : sourceFileFingerprint(selectedSourceFilePath(geometry.sourcePath));
                    orientationOverrideStale = overrideIt->second.sourceFingerprint != 0 &&
                        currentSourceFingerprint != overrideIt->second.sourceFingerprint;
                    orientationOverrideActive = canonicalCurrent && !orientationOverrideStale;
                }
            }
            bool orientationSourceRevisionCurrent = geometry.sourcePath.empty();
            if (!geometry.sourcePath.empty())
            {
                const auto currentSourceFingerprint = sourceFileFingerprint(
                    selectedSourceFilePath(geometry.sourcePath));
                const auto sourceLodIt = m_sourceMeshFingerprints.find(li);
                if (sourceLodIt != m_sourceMeshFingerprints.end())
                {
                    const auto acceptedIt = sourceLodIt->second.find(geometry.sourcePath);
                    orientationSourceRevisionCurrent = acceptedIt != sourceLodIt->second.end() &&
                        currentSourceFingerprint != 0 && acceptedIt->second == currentSourceFingerprint;
                }
            }
            if (canonicalCurrent)
            {
                ++canonicalCount;
                removedDegenerateTrianglesTotal += record->removedDegenerateTriangles;
                removedDuplicateTrianglesTotal += record->removedDuplicateTriangles;
            }
            const std::size_t rowSourceNonManifoldEdges = canonicalCurrent
                ? record->sourceNonManifoldEdges : canonical.sourceNonManifoldEdges;
            sourceNonManifoldEdges += rowSourceNonManifoldEdges;

            const std::size_t rowSourceVertices = canonicalCurrent ? record->sourceRenderVertices : geometry.mesh.vertices.size();
            const std::size_t rowSourceTriangles = canonicalCurrent ? record->sourceTriangles : geometry.mesh.triangles.size();
            const std::size_t rowGeometricPoints = canonicalCurrent ? record->geometricPoints : canonical.geometricPoints;
            const std::size_t rowOutputVertices = canonicalCurrent ? record->outputRenderVertices : geometry.mesh.vertices.size();
            const std::size_t rowOutputTriangles = canonicalCurrent ? record->outputTriangles : geometry.mesh.triangles.size();
            sourceRenderVertices += rowSourceVertices;
            canonicalGeometricPoints += rowGeometricPoints;
            outputRenderVertices += rowOutputVertices;

            std::string explicitClass;
            const auto lodIt = m_geometryTopologyClasses.find(li);
            if (lodIt != m_geometryTopologyClasses.end())
            {
                const auto classIt = lodIt->second.find(geometry.id);
                if (classIt != lodIt->second.end()) explicitClass = classIt->second;
            }
            const auto explicitParsed = preflightTopologyClassFromName(explicitClass);
            const auto suggested = canonicalCurrent && canonical.structuralInvalid
                ? PreflightTopologyClass::Invalid : audit.suggestedClass;
            const auto effective = explicitParsed == PreflightTopologyClass::Auto ? suggested : explicitParsed;
            const bool autoResolved = canonicalCurrent && !canonical.structuralInvalid &&
                (suggested == PreflightTopologyClass::ClosedVolume ||
                 (suggested == PreflightTopologyClass::ThinTwoSided && audit.confidence >= 0.90));
            const bool needsPreparation = !canonicalCurrent;
            const bool structuralBlocker = canonicalCurrent && canonical.structuralInvalid;
            const bool needsReview = canonicalCurrent && explicitParsed == PreflightTopologyClass::Auto &&
                !autoResolved && !canonical.structuralInvalid;
            const bool sourceBaseGeometry = usage[gi] != 0 && !isRenderVariantGeometryId(geometry.id);

            const SurfaceMode desiredSurface = effective == PreflightTopologyClass::ThinOneSided
                ? SurfaceMode::ThinOneSided
                : effective == PreflightTopologyClass::ThinTwoSided
                    ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
            const bool classResolved = effective == PreflightTopologyClass::ClosedVolume ||
                effective == PreflightTopologyClass::ThinOneSided ||
                effective == PreflightTopologyClass::ThinTwoSided || effective == PreflightTopologyClass::BreachedVolume;
            const bool surfaceMismatch = classResolved && geometry.surfaceMode != desiredSurface;
            // Classification/surface authoring is intentionally advisory in
            // the LODS stage. Only an unprepared or structurally invalid LOD0
            // base mesh blocks geometric LOD analysis.
            const bool blocksLod0 = li == 0 && sourceBaseGeometry &&
                (needsPreparation || (canonicalCurrent && structuralBlocker));

            if (needsPreparation) ++sourceReloadCount;
            if (needsReview) ++reviewCount;
            if (structuralBlocker) ++structuralBlockerCount;
            if (blocksLod0) ++blockerCount;
            if (needsPreparation || (canonicalCurrent && structuralBlocker) || needsReview || surfaceMismatch) ++unresolvedCount;

            std::string action = "ready";
            if (needsPreparation) action = "prepare_required";
            else if (structuralBlocker) action = "manual_fix";
            else if (needsReview) action = "classify";
            else if (surfaceMismatch) action = "apply_class";

            rows.push_back({
                {"lodIndex", li}, {"geometryIndex", gi}, {"geometryId", geometry.id},
                {"sourcePath", geometry.sourcePath}, {"usageCount", usage[gi]},
                {"isSourceVariant", isRenderVariantGeometryId(geometry.id)},
                {"triangleCount", geometry.mesh.triangles.size()}, {"vertexCount", geometry.mesh.vertices.size()},
                {"components", audit.components.size()}, {"closedComponents", audit.closedComponents},
                {"openComponents", audit.openComponents}, {"boundaryEdges", canonical.boundaryEdges},
                {"nonManifoldEdges", canonical.canonicalMultiUseEdges},
                {"canonicalMultiUseEdges", canonical.canonicalMultiUseEdges},
                {"sourceNonManifoldEdges", rowSourceNonManifoldEdges},
                {"windingConflicts", canonical.windingConflicts},
                {"insideOutComponents", canonical.insideOutClosedComponents},
                {"invertedNormalCorners", std::size_t(0)},
                {"degenerateTriangles", canonical.degenerateTriangles},
                {"duplicateTriangles", canonical.duplicateTriangles},
                {"invalidTriangles", canonical.invalidTriangles},
                {"suggestedClass", preflightTopologyClassName(suggested)},
                {"suggestedConfidence", audit.confidence}, {"explicitClass", explicitClass},
                {"effectiveClass", preflightTopologyClassName(effective)},
                {"surfaceMode", surfaceModeName(geometry.surfaceMode)}, {"surfaceMismatch", surfaceMismatch},
                {"needsReview", needsReview},
                {"canonicalCurrent", canonicalCurrent}, {"canonicalRecordStale", canonicalRecordStale},
                {"needsSourceReload", needsPreparation}, {"needsPreparation", needsPreparation},
                {"structuralBlocker", structuralBlocker}, {"invalidReason", canonical.invalidReason},
                {"blocksLod0", blocksLod0},
                {"orientationProblem", canonical.windingFlipsRequired != 0 ||
                    (canonical.insideOutClosedComponents != 0 && !orientationOverrideActive)},
                {"orientationOverride", orientationOverride},
                {"orientationOverrideActive", orientationOverrideActive},
                {"orientationOverrideStale", orientationOverrideStale},
                {"orientationSourceRevisionCurrent", orientationSourceRevisionCurrent},
                {"action", action},
                {"sourceRenderVertices", rowSourceVertices}, {"sourceTriangles", rowSourceTriangles},
                {"canonicalGeometricPoints", rowGeometricPoints},
                {"outputRenderVertices", rowOutputVertices}, {"outputTriangles", rowOutputTriangles},
                {"removedDegenerateTriangles", canonicalCurrent ? record->removedDegenerateTriangles : std::size_t(0)},
                {"removedDuplicateTriangles", canonicalCurrent ? record->removedDuplicateTriangles : std::size_t(0)},
                {"normalIslands", canonicalCurrent ? record->normalIslands : std::size_t(0)},
                {"rebuiltEdges", canonicalCurrent ? record->rebuiltEdges : geometry.mesh.edges.size()}
            });
        }
    }

    std::string reason;
    const bool readyForLod = modelPreflightReadyForLod(&reason);
    m_server.broadcastText(json({
        {"type", "model_preflight_result"}, {"geometryCount", geometryCount},
        {"componentCount", componentCount}, {"closedComponents", closedComponents},
        {"openComponents", openComponents}, {"boundaryEdges", boundaryEdges},
        {"nonManifoldEdges", canonicalMultiUseEdges}, {"canonicalMultiUseEdges", canonicalMultiUseEdges},
        {"sourceNonManifoldEdges", sourceNonManifoldEdges}, {"windingConflicts", windingConflicts},
        {"insideOutComponents", insideOutComponents}, {"invertedNormalCorners", std::size_t(0)},
        {"degenerateTriangles", degenerateTriangles}, {"duplicateTriangles", duplicateTriangles},
        {"removedDegenerateTriangles", removedDegenerateTrianglesTotal},
        {"removedDuplicateTriangles", removedDuplicateTrianglesTotal},
        {"invalidTriangles", invalidTriangles},
        {"sourceReloadCount", sourceReloadCount}, {"canonicalCount", canonicalCount},
        {"reviewCount", reviewCount}, {"blockerCount", blockerCount},
        {"structuralBlockerCount", structuralBlockerCount}, {"unresolvedCount", unresolvedCount},
        {"sourceRenderVertices", sourceRenderVertices},
        {"canonicalGeometricPoints", canonicalGeometricPoints},
        {"outputRenderVertices", outputRenderVertices},
        {"allLoadedGeometryResolved", unresolvedCount == 0},
        {"readyForLod", readyForLod}, {"readyReason", reason}, {"rows", std::move(rows)},
        {"algorithm", CanonicalMeshAlgorithmId}
    }).dump());
    sendStatus(
        std::string("Model preflight: ") + (readyForLod ? "LOD0 ready" : "review required") +
        ", invalid=" + std::to_string(structuralBlockerCount) +
        ", review=" + std::to_string(reviewCount));
    return true;
}

bool ModelAssetEditorSession::canonicalizeLoadedWorkingSet(
    const std::string& invalidationStage,
    bool reportStatus,
    bool* payloadChangedOut,
    std::vector<std::size_t>* changedLodsOut,
    std::size_t scopeLod,
    const std::string& scopeGeometryId,
    bool onlyUncheckedLodMeshes)
{
    std::size_t canonicalizedGeometries = 0;
    std::size_t changedGeometries = 0;
    std::size_t invalidGeometries = 0;
    std::size_t splitTopologyVertices = 0;
    std::size_t raycastPatches = 0;
    std::size_t raycastFlippedTriangles = 0;
    std::size_t skippedCertifiedGeometries = 0;
    std::vector<std::string> canonicalFailures;
    bool payloadChanged = false;
    bool authoringStateChanged = false;
    std::set<std::size_t> changedLods;
    const auto repairLogPath = wizardLogPath("mesh_repair.log");
    resetMeshRepairDiagnostic(repairLogPath, m_asset);

    const auto lodStageValue = [&](std::size_t lodIndex, const std::string& geometryId) -> std::string {
        const auto lodIt = m_meshSourceRecords.find(lodIndex);
        if (lodIt == m_meshSourceRecords.end()) return {};
        const auto geometryIt = lodIt->second.find(geometryId);
        if (geometryIt == lodIt->second.end()) return {};
        const auto stageIt = geometryIt->second.stageChecks.find("lods");
        return stageIt == geometryIt->second.stageChecks.end() ? std::string("not_checked") : stageIt->second;
    };
    const auto setLodStageValue = [&](std::size_t lodIndex, const RenderGeometryDefinition& geometry, const char* value) {
        if (geometry.sourcePath.empty()) return;
        auto lodIt = m_meshSourceRecords.find(lodIndex);
        if (lodIt == m_meshSourceRecords.end()) return;
        auto geometryIt = lodIt->second.find(geometry.id);
        if (geometryIt == lodIt->second.end()) return;
        auto& stage = geometryIt->second.stageChecks["lods"];
        if (stage != value)
        {
            stage = value;
            authoringStateChanged = true;
        }
    };

    // Explicit authoring operation. Load/restore/reimport deliberately leave
    // resident meshes untouched; this function mutates the working copy only
    // after the user presses PREPARE MESHES. Current canonical payloads are
    // skipped by their fingerprint so repeated preparation is idempotent.
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        if (scopeLod != std::size_t(-1) && li != scopeLod) continue;
        if (li >= m_lodState.size() || !m_lodState[li].loaded) continue;
        auto& lod = m_asset.renderLods[li];
        for (auto& geometry : lod.geometries)
        {
            if (!scopeGeometryId.empty() && geometry.id != scopeGeometryId) continue;
            if (onlyUncheckedLodMeshes && !geometry.sourcePath.empty() && lodStageValue(li, geometry.id) == "passed")
            {
                ++skippedCertifiedGeometries;
                continue;
            }
            bool canonicalCurrent = false;
            bool hadPreviousCanonicalEvidence = false;
            std::uint64_t previousCanonicalFingerprint = 0;
            auto prepLodIt = m_meshPreparationRecords.find(li);
            if (prepLodIt != m_meshPreparationRecords.end())
            {
                const auto prepIt = prepLodIt->second.find(geometry.id);
                if (prepIt != prepLodIt->second.end())
                {
                    hadPreviousCanonicalEvidence = true;
                    previousCanonicalFingerprint = prepIt->second.outputFingerprint;
                    canonicalCurrent = prepIt->second.algorithm == CanonicalMeshAlgorithmId &&
                        prepIt->second.outputFingerprint == canonicalMeshFingerprint(geometry.mesh);
                }
            }

            if (!canonicalCurrent)
            {
                const auto residentFingerprintBefore = canonicalMeshFingerprint(geometry.mesh);
                // PREPARE is the explicit authoring mutation boundary. It performs
                // topology-aware cleanup, libigl/Embree orientation and
                // normal/render-edge rebuild. Geometry-class decisions remain
                // exclusively in ANALYZE. Keep the exact resident pre-PREPARE
                // payload for the session-only SOURCE viewport.
                const auto rawSnapshotStarted = std::chrono::steady_clock::now();
                m_rawMeshSnapshots[li][geometry.id] = geometry.mesh;
                const auto rawSnapshotMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - rawSnapshotStarted).count();
                const auto geometryPrepareStarted = std::chrono::steady_clock::now();
                const auto built = canonicalizeMesh(geometry.mesh);
                bool manualOrientationFlipApplied = false;
                bool manualOrientationOverrideStale = false;
                if (built.success)
                {
                    const auto overrideLodIt = m_meshOrientationOverrides.find(li);
                    if (overrideLodIt != m_meshOrientationOverrides.end())
                    {
                        const auto overrideIt = overrideLodIt->second.find(geometry.id);
                        if (overrideIt != overrideLodIt->second.end() && overrideIt->second.mode == "flipped")
                        {
                            const auto currentSourceFingerprint = geometry.sourcePath.empty()
                                ? std::uint64_t(0)
                                : sourceFileFingerprint(selectedSourceFilePath(geometry.sourcePath));
                            manualOrientationOverrideStale = overrideIt->second.sourceFingerprint != 0 &&
                                currentSourceFingerprint != overrideIt->second.sourceFingerprint;
                            if (!manualOrientationOverrideStale)
                            {
                                flipMeshOrientation(geometry.mesh);
                                manualOrientationFlipApplied = true;
                            }
                        }
                    }
                }
                const auto geometryPrepareMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - geometryPrepareStarted).count();
                std::cerr << "[ModelAssetEditor][prepare] LOD" << li
                          << " geometry=" << geometry.id
                          << " raw_snapshot_ms=" << std::fixed << std::setprecision(1) << rawSnapshotMs
                          << " canonical_ms=" << geometryPrepareMs
                          << " success=" << (built.success ? 1 : 0)
                          << " changed=" << (residentFingerprintBefore != canonicalMeshFingerprint(geometry.mesh) ? 1 : 0)
                          << " manual_orientation_flip=" << (manualOrientationFlipApplied ? 1 : 0)
                          << " manual_orientation_stale=" << (manualOrientationOverrideStale ? 1 : 0)
                          << " raycast_patches=" << built.raycastPatches
                          << " raycast_flips=" << built.raycastFlippedTriangles
                          << '\n';
                appendMeshRepairDiagnostic(repairLogPath, m_asset, li, geometry, built);
                if (!built.success)
                {
                    auto recordLodIt = m_meshPreparationRecords.find(li);
                    if (recordLodIt != m_meshPreparationRecords.end())
                    {
                        authoringStateChanged = recordLodIt->second.erase(geometry.id) != 0 || authoringStateChanged;
                        if (recordLodIt->second.empty()) m_meshPreparationRecords.erase(recordLodIt);
                    }
                    ++invalidGeometries;
                    setLodStageValue(li, geometry, "failed");
                    canonicalFailures.push_back(
                        "LOD" + std::to_string(li) + "/" + geometry.id + ": " +
                        (built.error.empty() ? std::string("canonical authoring pass failed") : built.error));
                    continue;
                }

                MeshPreparationRecord record;
                record.algorithm = CanonicalMeshAlgorithmId;
                record.sourceRenderVertices = built.before.renderVertices;
                record.sourceTriangles = built.before.triangles;
                record.geometricPoints = built.after.geometricPoints;
                record.outputRenderVertices = built.after.renderVertices;
                record.outputTriangles = built.after.triangles;
                record.removedDegenerateTriangles = built.removedDegenerateTriangles;
                record.removedDuplicateTriangles = built.removedDuplicateTriangles;
                record.sourceNonManifoldEdges = built.before.sourceNonManifoldEdges;
                record.normalIslands = built.normalIslands;
                record.rebuiltEdges = built.rebuiltEdges;
                record.splitTopologyVertices = built.splitTopologyVertices;
                record.raycastPatches = built.raycastPatches;
                record.raycastFlippedTriangles = built.raycastFlippedTriangles;
                record.outputFingerprint = canonicalMeshFingerprint(geometry.mesh);
                m_meshPreparationRecords[li][geometry.id] = record;

                // An explicit open-geometry classification is valid only for
                // the canonical payload on which the author made that choice.
                // Preserve it across a byte-equivalent reimport; otherwise
                // force one new ThinTwoSided/Breached decision instead of
                // silently carrying topology intent onto different geometry.
                if (!hadPreviousCanonicalEvidence ||
                    previousCanonicalFingerprint != record.outputFingerprint)
                {
                    auto classLodIt = m_geometryTopologyClasses.find(li);
                    if (classLodIt != m_geometryTopologyClasses.end())
                    {
                        authoringStateChanged = classLodIt->second.erase(geometry.id) != 0 || authoringStateChanged;
                        if (classLodIt->second.empty()) m_geometryTopologyClasses.erase(classLodIt);
                    }
                }
                canonicalCurrent = true;
                ++canonicalizedGeometries;
                authoringStateChanged = true;

                splitTopologyVertices += built.splitTopologyVertices;
                raycastPatches += built.raycastPatches;
                raycastFlippedTriangles += built.raycastFlippedTriangles;
                if (residentFingerprintBefore != record.outputFingerprint)
                {
                    ++changedGeometries;
                    payloadChanged = true;
                    changedLods.insert(li);
                    markLodDirty(li);
                }
            }

            // PREPARE is the LODS per-mesh certification boundary. A SOURCE-backed
            // mesh turns green as soon as its resident payload is canonical/current.
            // ANALYZE remains a separate read-only topology/classification audit.
            if (canonicalCurrent) setLodStageValue(li, geometry, "passed");

            // Preparation ends here. Topology classification/validation is intentionally
            // NOT run by this command; ANALYZE is the only expensive audit path.
            (void)canonicalCurrent;
        }
    }

    // Drop preparation records for geometry ids that no longer exist.
    for (auto lodIt = m_meshPreparationRecords.begin(); lodIt != m_meshPreparationRecords.end(); )
    {
        const auto li = lodIt->first;
        if (li >= m_asset.renderLods.size() || li >= m_lodState.size() || !m_lodState[li].loaded)
        {
            ++lodIt;
            continue;
        }
        const auto& geometries = m_asset.renderLods[li].geometries;
        for (auto recordIt = lodIt->second.begin(); recordIt != lodIt->second.end(); )
        {
            const bool exists = std::any_of(
                geometries.begin(), geometries.end(),
                [&](const RenderGeometryDefinition& geometry) { return geometry.id == recordIt->first; });
            if (!exists)
            {
                recordIt = lodIt->second.erase(recordIt);
                authoringStateChanged = true;
            }
            else ++recordIt;
        }
        if (lodIt->second.empty()) lodIt = m_meshPreparationRecords.erase(lodIt);
        else ++lodIt;
    }

    // Drop manual-orientation records for geometry ids that no longer exist.
    for (auto lodIt = m_meshOrientationOverrides.begin(); lodIt != m_meshOrientationOverrides.end(); )
    {
        const auto li = lodIt->first;
        if (li >= m_asset.renderLods.size() || li >= m_lodState.size() || !m_lodState[li].loaded)
        {
            ++lodIt;
            continue;
        }
        const auto& geometries = m_asset.renderLods[li].geometries;
        for (auto recordIt = lodIt->second.begin(); recordIt != lodIt->second.end(); )
        {
            const bool exists = std::any_of(
                geometries.begin(), geometries.end(),
                [&](const RenderGeometryDefinition& geometry) { return geometry.id == recordIt->first; });
            if (!exists)
            {
                recordIt = lodIt->second.erase(recordIt);
                authoringStateChanged = true;
            }
            else ++recordIt;
        }
        if (lodIt->second.empty()) lodIt = m_meshOrientationOverrides.erase(lodIt);
        else ++lodIt;
    }

    if (payloadChanged && !invalidationStage.empty())
        invalidateWizardFrom(invalidationStage);
    // Authoring evidence without mesh mutation belongs to the mutable working
    // state and is persisted only by the global SAVE action. PREPARE therefore
    // makes the per-mesh LODS graph dirty even when the canonical bytes were
    // already current and no payload crossed the transport boundary.
    if (authoringStateChanged) markEditorStateDirty();

    if (payloadChangedOut) *payloadChangedOut = payloadChanged;
    if (changedLodsOut)
        changedLodsOut->assign(changedLods.begin(), changedLods.end());

    if (reportStatus && (canonicalizedGeometries != 0 || invalidGeometries != 0))
    {
        sendStatus(
            "Mesh preparation: ready=" + std::to_string(canonicalizedGeometries) +
            ", changed=" + std::to_string(changedGeometries) +
            ", skipped certified=" + std::to_string(skippedCertifiedGeometries) +
            ", failed=" + std::to_string(invalidGeometries) +
            ", split vertices=" + std::to_string(splitTopologyVertices) +
            ", raycast patches=" + std::to_string(raycastPatches) +
            ", raycast flips=" + std::to_string(raycastFlippedTriangles) +
            "; details: " + repairLogPath.generic_string());
    }
    else if (reportStatus)
    {
        sendStatus("Mesh preparation: pending workset empty; certified meshes skipped=" +
            std::to_string(skippedCertifiedGeometries) + "; changed=0");
    }

    if (!canonicalFailures.empty())
    {
        std::string message =
            "MESH PREPARATION INCOMPLETE: canonical authoring pass failed for " +
            std::to_string(canonicalFailures.size()) + " geometry(s); failed meshes remain unchanged and visible. " + canonicalFailures.front();
        if (canonicalFailures.size() > 1)
            message += " (and " + std::to_string(canonicalFailures.size() - 1) + " more)";
        sendStatus(message, true);
        return false;
    }
    return true;
}

bool ModelAssetEditorSession::verifyLoadedWorkingSetCanonical(std::string* reason) const
{
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        if (li >= m_lodState.size() || !m_lodState[li].loaded) continue;
        const auto& lod = m_asset.renderLods[li];
        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            const auto& geometry = lod.geometries[gi];
            const auto lodIt = m_meshPreparationRecords.find(li);
            if (lodIt == m_meshPreparationRecords.end())
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " has no canonical SOURCE record";
                return false;
            }
            const auto recordIt = lodIt->second.find(geometry.id);
            if (recordIt == lodIt->second.end())
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " has no canonical SOURCE record";
                return false;
            }
            const auto fingerprint = canonicalMeshFingerprint(geometry.mesh);
            const bool generatedRecord =
                lod.sourceKind == "generated" && lod.generatedFromLod >= 0 &&
                recordIt->second.algorithm == GeneratedLodComponentCullAlgorithmId;
            if ((!generatedRecord && recordIt->second.algorithm != CanonicalMeshAlgorithmId) ||
                recordIt->second.outputFingerprint != fingerprint)
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " changed after the canonical SOURCE boundary";
                return false;
            }
            const auto analysis = analyzeCanonicalMesh(geometry.mesh);
            bool manualOrientationActive = false;
            const auto overrideLodIt = m_meshOrientationOverrides.find(li);
            if (overrideLodIt != m_meshOrientationOverrides.end())
            {
                const auto overrideIt = overrideLodIt->second.find(geometry.id);
                if (overrideIt != overrideLodIt->second.end() && overrideIt->second.mode == "flipped")
                {
                    const auto currentSourceFingerprint = geometry.sourcePath.empty()
                        ? std::uint64_t(0)
                        : sourceFileFingerprint(selectedSourceFilePath(geometry.sourcePath));
                    manualOrientationActive = overrideIt->second.sourceFingerprint == 0 ||
                        overrideIt->second.sourceFingerprint == currentSourceFingerprint;
                }
            }
            if (analysis.structuralInvalid || analysis.degenerateTriangles != 0 ||
                analysis.duplicateTriangles != 0 || analysis.windingConflicts != 0 ||
                (analysis.insideOutClosedComponents != 0 && !manualOrientationActive))
            {
                if (reason) *reason = "LOD" + std::to_string(li) + " G" + std::to_string(gi) +
                    " violates canonical SOURCE invariants" +
                    (analysis.invalidReason.empty() ? std::string() : ": " + analysis.invalidReason);
                return false;
            }
        }
    }
    if (reason) reason->clear();
    return true;
}

bool ModelAssetEditorSession::setGeometryTopologyClass(
    std::size_t lodIndex,
    std::size_t geometryIndex,
    const std::string& topologyClass,
    bool analyzeAfter,
    bool publishAfter)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    if (geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
        throw std::runtime_error("invalid geometry index");
    const auto parsed = preflightTopologyClassFromName(topologyClass);
    if (parsed != PreflightTopologyClass::Auto &&
        parsed != PreflightTopologyClass::ClosedVolume &&
        parsed != PreflightTopologyClass::ThinOneSided &&
        parsed != PreflightTopologyClass::ThinTwoSided &&
        parsed != PreflightTopologyClass::BreachedVolume)
        throw std::runtime_error("unsupported preflight topology class");

    auto& geometry = m_asset.renderLods[lodIndex].geometries[geometryIndex];
    const std::string maintenanceId = maintenanceComponentId(lodIndex, geometry);
    const bool maintenanceLocal = !maintenanceId.empty() &&
        m_componentMaintenanceIssues.find(maintenanceId) != m_componentMaintenanceIssues.end();
    bool changed = false;
    if (parsed == PreflightTopologyClass::Auto)
    {
        auto lodIt = m_geometryTopologyClasses.find(lodIndex);
        if (lodIt != m_geometryTopologyClasses.end())
        {
            const bool erased = lodIt->second.erase(geometry.id) != 0;
            if (erased) markLodDirty(lodIndex);
            changed = erased || changed;
            if (lodIt->second.empty()) m_geometryTopologyClasses.erase(lodIt);
        }
        const auto audit = auditPreflightGeometry(geometry.mesh);
        const bool autoClosed = audit.suggestedClass == PreflightTopologyClass::ClosedVolume;
        const bool autoThin = audit.suggestedClass == PreflightTopologyClass::ThinTwoSided && audit.confidence >= 0.90;
        if (autoClosed || autoThin)
        {
            const auto desired = autoThin ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
            if (geometry.surfaceMode != desired)
            {
                geometry.surfaceMode = desired;
                markLodDirty(lodIndex);
                changed = true;
            }
        }
    }
    else
    {
        auto& value = m_geometryTopologyClasses[lodIndex][geometry.id];
        const std::string normalized = preflightTopologyClassName(parsed);
        if (value != normalized)
        {
            value = normalized;
            markLodDirty(lodIndex);
            changed = true;
        }
        const auto desired = parsed == PreflightTopologyClass::ThinOneSided
            ? SurfaceMode::ThinOneSided
            : parsed == PreflightTopologyClass::ThinTwoSided
                ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
        if (geometry.surfaceMode != desired)
        {
            geometry.surfaceMode = desired;
            markLodDirty(lodIndex);
            changed = true;
        }
    }
    if (!changed)
    {
        if (publishAfter) sendStatus("NO CHANGES: topology classification already matches");
        if (analyzeAfter) analyzeModelPreflight();
        return true;
    }
    if (publishAfter)
    {
        if (!maintenanceLocal) invalidateWizardFrom("surfaces");
        sendSurfaceMetadataPatch({{lodIndex, geometryIndex}});
        if (maintenanceLocal) sendAssetMetadata();
        sendStatus("Topology class: LOD" + std::to_string(lodIndex) + " G" + std::to_string(geometryIndex) + " → " +
            std::string(parsed == PreflightTopologyClass::Auto ? "AUTO" : preflightTopologyClassName(parsed)));
    }
    if (analyzeAfter) analyzeModelPreflight();
    return true;
}

bool ModelAssetEditorSession::setLodRelativeGeometricError(
    std::size_t lodIndex,
    double relativeError)
{
    if (lodIndex >= m_asset.renderLods.size())
    {
        sendStatus("LOD SSE edit references an invalid LOD", true);
        return false;
    }
    if (!std::isfinite(relativeError))
    {
        sendStatus("LOD relative geometric error must be finite", true);
        return false;
    }

    float authored = -1.0f;
    if (lodIndex == 0)
        authored = 0.0f;
    else if (relativeError >= 0.0)
    {
        if (relativeError <= 0.0 || relativeError > 1.0)
        {
            sendStatus("LOD relative geometric error must be > 0 and <= 1, or negative to clear", true);
            return false;
        }
        authored = static_cast<float>(relativeError);
        for (std::size_t i = 1; i < lodIndex; ++i)
        {
            const float previous = m_asset.renderLods[i].relativeGeometricError;
            if (previous > authored)
            {
                sendStatus("LOD SSE must be non-decreasing: an earlier LOD has a larger error", true);
                return false;
            }
        }
        for (std::size_t i = lodIndex + 1; i < m_asset.renderLods.size(); ++i)
        {
            const float later = m_asset.renderLods[i].relativeGeometricError;
            if (later >= 0.0f && later < authored)
            {
                sendStatus("LOD SSE must be non-decreasing: a later LOD has a smaller error", true);
                return false;
            }
        }
    }

    auto& lod = m_asset.renderLods[lodIndex];
    if (std::abs(lod.relativeGeometricError - authored) <= 1.0e-9f)
    {
        sendStatus("NO CHANGES: LOD runtime SSE metadata already matches");
        return true;
    }
    lod.relativeGeometricError = authored;
    markManifestDirty();
    invalidateWizardFrom("lods");
    m_server.broadcastText(json({
        {"type", "lod_runtime_metadata_patch"},
        {"lodIndex", lodIndex},
        {"relativeGeometricError", lodIndex == 0 ? 0.0f : lod.relativeGeometricError},
        {"dirty", m_dirty},
        {"wizard", serializeWizard()}
    }).dump());
    sendStatus(
        lodIndex == 0
            ? "LOD0 runtime SSE fixed at zero"
            : authored < 0.0f
                ? "LOD" + std::to_string(lodIndex) + " runtime SSE cleared; runtime will not auto-select it"
                : "LOD" + std::to_string(lodIndex) + " relative geometric error = " + std::to_string(authored));
    return true;
}

bool ModelAssetEditorSession::analyzeLodRequirements(std::size_t lodIndex)
{
    if (!ensureAllLodsLoaded()) return false;
    std::string canonicalReason;
    if (!verifyLoadedWorkingSetCanonical(&canonicalReason))
    {
        sendStatus("LOD Generator blocked: canonical SOURCE invariant failed: " + canonicalReason, true);
        return false;
    }
    std::string preflightReason;
    if (!modelPreflightReadyForLod(&preflightReason))
    {
        sendStatus("LOD Generator blocked by Model Preflight: " + preflightReason, true);
        return false;
    }
    if (!ensureLodLoaded(lodIndex)) return false;
    const auto& lod = m_asset.renderLods.at(lodIndex);
    sendStatus(
        "Analyzing LOD" + std::to_string(lodIndex) +
        " feature thickness at 2560x1440 / 70 deg / 2 px...",
        false,
        "working");

    std::vector<std::size_t> usage(lod.geometries.size(), 0);
    for (const auto& node : lod.nodes)
        if (node.enabled && node.geometryIndex >= 0 &&
            static_cast<std::size_t>(node.geometryIndex) < usage.size())
            ++usage[static_cast<std::size_t>(node.geometryIndex)];

    std::vector<LodGeometryAnalysis> analyses;
    std::vector<double> removableFeatures;
    std::size_t totalConnectedComponents = 0;
    std::size_t removableComponents = 0;
    std::size_t totalRenderedTriangles = 0;
    std::size_t totalStoredTriangles = 0;
    std::size_t analyzedGeometries = 0;
    std::size_t additionalGeometryCount = 0;
    json meshCatalog = json::array();

    // LOD generation is an asset-wide operation. The visible default assembly
    // determines rendered-triangle budgets, but every resident LOD0 geometry --
    // including additional/replacement meshes with no RenderNode -- is analyzed
    // and receives the same generated LOD levels.
    for (std::size_t geometryIndex = 0; geometryIndex < lod.geometries.size(); ++geometryIndex)
    {
        const auto& geometry = lod.geometries[geometryIndex];
        const bool variant = isRenderVariantGeometryId(geometry.id);
        if (variant) ++additionalGeometryCount;

        LodGeometryAnalysis analysis;
        analysis.geometryIndex = geometryIndex;
        analysis.usageCount = usage[geometryIndex];
        analysis.totalTriangles = geometry.mesh.triangles.size();
        analysis.components = analyzeConnectedComponents(geometry.mesh);
        totalConnectedComponents += analysis.components.size();
        totalStoredTriangles += analysis.totalTriangles;
        totalRenderedTriangles += analysis.totalTriangles * analysis.usageCount;
        ++analyzedGeometries;
        for (const auto& component : analysis.components)
        {
            if (component.protectedStructure) continue;
            ++removableComponents;
            if (component.featureMeters > 1.0e-6)
                removableFeatures.push_back(component.featureMeters);
        }
        meshCatalog.push_back({
            {"geometryIndex", geometryIndex},
            {"geometryId", geometry.id},
            {"sourcePath", geometry.sourcePath},
            {"isSourceVariant", variant},
            {"usageCount", usage[geometryIndex]},
            {"triangles", geometry.mesh.triangles.size()}
        });
        analyses.push_back(std::move(analysis));
    }

    // Relative geometric error is normalized by the complete placed LOD0
    // characteristic size. This is source-scale only; runtime multiplies the
    // ratio by the object's actual projected characteristic size in pixels.
    const double modelCharacteristic = renderLodPlacedCharacteristicSize(lod);

    const double featureMin = removableFeatures.empty() ? 0.0 : *std::min_element(removableFeatures.begin(), removableFeatures.end());
    const double featureMedian = percentile(removableFeatures, 0.50);
    const double featureMax = removableFeatures.empty() ? 0.0 : *std::max_element(removableFeatures.begin(), removableFeatures.end());
    const double seedFeature = removableFeatures.empty()
        ? std::max(0.01, modelCharacteristic / 512.0)
        : std::max(0.001, percentile(removableFeatures, 0.15));

    json levels = json::array();
    double threshold = std::max(seedFeature * 2.0, modelCharacteristic / 4096.0);
    for (std::size_t targetLevel = 1; targetLevel < LodMaximumRecommendedLevels; ++targetLevel)
    {
        std::size_t candidateRenderedTriangles = 0;
        std::size_t candidateStoredTriangles = 0;
        std::size_t candidateComponents = 0;
        for (const auto& analysis : analyses)
        {
            for (const auto& component : analysis.components)
            {
                if (component.protectedStructure || component.featureMeters >= threshold) continue;
                candidateRenderedTriangles += component.triangleIndices.size() * analysis.usageCount;
                candidateStoredTriangles += component.triangleIndices.size();
                ++candidateComponents;
            }
        }
        const double relativeGeometricError = threshold / modelCharacteristic;
        levels.push_back({
            {"level", targetLevel},
            {"featureThresholdMeters", threshold},
            {"relativeGeometricError", relativeGeometricError},
            {"twoPixelProjectedCharacteristicPx", LodVisibilityCutoffPx / relativeGeometricError},
            {"candidateComponents", candidateComponents},
            {"candidateRenderedTriangles", candidateRenderedTriangles},
            {"candidateStoredTriangles", candidateStoredTriangles},
            {"candidatePercent", totalRenderedTriangles == 0 ? 0.0 :
                100.0 * static_cast<double>(candidateRenderedTriangles) / static_cast<double>(totalRenderedTriangles)},
            {"existing", targetLevel < m_asset.renderLods.size()}
        });
        if (threshold >= modelCharacteristic * 0.25) break;
        threshold *= LodFeatureBandFactor;
    }

    m_server.broadcastText(json({
        {"type", "lod_analysis_result"},
        {"lodIndex", lodIndex},
        {"visibilityCutoffPx", LodVisibilityCutoffPx},
        {"modelCharacteristicSourceUnits", modelCharacteristic},
        {"analyzedGeometries", analyzedGeometries},
        {"additionalGeometryCount", additionalGeometryCount},
        {"connectedComponents", totalConnectedComponents},
        {"removableComponents", removableComponents},
        {"totalRenderedTriangles", totalRenderedTriangles},
        {"totalStoredTriangles", totalStoredTriangles},
        {"meshes", std::move(meshCatalog)},
        {"featureMinMeters", featureMin},
        {"featureMedianMeters", featureMedian},
        {"featureMaxMeters", featureMax},
        {"recommendedTotalLods", 1u + levels.size()},
        {"levels", std::move(levels)},
        {"algorithm", "disconnected_component_middle_obb_extent_v1"}
    }).dump());
    sendStatus(
        "LOD analysis complete: " + std::to_string(totalConnectedComponents) +
        " connected components, " + std::to_string(removableComponents) +
        " safe first-pass detail candidates");
    return true;
}

bool ModelAssetEditorSession::previewLodComponentCull(
    std::size_t lodIndex,
    double thresholdMeters)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    if (!std::isfinite(thresholdMeters) || thresholdMeters <= 0.0)
    {
        sendStatus("LOD preview threshold must be positive", true);
        return false;
    }

    const auto& lod = m_asset.renderLods.at(lodIndex);
    std::vector<std::size_t> usage(lod.geometries.size(), 0);
    for (const auto& node : lod.nodes)
        if (node.enabled && node.geometryIndex >= 0 &&
            static_cast<std::size_t>(node.geometryIndex) < usage.size())
            ++usage[static_cast<std::size_t>(node.geometryIndex)];

    json geometries = json::array();
    std::size_t removedUniqueTriangles = 0;
    std::size_t removedRenderedTriangles = 0;
    std::size_t totalRenderedTriangles = 0;
    std::size_t totalStoredTriangles = 0;
    std::size_t removedComponents = 0;

    sendStatus(
        "Building non-destructive LOD component-cull preview...",
        false,
        "working");
    for (std::size_t geometryIndex = 0; geometryIndex < lod.geometries.size(); ++geometryIndex)
    {
        const auto& geometry = lod.geometries[geometryIndex];
        totalStoredTriangles += geometry.mesh.triangles.size();
        totalRenderedTriangles += geometry.mesh.triangles.size() * usage[geometryIndex];

        std::vector<std::size_t> removed;
        for (const auto& component : analyzeConnectedComponents(geometry.mesh))
        {
            if (component.protectedStructure || component.featureMeters >= thresholdMeters) continue;
            removed.insert(removed.end(), component.triangleIndices.begin(), component.triangleIndices.end());
            ++removedComponents;
        }
        if (removed.empty()) continue;
        const auto ranges = compressTriangleRanges(std::move(removed));
        std::size_t removedCount = 0;
        json jsonRanges = json::array();
        for (const auto& range : ranges)
        {
            jsonRanges.push_back(json::array({range[0], range[1]}));
            removedCount += range[1];
        }
        removedUniqueTriangles += removedCount;
        removedRenderedTriangles += removedCount * usage[geometryIndex];
        geometries.push_back({
            {"geometryIndex", geometryIndex},
            {"geometryId", geometry.id},
            {"removedTriangleRanges", std::move(jsonRanges)},
            {"originalTriangles", geometry.mesh.triangles.size()},
            {"remainingTriangles", geometry.mesh.triangles.size() - std::min(geometry.mesh.triangles.size(), removedCount)},
            {"removedTriangles", removedCount},
            {"usageCount", usage[geometryIndex]},
            {"isSourceVariant", isRenderVariantGeometryId(geometry.id)}
        });
    }

    m_server.broadcastText(json({
        {"type", "lod_generator_preview_result"},
        {"algorithm", "component_cull_v1"},
        {"lodIndex", lodIndex},
        {"thresholdMeters", thresholdMeters},
        {"visibilityCutoffPx", LodVisibilityCutoffPx},
        {"removedComponents", removedComponents},
        {"removedUniqueTriangles", removedUniqueTriangles},
        {"removedRenderedTriangles", removedRenderedTriangles},
        {"totalRenderedTriangles", totalRenderedTriangles},
        {"totalStoredTriangles", totalStoredTriangles},
        {"removedRenderedPercent", totalRenderedTriangles == 0 ? 0.0 :
            100.0 * static_cast<double>(removedRenderedTriangles) / static_cast<double>(totalRenderedTriangles)},
        {"geometries", std::move(geometries)}
    }).dump());
    sendStatus(
        "LOD preview ready: " + std::to_string(removedComponents) +
        " disconnected detail components / " + std::to_string(removedRenderedTriangles) +
        " rendered triangles hidden; asset unchanged");
    return true;
}



bool ModelAssetEditorSession::applyGeneratedLods(
    std::size_t sourceLodIndex,
    const nlohmann::json& levels)
{
    if (sourceLodIndex != 0)
    {
        sendStatus("Generated LOD authoring currently requires canonical LOD0 as source", true);
        return false;
    }
    if (!levels.is_array())
    {
        sendStatus("Generated LOD apply request has no level selection", true);
        return false;
    }
    if (!ensureAllLodsLoaded()) return false;

    std::string canonicalReason;
    if (!verifyLoadedWorkingSetCanonical(&canonicalReason))
    {
        sendStatus("Generated LOD apply blocked: canonical SOURCE invariant failed: " + canonicalReason, true);
        return false;
    }
    std::string preflightReason;
    if (!modelPreflightReadyForLod(&preflightReason))
    {
        sendStatus("Generated LOD apply blocked by Model Preflight: " + preflightReason, true);
        return false;
    }
    if (m_asset.renderLods.empty() || sourceLodIndex >= m_asset.renderLods.size())
    {
        sendStatus("Generated LOD apply blocked: LOD0 is missing", true);
        return false;
    }

    struct Selection
    {
        std::size_t level = 0;
        double thresholdMeters = 0.0;
    };
    std::vector<Selection> selected;
    for (const auto& item : levels)
    {
        if (!item.is_object() || !item.value("selected", true)) continue;
        const auto level = item.value("level", std::size_t(0));
        const double threshold = item.value("thresholdMeters", 0.0);
        if (level == 0 || level > 32 || !std::isfinite(threshold) || threshold <= 0.0)
        {
            sendStatus("Generated LOD apply request contains an invalid level/threshold", true);
            return false;
        }
        selected.push_back({level, threshold});
    }
    std::sort(selected.begin(), selected.end(), [](const Selection& a, const Selection& b) {
        return a.level < b.level;
    });
    selected.erase(std::unique(selected.begin(), selected.end(), [](const Selection& a, const Selection& b) {
        return a.level == b.level;
    }), selected.end());
    if (selected.empty())
    {
        sendStatus("NO CHANGES: no generated LOD levels were selected");
        return true;
    }

    // RenderLod is vector-backed and therefore cannot contain holes. Existing
    // authored levels may be kept by leaving their checkbox off, but a brand-new
    // LOD slot may only be created when all preceding slots already exist or are
    // selected in this apply operation.
    std::size_t simulatedLodCount = m_asset.renderLods.size();
    for (const auto& selection : selected)
    {
        if (selection.level > simulatedLodCount)
        {
            sendStatus(
                "Cannot create LOD" + std::to_string(selection.level) +
                " while LOD" + std::to_string(simulatedLodCount) +
                " is missing; generated LOD slots must remain contiguous", true);
            return false;
        }
        if (selection.level == simulatedLodCount) ++simulatedLodCount;
    }

    const RenderLod source = m_asset.renderLods[sourceLodIndex];
    const double sourceCharacteristic = renderLodPlacedCharacteristicSize(source);
    const auto sourceBaseVisuals = m_baseVisualIds.find(sourceLodIndex);
    const auto sourceExtraIds = m_sourceExtraMeshIds.find(sourceLodIndex);
    const auto sourceTopologyClasses = m_geometryTopologyClasses.find(sourceLodIndex);

    struct Candidate
    {
        Selection selection;
        RenderLod lod;
        std::size_t removedTriangles = 0;
        std::size_t removedComponents = 0;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(selected.size());

    sendStatus(
        "Generating " + std::to_string(selected.size()) +
        " selected LOD level(s) from canonical LOD0...", false, "working");
    std::size_t generationCompleted = 0;

    // Build and validate every selected level before mutating the authored
    // asset. APPLY is transactional: one bad generated mesh must not leave half
    // the LOD chain replaced and half untouched.
    for (const auto& selection : selected)
    {
        sendProgress(
            "working",
            "GENERATE LOD" + std::to_string(selection.level),
            generationCompleted, selected.size());
        Candidate candidate;
        candidate.selection = selection;
        candidate.lod = buildGeneratedComponentCullLod(
            source,
            selection.level,
            selection.thresholdMeters,
            &candidate.removedTriangles,
            &candidate.removedComponents);
        candidate.lod.relativeGeometricError = static_cast<float>(
            selection.thresholdMeters / sourceCharacteristic);
        for (std::size_t gi = 0; gi < candidate.lod.geometries.size(); ++gi)
        {
            const auto analysis = analyzeCanonicalMesh(candidate.lod.geometries[gi].mesh);
            if (analysis.structuralInvalid || analysis.degenerateTriangles != 0 ||
                analysis.duplicateTriangles != 0 || analysis.windingConflicts != 0 ||
                analysis.insideOutClosedComponents != 0)
            {
                sendStatus(
                    "Generated LOD" + std::to_string(selection.level) + "/G" + std::to_string(gi) +
                    " violates canonical geometry contract" +
                    (analysis.invalidReason.empty() ? std::string() : ": " + analysis.invalidReason), true);
                return false;
            }
        }
        candidates.push_back(std::move(candidate));
        ++generationCompleted;
        sendProgress(
            "working",
            "GENERATE LOD" + std::to_string(selection.level),
            generationCompleted, selected.size());
    }

    json applied = json::array();
    std::size_t replaced = 0;
    std::size_t created = 0;
    for (auto& candidate : candidates)
    {
        const auto& selection = candidate.selection;
        const bool replacing = selection.level < m_asset.renderLods.size();
        if (replacing)
        {
            m_asset.renderLods[selection.level] = std::move(candidate.lod);
            ++replaced;
        }
        else
        {
            m_asset.renderLods.push_back(std::move(candidate.lod));
            ++created;
        }

        if (m_lodState.size() < m_asset.renderLods.size())
            m_lodState.resize(m_asset.renderLods.size());
        m_lodState[selection.level].loaded = true;
        m_lodState[selection.level].dirty = true;

        // Generated LODs inherit stable authoring identity from LOD0. This is
        // essential for additional/replacement meshes: their opaque variant id
        // and the base visual family they may replace must stay the same across
        // every generated render document.
        if (sourceBaseVisuals != m_baseVisualIds.end())
            m_baseVisualIds[selection.level] = sourceBaseVisuals->second;
        else
            m_baseVisualIds.erase(selection.level);
        if (sourceExtraIds != m_sourceExtraMeshIds.end())
            m_sourceExtraMeshIds[selection.level] = sourceExtraIds->second;
        else
            m_sourceExtraMeshIds.erase(selection.level);
        if (sourceTopologyClasses != m_geometryTopologyClasses.end())
            m_geometryTopologyClasses[selection.level] = sourceTopologyClasses->second;
        else
            m_geometryTopologyClasses.erase(selection.level);
        m_rawMeshSnapshots.erase(selection.level);

        auto& preparation = m_meshPreparationRecords[selection.level];
        preparation.clear();
        const auto& resident = m_asset.renderLods[selection.level];
        for (std::size_t gi = 0; gi < resident.geometries.size(); ++gi)
        {
            const auto& geometry = resident.geometries[gi];
            const auto& sourceGeometry = source.geometries[gi];
            const auto analysis = analyzeCanonicalMesh(geometry.mesh);
            MeshPreparationRecord record;
            record.algorithm = GeneratedLodComponentCullAlgorithmId;
            record.sourceRenderVertices = sourceGeometry.mesh.vertices.size();
            record.sourceTriangles = sourceGeometry.mesh.triangles.size();
            record.geometricPoints = analysis.geometricPoints;
            record.outputRenderVertices = analysis.renderVertices;
            record.outputTriangles = analysis.triangles;
            record.normalIslands = analysis.renderVertices;
            record.rebuiltEdges = geometry.mesh.edges.size();
            record.outputFingerprint = canonicalMeshFingerprint(geometry.mesh);
            preparation[geometry.id] = std::move(record);
        }

        applied.push_back({
            {"level", selection.level},
            {"replaced", replacing},
            {"thresholdMeters", selection.thresholdMeters},
            {"relativeGeometricError", resident.relativeGeometricError},
            {"geometries", resident.geometries.size()},
            {"additionalGeometries", std::count_if(
                resident.geometries.begin(), resident.geometries.end(),
                [](const RenderGeometryDefinition& geometry) { return isRenderVariantGeometryId(geometry.id); })},
            {"removedTriangles", candidate.removedTriangles},
            {"removedComponents", candidate.removedComponents}
        });
    }

    if (!m_asset.renderLods.empty())
        m_asset.renderLods[0].relativeGeometricError = 0.0f;
    markManifestDirty();
    for (const auto& selection : selected) markLodDirty(selection.level);
    invalidateWizardFrom("lods");

    json invalidatedPayloads = json::array();
    for (const auto& selection : selected) invalidatedPayloads.push_back(selection.level);

    // Do not publish every generated geometry payload in one giant browser
    // message. The station can easily turn that into hundreds of megabytes of
    // JSON and makes APPLY look frozen. Publish compact authored metadata now;
    // an individual LOD payload is requested only when the author views it.
    sendAssetMetadata({{"invalidatedLodPayloads", invalidatedPayloads}});
    m_server.broadcastText(json({
        {"type", "lod_generator_apply_result"},
        {"sourceLodIndex", sourceLodIndex},
        {"replaced", replaced},
        {"created", created},
        {"levels", std::move(applied)}
    }).dump());
    sendStatus(
        "Generated LODs applied to authored asset: replaced=" + std::to_string(replaced) +
        ", created=" + std::to_string(created) +
        "; press SAVE to persist the current authored LODs");
    return true;
}

bool ModelAssetEditorSession::previewLodCoplanarCollapse(std::size_t lodIndex)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    const auto& lod = m_asset.renderLods.at(lodIndex);

    std::vector<std::size_t> usage(lod.geometries.size(), 0);
    for (const auto& node : lod.nodes)
        if (node.enabled && node.geometryIndex >= 0 &&
            static_cast<std::size_t>(node.geometryIndex) < usage.size())
            ++usage[static_cast<std::size_t>(node.geometryIndex)];

    json geometries = json::array();
    std::size_t totalUniqueTriangles = 0;
    std::size_t totalRenderedTriangles = 0;
    std::size_t removedUniqueTriangles = 0;
    std::size_t addedUniqueTriangles = 0;
    std::size_t removedRenderedTriangles = 0;
    std::size_t addedRenderedTriangles = 0;
    std::size_t candidateRegions = 0;
    std::size_t collapsedRegions = 0;

    sendStatus("Building non-destructive coplanar-region collapse preview...", false, "working");
    for (std::size_t geometryIndex = 0; geometryIndex < lod.geometries.size(); ++geometryIndex)
    {
        sendProgress("working", "COPLANAR LOD PREVIEW", geometryIndex, lod.geometries.size());
        const auto& geometry = lod.geometries[geometryIndex];
        totalUniqueTriangles += geometry.mesh.triangles.size();
        totalRenderedTriangles += geometry.mesh.triangles.size() * usage[geometryIndex];

        const auto preview = analyzeCoplanarCollapse(geometry.mesh);
        candidateRegions += preview.candidateRegions;
        collapsedRegions += preview.collapsedRegions;
        if (preview.removedTriangleIndices.empty() || preview.addedTriangleIndices.empty()) continue;

        const auto ranges = compressTriangleRanges(preview.removedTriangleIndices);
        json jsonRanges = json::array();
        std::size_t removedCount = 0;
        for (const auto& range : ranges)
        {
            jsonRanges.push_back(json::array({range[0], range[1]}));
            removedCount += range[1];
        }
        const std::size_t addedCount = preview.addedTriangleIndices.size() / 3u;
        removedUniqueTriangles += removedCount;
        addedUniqueTriangles += addedCount;
        removedRenderedTriangles += removedCount * usage[geometryIndex];
        addedRenderedTriangles += addedCount * usage[geometryIndex];

        geometries.push_back({
            {"geometryIndex", geometryIndex},
            {"geometryId", geometry.id},
            {"removedTriangleRanges", std::move(jsonRanges)},
            {"addedTriangles", preview.addedTriangleIndices},
            {"originalTriangles", geometry.mesh.triangles.size()},
            {"remainingTriangles", geometry.mesh.triangles.size() - removedCount + addedCount},
            {"removedTriangles", removedCount},
            {"addedTriangleCount", addedCount},
            {"collapsedRegions", preview.collapsedRegions},
            {"candidateRegions", preview.candidateRegions},
            {"usageCount", usage[geometryIndex]},
            {"isSourceVariant", isRenderVariantGeometryId(geometry.id)}
        });
    }

    sendProgress("working", "COPLANAR LOD PREVIEW", lod.geometries.size(), lod.geometries.size());

    const std::size_t remainingUniqueTriangles =
        totalUniqueTriangles - std::min(totalUniqueTriangles, removedUniqueTriangles) + addedUniqueTriangles;
    const std::size_t remainingRenderedTriangles =
        totalRenderedTriangles - std::min(totalRenderedTriangles, removedRenderedTriangles) + addedRenderedTriangles;

    m_server.broadcastText(json({
        {"type", "lod_generator_preview_result"},
        {"algorithm", "coplanar_collapse_v1"},
        {"lodIndex", lodIndex},
        {"visibilityCutoffPx", LodVisibilityCutoffPx},
        {"candidateRegions", candidateRegions},
        {"collapsedRegions", collapsedRegions},
        {"totalUniqueTriangles", totalUniqueTriangles},
        {"remainingUniqueTriangles", remainingUniqueTriangles},
        {"totalRenderedTriangles", totalRenderedTriangles},
        {"remainingRenderedTriangles", remainingRenderedTriangles},
        {"removedUniqueTriangles", removedUniqueTriangles},
        {"addedUniqueTriangles", addedUniqueTriangles},
        {"removedRenderedTriangles", removedRenderedTriangles},
        {"addedRenderedTriangles", addedRenderedTriangles},
        {"reductionRenderedPercent", totalRenderedTriangles == 0 ? 0.0 :
            100.0 * static_cast<double>(totalRenderedTriangles - remainingRenderedTriangles) /
            static_cast<double>(totalRenderedTriangles)},
        {"geometries", std::move(geometries)}
    }).dump());
    sendStatus(
        "Coplanar preview ready: " + std::to_string(collapsedRegions) +
        " regions collapsed; " + std::to_string(totalRenderedTriangles) +
        " -> " + std::to_string(remainingRenderedTriangles) +
        " rendered triangles; asset unchanged");
    return true;
}

bool ModelAssetEditorSession::saveSettings(
    const std::filesystem::path& sourceAssetsRoot,
    const std::filesystem::path& compiledModelsRoot,
    const std::filesystem::path& workingFilesRoot,
    const std::string& locale)
{
    static const std::array<const char*, 5> supportedLocales {"en", "ru", "zh-Hans", "es", "ja"};
    if (std::find(supportedLocales.begin(), supportedLocales.end(), locale) == supportedLocales.end())
    {
        sendStatus("Unsupported editor locale: " + locale, true);
        return false;
    }
    if (sourceAssetsRoot.empty() || compiledModelsRoot.empty() || workingFilesRoot.empty())
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
    ec.clear();
    std::filesystem::create_directories(workingFilesRoot, ec);
    if (ec)
    {
        sendStatus("Cannot create WORKING files directory: " + workingFilesRoot.generic_string(), true);
        return false;
    }

    m_sourceAssetsRoot = sourceAssetsRoot.lexically_normal();
    m_compiledModelsRoot = compiledModelsRoot.lexically_normal();
    m_workingFilesRoot = workingFilesRoot.lexically_normal();
    m_locale = locale;

    if (!writeSettingsFile())
        return false;

    // Acknowledge the write first. Catalog/asset refreshes can be comparatively
    // expensive in the Web UI and must never hide the result of pressing Save.
    m_server.broadcastText(json({
        {"type", "settings_saved"},
        {"sourceAssetsRoot", m_sourceAssetsRoot.generic_string()},
        {"compiledModelsRoot", m_compiledModelsRoot.generic_string()},
        {"workingFilesRoot", m_workingFilesRoot.generic_string()},
        {"settingsPath", settingsPath().generic_string()},
        {"locale", m_locale}
    }).dump());
    std::cerr << "[ModelAssetEditor] settings saved: "
              << settingsPath().generic_string()
              << " source=" << m_sourceAssetsRoot.generic_string()
              << " compiled=" << m_compiledModelsRoot.generic_string()
              << " working=" << m_workingFilesRoot.generic_string()
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
    m_dirty = m_editorStateDirty || m_manifestDirty || std::any_of(
        m_lodState.begin(), m_lodState.end(), [](const LodEditState& state) { return state.dirty; });
}

void ModelAssetEditorSession::resetLodState(bool loaded, bool dirty)
{
    m_lodState.assign(lodCount(), LodEditState{loaded, dirty});
    if (loaded)
        for (auto& lod : m_asset.renderLods)
        {
            lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
            lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
        }
    m_manifestDirty = dirty;
    m_editorStateDirty = dirty;
    syncDirty();
}

void ModelAssetEditorSession::markManifestDirty()
{
    m_manifestDirty = true;
    syncDirty();
}

void ModelAssetEditorSession::markEditorStateDirty()
{
    m_editorStateDirty = true;
    syncDirty();
}

void ModelAssetEditorSession::markLodDirty(std::size_t lodIndex)
{
    if (lodIndex >= m_lodState.size())
        m_lodState.resize(lodIndex + 1);
    if (lodIndex < m_asset.renderLods.size())
    {
        auto& lod = m_asset.renderLods[lodIndex];
        lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
        lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
    }
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

bool ModelAssetEditorSession::loadLodData(
    std::size_t lodIndex,
    bool forceReload,
    std::string* error)
{
    if (error) error->clear();
    const auto fail = [&](const std::string& message)
    {
        if (error) *error = message;
        return false;
    };

    if (m_selectedId.empty() || m_asset.assetId.empty())
        return fail("No asset selected");
    if (lodIndex >= m_lodState.size())
        return fail("LOD" + std::to_string(lodIndex) + " is not declared by this asset");

    auto& state = m_lodState[lodIndex];
    if (state.loaded && !forceReload)
        return true;
    if (state.dirty && !forceReload)
        return fail("LOD" + std::to_string(lodIndex) +
            " has unsaved changes; use Reload LOD only after confirming discard");

    const auto manifest = std::filesystem::exists(workingAssetPath())
        ? workingAssetPath() : compiledPath(m_selectedId);
    if (!std::filesystem::exists(manifest))
        return fail("Cannot load independent LOD before a WORKING or production package has been saved");

    std::string loadError;
    if (!ModelAssetBinary::loadLod(manifest.string(), m_asset, lodIndex, &loadError))
        return fail("LOD" + std::to_string(lodIndex) + " load failed: " + loadError);

    state.loaded = true;
    state.dirty = false;
    syncDirty();
    return true;
}

bool ModelAssetEditorSession::loadLodOnly(std::size_t lodIndex, bool forceReload)
{
    if (lodIndex >= m_lodState.size())
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " is not declared by this asset", true);
        return false;
    }
    const bool wasLoaded = m_lodState[lodIndex].loaded;
    if (wasLoaded && !forceReload)
    {
        // The LOD may have become backend-resident because a computation needed
        // it while the browser still has no payload. An explicit UI LOAD must
        // therefore publish the resident document rather than treating backend
        // residency as proof that the viewport already owns it.
        sendAsset({lodIndex});
        sendStatus("LOD" + std::to_string(lodIndex) + " was already resident; viewport payload published");
        return true;
    }

    const auto manifest = std::filesystem::exists(workingAssetPath())
        ? workingAssetPath() : compiledPath(m_selectedId);
    const auto lodPath = ModelAssetBinary::lodPayloadPath(manifest.string(), lodIndex);
    sendStatus((forceReload ? "Reloading " : "Loading ") + std::string("LOD") +
        std::to_string(lodIndex) + "...", false, "reading");
    sendProgress("reading", forceReload ? "RELOAD LOD" : "LOAD LOD", 0, 1, lodPath);

    std::string error;
    if (!loadLodData(lodIndex, forceReload, &error))
    {
        sendStatus(error, true);
        return false;
    }

    sendProgress("reading", forceReload ? "RELOAD LOD" : "LOAD LOD", 1, 1, lodPath);
    // Explicit LOAD/RELOAD is a viewport-facing command. Internal algorithms use
    // loadLodData()/ensure*() and therefore do not publish geometry merely because
    // they needed it in backend memory.
    sendAsset({lodIndex});
    sendStatus(std::string(forceReload ? "Reloaded " : "Loaded ") + "LOD" +
        std::to_string(lodIndex) + " from " + lodPath.filename().string());
    return true;
}

bool ModelAssetEditorSession::ensureLodLoaded(std::size_t lodIndex)
{
    if (lodIndex >= m_lodState.size())
        return false;
    if (m_lodState[lodIndex].loaded)
        return true;
    std::string error;
    if (!loadLodData(lodIndex, false, &error))
    {
        sendStatus(error, true);
        return false;
    }
    return true;
}

bool ModelAssetEditorSession::ensureAllLodsLoaded()
{
    for (std::size_t lodIndex = 0; lodIndex < m_lodState.size(); ++lodIndex)
    {
        if (m_lodState[lodIndex].loaded) continue;
        std::string error;
        if (!loadLodData(lodIndex, false, &error))
        {
            sendStatus(error, true);
            return false;
        }
    }
    return true;
}

bool ModelAssetEditorSession::loadAllDeclaredLodsForSource()
{
    if (m_asset.assetId.empty()) return false;
    if (m_lodState.size() < m_asset.renderLods.size())
        m_lodState.resize(m_asset.renderLods.size());

    const auto manifest = std::filesystem::exists(workingAssetPath())
        ? workingAssetPath() : compiledPath(m_selectedId);
    for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex)
    {
        if (m_lodState[lodIndex].loaded) continue;
        if (!std::filesystem::exists(manifest))
        {
            sendStatus("Cannot load complete SOURCE set: working/production package is missing while LOD" +
                std::to_string(lodIndex) + " is not resident", true);
            return false;
        }
        std::string error;
        const auto lodPath = ModelAssetBinary::lodPayloadPath(manifest.string(), lodIndex);
        sendProgress("reading", "SOURCE LOAD LOD", lodIndex, m_asset.renderLods.size(), lodPath);
        if (!loadLodData(lodIndex, false, &error))
        {
            sendStatus("Cannot load complete SOURCE set: " + error, true);
            return false;
        }
    }
    syncDirty();
    sendProgress("reading", "SOURCE LOAD LOD", m_asset.renderLods.size(), m_asset.renderLods.size());
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
        {"version", 3},
        {"sourceAssetsRoot", m_sourceAssetsRoot.generic_string()},
        {"compiledModelsRoot", m_compiledModelsRoot.generic_string()},
        {"workingFilesRoot", m_workingFilesRoot.generic_string()},
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
        {"workingFilesRoot", m_workingFilesRoot.generic_string()},
        {"defaultSourceAssetsRoot", defaultEditorSourceAssetsRoot(m_sourceRoot).generic_string()},
        {"defaultCompiledModelsRoot", (m_sourceRoot / "src" / "assets" / "compiled" / "models").generic_string()},
        {"defaultWorkingFilesRoot", (m_sourceRoot / "build" / "tools" / "model_asset_editor" / "workspaces").generic_string()},
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
            {"legacyVersion", legacyVersion},
            {"sourceAuthority", entry.sourceAuthority == CatalogSourceAuthority::Folder ? "folder" : "runtime_registry"},
            {"sourceDirectory", entry.sourceDirectory.generic_string()}
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

    const bool sameSelection = m_selectedId == id;
    const bool preservePhysicalProfile = forceReimport && sameSelection;
    const PhysicalSizeProfile previousPhysicalProfile = m_asset.physicalSize;
    const bool preserveOrientationOverrides = forceReimport && sameSelection;
    const auto previousOrientationOverrides = m_meshOrientationOverrides;
    if (!sameSelection)
    {
        m_workingSavedAtUtc.clear();
        m_workingSaveRevision = 0;
    }
    m_openAuthority = "none";
    m_selectedId = id;
    m_loadedSourceAssetDirectory = it->sourceDirectory.lexically_normal();
    loadWizardState();
    if (preserveOrientationOverrides) m_meshOrientationOverrides = previousOrientationOverrides;
    ModelAsset loaded;
    std::string error;
    std::string warning;
    bool sourceImported = false;
    bool productionLoaded = false;
    bool workingLoaded = false;
    const auto working = workingAssetPath();
    const auto binary = compiledPath(id);
    const auto legacyBinary = legacyCompiledPath(id);
    const bool haveWorking = std::filesystem::exists(working);
    const bool havePackage = std::filesystem::exists(binary);
    const bool haveLegacyV2 = !havePackage && std::filesystem::exists(legacyBinary);
    const auto readPath = havePackage ? binary : legacyBinary;
    const auto importProgress = [&](const ImportProgress& update)
    {
        sendProgress("reading", update.stage, update.completed, update.total, update.path);
    };
    // Compatibility phrase retained for architecture guards: source OBJ/assembly.
    const auto importSelectedSource = [&](ModelAsset& target) -> bool
    {
        if (it->sourceAuthority == CatalogSourceAuthority::Folder &&
            !sourceFolderAssetAvailable(m_sourceAssetsRoot, it->sourceDirectory))
        {
            error = "folder-authoritative source is unavailable for '" + it->id +
                "': expected " + it->sourceDirectory.generic_string() +
                "/LOD0/*.obj under configured source root " +
                m_sourceAssetsRoot.generic_string();
            return false;
        }
        if (it->bootstrapMode == CatalogBootstrapMode::RuntimeAssembly)
            return importRuntimeAssembly(
                m_sourceAssetsRoot, it->sourceDirectory, it->type, it->id, it->displayName, target,
                &error, &warning, importProgress);
        return importSourceFolderAsset(
            m_sourceAssetsRoot, it->sourceDirectory, it->type, it->id,
            it->displayName, target, &error, &warning, importProgress);
    };

    // v0.10.33: the saved WORKING ASSET is the editor resume head.
    // There is no snapshot history and no background save.
    if (!forceReimport && haveWorking)
    {
        sendStatus("Reading persistent working manifest...", false, "reading");
        sendProgress("reading", "READ WORKING MANIFEST", 0, 1, working);
        bool legacyWorking = false;
        if (!ModelAssetBinary::loadManifest(working.string(), loaded, &legacyWorking, &error))
        {
            sendStatus("Cannot load persistent working manifest: " + error, true);
            return false;
        }
        if (legacyWorking)
        {
            sendStatus("Persistent WORKING ASSET is not a v4 package; explicit SOURCE reimport is required", true);
            return false;
        }
        if (!loaded.assetId.empty() && loaded.assetId != id)
        {
            sendStatus("Working asset id '" + loaded.assetId + "' does not match selected asset '" + id + "'", true);
            return false;
        }
        m_asset = std::move(loaded);
        resetLodState(false, false);
        std::string lodError;
        if (m_asset.renderLods.empty() || !loadLodData(0, false, &lodError))
        {
            sendStatus("Cannot load WORKING LOD0: " + lodError, true);
            return false;
        }

        EditorAuthoringState workingEditorState;
        StageValidityState workingValidity;
        std::string stateError;
        std::string savedAtUtc;
        std::uint64_t saveRevision = 0;
        std::filesystem::path savedSourceDirectory;
        if (loadWorkingEditorState(
                workingEditorState, workingValidity, &savedAtUtc, &saveRevision,
                &savedSourceDirectory, &stateError))
        {
            applyEditorAuthoringState(std::move(workingEditorState));
            applyStageValidity(workingValidity);
            m_workingSavedAtUtc = savedAtUtc;
            m_workingSaveRevision = saveRevision;
            if (!savedSourceDirectory.empty()) m_loadedSourceAssetDirectory = savedSourceDirectory.lexically_normal();
        }
        else
        {
            applyEditorAuthoringState(EditorAuthoringState{});
            applyStageValidity(StageValidityState{});
            m_workingSavedAtUtc.clear();
            m_workingSaveRevision = 0;
            warning = "Persistent working geometry loaded, but its editor_state.json is unavailable or mismatched (" +
                stateError + "). Editor-only PREPARE/topology/source-baseline evidence requires review.";
            markEditorStateDirty();
        }
        // Folder-authoritative editor sessions keep every declared render LOD
        // resident. The source/change graph is per-mesh across all LODs; a hidden
        // unloaded sibling is not an acceptable source-maintenance state.
        if (it->sourceAuthority == CatalogSourceAuthority::Folder && !ensureAllLodsLoaded()) return false;
        if (m_asset.physicalSize.geometrySpace == PhysicalGeometrySpace::Meters)
        {
            if (!m_asset.physicalSize.enabled || !std::isfinite(m_asset.physicalSize.sourceToMeters) ||
                m_asset.physicalSize.sourceToMeters <= 0.0f)
            {
                sendStatus("WORKING package says geometry is metric but has no valid sourceToMeters calibration", true);
                return false;
            }
            if (!ensureAllLodsLoaded()) return false;
            scaleAuthoringDistancesUniform(m_asset, 1.0f / m_asset.physicalSize.sourceToMeters);
            m_asset.physicalSize.geometrySpace = PhysicalGeometrySpace::Authoring;
            markManifestDirty();
            markAllLoadedLodsDirty();
            warning = warning.empty()
                ? "Metric WORKING package was converted back to raw authoring space in memory. SAVE to persist the corrected editor contract."
                : warning + " Metric WORKING package was converted back to raw authoring space in memory.";
        }
        synchronizeMeshSourceRecords(true);
        reconcileAuthoringVisualRegistry();
        workingLoaded = true;
        m_openAuthority = "working";
        sendProgress("reading", "READ WORKING LODS", m_asset.renderLods.size(), m_asset.renderLods.size());
    }
    else if (!forceReimport && (havePackage || haveLegacyV2))
    {
        sendStatus("Reading production model asset " + readPath.filename().string() + "...", false, "reading");
        sendProgress("reading", "READ PRODUCTION", 0, 1, readPath);
        bool legacyPackage = false;
        if (!ModelAssetBinary::loadManifest(readPath.string(), loaded, &legacyPackage, &error))
        {
            if (error == "unsupported model asset version")
            {
                sendStatus("Compiled asset format is obsolete; reimporting source into the working asset...", false, "reading");
                if (!importSelectedSource(loaded))
                {
                    sendStatus("Cannot reimport obsolete compiled asset: " + error, true);
                    return false;
                }
                buildIndependentRenderLodsFromLegacy(loaded);
                loaded.formatVersion = ModelAssetFormatVersion;
                m_asset = std::move(loaded);
                resetLodState(true, true);
                sourceImported = true;
                m_openAuthority = "source";
            }
            else
            {
                sendStatus("Cannot load production asset: " + error, true);
                return false;
            }
        }
        else if (legacyPackage)
        {
            const std::uint32_t oldVersion = loaded.formatVersion;
            sendProgress("reading", "MIGRATE LEGACY PACKAGE", 0, 1, readPath);
            if (!ModelAssetBinary::load(readPath.string(), loaded, &error))
            {
                sendStatus("Cannot migrate legacy production asset: " + error, true);
                return false;
            }
            loaded.formatVersion = ModelAssetFormatVersion;
            m_asset = std::move(loaded);
            resetLodState(true, true);
            m_openAuthority = "production";
            warning = "Legacy production asset v" + std::to_string(oldVersion) +
                " loaded as the initial working asset. SAVE persists the workspace; BUILD upgrades production.";
            sendProgress("reading", "MIGRATE LEGACY PACKAGE", 1, 1, readPath);
        }
        else
        {
            m_asset = std::move(loaded);
            resetLodState(false, false);
            if (m_asset.renderLods.empty() || !ModelAssetBinary::loadLod(readPath.string(), m_asset, 0, &error))
            {
                sendStatus("Cannot load production LOD0: " + error, true);
                return false;
            }
            if (m_lodState.empty()) m_lodState.resize(1);
            m_lodState[0].loaded = true;
            m_lodState[0].dirty = false;
            if (it->sourceAuthority == CatalogSourceAuthority::Folder && !ensureAllLodsLoaded()) return false;
            if (m_asset.physicalSize.geometrySpace == PhysicalGeometrySpace::Meters)
            {
                if (!m_asset.physicalSize.enabled || !std::isfinite(m_asset.physicalSize.sourceToMeters) ||
                    m_asset.physicalSize.sourceToMeters <= 0.0f)
                {
                    sendStatus("Production package says geometry is metric but has no valid sourceToMeters calibration", true);
                    return false;
                }
                // Production is stored in meters, but the editor always authors in
                // SOURCE coordinates. Load every payload before applying the one
                // inverse scale so later lazy LOD loads cannot mix coordinate spaces.
                if (!ensureAllLodsLoaded()) return false;
                scaleAuthoringDistancesUniform(m_asset, 1.0f / m_asset.physicalSize.sourceToMeters);
                m_asset.physicalSize.geometrySpace = PhysicalGeometrySpace::Authoring;
                markManifestDirty();
                markAllLoadedLodsDirty();
            }
            productionLoaded = true;
            m_openAuthority = "production";
            sendProgress("reading", "READ PRODUCTION LODS", m_asset.renderLods.size(), m_asset.renderLods.size());
        }
    }
    else
    {
        sendStatus("Importing source model into the persistent working asset...", false, "reading");
        const auto sourceImportStarted = std::chrono::steady_clock::now();
        if (!importSelectedSource(loaded))
        {
            sendStatus("Cannot import source model: " + error, true);
            return false;
        }
        std::cerr << "[ModelAssetEditor][perf] source model import_ms="
                  << std::fixed << std::setprecision(1)
                  << std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - sourceImportStarted).count()
                  << '\n';
        buildIndependentRenderLodsFromLegacy(loaded);
        loaded.formatVersion = ModelAssetFormatVersion;
        m_asset = std::move(loaded);
        if (preservePhysicalProfile)
        {
            const PhysicalSizeProfile importedContext = m_asset.physicalSize;
            m_asset.physicalSize = previousPhysicalProfile;
            // A SOURCE reimport refreshes only the read-only game link. Manual
            // calibration remains the user's authority and is never auto-applied.
            m_asset.physicalSize.gameLinked = importedContext.gameLinked;
            m_asset.physicalSize.gameDimensionsMeters = importedContext.gameDimensionsMeters;
            m_asset.physicalSize.autoApplyOnSourceImport = false;
            m_asset.physicalSize.geometrySpace = PhysicalGeometrySpace::Authoring;
            if (!std::isfinite(m_asset.physicalSize.targetMeters) || m_asset.physicalSize.targetMeters <= 0.0f)
            {
                m_asset.physicalSize.axis = importedContext.axis;
                m_asset.physicalSize.targetMeters = importedContext.targetMeters;
            }
            // Old SIZE chunks do not contain the applied coefficient. Explicit
            // full SOURCE reimport is the migration boundary: raw coordinates are
            // restored first, then the old target is converted into a new stored
            // coefficient after all source meshes are resident.
            if (previousPhysicalProfile.geometrySpace == PhysicalGeometrySpace::LegacyUnknown)
            {
                m_asset.physicalSize.enabled = false;
                m_asset.physicalSize.sourceExtent = 0.0f;
                m_asset.physicalSize.sourceToMeters = 1.0f;
            }
        }
        resetLodState(true, true);
        sourceImported = true;
        m_openAuthority = "source";
    }

    if (productionLoaded)
    {
        EditorAuthoringState productionEditorState;
        StageValidityState productionValidity;
        std::string stateError;
        std::uint64_t productionRevision = 0;
        std::filesystem::path productionSourceDirectory;
        if (loadProductionEditorState(
                productionEditorState, productionValidity, &productionRevision,
                &productionSourceDirectory, &stateError))
        {
            applyEditorAuthoringState(std::move(productionEditorState));
            applyStageValidity(productionValidity);
            m_workingSaveRevision = productionRevision;
            if (!productionSourceDirectory.empty()) m_loadedSourceAssetDirectory = productionSourceDirectory.lexically_normal();
        }
        else
        {
            applyEditorAuthoringState(EditorAuthoringState{});
            applyStageValidity(StageValidityState{});
            warning = warning.empty()
                ? "Production asset loaded as the initial working copy. No matching production_state.json was found (" + stateError + "); editor-only evidence requires review."
                : warning + " No matching production_state.json was found; editor-only evidence requires review.";
        }
        synchronizeMeshSourceRecords(true);
        reconcileAuthoringVisualRegistry();
    }

    if (sourceImported)
    {
        const auto variantsStarted = std::chrono::steady_clock::now();
        if (!refreshSourceVariants(true, false)) return false;
        std::cerr << "[ModelAssetEditor][perf] additional source meshes import_ms="
                  << std::fixed << std::setprecision(1)
                  << std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - variantsStarted).count()
                  << '\n';
        m_componentMaintenanceIssues.clear();

        // SOURCE/WORKING are permanently raw authoring space. Never resize newly
        // imported meshes here. If this explicit full reimport is migrating an
        // old destructive SIZE profile, derive the new coefficient from the raw
        // complete SOURCE extent and the previously authored physical target.
        if (preservePhysicalProfile &&
            previousPhysicalProfile.geometrySpace == PhysicalGeometrySpace::LegacyUnknown)
        {
            if (!ensureAllLodsLoaded()) return false;
            const float current = authoringAxisExtent(m_asset, m_asset.physicalSize.axis);
            const float target = m_asset.physicalSize.targetMeters;
            if (std::isfinite(current) && current > 1.0e-6f &&
                std::isfinite(target) && target > 0.0f)
            {
                m_asset.physicalSize.enabled = true;
                m_asset.physicalSize.sourceExtent = current;
                m_asset.physicalSize.sourceToMeters = target / current;
                m_asset.physicalSize.geometrySpace = PhysicalGeometrySpace::Authoring;
                m_asset.physicalSize.autoApplyOnSourceImport = false;
                warning = warning.empty()
                    ? "Legacy destructive physical scale migrated by full SOURCE reimport; WORKING is now raw authoring space. SAVE to keep the migration."
                    : warning + " Legacy destructive physical scale migrated by full SOURCE reimport; WORKING is now raw authoring space.";
                markManifestDirty();
            }
        }
        captureCurrentSourceFingerprintBaseline();
    }

    if (forceReimport)
    {
        m_meshPreparationRecords.clear();
        m_rawMeshSnapshots.clear();
        m_geometryTopologyClasses.clear();
        invalidateWizardFrom("source");
    }

    // Create a saved baseline only on first adoption. Normal edits and REIMPORT
    // remain in memory until the author presses the global SAVE button.
    const bool createInitialWorkingBaseline = !haveWorking && !forceReimport;
    if (createInitialWorkingBaseline && !saveWorkingAsset(true)) return false;

    sendProgress("reading", "LOAD VIEW", 0, 1, workingAssetPath());
    sendAsset();
    if (!warning.empty()) sendStatus(warning);
    else if (forceReimport)
        sendStatus("SOURCE reimported. Nothing was saved; loaded WORKING revision remains r" +
            std::to_string(m_workingSaveRevision) + ". Use SAVE to keep it or RESTORE to discard it.");
    else if (workingLoaded)
        sendStatus("Loaded WORKING r" + std::to_string(m_workingSaveRevision) +
            (m_workingSavedAtUtc.empty() ? std::string() : "; savedAt=" + m_workingSavedAtUtc) + ".");
    else if (productionLoaded)
        sendStatus("Production asset adopted as the initial saved WORKING ASSET; production bytes were not rewritten.");
    else
        sendStatus("Source asset imported as the initial saved WORKING ASSET; BUILD has not been written yet.");
    return true;
}

bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)
{
    if (m_selectedId.empty() || m_asset.assetId.empty())
    {
        if (!quiet) sendStatus("No asset selected", true);
        return false;
    }

    // SAVE is a persistence boundary, not an implicit LOD-load operation.
    // Dirty resident LODs are serialized. Clean unloaded LOD payloads already in
    // WORKING stay byte-for-byte untouched. On the first adoption of a modern
    // production package, untouched unloaded payloads are copied without parsing.
    m_asset.formatVersion = ModelAssetFormatVersion;
    const auto path = workingAssetPath();
    std::filesystem::create_directories(path.parent_path());
    const bool manifestExists = std::filesystem::exists(path);
    const auto production = compiledPath(m_selectedId);

    std::string error;
    syncDirty();
    if (!quiet) sendStatus("Saving WORKING ASSET...", false, "writing");

    std::size_t work = (m_manifestDirty || !manifestExists) ? 1u : 0u;
    for (std::size_t i = 0; i < m_asset.renderLods.size(); ++i)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(path.string(), i);
        const bool loaded = i < m_lodState.size() && m_lodState[i].loaded;
        const bool dirty = i < m_lodState.size() && m_lodState[i].dirty;
        if (dirty || !std::filesystem::exists(lodPath)) ++work;
        if (dirty && !loaded)
        {
            sendStatus("WORKING LOD" + std::to_string(i) + " is dirty but not resident; refusing an incoherent save", true);
            return false;
        }
    }

    std::size_t completed = 0;
    for (std::size_t i = 0; i < m_asset.renderLods.size(); ++i)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(path.string(), i);
        const bool targetExists = std::filesystem::exists(lodPath);
        const bool loaded = i < m_lodState.size() && m_lodState[i].loaded;
        const bool dirty = i < m_lodState.size() && m_lodState[i].dirty;
        if (!dirty && targetExists) continue;

        if (!quiet) sendProgress("writing", "SAVE WORKING LOD" + std::to_string(i), completed, work, lodPath);
        if (loaded)
        {
            if (!ModelAssetBinary::saveLod(path.string(), m_asset, i, &error))
            {
                sendStatus("WORKING ASSET save failed at LOD" + std::to_string(i) + ": " + error, true);
                return false;
            }
            if (i >= m_lodState.size()) m_lodState.resize(i + 1);
            m_lodState[i].dirty = false;
        }
        else
        {
            // The only legal missing/unloaded case is first adoption from a
            // production v4 package. Never repair a damaged WORKING package by
            // silently borrowing geometry from another authority.
            if (manifestExists)
            {
                sendStatus("WORKING LOD" + std::to_string(i) +
                    " payload is missing while unloaded; LOAD LOD or REIMPORT SOURCE before SAVE", true);
                return false;
            }
            const auto sourceLod = ModelAssetBinary::lodPayloadPath(production.string(), i);
            std::error_code ec;
            if (!std::filesystem::is_regular_file(sourceLod, ec) || ec)
            {
                sendStatus("Cannot establish initial WORKING LOD" + std::to_string(i) +
                    ": production payload is unavailable", true);
                return false;
            }
            ec.clear();
            std::filesystem::copy_file(sourceLod, lodPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                sendStatus("Cannot copy initial WORKING LOD" + std::to_string(i) + ": " + ec.message(), true);
                return false;
            }
        }
        ++completed;
    }

    if (m_manifestDirty || !manifestExists)
    {
        if (!quiet) sendProgress("writing", "SAVE WORKING MANIFEST", completed, work, path);
        if (!ModelAssetBinary::saveManifest(path.string(), m_asset, &error))
        {
            sendStatus("WORKING ASSET manifest save failed: " + error, true);
            return false;
        }
        m_manifestDirty = false;
        ++completed;
    }
    if (!ModelAssetBinary::pruneStaleLods(path.string(), m_asset, &error))
    {
        sendStatus("WORKING ASSET saved but stale-LOD cleanup failed: " + error, true);
        return false;
    }
    syncDirty();
    if (!quiet) sendProgress("writing", "SAVE WORKING ASSET", completed, std::max<std::size_t>(work, completed), path);

    // Editor-only identities, SOURCE hashes/per-mesh stage graph, maintenance
    // debt and aggregate stage validity are part of the same single mutable
    // WORKING snapshot. There is no save history: revision is monotonic metadata
    // inside the one editor_state.json that accompanies the one WORKING package.
    synchronizeMeshSourceRecords(true);
    const std::string savedAtUtc = utcTimestampNow();
    const std::uint64_t nextSaveRevision = m_workingSaveRevision + 1;
    if (!writeWorkingEditorState(savedAtUtc, nextSaveRevision, &error))
    {
        sendStatus("WORKING ASSET bytes were saved, but editor state failed: " + error, true);
        return false;
    }
    m_workingSavedAtUtc = savedAtUtc;
    m_workingSaveRevision = nextSaveRevision;
    m_editorStateDirty = false;
    syncDirty();

    json savedLods = json::array();
    for (std::size_t i = 0; i < m_asset.renderLods.size(); ++i)
    {
        const auto lodPath = ModelAssetBinary::lodPayloadPath(path.string(), i);
        savedLods.push_back({
            {"lod", i}, {"path", lodPath.generic_string()}, {"bytes", safeFileBytes(lodPath)},
            {"declared", true}, {"loaded", i < m_lodState.size() && m_lodState[i].loaded},
            {"dirty", false}
        });
    }
    m_server.broadcastText(json({
        {"type", "working_saved"},
        {"dirty", m_dirty},
        {"savedAtUtc", m_workingSavedAtUtc},
        {"saveRevision", m_workingSaveRevision},
        {"sourceAssetDirectory", m_loadedSourceAssetDirectory.generic_string()},
        {"workingAssetPath", path.generic_string()},
        {"workingManifestBytes", safeFileBytes(path)},
        {"lodPayloads", std::move(savedLods)}
    }).dump());

    if (!quiet)
    {
        sendAssetMetadata();
        sendStatus("Saved WORKING r" + std::to_string(m_workingSaveRevision) +
            " at " + m_workingSavedAtUtc + ". Production changes only at BUILD.");
    }
    return true;
}

bool ModelAssetEditorSession::saveAsset()
{
    return saveWorkingAsset(false);
}

bool ModelAssetEditorSession::restoreWorkingAsset()
{
    if (m_selectedId.empty() || m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return false;
    }
    if (!std::filesystem::exists(workingAssetPath()))
    {
        sendStatus("No saved WORKING ASSET exists yet", true);
        return false;
    }

    const std::string id = m_selectedId;
    sendStatus("Restoring last saved WORKING ASSET...", false, "reading");
    if (!selectAsset(id, false)) return false;
    sendStatus("Restored last saved WORKING ASSET. Unsaved changes were discarded.");
    return true;
}

bool ModelAssetEditorSession::buildProductionAsset()
{
    if (m_selectedId.empty() || m_asset.assetId.empty())
    {
        sendStatus("No asset selected", true);
        return false;
    }
    if (!ensureAllLodsLoaded()) return false;
    if (!m_asset.physicalSize.enabled ||
        m_asset.physicalSize.geometrySpace != PhysicalGeometrySpace::Authoring ||
        !std::isfinite(m_asset.physicalSize.sourceToMeters) || m_asset.physicalSize.sourceToMeters <= 0.0f)
    {
        sendStatus("BUILD requires an explicit authoring-space -> meter calibration", true);
        return false;
    }

    // SOURCE/WORKING are immutable authoring-space coordinates. BUILD is the
    // only normal meter-conversion boundary and operates on a copy so viewport,
    // hit volumes, SOURCE reload and saved WORKING geometry can never diverge.
    ModelAsset productionAsset = m_asset;
    productionAsset.formatVersion = ModelAssetFormatVersion;
    scaleAuthoringDistancesUniform(productionAsset, productionAsset.physicalSize.sourceToMeters);
    productionAsset.physicalSize.geometrySpace = PhysicalGeometrySpace::Meters;
    productionAsset.physicalSize.autoApplyOnSourceImport = false;

    std::string error;
    const auto path = compiledPath(m_selectedId);
    std::filesystem::create_directories(path.parent_path());
    sendStatus("BUILD: writing metric production copy...", false, "writing");
    sendProgress("writing", "BUILD PRODUCTION", 0, 1, path);
    if (!ModelAssetBinary::save(path.string(), productionAsset, &error))
    {
        sendStatus("BUILD production save failed: " + error, true);
        return false;
    }
    if (!ModelAssetBinary::pruneStaleLods(path.string(), productionAsset, &error))
    {
        sendStatus("BUILD wrote production package but stale-LOD cleanup failed: " + error, true);
        return false;
    }
    // The production editor sidecar is finalized by checkWizardStage() only
    // after the BUILD stage evidence has been marked passed for every mesh.
    // Writing it here would serialize a pre-BUILD validation graph.
    sendProgress("writing", "BUILD PRODUCTION", 1, 1, path);
    sendCatalog();
    sendAssetMetadata();
    sendStatus("BUILD complete: production package is metric; saved WORKING remains unchanged authoring space.");
    return true;
}

std::string ModelAssetEditorSession::maintenanceComponentId(
    std::size_t lodIndex,
    const RenderGeometryDefinition& geometry) const
{
    if (isRenderVariantGeometryId(geometry.id))
    {
        const auto variantId = sourceVariantAuthoringId(lodIndex, geometry);
        return variantId.empty() ? std::string("variant:") + geometry.id
                                 : std::string("variant:") + variantId;
    }
    const auto stable = baseVisualId(lodIndex, geometry.id);
    return stable.empty() ? geometry.id : stable;
}

void ModelAssetEditorSession::markMaintenanceIssues(
    const std::string& componentId,
    std::initializer_list<const char*> issues)
{
    if (componentId.empty()) return;
    auto& target = m_componentMaintenanceIssues[componentId];
    for (const char* issue : issues)
        if (issue && *issue) target.insert(issue);
    if (target.empty()) m_componentMaintenanceIssues.erase(componentId);
}

void ModelAssetEditorSession::clearMaintenanceIssue(
    const std::string& componentId,
    const std::string& issue)
{
    const auto it = m_componentMaintenanceIssues.find(componentId);
    if (it == m_componentMaintenanceIssues.end()) return;
    it->second.erase(issue);
    if (it->second.empty()) m_componentMaintenanceIssues.erase(it);
}

void ModelAssetEditorSession::refreshMaintenanceSemanticIssue(const std::string& componentId)
{
    if (componentId.empty()) return;
    bool found = false;
    bool allBound = true;
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        if (li >= m_lodState.size() || !m_lodState[li].loaded) continue;
        const auto& lod = m_asset.renderLods[li];
        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            const auto& geometry = lod.geometries[gi];
            if (isRenderVariantGeometryId(geometry.id) || maintenanceComponentId(li, geometry) != componentId)
                continue;
            for (const auto& node : lod.nodes)
            {
                if (!node.enabled || node.geometryIndex != static_cast<std::int32_t>(gi)) continue;
                found = true;
                if (node.semanticNodeIndex < 0 ||
                    static_cast<std::size_t>(node.semanticNodeIndex) >= m_asset.nodes.size())
                    allBound = false;
            }
        }
    }
    if (found && allBound) clearMaintenanceIssue(componentId, "semantics");
    else if (found) markMaintenanceIssues(componentId, {"semantics"});
}

nlohmann::json ModelAssetEditorSession::serializeMaintenance() const
{
    json components = json::array();
    std::size_t issueCount = 0;
    for (const auto& [componentId, issues] : m_componentMaintenanceIssues)
    {
        if (componentId.empty() || issues.empty()) continue;
        issueCount += issues.size();
        components.push_back({
            {"componentId", componentId},
            {"issues", std::vector<std::string>(issues.begin(), issues.end())}
        });
    }
    return {
        {"pendingComponents", components.size()},
        {"pendingIssues", issueCount},
        {"components", std::move(components)}
    };
}

void ModelAssetEditorSession::synchronizeMeshSourceRecords(bool preserveChecks)
{
    std::map<std::size_t, std::map<std::string, MeshSourceRecord>> next;
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        if (li >= m_lodState.size() || !m_lodState[li].loaded) continue;
        for (const auto& geometry : m_asset.renderLods[li].geometries)
        {
            if (geometry.sourcePath.empty()) continue;
            MeshSourceRecord record;
            const auto lodIt = m_meshSourceRecords.find(li);
            if (preserveChecks && lodIt != m_meshSourceRecords.end())
            {
                const auto old = lodIt->second.find(geometry.id);
                if (old != lodIt->second.end()) record = old->second;
            }
            record.sourcePath = geometry.sourcePath;
            record.sourceFileName = std::filesystem::path(geometry.sourcePath).filename().string();
            const auto fpLod = m_sourceMeshFingerprints.find(li);
            if (fpLod != m_sourceMeshFingerprints.end())
            {
                const auto fp = fpLod->second.find(geometry.sourcePath);
                if (fp != fpLod->second.end() && fp->second != 0) record.sourceHash = fp->second;
            }
            for (const char* stage : wizardStageOrder())
            {
                auto it = record.stageChecks.find(stage);
                if (it != record.stageChecks.end() &&
                    (it->second == "passed" || it->second == "failed" || it->second == "not_checked"))
                    continue;
                const auto global = m_wizardStages.find(stage);
                record.stageChecks[stage] = global != m_wizardStages.end() && global->second.status == "complete"
                    ? "passed" : "not_checked";
            }
            next[li][geometry.id] = std::move(record);
        }
    }
    m_meshSourceRecords = std::move(next);
}

void ModelAssetEditorSession::resetMeshStageChecks(
    std::size_t lodIndex,
    const std::string& geometryId)
{
    if (geometryId.empty()) return;
    auto& record = m_meshSourceRecords[lodIndex][geometryId];
    for (const char* stage : wizardStageOrder()) record.stageChecks[stage] = "not_checked";
    markEditorStateDirty();
}

void ModelAssetEditorSession::recordMeshStageResult(const std::string& stage, bool passed, bool markDirty)
{
    if (wizardStageIndex(stage) >= wizardStageOrder().size()) return;
    synchronizeMeshSourceRecords(true);
    for (auto& [lodIndex, byGeometry] : m_meshSourceRecords)
        for (auto& [geometryId, record] : byGeometry)
        {
            (void)lodIndex;
            (void)geometryId;
            auto& value = record.stageChecks[stage];
            if (passed)
            {
                // A successful whole-stage CHECK certifies every resident SOURCE
                // mesh against the current revision of that stage.
                value = "passed";
            }
            else if (value != "passed")
            {
                // Preserve evidence for unchanged meshes that had already passed
                // this stage. SOURCE replacement/new import clears only the
                // affected mesh first, so a later failed CHECK marks precisely
                // the still-unverified workset instead of painting the whole model red.
                value = "failed";
            }
        }
    if (markDirty) markEditorStateDirty();
}

bool ModelAssetEditorSession::meshSourceRecordPending(const MeshSourceRecord& record) const
{
    if (record.sourceMissing) return true;
    // Certification is complete only after every editor stage, including the
    // terminal VALIDATE and BUILD checks, has passed for this SOURCE revision.
    for (const char* stage : wizardStageOrder())
    {
        const auto it = record.stageChecks.find(stage);
        if (it == record.stageChecks.end() || it->second != "passed") return true;
    }
    return false;
}

nlohmann::json ModelAssetEditorSession::serializeMeshSourceRecords() const
{
    json rows = json::array();
    for (const auto& [lodIndex, byGeometry] : m_meshSourceRecords)
        for (const auto& [geometryId, record] : byGeometry)
        {
            json checks = json::object();
            for (const char* stage : wizardStageOrder())
            {
                const auto it = record.stageChecks.find(stage);
                checks[stage] = it == record.stageChecks.end() ? "not_checked" : it->second;
            }
            rows.push_back({
                {"lodIndex", lodIndex}, {"geometryId", geometryId},
                {"sourceFileName", record.sourceFileName}, {"sourcePath", record.sourcePath},
                {"sourceHash", sourceHashHex(record.sourceHash)},
                {"sourceMissing", record.sourceMissing},
                {"scalePolicy", "asset"},
                {"sourceToMeters", m_asset.physicalSize.sourceToMeters},
                {"stageChecks", std::move(checks)},
                {"validationPending", meshSourceRecordPending(record)}
            });
        }
    return rows;
}

nlohmann::json ModelAssetEditorSession::aggregateStageChecksJson() const
{
    json result = json::object();
    for (const char* stage : wizardStageOrder())
    {
        const auto global = m_wizardStages.find(stage);
        bool passed = global != m_wizardStages.end() && global->second.status == "complete";
        if (passed)
            for (const auto& [lodIndex, byGeometry] : m_meshSourceRecords)
                for (const auto& [geometryId, record] : byGeometry)
                {
                    (void)lodIndex;
                    (void)geometryId;
                    if (record.sourceMissing) passed = false;
                    const auto it = record.stageChecks.find(stage);
                    if (it == record.stageChecks.end() || it->second != "passed") passed = false;
                }
        result[stage] = passed;
    }
    return result;
}

void ModelAssetEditorSession::captureCurrentSourceFingerprintBaseline()
{
    m_sourceMeshFingerprints.clear();
    m_sourceMeshQuickStamps.clear();
    const auto catalog = std::find_if(
        m_catalog.begin(), m_catalog.end(), [&](const CatalogEntry& entry) {
            return entry.id == m_selectedId;
        });
    if (catalog == m_catalog.end() ||
        (catalog->sourceDirectory.empty() && catalog->sourceAuthority == CatalogSourceAuthority::Folder)) return;

    if (catalog->sourceAuthority == CatalogSourceAuthority::Folder)
        m_loadedSourceAssetDirectory = catalog->sourceDirectory.lexically_normal();

    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
    {
        const auto& lod = m_asset.renderLods[li];
        if (lod.sourceKind == "generated") continue;
        for (const auto& geometry : lod.geometries)
        {
            if (geometry.sourcePath.empty()) continue;
            const auto path = selectedSourceFilePath(geometry.sourcePath);
            const auto fingerprint = sourceFileFingerprint(path);
            if (fingerprint != 0)
                m_sourceMeshFingerprints[li][geometry.sourcePath] = fingerprint;
        }
    }
    synchronizeMeshSourceRecords(true);
    for (auto& [li, byGeometry] : m_meshSourceRecords)
        for (auto& [geometryId, record] : byGeometry)
        {
            (void)geometryId;
            const auto fpLod = m_sourceMeshFingerprints.find(li);
            if (fpLod == m_sourceMeshFingerprints.end()) continue;
            const auto fp = fpLod->second.find(record.sourcePath);
            if (fp != fpLod->second.end()) record.sourceHash = fp->second;
        }
}

void ModelAssetEditorSession::sendSourceChangeScan()
{
    if (m_asset.physicalSize.geometrySpace == PhysicalGeometrySpace::LegacyUnknown)
    {
        sendStatus("SOURCE scan refused: legacy destructive scale state must be migrated with RELOAD ALL SOURCE MESHES first", true);
        return;
    }
    if (m_asset.assetId.empty())
    {
        sendStatus("No asset loaded", true);
        return;
    }
    const auto catalog = std::find_if(
        m_catalog.begin(), m_catalog.end(), [&](const CatalogEntry& entry) {
            return entry.id == m_selectedId;
        });
    if (catalog == m_catalog.end() || catalog->sourceAuthority != CatalogSourceAuthority::Folder)
    {
        m_server.broadcastText(json({
            {"type", "source_change_scan_result"}, {"supported", false},
            {"message", "This asset has no folder-authoritative geometry SOURCE"},
            {"rows", json::array()}
        }).dump());
        sendStatus("SOURCE scan refused: selected asset has no folder-authoritative geometry source", true);
        return;
    }

    // The folder identity that created/last saved this working state is the scan
    // authority. Catalog discovery is only a bootstrap; SCAN never guesses a
    // sibling folder with a similar name.
    const auto sourceDirectory = m_loadedSourceAssetDirectory.empty()
        ? catalog->sourceDirectory.lexically_normal()
        : m_loadedSourceAssetDirectory.lexically_normal();
    if (sourceDirectory.empty())
    {
        sendStatus("SOURCE scan refused: the loaded save has no source folder identity", true);
        return;
    }

    const auto started = std::chrono::steady_clock::now();
    std::vector<std::string> warnings;
    SourceFolderMetadataInventory inventory;
    if (!scanSourceFolderMetadataInventory(
            m_sourceAssetsRoot, sourceDirectory, inventory, &warnings))
    {
        m_server.broadcastText(json({
            {"type", "source_change_scan_result"}, {"supported", true},
            {"sourceAssetDirectory", sourceDirectory.generic_string()},
            {"sourceAssetRoot", inventory.assetRoot.generic_string()},
            {"warnings", warnings}, {"rows", json::array()}
        }).dump());
        sendStatus("SOURCE scan cannot inventory the folder recorded by the loaded save", true);
        return;
    }

    // Folder-authoritative OPEN is eager by contract. This guard catches a bad
    // future patch instead of making SCAN silently load .elmesh payloads itself.
    for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
        if (li >= m_lodState.size() || !m_lodState[li].loaded)
        {
            sendStatus("SOURCE scan refused: LOD" + std::to_string(li) +
                " is not resident; OPEN must load all folder-authoritative LODs", true);
            return;
        }

    synchronizeMeshSourceRecords(true);

    struct Candidate
    {
        SourceFolderMetadataEntry entry;
        std::string fileName;
        std::uint64_t hash = 0;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(inventory.entries.size());
    std::set<std::size_t> ordinarySourceLods;
    std::size_t hashFailures = 0;

    // Exact source hash is the only comparison authority. This reads OBJ bytes
    // (and explicitly referenced MTL bytes) once, but does not parse/import a
    // mesh unless the hash is new/different. No .elmesh, PREPARE, ANALYZE or
    // repair path is reachable from this comparison loop.
    sendProgress("reading", "HASH SOURCE FILES", 0, inventory.entries.size(), inventory.assetRoot);
    for (const auto& entry : inventory.entries)
    {
        Candidate candidate;
        candidate.entry = entry;
        candidate.fileName = entry.file.filename().string();
        candidate.hash = sourceFileFingerprint(entry.file);
        if (candidate.hash == 0)
        {
            ++hashFailures;
            warnings.push_back("cannot hash source file: " + entry.file.generic_string());
        }
        if (!entry.variant) ordinarySourceLods.insert(entry.lodIndex);
        candidates.push_back(std::move(candidate));
    }
    sendProgress("reading", "HASH SOURCE FILES", inventory.entries.size(), inventory.entries.size(), inventory.assetRoot);

    // Source LOD folders are authoritative. If Blender introduces a new authored
    // LOD, create the resident RenderLod document before importing its meshes.
    std::size_t maxOrdinaryLod = 0;
    if (!ordinarySourceLods.empty()) maxOrdinaryLod = *ordinarySourceLods.rbegin();
    bool lodStructureChanged = false;
    while (!ordinarySourceLods.empty() && m_asset.renderLods.size() <= maxOrdinaryLod)
    {
        const std::size_t li = m_asset.renderLods.size();
        if (!ordinarySourceLods.count(li))
        {
            warnings.push_back("cannot create non-contiguous authored LOD" + std::to_string(li));
            break;
        }
        RenderLod lod;
        lod.level = static_cast<std::uint32_t>(li);
        lod.sourceKind = "source";
        lod.relativeGeometricError = li == 0 ? 0.0f : -1.0f;
        m_asset.renderLods.push_back(std::move(lod));
        m_lodState.push_back({true, true});
        lodStructureChanged = true;
    }
    for (const auto li : ordinarySourceLods)
    {
        if (li >= m_asset.renderLods.size()) continue;
        auto& lod = m_asset.renderLods[li];
        if (lod.sourceKind != "generated") continue;
        // A real authored LOD<N> folder supersedes an old generated document at
        // the same level. Do not blend two authorities in one render document.
        lod.sourceKind = "source";
        lod.generatedFromLod = NoIndex;
        lod.geometries.clear();
        lod.nodes.clear();
        lod.declaredGeometryCount = 0;
        lod.declaredNodeCount = 0;
        if (li >= m_lodState.size()) m_lodState.resize(li + 1);
        m_lodState[li] = {true, true};
        m_meshSourceRecords.erase(li);
        m_sourceMeshFingerprints.erase(li);
        m_sourceMeshQuickStamps.erase(li);
        lodStructureChanged = true;
    }
    if (lodStructureChanged) markManifestDirty();
    synchronizeMeshSourceRecords(true);

    struct ExistingRef
    {
        std::string geometryId;
        std::string sourcePath;
        std::uint64_t hash = 0;
        bool variant = false;
    };
    using FileKey = std::tuple<std::size_t, bool, std::string>;
    std::map<FileKey, ExistingRef> existingByFile;
    std::set<FileKey> ambiguousExisting;
    for (const auto& [li, byGeometry] : m_meshSourceRecords)
        for (const auto& [geometryId, record] : byGeometry)
        {
            if (record.sourcePath.empty()) continue;
            bool variant = false;
            if (li < m_asset.renderLods.size())
                for (const auto& geometry : m_asset.renderLods[li].geometries)
                    if (geometry.id == geometryId)
                    {
                        variant = isRenderVariantGeometryId(geometry.id);
                        break;
                    }
            const std::string fileName = lowerText(record.sourceFileName.empty()
                ? std::filesystem::path(record.sourcePath).filename().string()
                : record.sourceFileName);
            const FileKey key{li, variant, fileName};
            if (!existingByFile.emplace(key, ExistingRef{geometryId, record.sourcePath, record.sourceHash, variant}).second)
                ambiguousExisting.insert(key);
        }

    std::set<FileKey> sourceKeys;
    std::set<FileKey> ambiguousSource;
    for (const auto& candidate : candidates)
    {
        const FileKey key{candidate.entry.lodIndex, candidate.entry.variant, lowerText(candidate.fileName)};
        if (!sourceKeys.insert(key).second) ambiguousSource.insert(key);
    }

    auto geometryIndexById = [&](std::size_t li, const std::string& geometryId) -> std::size_t
    {
        if (li >= m_asset.renderLods.size()) return std::size_t(-1);
        const auto& geometries = m_asset.renderLods[li].geometries;
        for (std::size_t gi = 0; gi < geometries.size(); ++gi)
            if (geometries[gi].id == geometryId) return gi;
        return std::size_t(-1);
    };

    json rows = json::array();
    std::set<std::size_t> changedLods;
    std::set<FileKey> seen;
    std::size_t unchanged = 0, replaced = 0, added = 0, missingSource = 0, failed = hashFailures;

    for (const auto& candidate : candidates)
    {
        const auto li = candidate.entry.lodIndex;
        const bool variant = candidate.entry.variant;
        const FileKey key{li, variant, lowerText(candidate.fileName)};
        seen.insert(key);
        json row = {
            {"lodIndex", li}, {"variant", variant}, {"sourceFileName", candidate.fileName},
            {"sourcePath", candidate.entry.sourcePath}, {"sourceHash", sourceHashHex(candidate.hash)}
        };
        if (candidate.hash == 0)
        {
            row["kind"] = "hash_error";
            rows.push_back(std::move(row));
            continue;
        }
        if (ambiguousSource.count(key) || ambiguousExisting.count(key))
        {
            ++failed;
            row["kind"] = "ambiguous_filename";
            rows.push_back(std::move(row));
            continue;
        }

        const auto existingIt = existingByFile.find(key);
        if (existingIt == existingByFile.end())
        {
            bool ok = false;
            try
            {
                if (li < m_asset.renderLods.size())
                    ok = variant
                        ? importSourceVariantMaintenance(li, candidate.entry.sourcePath, false, false, false)
                        : addSourcePart(li, candidate.entry.sourcePath, false, false);
            }
            catch (const std::exception& ex)
            {
                warnings.push_back(std::string("cannot add ") + candidate.entry.sourcePath + ": " + ex.what());
            }
            if (!ok)
            {
                ++failed;
                row["kind"] = "add_failed";
            }
            else
            {
                ++added;
                changedLods.insert(li);
                row["kind"] = "added";
                // Resolve the stable geometry identity created by the importer.
                synchronizeMeshSourceRecords(true);
                for (const auto& [geometryId, record] : m_meshSourceRecords[li])
                    if (lowerText(record.sourceFileName) == lowerText(candidate.fileName))
                    {
                        row["geometryId"] = geometryId;
                        break;
                    }
            }
            rows.push_back(std::move(row));
            continue;
        }

        const auto existing = existingIt->second;
        auto& existingRecord = m_meshSourceRecords[li][existing.geometryId];
        if (existingRecord.sourceMissing)
        {
            existingRecord.sourceMissing = false;
            markEditorStateDirty();
        }
        const auto gi = geometryIndexById(li, existing.geometryId);
        if (gi == std::size_t(-1))
        {
            ++failed;
            row["kind"] = "missing_geometry_identity";
            row["geometryId"] = existing.geometryId;
            rows.push_back(std::move(row));
            continue;
        }
        row["geometryId"] = existing.geometryId;
        row["previousHash"] = sourceHashHex(existing.hash);

        auto& geometry = m_asset.renderLods[li].geometries[gi];
        if (existing.hash == candidate.hash && existing.hash != 0)
        {
            ++unchanged;
            row["kind"] = "unchanged";
            // Filename is identity for scan matching. If only the path moved,
            // update provenance without touching mesh payload or stage checks.
            if (geometry.sourcePath != candidate.entry.sourcePath)
            {
                const auto oldPath = geometry.sourcePath;
                geometry.sourcePath = candidate.entry.sourcePath;
                auto& record = m_meshSourceRecords[li][geometry.id];
                record.sourcePath = candidate.entry.sourcePath;
                record.sourceFileName = candidate.fileName;
                m_sourceMeshFingerprints[li].erase(oldPath);
                m_sourceMeshFingerprints[li][candidate.entry.sourcePath] = candidate.hash;
                markLodDirty(li);
                changedLods.insert(li);
                row["provenancePathUpdated"] = true;
            }
            continue; // unchanged hashes are intentionally omitted from visible delta rows
        }

        const auto oldPath = geometry.sourcePath;
        geometry.sourcePath = candidate.entry.sourcePath;
        if (variant && oldPath != candidate.entry.sourcePath)
        {
            auto variantMap = m_sourceExtraMeshIds.find(li);
            if (variantMap != m_sourceExtraMeshIds.end())
            {
                const auto old = variantMap->second.find(oldPath);
                if (old != variantMap->second.end())
                {
                    const auto id = old->second;
                    variantMap->second.erase(old);
                    variantMap->second[candidate.entry.sourcePath] = id;
                }
            }
        }
        m_sourceMeshFingerprints[li].erase(oldPath);

        bool ok = false;
        try
        {
            ok = variant
                ? importSourceVariantMaintenance(li, candidate.entry.sourcePath, true, false, false)
                : replaceSourcePart(li, gi, false, false);
        }
        catch (const std::exception& ex)
        {
            warnings.push_back(std::string("cannot replace ") + candidate.entry.sourcePath + ": " + ex.what());
        }
        if (!ok)
        {
            geometry.sourcePath = oldPath;
            m_sourceMeshFingerprints[li].erase(candidate.entry.sourcePath);
            if (existing.hash != 0) m_sourceMeshFingerprints[li][oldPath] = existing.hash;
            auto& record = m_meshSourceRecords[li][existing.geometryId];
            record.sourcePath = existing.sourcePath;
            record.sourceFileName = std::filesystem::path(existing.sourcePath).filename().string();
            record.sourceHash = existing.hash;
            if (variant && oldPath != candidate.entry.sourcePath)
            {
                auto variantMap = m_sourceExtraMeshIds.find(li);
                if (variantMap != m_sourceExtraMeshIds.end())
                {
                    const auto moved = variantMap->second.find(candidate.entry.sourcePath);
                    if (moved != variantMap->second.end())
                    {
                        const auto id = moved->second;
                        variantMap->second.erase(moved);
                        variantMap->second[oldPath] = id;
                    }
                }
            }
            ++failed;
            row["kind"] = "replace_failed";
        }
        else
        {
            ++replaced;
            changedLods.insert(li);
            row["kind"] = "replaced";
            auto& record = m_meshSourceRecords[li][geometry.id];
            record.sourceFileName = candidate.fileName;
            record.sourcePath = candidate.entry.sourcePath;
            record.sourceHash = candidate.hash;
            record.sourceMissing = false;
            resetMeshStageChecks(li, geometry.id);
        }
        rows.push_back(std::move(row));
    }

    // A saved mesh whose source filename vanished is retained in WORKING, but
    // SOURCE validation is explicitly failed. SCAN never silently deletes user
    // geometry merely because an export file disappeared.
    for (auto& [key, existing] : existingByFile)
    {
        if (seen.count(key) || ambiguousExisting.count(key)) continue;
        ++missingSource;
        const auto li = std::get<0>(key);
        auto& record = m_meshSourceRecords[li][existing.geometryId];
        if (!record.sourceMissing)
        {
            record.sourceMissing = true;
            markEditorStateDirty();
        }
        rows.push_back({
            {"kind", "missing_source"}, {"lodIndex", li}, {"variant", existing.variant},
            {"geometryId", existing.geometryId}, {"sourceFileName", record.sourceFileName},
            {"sourcePath", record.sourcePath}, {"previousHash", sourceHashHex(record.sourceHash)}
        });
    }

    if (replaced != 0 || added != 0 || missingSource != 0 || lodStructureChanged)
    {
        invalidateWizardFrom("source");
        markEditorStateDirty();
    }
    synchronizeMeshSourceRecords(true);
    syncDirty();

    if (!changedLods.empty() || lodStructureChanged)
    {
        std::vector<std::size_t> payloads(changedLods.begin(), changedLods.end());
        if (lodStructureChanged)
            for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
                if (std::find(payloads.begin(), payloads.end(), li) == payloads.end()) payloads.push_back(li);
        sendAsset(payloads, true);
    }
    else
        sendAssetMetadata();

    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    m_server.broadcastText(json({
        {"type", "source_change_scan_result"}, {"supported", true},
        {"sourceAssetDirectory", sourceDirectory.generic_string()},
        {"sourceAssetRoot", inventory.assetRoot.generic_string()},
        {"directoryEnumerations", inventory.directoryEnumerations},
        {"metadataFiles", inventory.metadataFiles}, {"hashReads", candidates.size()},
        {"elapsedMs", elapsedMs}, {"unchanged", unchanged}, {"replaced", replaced},
        {"new", added}, {"missingSource", missingSource}, {"failed", failed},
        {"warnings", warnings}, {"maintenance", serializeMaintenance()},
        {"rows", std::move(rows)}
    }).dump());

    std::ostringstream result;
    result << "SOURCE scan " << std::fixed << std::setprecision(1) << elapsedMs << " ms: "
           << unchanged << " unchanged, " << replaced << " replaced, " << added
           << " added, " << missingSource << " missing, " << failed << " failed. "
           << "Changed/new meshes have all per-mesh stage checks cleared; nothing was saved.";
    sendStatus(result.str(), failed != 0);
}

bool ModelAssetEditorSession::confirmSourceMeshDeletion(
    std::size_t lodIndex,
    const std::string& geometryId)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size() || geometryId.empty())
    {
        sendStatus("Cannot confirm SOURCE deletion: invalid mesh identity", true);
        return false;
    }

    auto sourceLodIt = m_meshSourceRecords.find(lodIndex);
    if (sourceLodIt == m_meshSourceRecords.end())
    {
        sendStatus("Cannot confirm SOURCE deletion: mesh has no saved SOURCE graph", true);
        return false;
    }
    auto sourceIt = sourceLodIt->second.find(geometryId);
    if (sourceIt == sourceLodIt->second.end() || !sourceIt->second.sourceMissing)
    {
        sendStatus("Cannot confirm SOURCE deletion: SCAN has not marked this mesh as deleted", true);
        return false;
    }

    const std::string sourcePath = sourceIt->second.sourcePath;
    std::string sourceError;
    const auto sourceFile = selectedSourceFilePath(sourcePath, &sourceError);
    if (!sourceFile.empty())
    {
        sendStatus("SOURCE deletion refused: the file exists again; run SCAN SOURCE CHANGES", true);
        return false;
    }

    auto& lod = m_asset.renderLods[lodIndex];
    const auto geometryIt = std::find_if(
        lod.geometries.begin(), lod.geometries.end(), [&](const RenderGeometryDefinition& geometry) {
            return geometry.id == geometryId;
        });
    if (geometryIt == lod.geometries.end())
    {
        sendStatus("Cannot confirm SOURCE deletion: saved geometry identity is no longer resident", true);
        return false;
    }
    const std::size_t geometryIndex = static_cast<std::size_t>(
        std::distance(lod.geometries.begin(), geometryIt));
    const RenderGeometryDefinition deletedGeometry = *geometryIt;
    const bool variant = isRenderVariantGeometryId(deletedGeometry.id);
    const std::string variantId = variant
        ? sourceVariantAuthoringId(lodIndex, deletedGeometry) : std::string();
    const std::string baseId = variant
        ? std::string() : baseVisualId(lodIndex, deletedGeometry.id);
    const std::string maintenanceId = maintenanceComponentId(lodIndex, deletedGeometry);

    // Remove every render instance of this geometry. If one of those render
    // nodes was also a transform parent, preserve surviving children's world
    // placement while bypassing the removed node. Semantic nodes are not
    // deleted here: they are LOD-independent gameplay objects and can still be
    // represented by another LOD or intentionally remain semantic-only.
    std::set<std::size_t> removedNodes;
    for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
        if (lod.nodes[ri].geometryIndex == static_cast<std::int32_t>(geometryIndex))
            removedNodes.insert(ri);

    if (!removedNodes.empty())
    {
        std::vector<RigidTransform> oldWorld(lod.nodes.size());
        std::vector<std::uint8_t> state(lod.nodes.size(), 0);
        std::function<RigidTransform(std::size_t)> resolveWorld = [&](std::size_t nodeIndex) -> RigidTransform
        {
            if (nodeIndex >= lod.nodes.size()) return {};
            if (state[nodeIndex] == 2) return oldWorld[nodeIndex];
            if (state[nodeIndex] == 1) return renderNodeRigidTransform(lod.nodes[nodeIndex]);
            state[nodeIndex] = 1;
            const auto local = renderNodeRigidTransform(lod.nodes[nodeIndex]);
            const auto parent = lod.nodes[nodeIndex].parentIndex;
            oldWorld[nodeIndex] = parent >= 0 && static_cast<std::size_t>(parent) < lod.nodes.size()
                ? composeRigid(resolveWorld(static_cast<std::size_t>(parent)), local)
                : local;
            state[nodeIndex] = 2;
            return oldWorld[nodeIndex];
        };
        for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri) resolveWorld(ri);

        std::vector<std::int32_t> oldToNew(lod.nodes.size(), NoIndex);
        std::vector<RenderNode> survivors;
        survivors.reserve(lod.nodes.size() - removedNodes.size());
        for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
            if (!removedNodes.count(ri))
            {
                oldToNew[ri] = static_cast<std::int32_t>(survivors.size());
                survivors.push_back(lod.nodes[ri]);
            }

        std::size_t survivorIndex = 0;
        for (std::size_t oldIndex = 0; oldIndex < lod.nodes.size(); ++oldIndex)
        {
            if (removedNodes.count(oldIndex)) continue;
            auto& node = survivors[survivorIndex++];
            std::int32_t survivingParent = lod.nodes[oldIndex].parentIndex;
            while (survivingParent >= 0 &&
                   removedNodes.count(static_cast<std::size_t>(survivingParent)))
                survivingParent = lod.nodes[static_cast<std::size_t>(survivingParent)].parentIndex;

            const bool parentChanged = survivingParent != lod.nodes[oldIndex].parentIndex;
            node.parentIndex = survivingParent >= 0
                ? oldToNew[static_cast<std::size_t>(survivingParent)] : NoIndex;
            if (parentChanged)
            {
                const RigidTransform parentWorld = survivingParent >= 0
                    ? oldWorld[static_cast<std::size_t>(survivingParent)] : RigidTransform{};
                const RigidTransform local = composeRigid(inverseRigid(parentWorld), oldWorld[oldIndex]);
                setRenderNodeRigidTransform(node, local, node.pivot);
            }
            if (node.geometryIndex > static_cast<std::int32_t>(geometryIndex)) --node.geometryIndex;
            else if (node.geometryIndex == static_cast<std::int32_t>(geometryIndex)) node.geometryIndex = NoIndex;
        }
        lod.nodes = std::move(survivors);
    }
    else
    {
        // Defensive remap for an unused geometry definition.
        remapRenderGeometryAfterErase(lod, geometryIndex);
    }

    lod.geometries.erase(lod.geometries.begin() + static_cast<std::ptrdiff_t>(geometryIndex));
    lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
    lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
    recomputeRenderLodBounds(lod);
    if (lodIndex == 0)
    {
        m_asset.minBounds = lod.minBounds;
        m_asset.maxBounds = lod.maxBounds;
    }

    // Remove all editor-only records that are owned by this render geometry.
    m_meshPreparationRecords[lodIndex].erase(geometryId);
    m_meshOrientationOverrides[lodIndex].erase(geometryId);
    m_geometryTopologyClasses[lodIndex].erase(geometryId);
    m_rawMeshSnapshots[lodIndex].erase(geometryId);
    m_sourceMeshFingerprints[lodIndex].erase(sourcePath);
    m_sourceMeshQuickStamps[lodIndex].erase(sourcePath);
    m_meshSourceRecords[lodIndex].erase(geometryId);

    if (variant)
    {
        m_sourceExtraMeshIds[lodIndex].erase(sourcePath);
        if (!variantId.empty())
        {
            m_sourceVariantReplacements.erase(variantId);
            auto legacyLod = m_legacySourceVariantReplacements.find(lodIndex);
            if (legacyLod != m_legacySourceVariantReplacements.end())
            {
                legacyLod->second.erase(variantId);
                if (legacyLod->second.empty()) m_legacySourceVariantReplacements.erase(legacyLod);
            }
        }
    }
    else
    {
        m_baseVisualIds[lodIndex].erase(geometryId);
        if (!baseId.empty())
            for (auto it = m_sourceVariantReplacements.begin(); it != m_sourceVariantReplacements.end(); )
            {
                auto& replacements = it->second;
                replacements.erase(std::remove(replacements.begin(), replacements.end(), baseId), replacements.end());
                if (replacements.empty()) it = m_sourceVariantReplacements.erase(it);
                else ++it;
            }
        auto legacyLod = m_legacySourceVariantReplacements.find(lodIndex);
        if (legacyLod != m_legacySourceVariantReplacements.end())
            for (auto it = legacyLod->second.begin(); it != legacyLod->second.end(); )
            {
                auto& geometryIds = it->second;
                geometryIds.erase(std::remove(geometryIds.begin(), geometryIds.end(), geometryId), geometryIds.end());
                if (geometryIds.empty()) it = legacyLod->second.erase(it);
                else ++it;
            }
    }

    bool maintenanceStillOwned = false;
    for (std::size_t li = 0; li < m_asset.renderLods.size() && !maintenanceStillOwned; ++li)
        for (const auto& geometry : m_asset.renderLods[li].geometries)
            if (maintenanceComponentId(li, geometry) == maintenanceId)
            {
                maintenanceStillOwned = true;
                break;
            }
    if (!maintenanceStillOwned) m_componentMaintenanceIssues.erase(maintenanceId);

    markLodDirty(lodIndex);
    markManifestDirty();
    markEditorStateDirty();
    invalidateWizardFrom("source");
    syncDirty();
    sendAsset({lodIndex}, true);
    m_server.broadcastText(json({
        {"type", "source_mesh_deletion_confirmed"},
        {"lodIndex", lodIndex}, {"geometryId", geometryId},
        {"sourceFileName", deletedGeometry.sourcePath.empty()
            ? std::string() : std::filesystem::path(deletedGeometry.sourcePath).filename().string()}
    }).dump());
    sendStatus(
        "Confirmed SOURCE deletion: removed LOD" + std::to_string(lodIndex) + "/" + geometryId +
        ", " + std::to_string(removedNodes.size()) +
        " render instance(s), source/check graph and geometry-owned editor metadata. SAVE is still required.");
    return true;
}

bool ModelAssetEditorSession::reloadMeshFromSource(
    std::size_t lodIndex,
    std::size_t geometryIndex)
{
    if (m_asset.physicalSize.geometrySpace == PhysicalGeometrySpace::LegacyUnknown)
    {
        sendStatus("SOURCE reload refused: legacy destructive scale state must be migrated with RELOAD ALL SOURCE MESHES first", true);
        return false;
    }
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size() ||
        geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
    {
        sendStatus("Invalid mesh for SOURCE reload", true);
        return false;
    }
    const auto& geometry = m_asset.renderLods[lodIndex].geometries[geometryIndex];
    if (geometry.sourcePath.empty())
    {
        sendStatus("This mesh has no SOURCE link; generated geometry cannot be reloaded from SOURCE", true);
        return false;
    }

    std::string sourceError;
    const auto exactSource = selectedSourceFilePath(geometry.sourcePath, &sourceError);
    if (exactSource.empty())
    {
        sendStatus("SOURCE reload refused: " + sourceError, true);
        return false;
    }

    if (isRenderVariantGeometryId(geometry.id))
        return importSourceVariantMaintenance(lodIndex, geometry.sourcePath, true);
    return replaceSourcePart(lodIndex, geometryIndex);
}

bool ModelAssetEditorSession::replaceSourcePart(
    std::size_t lodIndex,
    std::size_t geometryIndex,
    bool publish,
    bool rescan)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size() || geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
        throw std::runtime_error("invalid source part geometry index");
    auto& lod = m_asset.renderLods[lodIndex];
    auto& geometry = lod.geometries[geometryIndex];
    if (isRenderVariantGeometryId(geometry.id) || geometry.sourcePath.empty())
        throw std::runtime_error("selected geometry is not an ordinary source part");

    std::string error;
    const auto sourceFile = selectedSourceFilePath(geometry.sourcePath, &error);
    if (sourceFile.empty())
    {
        sendStatus("Cannot reload selected mesh from SOURCE: " + error, true);
        return false;
    }
    MeshLod mesh;
    const std::size_t materialsBefore = m_asset.materials.size();
    sendStatus("Reimporting selected part " + geometry.id + "...", false, "reading");
    if (!importObjNative(sourceFile, m_asset, mesh, &error))
    {
        sendStatus("Cannot replace selected part: " + error, true);
        return false;
    }

    const std::string componentId = maintenanceComponentId(lodIndex, geometry);
    const bool samePayload = sameMeshLodExact(geometry.mesh, mesh);
    geometry.mesh = std::move(mesh);
    m_meshPreparationRecords[lodIndex].erase(geometry.id);
    m_geometryTopologyClasses[lodIndex].erase(geometry.id);
    m_rawMeshSnapshots[lodIndex].erase(geometry.id);
    const auto sourceHash = sourceFileFingerprint(sourceFile);
    m_sourceMeshFingerprints[lodIndex][geometry.sourcePath] = sourceHash;
    m_sourceMeshQuickStamps[lodIndex].erase(geometry.sourcePath); // schema-13 scan uses exact hashes only
    synchronizeMeshSourceRecords(true);
    auto& sourceRecord = m_meshSourceRecords[lodIndex][geometry.id];
    sourceRecord.sourcePath = geometry.sourcePath;
    sourceRecord.sourceFileName = sourceFile.filename().string();
    sourceRecord.sourceHash = sourceHash;
    sourceRecord.sourceMissing = false;
    resetMeshStageChecks(lodIndex, geometry.id);
    markMaintenanceIssues(componentId, {"prepare", "surfaces"});
    if (lodIndex == 0 && std::any_of(
            m_asset.renderLods.begin() + std::min<std::size_t>(1, m_asset.renderLods.size()),
            m_asset.renderLods.end(),
            [](const RenderLod& candidate) { return candidate.sourceKind == "generated"; }))
        markMaintenanceIssues(componentId, {"lods"});

    recomputeRenderLodBounds(lod);
    if (lodIndex == 0)
    {
        m_asset.minBounds = lod.minBounds;
        m_asset.maxBounds = lod.maxBounds;
    }
    markLodDirty(lodIndex);
    if (m_asset.materials.size() != materialsBefore) markManifestDirty();
    syncDirty();
    if (publish) sendAsset({lodIndex}, true);
    if (rescan) sendSourceChangeScan();
    if (publish)
        sendStatus(
            std::string(samePayload ? "Selected source part refreshed" : "Selected source part replaced") +
            "; stable render/semantic/physics identity preserved. All per-mesh stage checks were cleared. PREPARE/SURFACES" +
            (m_componentMaintenanceIssues[componentId].count("lods") ? "/derived LODs" : "") +
            " require review.");
    return true;
}

bool ModelAssetEditorSession::replaceSourcePartByPath(
    std::size_t lodIndex,
    const std::string& sourcePath)
{
    if (sourcePath.empty())
    {
        sendStatus("Cannot replace source part with an empty source path", true);
        return false;
    }
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size())
    {
        sendStatus("Invalid LOD for source part replacement", true);
        return false;
    }
    const auto wanted = lowerText(sourcePath);
    auto& lod = m_asset.renderLods[lodIndex];
    for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
    {
        const auto& geometry = lod.geometries[gi];
        if (isRenderVariantGeometryId(geometry.id) || geometry.sourcePath.empty()) continue;
        if (lowerText(geometry.sourcePath) == wanted)
            return replaceSourcePart(lodIndex, gi);
    }
    sendStatus("Linked source part not found in LOD" + std::to_string(lodIndex) + ": " + sourcePath, true);
    return false;
}

bool ModelAssetEditorSession::addSourcePart(
    std::size_t lodIndex,
    const std::string& sourcePath,
    bool publish,
    bool rescan)
{
    if (sourcePath.empty()) throw std::runtime_error("source path is empty");
    if (lodIndex >= m_asset.renderLods.size())
        throw std::runtime_error("source scan must create the authored LOD document before adding a mesh");
    if (!ensureLodLoaded(lodIndex)) return false;
    auto& lod = m_asset.renderLods[lodIndex];
    if (lod.sourceKind == "generated")
        throw std::runtime_error("cannot add authored source geometry directly into a generated LOD");
    for (const auto& geometry : lod.geometries)
        if (!isRenderVariantGeometryId(geometry.id) && lowerText(geometry.sourcePath) == lowerText(sourcePath))
            throw std::runtime_error("source part is already present in this LOD");

    std::string error;
    const auto sourceFile = selectedSourceFilePath(sourcePath, &error);
    if (sourceFile.empty())
    {
        sendStatus("Cannot add SOURCE mesh: " + error, true);
        return false;
    }
    MeshLod mesh;
    const std::size_t materialsBefore = m_asset.materials.size();
    if (!importObjNative(sourceFile, m_asset, mesh, &error))
    {
        sendStatus("Cannot add source part: " + error, true);
        return false;
    }

    std::set<std::string> geometryIds;
    std::set<std::string> renderNodeIds;
    for (const auto& geometry : lod.geometries) geometryIds.insert(geometry.id);
    for (const auto& node : lod.nodes) renderNodeIds.insert(node.id);
    std::string preferred = std::filesystem::path(sourcePath).stem().string();
    const std::string lodPrefix = "lod" + std::to_string(lodIndex) + "_";
    if (lowerText(preferred).rfind(lodPrefix, 0) == 0 && preferred.size() > lodPrefix.size())
        preferred.erase(0, lodPrefix.size());
    const std::string geometryId = allocateStableId(preferred, "geometry", geometryIds);

    RenderGeometryDefinition geometry;
    geometry.id = geometryId;
    geometry.sourcePath = sourcePath;
    geometry.mesh = std::move(mesh);
    const auto newGeometryIndex = static_cast<std::int32_t>(lod.geometries.size());
    lod.geometries.push_back(std::move(geometry));

    RenderNode node;
    node.id = allocateStableId(preferred, "render_node", renderNodeIds);
    node.geometryIndex = newGeometryIndex;
    node.semanticNodeIndex = NoIndex; // maintenance must not guess gameplay ownership
    node.localPosition = glm::vec3(0.0f);
    node.localRotationDeg = glm::vec3(0.0f);
    node.pivot = glm::vec3(0.0f);
    lod.nodes.push_back(std::move(node));
    lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
    lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
    recomputeRenderLodBounds(lod);
    if (lodIndex == 0)
    {
        m_asset.minBounds = lod.minBounds;
        m_asset.maxBounds = lod.maxBounds;
    }

    reconcileAuthoringVisualRegistry();
    const auto& resident = lod.geometries[static_cast<std::size_t>(newGeometryIndex)];
    const std::string componentId = maintenanceComponentId(lodIndex, resident);
    const auto sourceHash = sourceFileFingerprint(sourceFile);
    m_sourceMeshFingerprints[lodIndex][sourcePath] = sourceHash;
    m_sourceMeshQuickStamps[lodIndex].erase(sourcePath);
    synchronizeMeshSourceRecords(true);
    auto& sourceRecord = m_meshSourceRecords[lodIndex][resident.id];
    sourceRecord.sourcePath = sourcePath;
    sourceRecord.sourceFileName = sourceFile.filename().string();
    sourceRecord.sourceHash = sourceHash;
    sourceRecord.sourceMissing = false;
    resetMeshStageChecks(lodIndex, resident.id);
    markMaintenanceIssues(componentId, {"prepare", "surfaces", "semantics"});
    if (lodIndex == 0 && std::any_of(
            m_asset.renderLods.begin() + std::min<std::size_t>(1, m_asset.renderLods.size()),
            m_asset.renderLods.end(),
            [](const RenderLod& candidate) { return candidate.sourceKind == "generated"; }))
        markMaintenanceIssues(componentId, {"lods"});

    markLodDirty(lodIndex);
    if (m_asset.materials.size() != materialsBefore) markManifestDirty();
    syncDirty();
    if (publish) sendAsset({lodIndex});
    if (rescan) sendSourceChangeScan();
    if (publish)
        sendStatus(
            "Added source part " + resident.id +
            " without rebuilding the asset. All per-mesh stage checks were cleared. PREPARE/SURFACES/SEMANTICS" +
            (m_componentMaintenanceIssues[componentId].count("lods") ? "/derived LODs" : "") +
            " are now local maintenance tasks.");
    return true;
}

bool ModelAssetEditorSession::importSourceVariantMaintenance(
    std::size_t lodIndex,
    const std::string& sourcePath,
    bool requireExisting,
    bool publish,
    bool rescan)
{
    if (sourcePath.empty()) throw std::runtime_error("variant source path is empty");
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size())
        throw std::runtime_error("invalid variant LOD index");
    auto& lod = m_asset.renderLods[lodIndex];
    if (lod.sourceKind == "generated")
        throw std::runtime_error("authored variants belong to an authored source LOD, not a generated LOD");

    reconcileAuthoringVisualRegistry();
    auto existing = std::find_if(
        lod.geometries.begin(), lod.geometries.end(),
        [&](const RenderGeometryDefinition& geometry)
        {
            return isRenderVariantGeometryId(geometry.id) &&
                lowerText(geometry.sourcePath) == lowerText(sourcePath);
        });
    if (requireExisting && existing == lod.geometries.end())
        throw std::runtime_error("replacement variant is not present in the current asset");
    if (!requireExisting && existing != lod.geometries.end())
        throw std::runtime_error("replacement variant is already present; use replace/update instead");

    std::string error;
    const auto sourceFile = selectedSourceFilePath(sourcePath, &error);
    if (sourceFile.empty())
    {
        sendStatus("Cannot reload SOURCE variant: " + error, true);
        return false;
    }
    MeshLod mesh;
    const std::size_t materialsBefore = m_asset.materials.size();
    if (!importObjNative(sourceFile, m_asset, mesh, &error))
    {
        sendStatus("Cannot import replacement variant: " + error, true);
        return false;
    }

    std::size_t geometryIndex = 0;
    bool added = existing == lod.geometries.end();
    if (added)
    {
        auto& variantId = m_sourceExtraMeshIds[lodIndex][sourcePath];
        if (variantId.empty()) variantId = allocateSourceVariantId();
        RenderGeometryDefinition geometry;
        geometry.id = makeRenderVariantGeometryId(variantId);
        geometry.sourcePath = sourcePath;
        geometry.mesh = std::move(mesh);
        geometryIndex = lod.geometries.size();
        lod.geometries.push_back(std::move(geometry));
    }
    else
    {
        geometryIndex = static_cast<std::size_t>(std::distance(lod.geometries.begin(), existing));
        existing->mesh = std::move(mesh);
        existing->sourcePath = sourcePath;
        auto& variantId = m_sourceExtraMeshIds[lodIndex][sourcePath];
        if (variantId.empty()) variantId = sourceVariantAuthoringId(lodIndex, *existing);
    }

    auto& resident = lod.geometries[geometryIndex];
    const std::string componentId = maintenanceComponentId(lodIndex, resident);
    m_meshPreparationRecords[lodIndex].erase(resident.id);
    m_geometryTopologyClasses[lodIndex].erase(resident.id);
    m_rawMeshSnapshots[lodIndex].erase(resident.id);
    const auto sourceHash = sourceFileFingerprint(sourceFile);
    m_sourceMeshFingerprints[lodIndex][sourcePath] = sourceHash;
    m_sourceMeshQuickStamps[lodIndex].erase(sourcePath);
    synchronizeMeshSourceRecords(true);
    auto& sourceRecord = m_meshSourceRecords[lodIndex][resident.id];
    sourceRecord.sourcePath = sourcePath;
    sourceRecord.sourceFileName = sourceFile.filename().string();
    sourceRecord.sourceHash = sourceHash;
    sourceRecord.sourceMissing = false;
    resetMeshStageChecks(lodIndex, resident.id);
    markMaintenanceIssues(componentId, {"prepare", "surfaces"});
    const auto variantId = sourceVariantAuthoringId(lodIndex, resident);
    if (added && sourceVariantReplacementIds(variantId).empty())
        markMaintenanceIssues(componentId, {"replacement"});
    if (lodIndex == 0 && std::any_of(
            m_asset.renderLods.begin() + std::min<std::size_t>(1, m_asset.renderLods.size()),
            m_asset.renderLods.end(),
            [](const RenderLod& candidate) { return candidate.sourceKind == "generated"; }))
        markMaintenanceIssues(componentId, {"lods"});

    recomputeRenderLodBounds(lod);
    markLodDirty(lodIndex);
    if (m_asset.materials.size() != materialsBefore) markManifestDirty();
    syncDirty();
    if (publish) sendAsset({lodIndex}, true);
    if (rescan) sendSourceChangeScan();
    if (publish)
        sendStatus(
            std::string(added ? "Added replacement variant " : "Replaced source payload for variant ") +
            variantId + "; compatibility/state ownership was preserved where it already existed. All per-mesh stage checks were cleared. Local PREPARE/SURFACES" +
            (m_componentMaintenanceIssues[componentId].count("lods") ? "/derived LODs" : "") +
            (m_componentMaintenanceIssues[componentId].count("replacement") ? "/replacement assignment" : "") +
            " require review.");
    return true;
}

bool ModelAssetEditorSession::prepareOneGeometry(
    std::size_t lodIndex,
    std::size_t geometryIndex)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size() || geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
        throw std::runtime_error("invalid geometry index");
    const auto geometryId = m_asset.renderLods[lodIndex].geometries[geometryIndex].id;
    const auto componentId = maintenanceComponentId(lodIndex, m_asset.renderLods[lodIndex].geometries[geometryIndex]);
    bool payloadChanged = false;
    std::vector<std::size_t> changedLods;
    const bool ok = canonicalizeLoadedWorkingSet(
        {}, true, &payloadChanged, &changedLods, lodIndex, geometryId);
    if (!ok) return false;
    clearMaintenanceIssue(componentId, "prepare");
    if (!changedLods.empty()) sendAsset(changedLods);
    else sendAssetMetadata();
    sendStatus("Selected part prepared: LOD" + std::to_string(lodIndex) + "/" + geometryId);
    return true;
}

bool ModelAssetEditorSession::setGeometryOrientationOverride(
    std::size_t lodIndex,
    std::size_t geometryIndex,
    const std::string& mode)
{
    if (mode != "auto" && mode != "flipped")
        throw std::runtime_error("invalid mesh orientation override mode");
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size() || geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
        throw std::runtime_error("invalid geometry index");

    auto& geometry = m_asset.renderLods[lodIndex].geometries[geometryIndex];
    auto prepLodIt = m_meshPreparationRecords.find(lodIndex);
    if (prepLodIt == m_meshPreparationRecords.end())
    {
        sendStatus("ORIENTATION: prepare the selected mesh first", true);
        return false;
    }
    auto prepIt = prepLodIt->second.find(geometry.id);
    if (prepIt == prepLodIt->second.end() || prepIt->second.algorithm != CanonicalMeshAlgorithmId ||
        prepIt->second.outputFingerprint != canonicalMeshFingerprint(geometry.mesh))
    {
        sendStatus("ORIENTATION: prepare the selected mesh first", true);
        return false;
    }

    const auto currentSourceFingerprint = geometry.sourcePath.empty()
        ? std::uint64_t(0)
        : sourceFileFingerprint(selectedSourceFilePath(geometry.sourcePath));
    std::uint64_t acceptedSourceFingerprint = 0;
    const auto sourceLodIt = m_sourceMeshFingerprints.find(lodIndex);
    if (sourceLodIt != m_sourceMeshFingerprints.end())
    {
        const auto acceptedIt = sourceLodIt->second.find(geometry.sourcePath);
        if (acceptedIt != sourceLodIt->second.end()) acceptedSourceFingerprint = acceptedIt->second;
    }
    const bool sourceRevisionCurrent = geometry.sourcePath.empty() ||
        (currentSourceFingerprint != 0 && acceptedSourceFingerprint == currentSourceFingerprint);

    auto overrideLodIt = m_meshOrientationOverrides.find(lodIndex);
    MeshOrientationOverrideRecord* existing = nullptr;
    if (overrideLodIt != m_meshOrientationOverrides.end())
    {
        const auto overrideIt = overrideLodIt->second.find(geometry.id);
        if (overrideIt != overrideLodIt->second.end()) existing = &overrideIt->second;
    }
    const bool existingStale = existing && existing->mode == "flipped" && existing->sourceFingerprint != 0 &&
        existing->sourceFingerprint != currentSourceFingerprint;
    if (existingStale && !sourceRevisionCurrent)
    {
        sendStatus("ORIENTATION: source revision changed; reimport and PREPARE the part before changing the manual override", true);
        return false;
    }
    const bool existingActive = existing && existing->mode == "flipped" && !existingStale;

    bool geometryChanged = false;
    if (mode == "flipped")
    {
        if (!existingActive)
        {
            flipMeshOrientation(geometry.mesh);
            geometryChanged = true;
        }
        auto& target = m_meshOrientationOverrides[lodIndex][geometry.id];
        target.mode = "flipped";
        target.sourceFingerprint = currentSourceFingerprint;
    }
    else
    {
        if (existingActive)
        {
            flipMeshOrientation(geometry.mesh);
            geometryChanged = true;
        }
        auto lodIt = m_meshOrientationOverrides.find(lodIndex);
        if (lodIt != m_meshOrientationOverrides.end())
        {
            lodIt->second.erase(geometry.id);
            if (lodIt->second.empty()) m_meshOrientationOverrides.erase(lodIt);
        }
    }

    prepIt->second.outputFingerprint = canonicalMeshFingerprint(geometry.mesh);
    markEditorStateDirty();
    if (geometryChanged)
    {
        markLodDirty(lodIndex);
        const auto componentId = maintenanceComponentId(lodIndex, geometry);
        markMaintenanceIssues(componentId, {"surfaces"});
        if (lodIndex == 0) markMaintenanceIssues(componentId, {"lods"});
        invalidateWizardFrom("lods");
        sendAsset({lodIndex});
    }
    else sendAssetMetadata();
    analyzeModelPreflight();
    sendStatus(
        std::string("Mesh orientation override: LOD") + std::to_string(lodIndex) + "/" + geometry.id +
        (mode == "flipped" ? " = MANUAL FLIP" : " = AUTO"));
    return true;
}

bool ModelAssetEditorSession::analyzeOneGeometry(
    std::size_t lodIndex,
    std::size_t geometryIndex)
{
    if (!ensureLodLoaded(lodIndex)) return false;
    if (lodIndex >= m_asset.renderLods.size() || geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
        throw std::runtime_error("invalid geometry index");
    const auto& geometry = m_asset.renderLods[lodIndex].geometries[geometryIndex];
    const auto componentId = maintenanceComponentId(lodIndex, geometry);
    const auto canonical = analyzeCanonicalMesh(geometry.mesh);
    const auto audit = auditPreflightGeometry(geometry.mesh);

    const MeshPreparationRecord* record = nullptr;
    const auto prepLodIt = m_meshPreparationRecords.find(lodIndex);
    if (prepLodIt != m_meshPreparationRecords.end())
    {
        const auto prepIt = prepLodIt->second.find(geometry.id);
        if (prepIt != prepLodIt->second.end()) record = &prepIt->second;
    }
    const bool canonicalCurrent = record && record->algorithm == CanonicalMeshAlgorithmId &&
        record->outputFingerprint == canonicalMeshFingerprint(geometry.mesh);
    if (canonicalCurrent) clearMaintenanceIssue(componentId, "prepare");
    else markMaintenanceIssues(componentId, {"prepare"});

    std::string explicitClass;
    const auto classLodIt = m_geometryTopologyClasses.find(lodIndex);
    if (classLodIt != m_geometryTopologyClasses.end())
    {
        const auto it = classLodIt->second.find(geometry.id);
        if (it != classLodIt->second.end()) explicitClass = it->second;
    }
    const auto explicitParsed = preflightTopologyClassFromName(explicitClass);
    const auto suggested = canonicalCurrent && canonical.structuralInvalid
        ? PreflightTopologyClass::Invalid : audit.suggestedClass;
    const bool autoResolved = canonicalCurrent && !canonical.structuralInvalid &&
        (suggested == PreflightTopologyClass::ClosedVolume ||
         (suggested == PreflightTopologyClass::ThinTwoSided && audit.confidence >= 0.90));
    const auto effective = explicitParsed == PreflightTopologyClass::Auto ? suggested : explicitParsed;
    const auto desiredSurface = effective == PreflightTopologyClass::ThinOneSided
        ? SurfaceMode::ThinOneSided
        : effective == PreflightTopologyClass::ThinTwoSided
            ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
    const bool classResolved = effective == PreflightTopologyClass::ClosedVolume ||
        effective == PreflightTopologyClass::ThinOneSided ||
        effective == PreflightTopologyClass::ThinTwoSided ||
        effective == PreflightTopologyClass::BreachedVolume;
    const bool needsReview = canonicalCurrent && explicitParsed == PreflightTopologyClass::Auto &&
        !autoResolved && !canonical.structuralInvalid;
    const bool surfaceMismatch = classResolved && geometry.surfaceMode != desiredSurface;
    bool invalidMaterial = false;
    for (const auto& triangle : geometry.mesh.triangles)
        if (triangle.materialIndex != NoIndex &&
            (triangle.materialIndex < 0 || static_cast<std::size_t>(triangle.materialIndex) >= m_asset.materials.size()))
            invalidMaterial = true;
    const bool ready = canonicalCurrent && !canonical.structuralInvalid && !needsReview &&
        !surfaceMismatch && !invalidMaterial;
    if (ready) clearMaintenanceIssue(componentId, "surfaces");
    else markMaintenanceIssues(componentId, {"surfaces"});

    m_server.broadcastText(json({
        {"type", "geometry_preflight_result"},
        {"lodIndex", lodIndex}, {"geometryIndex", geometryIndex},
        {"geometryId", geometry.id}, {"componentId", componentId},
        {"canonicalCurrent", canonicalCurrent}, {"structuralInvalid", canonical.structuralInvalid},
        {"invalidReason", canonical.invalidReason}, {"needsReview", needsReview},
        {"suggestedClass", preflightTopologyClassName(suggested)},
        {"explicitClass", explicitClass}, {"effectiveClass", preflightTopologyClassName(effective)},
        {"surfaceMismatch", surfaceMismatch}, {"invalidMaterial", invalidMaterial},
        {"ready", ready}, {"maintenance", serializeMaintenance()}
    }).dump());
    sendAssetMetadata();
    sendStatus(ready
        ? "Selected part check passed"
        : "Selected part check requires PREPARE/SURFACES review", !canonicalCurrent || canonical.structuralInvalid);
    return ready;
}

bool ModelAssetEditorSession::regenerateDerivedLodsForGeometry(
    std::size_t lodIndex,
    std::size_t geometryIndex)
{
    if (lodIndex != 0)
    {
        sendStatus("Derived LOD regeneration is anchored to the selected LOD0 part/variant", true);
        return false;
    }
    if (!ensureLodLoaded(0)) return false;
    if (m_asset.renderLods.empty() || geometryIndex >= m_asset.renderLods[0].geometries.size())
        throw std::runtime_error("invalid LOD0 geometry index");
    auto& sourceLod = m_asset.renderLods[0];
    const auto& sourceGeometry = sourceLod.geometries[geometryIndex];
    const bool sourceIsVariant = isRenderVariantGeometryId(sourceGeometry.id);
    const std::string sourceVariantId = sourceIsVariant
        ? sourceVariantAuthoringId(0, sourceGeometry) : std::string();

    const auto componentId = maintenanceComponentId(0, sourceGeometry);
    const auto prepLodIt = m_meshPreparationRecords.find(0);
    if (prepLodIt == m_meshPreparationRecords.end())
    {
        sendStatus("Prepare the selected LOD0 part before regenerating its derived LODs", true);
        return false;
    }
    const auto prepIt = prepLodIt->second.find(sourceGeometry.id);
    const bool prepared = prepIt != prepLodIt->second.end() &&
        prepIt->second.algorithm == CanonicalMeshAlgorithmId &&
        prepIt->second.outputFingerprint == canonicalMeshFingerprint(sourceGeometry.mesh);
    if (!prepared)
    {
        sendStatus("Prepare the selected LOD0 part before regenerating its derived LODs", true);
        return false;
    }

    const double sourceCharacteristic = renderLodPlacedCharacteristicSize(sourceLod);
    std::size_t regenerated = 0, created = 0, manualSkipped = 0;
    std::vector<std::size_t> changedLods;
    for (std::size_t li = 1; li < m_asset.renderLods.size(); ++li)
    {
        auto& targetLod = m_asset.renderLods[li];
        if (targetLod.sourceKind != "generated" || targetLod.generatedFromLod != 0)
        {
            ++manualSkipped;
            continue;
        }
        if (!ensureLodLoaded(li)) return false;
        const double threshold = std::max(
            1.0e-9,
            static_cast<double>(targetLod.relativeGeometricError) * sourceCharacteristic);
        LodCullBuildStats stats;
        MeshLod generatedMesh = buildComponentCullMesh(sourceGeometry.mesh, threshold, &stats);

        std::size_t targetGeometryIndex = std::size_t(-1);
        if (sourceIsVariant)
        {
            for (std::size_t gi = 0; gi < targetLod.geometries.size(); ++gi)
            {
                const auto& geometry = targetLod.geometries[gi];
                if (isRenderVariantGeometryId(geometry.id) &&
                    sourceVariantAuthoringId(li, geometry) == sourceVariantId)
                {
                    targetGeometryIndex = gi;
                    break;
                }
            }
        }
        else
        {
            const auto baseIt = m_baseVisualIds.find(li);
            if (baseIt != m_baseVisualIds.end())
            {
                for (std::size_t gi = 0; gi < targetLod.geometries.size(); ++gi)
                {
                    const auto idIt = baseIt->second.find(targetLod.geometries[gi].id);
                    if (idIt != baseIt->second.end() && idIt->second == componentId)
                    {
                        targetGeometryIndex = gi;
                        break;
                    }
                }
            }
        }

        if (targetGeometryIndex == std::size_t(-1))
        {
            RenderGeometryDefinition geometry = sourceGeometry;
            geometry.mesh = std::move(generatedMesh);
            if (sourceIsVariant)
            {
                geometry.id = makeRenderVariantGeometryId(sourceVariantId);
                m_sourceExtraMeshIds[li][sourceGeometry.sourcePath] = sourceVariantId;
            }
            else
            {
                std::set<std::string> ids;
                for (const auto& g : targetLod.geometries) ids.insert(g.id);
                geometry.id = allocateStableId(sourceGeometry.id, "geometry", ids);
            }
            targetGeometryIndex = targetLod.geometries.size();
            targetLod.geometries.push_back(std::move(geometry));
            if (!sourceIsVariant)
            {
                m_baseVisualIds[li][targetLod.geometries.back().id] = componentId;
                for (const auto& sourceNode : sourceLod.nodes)
                {
                    if (sourceNode.geometryIndex != static_cast<std::int32_t>(geometryIndex)) continue;
                    std::set<std::string> nodeIds;
                    for (const auto& n : targetLod.nodes) nodeIds.insert(n.id);
                    RenderNode node = sourceNode;
                    node.id = allocateStableId(sourceNode.id, "render_node", nodeIds);
                    node.geometryIndex = static_cast<std::int32_t>(targetGeometryIndex);
                    targetLod.nodes.push_back(std::move(node));
                }
            }
            ++created;
        }
        else
        {
            targetLod.geometries[targetGeometryIndex].mesh = std::move(generatedMesh);
            ++regenerated;
        }

        auto& record = m_meshPreparationRecords[li][targetLod.geometries[targetGeometryIndex].id];
        const auto analysis = analyzeCanonicalMesh(targetLod.geometries[targetGeometryIndex].mesh);
        record = MeshPreparationRecord{};
        record.algorithm = GeneratedLodComponentCullAlgorithmId;
        record.sourceRenderVertices = sourceGeometry.mesh.vertices.size();
        record.sourceTriangles = sourceGeometry.mesh.triangles.size();
        record.geometricPoints = analysis.geometricPoints;
        record.outputRenderVertices = analysis.renderVertices;
        record.outputTriangles = analysis.triangles;
        record.normalIslands = analysis.renderVertices;
        record.rebuiltEdges = targetLod.geometries[targetGeometryIndex].mesh.edges.size();
        record.outputFingerprint = canonicalMeshFingerprint(targetLod.geometries[targetGeometryIndex].mesh);
        targetLod.declaredGeometryCount = static_cast<std::uint32_t>(targetLod.geometries.size());
        targetLod.declaredNodeCount = static_cast<std::uint32_t>(targetLod.nodes.size());
        recomputeRenderLodBounds(targetLod);
        markLodDirty(li);
        changedLods.push_back(li);
    }

    if (manualSkipped == 0) clearMaintenanceIssue(componentId, "lods");
    else markMaintenanceIssues(componentId, {"lods"});
    if (!changedLods.empty()) sendAsset(changedLods);
    else sendAssetMetadata();
    sendStatus(
        std::string(sourceIsVariant ? "Selected variant" : "Selected part") +
        " derived LODs: regenerated=" + std::to_string(regenerated) +
        ", created=" + std::to_string(created) +
        ", manual LODs left untouched=" + std::to_string(manualSkipped));
    return true;
}

nlohmann::json ModelAssetEditorSession::serializeSemanticTreeOrder() const
{
    json out = json::object();
    for (const auto& [parentId, childIds] : m_semanticChildOrder)
        if (!parentId.empty() && !childIds.empty()) out[parentId] = childIds;
    return out;
}

nlohmann::json ModelAssetEditorSession::serializeSemanticNodes() const
{
    json nodes = json::array();
    for (std::size_t ni = 0; ni < m_asset.nodes.size(); ++ni)
    {
        const auto& n = m_asset.nodes[ni];
        std::size_t variantCount = 0;
        for (const auto& variant : m_asset.stateVariants)
            if (variant.nodeIndex == static_cast<std::int32_t>(ni)) ++variantCount;
        const auto semanticUsage = inspectSemanticNodeUsage(m_asset, ni);
        nodes.push_back({
            {"index", ni}, {"id", n.id}, {"moduleId", n.moduleId}, {"parentIndex", n.parentIndex},
            {"defaultStateId", n.defaultStateId}, {"stateVariantCount", variantCount},
            {"localPosition", vec3Json(n.localPosition)}, {"localRotationDeg", vec3Json(n.localRotationDeg)}, {"pivot", vec3Json(n.pivot)}, {"enabled", n.enabled},
            {"joint", {{"type", jointTypeName(n.joint.type)}, {"pivot", vec3Json(n.joint.pivot)}, {"axis", vec3Json(n.joint.axis)}, {"defaultRateDegPerSec", n.joint.defaultRateDegPerSec}, {"minAngleDeg", n.joint.minAngleDeg}, {"maxAngleDeg", n.joint.maxAngleDeg}, {"breakable", n.joint.breakable}, {"breakForceN", n.joint.breakForceN}, {"breakTorqueNm", n.joint.breakTorqueNm}}},
            {"physics", {{"mode", massModeName(n.physics.mode)}, {"densityKgM3", n.physics.densityKgM3}, {"massKg", n.physics.massKg}, {"centerOfMass", vec3Json(n.physics.centerOfMass)}, {"inertiaDiagonal", vec3Json(n.physics.inertiaDiagonal)}, {"inertiaProducts", vec3Json(n.physics.inertiaProducts)}}},
            {"semanticUsage", {
                {"renderBindings", semanticUsage.renderBindings}, {"children", semanticUsage.children},
                {"collisions", semanticUsage.collisions}, {"legacySourceBootstrapCollisions", semanticUsage.legacySourceBootstrapCollisions},
                {"sockets", semanticUsage.sockets}, {"stateVariants", semanticUsage.stateVariants},
                {"hitRegions", semanticUsage.hitRegions}, {"openings", semanticUsage.openings},
                {"repairTargets", semanticUsage.repairTargets}, {"structuralLinks", semanticUsage.structuralLinks}, {"physicsEnabled", semanticUsage.physicsEnabled},
                {"orphanCandidate", semanticUsage.isOrphanCandidate()}
            }}
        });
    }
    return nodes;
}

nlohmann::json ModelAssetEditorSession::serializeAssetMetadata() const
{
    json out;
    out["assetId"] = m_asset.assetId;
    out["displayName"] = m_asset.displayName;
    out["formatVersion"] = m_asset.formatVersion;
    out["sourceObjectType"] = m_asset.sourceObjectType;
    out["lodSwitchDistance"] = m_asset.lodSwitchDistance;
    out["minBounds"] = vec3Json(m_asset.minBounds);
    out["maxBounds"] = vec3Json(m_asset.maxBounds);
    const glm::vec3 authoringExtents = authoringPlacedExtents(m_asset);
    const float sourceToMeters = authoringToMetersScale(m_asset);
    const glm::vec3 physicalExtents = m_asset.physicalSize.enabled
        ? authoringExtents * sourceToMeters
        : glm::vec3(0.0f);
    out["physicalSize"] = {
        {"enabled", m_asset.physicalSize.enabled},
        {"axis", physicalSizeAxisName(m_asset.physicalSize.axis)},
        {"targetMeters", m_asset.physicalSize.targetMeters},
        {"sourceExtent", m_asset.physicalSize.sourceExtent},
        {"sourceToMeters", m_asset.physicalSize.sourceToMeters},
        {"geometrySpace", physicalGeometrySpaceName(m_asset.physicalSize.geometrySpace)},
        {"legacyMigrationRequired", m_asset.physicalSize.geometrySpace == PhysicalGeometrySpace::LegacyUnknown},
        {"authoringExtents", vec3Json(authoringExtents)},
        {"physicalExtents", vec3Json(physicalExtents)},
        {"gameLinked", m_asset.physicalSize.gameLinked},
        {"gameDimensionsMeters", vec3Json(m_asset.physicalSize.gameDimensionsMeters)}
    };
    out["binaryPath"] = compiledPath(m_asset.assetId).generic_string();
    out["workingAssetPath"] = workingAssetPath().generic_string();
    out["workingSavedAtUtc"] = m_workingSavedAtUtc;
    out["workingSaveRevision"] = m_workingSaveRevision;
    out["openAuthority"] = m_openAuthority;
    out["sourceAssetsRoot"] = m_sourceAssetsRoot.generic_string();
    out["sourceAssetDirectory"] = m_loadedSourceAssetDirectory.generic_string();
    out["sourceAssetRoot"] = selectedSourceAssetRoot().generic_string();
    out["aggregateStageChecks"] = aggregateStageChecksJson();
    out["meshSourceRecords"] = serializeMeshSourceRecords();
    out["sourceBasis"] = {{"preset", m_asset.sourceBasis.preset}, {"right", static_cast<int>(m_asset.sourceBasis.right)}, {"up", static_cast<int>(m_asset.sourceBasis.up)}, {"forward", static_cast<int>(m_asset.sourceBasis.forward)}, {"canonicalized", m_asset.sourceBasis.canonicalized}};
    out["manifestDirty"] = m_manifestDirty;
    out["geometryPayloadIncluded"] = false;
    out["wizard"] = serializeWizard();
    out["maintenance"] = serializeMaintenance();
    out["semanticTreeOrder"] = serializeSemanticTreeOrder();

    out["materials"] = json::array();
    for (std::size_t i = 0; i < m_asset.materials.size(); ++i)
    {
        const auto& m = m_asset.materials[i];
        out["materials"].push_back({{"index", i}, {"id", m.id}, {"sourceName", m.sourceName}, {"baseColor", vec4Json(m.baseColor)}, {"emissiveColor", vec3Json(m.emissiveColor)}, {"emissiveStrength", m.emissiveStrength}, {"metallic", m.metallic}, {"roughness", m.roughness}, {"twoSided", m.twoSided}, {"baseColorTexture", m.baseColorTexture}, {"emissiveTexture", m.emissiveTexture}});
    }

    // Semantic assembly is deliberately render-LOD agnostic. Keep this
    // serializer independent from render geometry so semantic-only edits can
    // publish a bounded delta instead of rescanning every triangle.
    out["nodes"] = serializeSemanticNodes();

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
        const bool lodLoaded = li < m_lodState.size() && m_lodState[li].loaded;
        json jl = {
            {"index", li}, {"level", lod.level}, {"sourceKind", lod.sourceKind}, {"generatedFromLod", lod.generatedFromLod},
            {"relativeGeometricError", lod.level == 0 ? 0.0f : lod.relativeGeometricError},
            {"minBounds", vec3Json(lod.minBounds)}, {"maxBounds", vec3Json(lod.maxBounds)},
            {"loaded", lodLoaded},
            {"dirty", li < m_lodState.size() && m_lodState[li].dirty},
            {"declaredGeometryCount", lodLoaded ? lod.geometries.size() : lod.declaredGeometryCount},
            {"declaredNodeCount", lodLoaded ? lod.nodes.size() : lod.declaredNodeCount}
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
            std::map<std::int32_t, std::size_t> materialUsage;
            std::size_t unassignedMaterialTriangles = 0;
            for (const auto& triangle : mesh.triangles)
            {
                if (triangle.materialIndex == NoIndex) ++unassignedMaterialTriangles;
                else ++materialUsage[triangle.materialIndex];
            }
            json materialSlots = json::array();
            for (const auto& [materialIndex, triangleCount] : materialUsage)
                materialSlots.push_back({{"materialIndex", materialIndex}, {"triangleCount", triangleCount}});
            std::string explicitTopologyClass;
            const auto topologyLodIt = m_geometryTopologyClasses.find(li);
            if (topologyLodIt != m_geometryTopologyClasses.end())
            {
                const auto topologyIt = topologyLodIt->second.find(geometry.id);
                if (topologyIt != topologyLodIt->second.end()) explicitTopologyClass = topologyIt->second;
            }
            const std::string maintenanceId = maintenanceComponentId(li, geometry);
            const auto maintenanceIt = m_componentMaintenanceIssues.find(maintenanceId);
            const std::vector<std::string> maintenanceIssues = maintenanceIt == m_componentMaintenanceIssues.end()
                ? std::vector<std::string>()
                : std::vector<std::string>(maintenanceIt->second.begin(), maintenanceIt->second.end());
            MeshSourceRecord sourceRecord;
            bool haveSourceRecord = false;
            const auto sourceLodIt = m_meshSourceRecords.find(li);
            if (sourceLodIt != m_meshSourceRecords.end())
            {
                const auto sourceIt = sourceLodIt->second.find(geometry.id);
                if (sourceIt != sourceLodIt->second.end())
                {
                    sourceRecord = sourceIt->second;
                    haveSourceRecord = true;
                }
            }
            json stageChecks = json::object();
            if (haveSourceRecord)
                for (const char* stage : wizardStageOrder())
                {
                    const auto check = sourceRecord.stageChecks.find(stage);
                    stageChecks[stage] = check == sourceRecord.stageChecks.end() ? "not_checked" : check->second;
                }
            json g = {
                {"index", gi}, {"id", geometry.id}, {"sourcePath", geometry.sourcePath},
                {"maintenanceComponentId", maintenanceId}, {"maintenanceIssues", maintenanceIssues},
                {"sourceFileName", haveSourceRecord && !sourceRecord.sourceFileName.empty()
                    ? sourceRecord.sourceFileName : std::filesystem::path(geometry.sourcePath).filename().string()},
                {"sourceHash", haveSourceRecord ? sourceHashHex(sourceRecord.sourceHash) : std::string()},
                {"sourceMissing", haveSourceRecord && sourceRecord.sourceMissing},
                {"stageChecks", std::move(stageChecks)},
                {"validationPending", haveSourceRecord && meshSourceRecordPending(sourceRecord)},
                {"surfaceMode", surfaceModeName(geometry.surfaceMode)},
                {"surfaceIntent", explicitTopologyClass.empty() ? std::string("auto") : explicitTopologyClass},
                {"materialSlots", std::move(materialSlots)},
                {"unassignedMaterialTriangles", unassignedMaterialTriangles},
                {"usageCount", usedBy.size()}, {"usedBy", std::move(usedBy)},
                {"isSourceVariant", variantIdentity.isVariant},
                {"variantId", authoringVariantId},
                {"baseVisualId", stableBaseVisualId},
                {"replacesBaseVisualIds", replacementIds},
                {"minBounds", vec3Json(mesh.minBounds)}, {"maxBounds", vec3Json(mesh.maxBounds)},
                {"vertexCount", mesh.vertices.size()}, {"triangleCount", mesh.triangles.size()}, {"edgeCount", mesh.edges.size()},
                {"estimatedBinaryBytes", estimatedRenderGeometryBinaryBytes(geometry)}
            };
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
        out["sockets"].push_back({{"index", si}, {"id", socket.id}, {"kind", socket.kind}, {"moduleId", socket.moduleId}, {"parentNodeIndex", socket.parentNodeIndex}, {"activeStates", socket.activeStates}, {"localPosition", vec3Json(socket.localPosition)}, {"localRotationDeg", vec3Json(socket.localRotationDeg)}, {"extent", vec3Json(socket.extent)}, {"interfaceProfile", socket.interfaceProfile}, {"previewFovDeg", socket.previewFovDeg}, {"enabled", socket.enabled}, {"light", {{"type", lightTypeName(socket.light.type)}, {"color", vec3Json(socket.light.color)}, {"intensity", socket.light.intensity}, {"rangeMeters", socket.light.rangeMeters}, {"outerConeDeg", socket.light.outerConeDeg}}}});
    }

    out["structuralLinks"] = json::array();
    for (std::size_t li = 0; li < m_asset.structuralLinks.size(); ++li)
    {
        const auto& link = m_asset.structuralLinks[li];
        json proxies = json::array();
        for (std::size_t pi = 0; pi < link.damageProxies.size(); ++pi)
        {
            const auto& proxy = link.damageProxies[pi];
            proxies.push_back({{"index", pi}, {"id", proxy.id}, {"shape", structuralDamageShapeName(proxy.shape)},
                {"parentNodeIndex", proxy.parentNodeIndex}, {"localPosition", vec3Json(proxy.localPosition)},
                {"localRotationDeg", vec3Json(proxy.localRotationDeg)}, {"halfSize", vec3Json(proxy.halfSize)},
                {"radius", proxy.radius}, {"halfHeight", proxy.halfHeight}, {"enabled", proxy.enabled}});
        }
        out["structuralLinks"].push_back({{"index", li}, {"id", link.id}, {"nodeAIndex", link.nodeAIndex}, {"nodeBIndex", link.nodeBIndex},
            {"kind", structuralLinkKindName(link.kind)}, {"loadBearing", link.loadBearing}, {"commandable", link.commandable},
            {"breakForceN", link.breakForceN}, {"breakTorqueNm", link.breakTorqueNm}, {"enabled", link.enabled}, {"damageProxies", std::move(proxies)}});
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
                sourceBytes += safeFileBytes(selectedSourceFilePath(geometry.sourcePath));
        }
    }

    const auto binary = compiledPath(m_asset.assetId);
    const auto workingBinary = workingAssetPath();
    const auto legacyBinary = legacyCompiledPath(m_asset.assetId);

    // Global SAVE owns the workspace/working package. Dirty flags therefore
    // describe unsaved WORKING changes, while
    // production bytes are reported separately and change only at BUILD.
    json lodPayloads = json::array();
    std::uint64_t workingPackageBytes = safeFileBytes(workingBinary);
    const auto workingSavedLodPayloads = discoverSavedLodPayloads(workingBinary);
    std::set<std::size_t> workingLodIndices;
    for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex)
        workingLodIndices.insert(lodIndex);
    for (const auto& [lodIndex, unusedPath] : workingSavedLodPayloads)
    {
        (void)unusedPath;
        workingLodIndices.insert(lodIndex);
    }
    for (const std::size_t lodIndex : workingLodIndices)
    {
        const auto saved = workingSavedLodPayloads.find(lodIndex);
        const auto lodPath = saved != workingSavedLodPayloads.end()
            ? saved->second : ModelAssetBinary::lodPayloadPath(workingBinary.string(), lodIndex);
        const auto bytes = safeFileBytes(lodPath);
        if (saved != workingSavedLodPayloads.end()) workingPackageBytes += bytes;
        lodPayloads.push_back({
            {"lod", lodIndex}, {"path", lodPath.generic_string()}, {"bytes", bytes},
            {"declared", lodIndex < m_asset.renderLods.size()},
            {"loaded", lodIndex < m_lodState.size() && m_lodState[lodIndex].loaded},
            {"dirty", lodIndex < m_lodState.size() && m_lodState[lodIndex].dirty}
        });
    }

    json productionLodPayloads = json::array();
    std::uint64_t productionPackageBytes = safeFileBytes(binary);
    const auto productionSavedLodPayloads = discoverSavedLodPayloads(binary);
    for (const auto& [lodIndex, lodPath] : productionSavedLodPayloads)
    {
        const auto bytes = safeFileBytes(lodPath);
        productionPackageBytes += bytes;
        productionLodPayloads.push_back({
            {"lod", lodIndex}, {"path", lodPath.generic_string()}, {"bytes", bytes},
            {"declared", lodIndex < m_asset.renderLods.size()}
        });
    }

    out["storage"] = {
        {"binaryPath", binary.generic_string()}, {"manifestBytes", safeFileBytes(binary)},
        {"productionPackageBytes", productionPackageBytes}, {"productionLodPayloads", std::move(productionLodPayloads)},
        {"workingBinaryPath", workingBinary.generic_string()}, {"workingManifestBytes", safeFileBytes(workingBinary)},
        {"workingPackageBytes", workingPackageBytes}, {"manifestDirty", m_manifestDirty},
        {"lodPayloads", std::move(lodPayloads)},
        // Compatibility alias retained for older browser/reporting code. It now
        // means the last BUILD package, not the mutable editor save target.
        {"savedPackageBytes", productionPackageBytes},
        {"legacyBinaryPath", legacyBinary.generic_string()}, {"legacyBinaryBytes", safeFileBytes(legacyBinary)},
        {"sourceMeshBytes", sourceBytes}, {"estimatedGeometryPayloadBytes", estimatedGeometryBytes},
        {"estimatedUnusedGeometryBytes", estimatedUnusedGeometryBytes}, {"unusedGeometryCount", unusedGeometryCount},
        {"sourceVariantGeometryCount", sourceVariantGeometryCount}, {"sourceFilesReadOnly", true}
    };
    return out;
}

std::uint32_t ModelAssetEditorSession::nextWireTransferId()
{
    if (m_nextWireTransferId == 0u)
        m_nextWireTransferId = 1u;
    return m_nextWireTransferId++;
}

void ModelAssetEditorSession::sendAsset(const std::vector<std::size_t>& requestedPayloadLods, bool preserveUiSelection)
{
    // Preserve the old application terminal: the browser receives metadata and
    // binary mesh arrays, reassembles the same full asset object, then invokes
    // the existing `asset` handler. Geometry never enters JSON.
    //
    // An empty requestedPayloadLods means a self-contained/full publication
    // (initial load, RESTORE, reconnect, whole-mesh rewrite). A
    // non-empty list is a transport delta: only those changed LOD arrays cross
    // the socket; unchanged resident LOD arrays are reused by the transport
    // adapter before the exact same old `asset` handler is invoked.
    reconcileAuthoringVisualRegistry();
    const bool reuseExistingPayloads = !requestedPayloadLods.empty();
    std::set<std::size_t> selectedPayloadLods;
    if (reuseExistingPayloads)
    {
        for (const auto lodIndex : requestedPayloadLods)
        {
            if (lodIndex >= m_asset.renderLods.size() ||
                lodIndex >= m_lodState.size() || !m_lodState[lodIndex].loaded)
                throw std::runtime_error("cannot publish non-resident LOD transport delta");
            selectedPayloadLods.insert(lodIndex);
        }
    }
    else
    {
        for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex)
            if (lodIndex < m_lodState.size() && m_lodState[lodIndex].loaded)
                selectedPayloadLods.insert(lodIndex);
    }

    const auto transferId = nextWireTransferId();
    json payloadLods = json::array();
    for (const auto lodIndex : selectedPayloadLods) payloadLods.push_back(lodIndex);

    const auto metadataStarted = std::chrono::steady_clock::now();
    auto metadata = serializeAssetMetadata();
    const auto metadataMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - metadataStarted).count();
    m_server.broadcastText(json({
        {"type", "asset_binary_begin"},
        {"transferId", transferId},
        {"dirty", m_dirty},
        {"payloadLods", payloadLods},
        {"reuseExistingPayloads", reuseExistingPayloads},
        {"preserveUiSelection", preserveUiSelection},
        {"asset", std::move(metadata)}
    }).dump());

    std::uint64_t wireBytes = 0;
    double encodeMs = 0.0;
    for (const auto lodIndex : selectedPayloadLods)
    {
        const auto encodeStarted = std::chrono::steady_clock::now();
        auto frame = wire::encodeLodGeometryPayload(
            transferId,
            static_cast<std::uint32_t>(lodIndex),
            m_asset.renderLods[lodIndex]);
        encodeMs += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - encodeStarted).count();
        wireBytes += static_cast<std::uint64_t>(frame.size());
        m_server.broadcastBinary(std::move(frame));
    }
    std::cerr << "[ModelAssetEditor][transport] asset transfer=" << transferId
              << " lods=" << selectedPayloadLods.size()
              << " mode=" << (reuseExistingPayloads ? "delta" : "full")
              << " metadata_ms=" << std::fixed << std::setprecision(1) << metadataMs
              << " encode_ms=" << encodeMs
              << " wire_mib=" << (static_cast<double>(wireBytes) / (1024.0 * 1024.0))
              << '\n';
}

void ModelAssetEditorSession::sendAssetMetadata(const nlohmann::json& hints)
{
    reconcileAuthoringVisualRegistry();
    json payload = {{"type", "asset_metadata"}, {"dirty", m_dirty}, {"asset", serializeAssetMetadata()}};
    if (hints.is_object())
        for (const auto& [key, value] : hints.items())
            payload[key] = value;
    m_server.broadcastText(payload.dump());
}


void ModelAssetEditorSession::sendSemanticTreePatch()
{
    // Parent/joint/semantic-frame edits do not touch render geometry. Publishing
    // the full asset metadata here used to rescan every triangle merely to
    // rebuild material statistics, making a 29-node tree edit take seconds.
    // Keep the semantic authoring path bounded by semantic data size.
    json payload = {
        {"type", "semantic_tree_patch"},
        {"dirty", m_dirty},
        {"wizard", serializeWizard()},
        {"nodes", serializeSemanticNodes()},
        {"semanticTreeOrder", serializeSemanticTreeOrder()}
    };
    m_server.broadcastText(payload.dump());
}

void ModelAssetEditorSession::sendSurfaceMetadataPatch(
    const std::vector<std::pair<std::size_t, std::size_t>>& targets)
{
    json patches = json::array();
    std::set<std::pair<std::size_t, std::size_t>> uniqueTargets(targets.begin(), targets.end());
    for (const auto& [lodIndex, geometryIndex] : uniqueTargets)
    {
        if (lodIndex >= m_asset.renderLods.size() ||
            geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
            continue;
        const auto& geometry = m_asset.renderLods[lodIndex].geometries[geometryIndex];
        std::string intent = "auto";
        const auto lodIt = m_geometryTopologyClasses.find(lodIndex);
        if (lodIt != m_geometryTopologyClasses.end())
        {
            const auto classIt = lodIt->second.find(geometry.id);
            if (classIt != lodIt->second.end() && !classIt->second.empty())
                intent = classIt->second;
        }
        patches.push_back({
            {"lodIndex", lodIndex},
            {"geometryIndex", geometryIndex},
            {"geometryId", geometry.id},
            {"surfaceMode", surfaceModeName(geometry.surfaceMode)},
            {"surfaceIntent", intent},
            {"lodDirty", lodIndex < m_lodState.size() && m_lodState[lodIndex].dirty}
        });
    }
    m_server.broadcastText(json({
        {"type", "surface_metadata_patch"},
        {"dirty", m_dirty},
        {"wizard", serializeWizard()},
        {"patches", std::move(patches)}
    }).dump());
}

void ModelAssetEditorSession::sendSemanticBindingPatch(
    const std::vector<std::pair<std::size_t, std::size_t>>& targets)
{
    json patches = json::array();
    std::set<std::pair<std::size_t, std::size_t>> uniqueTargets(targets.begin(), targets.end());
    for (const auto& [lodIndex, renderNodeIndex] : uniqueTargets)
    {
        if (lodIndex >= m_asset.renderLods.size() ||
            renderNodeIndex >= m_asset.renderLods[lodIndex].nodes.size())
            continue;
        const auto& node = m_asset.renderLods[lodIndex].nodes[renderNodeIndex];
        patches.push_back({
            {"lodIndex", lodIndex},
            {"renderNodeIndex", renderNodeIndex},
            {"renderNodeId", node.id},
            {"semanticNodeIndex", node.semanticNodeIndex},
            {"lodDirty", lodIndex < m_lodState.size() && m_lodState[lodIndex].dirty}
        });
    }
    m_server.broadcastText(json({
        {"type", "semantic_binding_patch"},
        {"dirty", m_dirty},
        {"wizard", serializeWizard()},
        {"patches", std::move(patches)}
    }).dump());
}

void ModelAssetEditorSession::sendLodPayload(std::size_t lodIndex, bool includeRawSnapshots)
{
    if (lodIndex >= m_asset.renderLods.size() ||
        lodIndex >= m_lodState.size() || !m_lodState[lodIndex].loaded)
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " viewport payload is not resident", true);
        return;
    }

    reconcileAuthoringVisualRegistry();
    const auto metadata = serializeAssetMetadata();
    const auto& lods = metadata.at("renderLods");
    if (lodIndex >= lods.size())
    {
        sendStatus("LOD" + std::to_string(lodIndex) + " viewport payload could not be serialized", true);
        return;
    }

    const auto transferId = nextWireTransferId();
    m_server.broadcastText(json({
        {"type", "lod_payload_binary_begin"},
        {"transferId", transferId},
        {"lodIndex", lodIndex},
        {"dirty", m_dirty},
        {"lod", lods.at(lodIndex)}
    }).dump());

    const auto rawIt = includeRawSnapshots ? m_rawMeshSnapshots.find(lodIndex) : m_rawMeshSnapshots.end();
    const auto* raw = rawIt == m_rawMeshSnapshots.end() ? nullptr : &rawIt->second;
    const auto encodeStarted = std::chrono::steady_clock::now();
    auto frame = wire::encodeLodGeometryPayload(
        transferId,
        static_cast<std::uint32_t>(lodIndex),
        m_asset.renderLods[lodIndex],
        raw);
    const auto encodeMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - encodeStarted).count();
    const auto wireBytes = frame.size();
    m_server.broadcastBinary(std::move(frame));
    std::cerr << "[ModelAssetEditor][transport] lod transfer=" << transferId
              << " lod=" << lodIndex
              << " encode_ms=" << std::fixed << std::setprecision(1) << encodeMs
              << " wire_mib=" << (static_cast<double>(wireBytes) / (1024.0 * 1024.0))
              << " raw=" << (includeRawSnapshots && raw ? 1 : 0)
              << '\n';
    sendStatus("LOD" + std::to_string(lodIndex) + " viewport payload ready");
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
    const auto variantGeometryIt = std::find_if(
        lod.geometries.begin(), lod.geometries.end(), [&](const RenderGeometryDefinition& geometry)
        {
            return isRenderVariantGeometryId(geometry.id) &&
                sourceVariantAuthoringId(lodIndex, geometry) == variantId;
        });
    if (variantGeometryIt == lod.geometries.end())
        throw std::runtime_error("additional mesh is not loaded in LOD" + std::to_string(lodIndex));
    const std::string maintenanceId = maintenanceComponentId(lodIndex, *variantGeometryIt);
    const bool maintenanceLocal = !maintenanceId.empty() &&
        m_componentMaintenanceIssues.find(maintenanceId) != m_componentMaintenanceIssues.end();

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
    const bool hasReplacementTargets = !values.empty();
    if (!hasReplacementTargets) m_sourceVariantReplacements.erase(variantId);

    // Compatibility is authoring data. DAMAGE later decides which compatible
    // visual is chosen for a concrete hit/state; GEOMETRY only defines the set.
    // A newly-added maintenance variant resolves its local replacement debt here
    // without invalidating already-valid unrelated production components.
    if (maintenanceLocal)
    {
        if (!hasReplacementTargets) markMaintenanceIssues(maintenanceId, {"replacement"});
        else clearMaintenanceIssue(maintenanceId, "replacement");
        sendAssetMetadata();
    }
    else
    {
        invalidateWizardFrom("geometry");
        sendAssetMetadata();
    }
    sendStatus(
        std::string(allowed ? "Allowed " : "Disallowed ") + variantId +
        " as replacement for " + requestedBaseVisualId +
        " in LOD" + std::to_string(lodIndex));
    return true;
}

bool ModelAssetEditorSession::refreshSourceVariants(bool sourceOwned, bool broadcastUpdates)
{
    if (!sourceOwned && m_asset.physicalSize.geometrySpace == PhysicalGeometrySpace::LegacyUnknown)
    {
        sendStatus("SOURCE set refresh refused: legacy destructive scale state must be migrated with RELOAD ALL SOURCE MESHES first", true);
        return false;
    }
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
    const auto selectedCatalog = std::find_if(
        m_catalog.begin(), m_catalog.end(), [&](const CatalogEntry& entry) {
            return entry.id == m_selectedId;
        });
    const bool folderSource = selectedCatalog != m_catalog.end() &&
        selectedCatalog->sourceAuthority == CatalogSourceAuthority::Folder;

    for (std::size_t lodIndex = 0; lodIndex < m_asset.renderLods.size(); ++lodIndex)
    {
        if (lodIndex >= m_lodState.size() || !m_lodState[lodIndex].loaded)
            continue;

        std::vector<SourceAdditionalMesh> additional;
        std::vector<std::string> lodWarnings;
        if (folderSource)
        {
            const auto variants = discoverSourceFolderVariants(
                m_sourceAssetsRoot, selectedCatalog->sourceDirectory, lodIndex, &lodWarnings);
            additional.reserve(variants.size());
            for (const auto& variant : variants)
                additional.push_back(SourceAdditionalMesh{variant.file, variant.sourcePath});

            const auto assetRoot = resolveSourceFolderAssetRoot(
                m_sourceAssetsRoot, selectedCatalog->sourceDirectory);
            if (!assetRoot.empty())
                scannedLodRoots.push_back(
                    "LOD" + std::to_string(lodIndex) + "=" + assetRoot.generic_string() +
                    "/LOD" + std::to_string(lodIndex) + "/variants");
        }
        else
        {
            // Legacy assets keep the old registry-assisted discovery until they
            // receive an asset-level sourceDirectory migration.
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

            additional = discoverAdditionalLodMeshes(
                m_sourceAssetsRoot, knownRuntimePaths, &lodWarnings);

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
        }

        discoveryWarnings.insert(
            discoveryWarnings.end(), lodWarnings.begin(), lodWarnings.end());
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
        if (broadcastUpdates)
            sendStatus(
                std::string("NO CHANGES: no additional OBJ files found in ") +
                (folderSource ? "variants directories" : "legacy loaded LOD directories") +
                (scannedLodRoots.empty() ? std::string() : "; scanned " + joinedRoots()));
        return true;
    }

    std::size_t added = 0, updated = 0, unchanged = 0, failed = 0, completed = 0;
    bool manifestChanged = false;
    bool canonicalEvidenceChanged = false;
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

        // Refresh is a literal source reload. Keep the decoded OBJ payload raw;
        // an explicit PREPARE MESHES action may canonicalize it afterwards.
        auto existing = std::find_if(
            lod.geometries.begin(), lod.geometries.end(),
            [&](const RenderGeometryDefinition& geometry)
            {
                if (!isRenderVariantGeometryId(geometry.id)) return false;
                if (!geometry.sourcePath.empty() && geometry.sourcePath == job.source.runtimePath)
                    return true;
                return sourceVariantAuthoringId(job.lodIndex, geometry) == job.variantId;
            });
        std::size_t residentGeometryIndex = 0;
        if (existing == lod.geometries.end())
        {
            residentGeometryIndex = lod.geometries.size();
            RenderGeometryDefinition variant;
            variant.id = variantGeometryId;
            variant.sourcePath = job.source.runtimePath;
            variant.mesh = std::move(mesh);
            lod.geometries.push_back(std::move(variant));
            changedLods.insert(job.lodIndex);
            ++added;
        }
        else
        {
            residentGeometryIndex = static_cast<std::size_t>(std::distance(lod.geometries.begin(), existing));
            if (existing->id == variantGeometryId &&
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
        }
        auto& residentGeometry = lod.geometries[residentGeometryIndex];
        (void)residentGeometry;

        // A source refresh intentionally invalidates previous preparation evidence
        // for this variant, even when the decoded RAW payload happens to compare
        // equal. The next PREPARE MESHES click establishes fresh evidence.
        auto prepLodIt = m_meshPreparationRecords.find(job.lodIndex);
        if (prepLodIt != m_meshPreparationRecords.end())
        {
            canonicalEvidenceChanged = prepLodIt->second.erase(variantGeometryId) != 0 || canonicalEvidenceChanged;
            if (prepLodIt->second.empty()) m_meshPreparationRecords.erase(prepLodIt);
        }
        auto rawLodIt = m_rawMeshSnapshots.find(job.lodIndex);
        if (rawLodIt != m_rawMeshSnapshots.end())
        {
            rawLodIt->second.erase(variantGeometryId);
            if (rawLodIt->second.empty()) m_rawMeshSnapshots.erase(rawLodIt);
        }
        auto classLodIt = m_geometryTopologyClasses.find(job.lodIndex);
        if (classLodIt != m_geometryTopologyClasses.end())
        {
            canonicalEvidenceChanged = classLodIt->second.erase(variantGeometryId) != 0 || canonicalEvidenceChanged;
            if (classLodIt->second.empty()) m_geometryTopologyClasses.erase(classLodIt);
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

    // Stable authoring/canonical evidence is part of the current WORKING ASSET
    // and persists only when the user presses the global SAVE button.
    (void)canonicalEvidenceChanged;
    if (!changedLods.empty() || manifestChanged || registryChanged)
        invalidateWizardFrom(sourceOwned ? "source" : "geometry");
    if (broadcastUpdates)
    {
        if (!changedLods.empty())
            sendAsset(std::vector<std::size_t>(changedLods.begin(), changedLods.end())); // Preserve old asset terminal; transport only changed LOD arrays.
        else if (manifestChanged || registryChanged)
            sendAssetMetadata();
    }

    std::string message =
        "Additional LOD meshes refreshed: " +
        std::to_string(added) + " added, " + std::to_string(updated) +
        " updated, " + std::to_string(unchanged) + " unchanged, " +
        std::to_string(failed) + " failed across " +
        std::to_string(changedLods.size()) + " loaded LOD(s)";
    if (!failures.empty()) message += "; first note: " + failures.front();
    if (broadcastUpdates || failed != 0) sendStatus(message, failed != 0);
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
            const auto working = std::filesystem::path(message.value("workingFilesRoot", std::string()));
            const auto locale = message.value("locale", m_locale);
            std::cerr << "[ModelAssetEditor] save_settings requested: source="
                      << source.generic_string()
                      << " compiled=" << compiled.generic_string()
                      << " working=" << working.generic_string()
                      << " locale=" << locale << '\n';
            saveSettings(source, compiled, working, locale);
            return;
        }
        if (command == "set_locale") { setLocale(message.value("locale", m_locale)); return; }
        if (command == "select_asset") { selectAsset(message.value("assetId", ""), false); return; }
        if (command == "reimport_asset") { selectAsset(m_selectedId, true); return; }
        if (command == "save_asset") { saveAsset(); return; }
        if (command == "restore_working_asset") { restoreWorkingAsset(); return; }
        if (command == "load_lod") { loadLodOnly(message.value("lodIndex", std::size_t(-1)), false); return; }
        if (command == "reload_lod") { loadLodOnly(message.value("lodIndex", std::size_t(-1)), true); return; }
        if (command == "unload_lod") { unloadLod(message.value("lodIndex", std::size_t(-1))); return; }
        if (command == "check_wizard_stage") { checkWizardStage(message.value("stage", std::string())); return; }
        if (command == "scan_render_duplicates")
        {
            scanRenderDuplicates(
                message.value("lodIndex", std::size_t(-1)),
                message.value("referenceRenderNodeIndex", std::size_t(-1)),
                jsonIndices(message.value("targetRenderNodeIndices", json::array())));
            return;
        }

        if (m_asset.assetId.empty()) { sendStatus("No asset loaded", true); return; }
        if (command == "scan_source_changes") { sendSourceChangeScan(); return; }
        if (command == "confirm_source_mesh_deletion")
        {
            confirmSourceMeshDeletion(
                message.value("lodIndex", std::size_t(-1)),
                message.value("geometryId", std::string()));
            return;
        }
        if (command == "reload_mesh_from_source")
        {
            reloadMeshFromSource(
                message.value("lodIndex", std::size_t(-1)),
                message.value("geometryIndex", std::size_t(-1)));
            return;
        }
        if (command == "replace_source_part")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto sourcePath = message.value("sourcePath", std::string());
            if (!sourcePath.empty())
                replaceSourcePartByPath(lodIndex, sourcePath);
            else
                replaceSourcePart(lodIndex, message.value("geometryIndex", std::size_t(-1)));
            return;
        }
        if (command == "add_source_part")
        {
            addSourcePart(
                message.value("lodIndex", std::size_t(-1)),
                message.value("sourcePath", std::string()));
            return;
        }
        if (command == "replace_source_variant")
        {
            importSourceVariantMaintenance(
                message.value("lodIndex", std::size_t(-1)),
                message.value("sourcePath", std::string()),
                true);
            return;
        }
        if (command == "add_source_variant")
        {
            importSourceVariantMaintenance(
                message.value("lodIndex", std::size_t(-1)),
                message.value("sourcePath", std::string()),
                false);
            return;
        }
        if (command == "prepare_geometry")
        {
            prepareOneGeometry(
                message.value("lodIndex", std::size_t(-1)),
                message.value("geometryIndex", std::size_t(-1)));
            return;
        }
        if (command == "set_geometry_orientation_override")
        {
            setGeometryOrientationOverride(
                message.value("lodIndex", std::size_t(-1)),
                message.value("geometryIndex", std::size_t(-1)),
                message.value("mode", std::string("auto")));
            return;
        }
        if (command == "analyze_geometry_preflight")
        {
            analyzeOneGeometry(
                message.value("lodIndex", std::size_t(-1)),
                message.value("geometryIndex", std::size_t(-1)));
            return;
        }
        if (command == "regenerate_geometry_lods")
        {
            regenerateDerivedLodsForGeometry(
                message.value("lodIndex", std::size_t(-1)),
                message.value("geometryIndex", std::size_t(-1)));
            return;
        }
        if (command == "prepare_model_meshes")
        {
            if (!ensureAllLodsLoaded()) return;
            synchronizeMeshSourceRecords(true);
            const auto prepareStarted = std::chrono::steady_clock::now();
            bool payloadChanged = false;
            std::vector<std::size_t> changedLods;
            const bool complete = canonicalizeLoadedWorkingSet(
                "lods", true, &payloadChanged, &changedLods,
                std::size_t(-1), {}, true);
            const auto computeMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - prepareStarted).count();

            // PREPARE mutates only backend-resident geometry. Preserve the old
            // full-asset application terminal, but let the transport delta carry
            // only LOD payloads whose mesh bytes actually changed. If the pass is
            // already current, synchronize metadata only; no mesh crosses the wire.
            const auto publishStarted = std::chrono::steady_clock::now();
            if (!changedLods.empty())
                sendAsset(changedLods);
            else
                sendAssetMetadata();
            const auto publishMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - publishStarted).count();
            std::cerr << "[ModelAssetEditor][prepare] compute_ms=" << std::fixed << std::setprecision(1)
                      << computeMs
                      << " changed_lods=" << changedLods.size()
                      << " payload_changed=" << (payloadChanged ? 1 : 0)
                      << " publish_ms=" << publishMs
                      << '\n';

            if (complete)
                sendStatus("Mesh preparation complete; run ANALYZE to classify/audit the canonical working set");
            return;
        }
        if (command == "analyze_model_preflight") { analyzeModelPreflight(); return; }
        if (command == "set_geometry_topology_class")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(0));
            const auto geometryIndex = message.value("geometryIndex", std::size_t(-1));
            const auto topologyClass = message.value("topologyClass", std::string("auto"));
            if (!message.value("applyAllLods", false))
            {
                setGeometryTopologyClass(lodIndex, geometryIndex, topologyClass);
                return;
            }

            if (!ensureAllLodsLoaded()) return;
            if (lodIndex >= m_asset.renderLods.size() || geometryIndex >= m_asset.renderLods[lodIndex].geometries.size())
                throw std::runtime_error("invalid render geometry index");
            const auto& selectedGeometry = m_asset.renderLods[lodIndex].geometries[geometryIndex];
            const auto selectedVariant = renderVariantIdentity(selectedGeometry.id);
            const std::string familyId = selectedVariant.isVariant
                ? sourceVariantAuthoringId(lodIndex, selectedGeometry)
                : baseVisualId(lodIndex, selectedGeometry.id);

            std::vector<std::pair<std::size_t, std::size_t>> targets;
            for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
            {
                const auto& geometries = m_asset.renderLods[li].geometries;
                for (std::size_t gi = 0; gi < geometries.size(); ++gi)
                {
                    const auto& candidate = geometries[gi];
                    const auto candidateVariant = renderVariantIdentity(candidate.id);
                    if (candidateVariant.isVariant != selectedVariant.isVariant) continue;
                    const std::string candidateFamily = candidateVariant.isVariant
                        ? sourceVariantAuthoringId(li, candidate)
                        : baseVisualId(li, candidate.id);
                    const bool sameFamily = !familyId.empty() && candidateFamily == familyId;
                    const bool sameStableGeometryId = !selectedVariant.isVariant && candidate.id == selectedGeometry.id;
                    if (sameFamily || sameStableGeometryId)
                    {
                        targets.emplace_back(li, gi);
                        break;
                    }
                }
            }
            for (const auto& [li, gi] : targets)
                setGeometryTopologyClass(li, gi, topologyClass, false, false);
            invalidateWizardFrom("surfaces");
            sendSurfaceMetadataPatch(targets);
            sendStatus("Surface intent applied to " + std::to_string(targets.size()) +
                " LOD(s) for visual family " + familyId);
            return;
        }
        if (command == "analyze_lod_requirements") { analyzeLodRequirements(message.value("lodIndex", std::size_t(0))); return; }
        if (command == "set_lod_relative_error") { setLodRelativeGeometricError(message.value("lodIndex", std::size_t(-1)), message.value("relativeGeometricError", -1.0)); return; }
        if (command == "preview_lod_component_cull") { previewLodComponentCull(message.value("lodIndex", std::size_t(0)), message.value("thresholdMeters", 0.0)); return; }
        if (command == "preview_lod_coplanar_collapse") { previewLodCoplanarCollapse(message.value("lodIndex", std::size_t(0))); return; }
        if (command == "apply_generated_lods")
        {
            applyGeneratedLods(
                message.value("sourceLodIndex", std::size_t(0)),
                message.value("levels", json::array()));
            return;
        }
        if (command == "request_lod_payload")
        {
            sendLodPayload(
                message.value("lodIndex", std::size_t(-1)),
                message.value("includeRaw", false));
            return;
        }
        if (command == "refresh_source_variants")
        {
            if (loadAllDeclaredLodsForSource())
                refreshSourceVariants(true, true);
            return;
        }
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
            markManifestDirty(); markAllLoadedLodsDirty();
            invalidateWizardFrom("source");
            sendAsset(); sendStatus("Converted source basis to canonical +X/+Y/-Z; mesh preparation is now stale until explicitly rerun"); return;
        }
        if (command == "add_semantic_node")
        {
            const auto parentIndex = message.value("parentIndex", std::int32_t(NoIndex));
            if (parentIndex < NoIndex || parentIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                throw std::runtime_error("invalid semantic parent index");
            std::set<std::string> used;
            for (const auto& existing : m_asset.nodes) used.insert(existing.id);
            const std::string parentId = parentIndex >= 0 ? m_asset.nodes[static_cast<std::size_t>(parentIndex)].id : std::string();
            const std::string requested = message.value("id", std::string());
            Node node;
            node.id = allocateChildStableId(parentId, requested, "part", used);
            node.moduleId = message.value("moduleId", node.id);
            node.parentIndex = parentIndex;
            node.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f));
            node.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            node.pivot = jsonVec3(message.value("pivot", json::array()), glm::vec3(0.0f));
            m_asset.nodes.push_back(std::move(node));
            const std::string orderKey = parentIndex >= 0 ? m_asset.nodes[static_cast<std::size_t>(parentIndex)].id : std::string("__ROOTS__");
            m_semanticChildOrder[orderKey].push_back(m_asset.nodes.back().id);
            markManifestDirty(); markEditorStateDirty(); invalidateWizardFrom("semantics"); sendSemanticTreePatch();
            sendStatus("Added semantic node: " + m_asset.nodes.back().id); return;
        }
        if (command == "set_node_parents")
        {
            std::vector<std::size_t> indices;
            if (message.contains("nodeIndices") && message["nodeIndices"].is_array())
                for (const auto& value : message["nodeIndices"])
                    if (value.is_number_unsigned() || value.is_number_integer())
                    {
                        const auto raw = value.get<std::int64_t>();
                        if (raw >= 0) indices.push_back(static_cast<std::size_t>(raw));
                    }
            if (indices.empty()) throw std::runtime_error("semantic move requires at least one node");
            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            for (const auto index : indices)
                if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid semantic node index");

            const std::string placement = message.value("placement", std::string("inside"));
            const auto targetIndex = message.value("targetIndex", std::size_t(-1));
            std::int32_t parentIndex = message.value("parentIndex", std::int32_t(NoIndex));
            if (placement == "inside")
            {
                if (targetIndex != std::size_t(-1))
                {
                    if (targetIndex >= m_asset.nodes.size()) throw std::runtime_error("invalid semantic drop target");
                    parentIndex = static_cast<std::int32_t>(targetIndex);
                }
            }
            else if (placement == "before" || placement == "after")
            {
                if (targetIndex >= m_asset.nodes.size()) throw std::runtime_error("semantic reorder requires a valid target node");
                parentIndex = m_asset.nodes[targetIndex].parentIndex;
            }
            else throw std::runtime_error("invalid semantic tree placement");

            std::vector<std::string> movingIds;
            movingIds.reserve(indices.size());
            for (const auto index : indices) movingIds.push_back(m_asset.nodes[index].id);
            const std::string targetId = targetIndex < m_asset.nodes.size() ? m_asset.nodes[targetIndex].id : std::string();

            const bool parentChanged = std::any_of(indices.begin(), indices.end(), [&](std::size_t index) {
                return m_asset.nodes[index].parentIndex != parentIndex;
            });
            if (parentChanged)
            {
                reparentSemanticNodesPreserveWorld(m_asset, indices, parentIndex);
                if (parentIndex < 0)
                    for (const auto index : indices) m_asset.nodes[index].joint = NodeJoint{};
            }

            // Tree display order is editor-only and stable-id based. Never reorder
            // ModelAsset::nodes merely to move a row in the authoring tree.
            for (auto& [parentId, order] : m_semanticChildOrder)
                order.erase(std::remove_if(order.begin(), order.end(), [&](const std::string& id) {
                    return std::find(movingIds.begin(), movingIds.end(), id) != movingIds.end();
                }), order.end());
            const std::string parentKey = parentIndex >= 0
                ? m_asset.nodes[static_cast<std::size_t>(parentIndex)].id : std::string("__ROOTS__");
            std::vector<std::string> actualChildren;
            for (std::size_t i = 0; i < m_asset.nodes.size(); ++i)
                if (m_asset.nodes[i].parentIndex == parentIndex) actualChildren.push_back(m_asset.nodes[i].id);
            auto& order = m_semanticChildOrder[parentKey];
            std::vector<std::string> normalized;
            for (const auto& id : order)
                if (std::find(actualChildren.begin(), actualChildren.end(), id) != actualChildren.end() &&
                    std::find(normalized.begin(), normalized.end(), id) == normalized.end()) normalized.push_back(id);
            for (const auto& id : actualChildren)
                if (std::find(normalized.begin(), normalized.end(), id) == normalized.end()) normalized.push_back(id);
            for (const auto& id : movingIds)
                normalized.erase(std::remove(normalized.begin(), normalized.end(), id), normalized.end());
            auto insertAt = normalized.end();
            if ((placement == "before" || placement == "after") && !targetId.empty())
            {
                insertAt = std::find(normalized.begin(), normalized.end(), targetId);
                if (insertAt != normalized.end() && placement == "after") ++insertAt;
            }
            normalized.insert(insertAt, movingIds.begin(), movingIds.end());
            order = std::move(normalized);

            if (parentChanged)
            {
                markManifestDirty();
                invalidateWizardFrom("semantics");
            }
            markEditorStateDirty();
            sendSemanticTreePatch();
            sendStatus((parentChanged ? "Reparented " : "Reordered ") + std::to_string(indices.size()) +
                " semantic subtree root(s) (" + placement + ")" +
                (parentChanged ? " without changing world pose" : " in editor tree only")); return;
        }
        if (command == "clean_legacy_semantics")
        {
            if (!ensureAllLodsLoaded()) return;
            const auto cleanup = cleanLegacySyntheticVisualSemanticNodes(m_asset);
            if (cleanup.removedNodeIds.empty())
            {
                sendStatus("CLEAN LEGACY SEMANTICS: no recognized module->visual semantic bootstrap nodes found");
                return;
            }

            const std::set<std::string> removedIds(cleanup.removedNodeIds.begin(), cleanup.removedNodeIds.end());
            for (auto it = m_semanticChildOrder.begin(); it != m_semanticChildOrder.end();)
            {
                if (removedIds.find(it->first) != removedIds.end())
                {
                    it = m_semanticChildOrder.erase(it);
                    continue;
                }
                auto& order = it->second;
                order.erase(std::remove_if(order.begin(), order.end(), [&](const std::string& id) {
                    return removedIds.find(id) != removedIds.end();
                }), order.end());
                ++it;
            }

            for (const auto lodIndex : cleanup.affectedRenderLods) markLodDirty(lodIndex);
            markManifestDirty();
            markEditorStateDirty();
            invalidateWizardFrom("semantics");
            sendAssetMetadata({
                {"legacySemanticCleanup", {
                    {"removedNodes", cleanup.removedNodeIds.size()},
                    {"reboundRenderNodes", cleanup.reboundRenderNodes},
                    {"clearedTransformOnlyBindings", cleanup.clearedTransformOnlyBindings}
                }}
            });
            sendStatus(
                "CLEAN LEGACY SEMANTICS: removed " + std::to_string(cleanup.removedNodeIds.size()) +
                " synthetic visual semantic node(s), rebound " + std::to_string(cleanup.reboundRenderNodes) +
                " RenderNode binding(s); transform parent chains and structural graph links were not changed");
            return;
        }
        if (command == "flatten_static_semantic_tree")
        {
            // This command is semantic-transform-only. Guard the resident render
            // geometry byte-equivalent payload explicitly: flattening must never
            // change triangle winding, normals or any mesh coordinates.
            std::vector<std::vector<std::uint64_t>> geometryFingerprintsBefore(m_asset.renderLods.size());
            for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
            {
                if (li >= m_lodState.size() || !m_lodState[li].loaded) continue;
                auto& fingerprints = geometryFingerprintsBefore[li];
                fingerprints.reserve(m_asset.renderLods[li].geometries.size());
                for (const auto& geometry : m_asset.renderLods[li].geometries)
                    fingerprints.push_back(canonicalMeshFingerprint(geometry.mesh));
            }

            auto candidates = staticSemanticFlattenCandidates(m_asset);
            if (candidates.empty())
            {
                sendStatus("STATIC TREE -> ASSET SPACE: no safe static transform edges found");
                return;
            }

            // Flatten shallow parents first. Every reparent preserves the node's
            // current world pose, so descendants keep the same world pose while
            // their own static edge is subsequently removed.
            std::stable_sort(candidates.begin(), candidates.end(), [&](std::size_t a, std::size_t b) {
                const auto da = semanticNodeDepth(m_asset, a);
                const auto db = semanticNodeDepth(m_asset, b);
                return da == db ? a < b : da < db;
            });

            std::vector<std::string> movedIds;
            movedIds.reserve(candidates.size());
            for (const auto index : candidates)
            {
                if (index >= m_asset.nodes.size() || m_asset.nodes[index].parentIndex < 0) continue;
                movedIds.push_back(m_asset.nodes[index].id);
                reparentSemanticNodesPreserveWorld(m_asset, {index}, NoIndex);
                m_asset.nodes[index].joint = NodeJoint{};
            }
            if (movedIds.empty())
            {
                sendStatus("STATIC TREE -> ASSET SPACE: nothing changed");
                return;
            }

            // Display order is editor-only. Remove moved ids from former parent
            // buckets and append them to the ASSET SPACE bucket without touching
            // ModelAsset::nodes indices or any structural graph endpoints.
            for (auto& [parentId, order] : m_semanticChildOrder)
                order.erase(std::remove_if(order.begin(), order.end(), [&](const std::string& id) {
                    return std::find(movedIds.begin(), movedIds.end(), id) != movedIds.end();
                }), order.end());
            auto& assetSpaceOrder = m_semanticChildOrder["__ROOTS__"];
            for (const auto& id : movedIds)
                if (std::find(assetSpaceOrder.begin(), assetSpaceOrder.end(), id) == assetSpaceOrder.end())
                    assetSpaceOrder.push_back(id);

            for (std::size_t li = 0; li < geometryFingerprintsBefore.size(); ++li)
            {
                const auto& before = geometryFingerprintsBefore[li];
                if (before.empty()) continue;
                if (li >= m_asset.renderLods.size() || before.size() != m_asset.renderLods[li].geometries.size())
                    throw std::runtime_error("STATIC TREE -> ASSET SPACE changed resident render geometry inventory; operation aborted");
                for (std::size_t gi = 0; gi < before.size(); ++gi)
                    if (before[gi] != canonicalMeshFingerprint(m_asset.renderLods[li].geometries[gi].mesh))
                        throw std::runtime_error("STATIC TREE -> ASSET SPACE changed resident render geometry payload; operation aborted");
            }

            markManifestDirty();
            markEditorStateDirty();
            invalidateWizardFrom("semantics");
            sendAssetMetadata({
                {"staticTreeFlatten", {
                    {"movedToAssetSpace", movedIds.size()},
                    {"remainingTransformEdges", static_cast<std::size_t>(std::count_if(
                        m_asset.nodes.begin(), m_asset.nodes.end(), [](const Node& node) { return node.parentIndex >= 0; }))}
                }}
            });
            sendSemanticTreePatch();
            sendStatus(
                "STATIC TREE -> ASSET SPACE: moved " + std::to_string(movedIds.size()) +
                " semantic part(s) to asset space without changing world pose; resident render geometry verified unchanged; STRUCTURAL GRAPH unchanged");
            return;
        }
        if (command == "set_node_transform")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid node index");
            auto& n = m_asset.nodes[index];
            if (message.contains("position")) n.localPosition = jsonVec3(message["position"], n.localPosition);
            if (message.contains("rotationDeg")) n.localRotationDeg = jsonVec3(message["rotationDeg"], n.localRotationDeg);
            if (message.contains("pivot")) n.pivot = jsonVec3(message["pivot"], n.pivot);
            markManifestDirty(); invalidateWizardFrom("semantics"); sendSemanticTreePatch(); sendStatus("Updated node transform: " + n.id); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Updated default semantic state: " + m_asset.nodes[nodeIndex].id + " / " + stateId); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Added semantic state variant: " + m_asset.nodes[nodeIndex].id + " / " + m_asset.stateVariants.back().id); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Updated semantic state variant: " + variant.id); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Deleted semantic state variant: " + variant.id); return;
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
            const bool applyAllLods = message.value("applyAllLods", false);
            if (!ensureLodLoaded(lodIndex)) return;
            if (lodIndex >= m_asset.renderLods.size()) throw std::runtime_error("invalid render LOD index");
            auto& sourceLod = m_asset.renderLods[lodIndex];
            if (renderNodeIndex >= sourceLod.nodes.size()) throw std::runtime_error("invalid render node index");
            const auto stateIds = jsonStrings(message.value("activeStates", json::array()));
            const std::string stableRenderNodeId = sourceLod.nodes[renderNodeIndex].id;

            std::vector<std::pair<std::size_t, std::size_t>> targets {{lodIndex, renderNodeIndex}};
            if (applyAllLods)
            {
                if (!ensureAllLodsLoaded()) return;
                targets.clear();
                for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
                {
                    const auto& lod = m_asset.renderLods[li];
                    for (std::size_t ri = 0; ri < lod.nodes.size(); ++ri)
                        if (lod.nodes[ri].id == stableRenderNodeId)
                        {
                            targets.emplace_back(li, ri);
                            break;
                        }
                }
            }
            for (const auto [li, ri] : targets)
            {
                const auto& node = m_asset.renderLods[li].nodes[ri];
                requireSemanticStates(m_asset, node.semanticNodeIndex, stateIds,
                    "render node " + node.id + " in LOD" + std::to_string(li));
            }
            for (const auto [li, ri] : targets)
            {
                m_asset.renderLods[li].nodes[ri].activeStates = stateIds;
                markLodDirty(li);
            }
            invalidateWizardFrom("damage");
            sendAssetMetadata();
            sendStatus("Updated render state selector: " + stableRenderNodeId +
                (applyAllLods ? "; exact-id matches across LODs=" + std::to_string(targets.size()) : std::string()));
            return;
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
            appendRenderInstanceFitDiagnostic(wizardLogPath("instance_fit.log"), m_asset, lodIndex, referenceNodeIndex, renderNodeIndex, fit);
            if (!fit.valid) throw std::runtime_error(
                "cannot consolidate render instance: " + fit.message +
                "; see " + wizardLogPath("instance_fit.log").generic_string());
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
                appendRenderInstanceFitDiagnostic(wizardLogPath("instance_fit.log"), m_asset, lodIndex, referenceNodeIndex, targetNodeIndex, fit);
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
            const bool applyAllLods = message.value("applyAllLods", false);
            if (semanticNodeIndex < NoIndex || semanticNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                throw std::runtime_error("invalid semantic node index");
            if (!ensureLodLoaded(lodIndex)) return;
            if (lodIndex >= m_asset.renderLods.size()) throw std::runtime_error("invalid render LOD index");
            auto& sourceLod = m_asset.renderLods[lodIndex];
            if (renderNodeIndex >= sourceLod.nodes.size()) throw std::runtime_error("invalid render node index");
            const std::string stableRenderNodeId = sourceLod.nodes[renderNodeIndex].id;
            std::string maintenanceComponent;
            if (sourceLod.nodes[renderNodeIndex].geometryIndex >= 0 &&
                static_cast<std::size_t>(sourceLod.nodes[renderNodeIndex].geometryIndex) < sourceLod.geometries.size())
                maintenanceComponent = maintenanceComponentId(
                    lodIndex, sourceLod.geometries[static_cast<std::size_t>(sourceLod.nodes[renderNodeIndex].geometryIndex)]);
            const bool maintenanceLocal = !maintenanceComponent.empty() &&
                m_componentMaintenanceIssues.find(maintenanceComponent) != m_componentMaintenanceIssues.end();

            if (applyAllLods && !ensureAllLodsLoaded()) return;

            std::vector<std::pair<std::size_t, std::size_t>> targets;
            if (applyAllLods)
            {
                for (std::size_t li = 0; li < m_asset.renderLods.size(); ++li)
                    for (std::size_t ri = 0; ri < m_asset.renderLods[li].nodes.size(); ++ri)
                        if (m_asset.renderLods[li].nodes[ri].id == stableRenderNodeId)
                            targets.emplace_back(li, ri);
            }
            else
            {
                targets.emplace_back(lodIndex, renderNodeIndex);
            }

            // Validate the whole batch first: cross-LOD apply is transactional and
            // matches only exact stable RenderNode ids. Coarse/manual proxy nodes
            // with different ids are intentionally bound separately.
            for (const auto& [li, ri] : targets)
            {
                const auto& node = m_asset.renderLods[li].nodes[ri];
                requireSemanticStates(m_asset, semanticNodeIndex, node.activeStates, "render node " + node.id);
            }
            for (const auto& [li, ri] : targets)
            {
                m_asset.renderLods[li].nodes[ri].semanticNodeIndex = semanticNodeIndex;
                markLodDirty(li);
            }
            if (maintenanceLocal)
            {
                refreshMaintenanceSemanticIssue(maintenanceComponent);
                sendAssetMetadata();
            }
            else
            {
                invalidateWizardFrom("semantics");
            }
            sendSemanticBindingPatch(targets);
            sendStatus("Updated semantic binding for render node '" + stableRenderNodeId + "' in " +
                std::to_string(targets.size()) + " LOD(s)" + (applyAllLods ? " (exact-id cross-LOD apply)" : ""));
            return;
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
            // GEOMETRY duplicates visual placement/geometry only. Semantic identity
            // is never cloned implicitly: one semantic part accidentally owning two
            // RenderNodes makes distinct visuals behave as one part in SEMANTICS.
            clone.semanticNodeIndex = NoIndex;
            clone.activeStates.clear();
            lod.nodes.push_back(std::move(clone));
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus("Created LOD" + std::to_string(lodIndex) + " render instance: " + lod.nodes.back().id +
                " (semantic binding intentionally UNBOUND)"); return;
        }
        if (command == "move_render_node_delta")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            const glm::vec3 delta = jsonVec3(
                message.value("deltaPosition", json::array()), glm::vec3(0.0f));
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size())
                throw std::runtime_error("invalid render node index");
            auto& node = lod.nodes[renderNodeIndex];
            node.localPosition += delta;
            markLodDirty(lodIndex);
            invalidateWizardFrom("geometry");
            sendAssetMetadata();
            sendStatus(
                "Moved LOD" + std::to_string(lodIndex) + " render element " + node.id +
                " by [" + std::to_string(delta.x) + ", " + std::to_string(delta.y) +
                ", " + std::to_string(delta.z) + "]");
            return;
        }
        if (command == "apply_radial_render_layout")
        {
            const auto lodIndex = message.value("lodIndex", std::size_t(-1));
            const auto renderNodeIndex = message.value("renderNodeIndex", std::size_t(-1));
            const int count = std::clamp(message.value("count", 2), 1, 64);
            const float totalAngle = message.value("totalAngleDeg", 120.0f);
            const std::string axisName = message.value("axis", std::string("y"));
            const glm::vec3 center = jsonVec3(
                message.value("pivot", json::array()), glm::vec3(0.0f));
            if (!std::isfinite(totalAngle) || std::abs(totalAngle) < 1.0e-6f)
                throw std::runtime_error("invalid or zero circular layout angle");
            if (!ensureLodLoaded(lodIndex)) return;
            auto& lod = m_asset.renderLods.at(lodIndex);
            if (renderNodeIndex >= lod.nodes.size())
                throw std::runtime_error("invalid render node index");

            const glm::vec3 axis = axisName == "x" ? glm::vec3(1,0,0)
                : axisName == "z" ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
            const auto orbitTransform = [&](float angleDeg)
            {
                const glm::quat q = glm::angleAxis(glm::radians(angleDeg), axis);
                const glm::mat3 rotation = glm::mat3_cast(q);
                return RigidTransform {rotation, center - rotation * center};
            };

            if (count == 1)
            {
                auto& node = lod.nodes[renderNodeIndex];
                const RigidTransform moved = composeRigid(
                    orbitTransform(totalAngle), renderNodeRigidTransform(node));
                setRenderNodeRigidTransform(node, moved, node.pivot);
                markLodDirty(lodIndex);
                invalidateWizardFrom("geometry");
                sendAssetMetadata();
                sendStatus(
                    "Moved LOD" + std::to_string(lodIndex) + " render element " + node.id +
                    " around the circular center by " + std::to_string(totalAngle) + " deg");
                return;
            }

            const RenderNode base = lod.nodes[renderNodeIndex];
            const bool closedCircle = std::abs(std::abs(totalAngle) - 360.0f) <= 1.0e-4f;
            const float divisor = static_cast<float>(closedCircle ? count : count - 1);
            for (int i = 1; i < count; ++i)
            {
                const float angleDeg = totalAngle * static_cast<float>(i) / divisor;
                RenderNode clone = base;
                clone.id = uniqueRenderNodeId(
                    lod, base.id + "_radial_" + std::to_string(i + 1));
                // A circular copy is a new visual element, not another occurrence
                // of the source semantic part. Bind it explicitly in SEMANTICS.
                clone.semanticNodeIndex = NoIndex;
                clone.activeStates.clear();
                const RigidTransform transformed = composeRigid(
                    orbitTransform(angleDeg), renderNodeRigidTransform(base));
                setRenderNodeRigidTransform(clone, transformed, base.pivot);
                lod.nodes.push_back(std::move(clone));
            }
            markLodDirty(lodIndex);
            invalidateWizardFrom("geometry");
            sendAssetMetadata();
            sendStatus(
                "Created " + std::to_string(count - 1) + " LOD" + std::to_string(lodIndex) +
                " circular render instance(s) across " + std::to_string(totalAngle) +
                " deg" + (closedCircle ? " closed circle" : " inclusive arc") +
                "; new copies are semantic UNBOUND until explicitly assigned");
            return;
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
            const auto formerSemanticIndex = lod.nodes[renderNodeIndex].semanticNodeIndex;
            lod.nodes.erase(lod.nodes.begin() + static_cast<std::ptrdiff_t>(renderNodeIndex));
            for (auto& node : lod.nodes)
                if (node.parentIndex > static_cast<std::int32_t>(renderNodeIndex)) --node.parentIndex;
            std::string orphanNote;
            if (formerSemanticIndex >= 0 && static_cast<std::size_t>(formerSemanticIndex) < m_asset.nodes.size())
            {
                const auto usage = inspectSemanticNodeUsage(m_asset, static_cast<std::size_t>(formerSemanticIndex));
                if (usage.isOrphanCandidate())
                    orphanNote = "; semantic part '" + m_asset.nodes[static_cast<std::size_t>(formerSemanticIndex)].id +
                        "' is now ORPHAN and must be deleted or reused in SEMANTICS";
            }
            markLodDirty(lodIndex); invalidateWizardFrom("geometry"); sendAssetMetadata();
            sendStatus("Deleted LOD" + std::to_string(lodIndex) + " render node: " + id + orphanNote); return;
        }

        if (command == "delete_semantic_node")
        {
            if (!ensureAllLodsLoaded()) return;
            const auto index = message.value("nodeIndex", std::size_t(-1));
            if (index >= m_asset.nodes.size()) throw std::runtime_error("invalid semantic node index");
            const auto usage = inspectSemanticNodeUsage(m_asset, index);
            const bool removeOwnedPayload = message.value("removeOwnedPayload", false);
            const std::string deletedId = m_asset.nodes[index].id;
            const auto result = eraseSemanticNode(m_asset, index, removeOwnedPayload);
            for (const auto lodIndex : result.affectedRenderLods)
                if (lodIndex < m_lodState.size()) markLodDirty(lodIndex);
            for (auto& [parentId, order] : m_semanticChildOrder)
                order.erase(std::remove(order.begin(), order.end(), deletedId), order.end());
            m_semanticChildOrder.erase(deletedId);
            markManifestDirty(); markEditorStateDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata();
            sendStatus(
                "Deleted semantic part " + result.deletedId +
                "; visuals unbound=" + std::to_string(result.unboundRenderNodes) +
                ", owned payload removed=" + std::to_string(
                    result.removedCollisions + result.removedSockets + result.removedStateVariants +
                    result.removedHitRegions + result.removedOpenings + result.removedRepairTargets + result.removedStructuralLinks) +
                (usage.isOrphanCandidate() ? " (orphan cleanup)" : ""));
            return;
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

            // Build and validate the complete incoming-link state before publishing it.
            // In particular, glm::normalize(vec3(0)) produces NaNs, which used to poison
            // the working state until a later SEMANTICS CHECK discovered the damage.
            auto next = m_asset.nodes[index].joint;
            next.type = jointTypeFromName(message.value("type", std::string(jointTypeName(next.type))));
            if (message.contains("pivot")) next.pivot = jsonVec3(message["pivot"], next.pivot);
            if (message.contains("axis")) next.axis = jsonVec3(message["axis"], next.axis);
            next.defaultRateDegPerSec = message.value("defaultRateDegPerSec", next.defaultRateDegPerSec);
            next.minAngleDeg = message.value("minAngleDeg", next.minAngleDeg);
            next.maxAngleDeg = message.value("maxAngleDeg", next.maxAngleDeg);
            next.breakable = message.value("breakable", next.breakable);
            next.breakForceN = message.value("breakForceN", next.breakForceN);
            next.breakTorqueNm = message.value("breakTorqueNm", next.breakTorqueNm);

            const auto finiteVec3 = [](const glm::vec3& v) {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            };
            const bool revolute = next.type == model_asset::JointType::Revolute;
            const float axisLengthSq = glm::dot(next.axis, next.axis);
            if (!finiteVec3(next.pivot) || !finiteVec3(next.axis) ||
                !std::isfinite(next.defaultRateDegPerSec) || !std::isfinite(next.minAngleDeg) ||
                !std::isfinite(next.maxAngleDeg) || next.minAngleDeg > next.maxAngleDeg ||
                !std::isfinite(next.breakForceN) || !std::isfinite(next.breakTorqueNm) ||
                next.breakForceN < 0.0f || next.breakTorqueNm < 0.0f ||
                (revolute && (!std::isfinite(axisLengthSq) || axisLengthSq <= 1.0e-8f)))
                throw std::runtime_error("invalid joint parameters");

            if (revolute)
                next.axis *= 1.0f / std::sqrt(axisLengthSq);
            m_asset.nodes[index].joint = next;
            markManifestDirty(); invalidateWizardFrom("semantics"); sendSemanticTreePatch(); sendStatus("Updated joint: " + m_asset.nodes[index].id); return;
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
            markManifestDirty(); invalidateWizardFrom("physics"); sendAssetMetadata(); sendStatus("Updated rigid-body properties: " + m_asset.nodes[index].id); return;
        }
        if (command == "estimate_physics")
        {
            const auto index = message.value("nodeIndex", std::size_t(-1));
            const float density = std::max(0.001f, message.value("densityKgM3", 780.0f));
            if (!m_asset.physicalSize.enabled || m_asset.physicalSize.geometrySpace != PhysicalGeometrySpace::Authoring)
                throw std::runtime_error("set the asset physical scale before estimating mass from collision");
            if (!estimatePhysicsFromCollision(m_asset, index, density)) throw std::runtime_error("node has no enabled local collision volumes");
            markManifestDirty(); invalidateWizardFrom("physics"); sendAssetMetadata(); sendStatus("Estimated rigid-body properties from collision: " + m_asset.nodes[index].id); return;
        }
        if (command == "set_surface_mode")
        {
            const auto li = message.value("lodIndex", std::size_t(0));
            const auto gi = message.value("geometryIndex", std::size_t(-1));
            if (!ensureLodLoaded(li)) return;
            if (li >= m_asset.renderLods.size() || gi >= m_asset.renderLods[li].geometries.size()) throw std::runtime_error("invalid render geometry index");
            auto& geometry = m_asset.renderLods[li].geometries[gi];
            const auto desired = surfaceModeFromName(message.value("surfaceMode", "closed"));
            if (geometry.surfaceMode == desired)
            {
                sendStatus("NO CHANGES: surface mode already matches");
                return;
            }
            geometry.surfaceMode = desired;
            markLodDirty(li);
            invalidateWizardFrom("surfaces");
            sendSurfaceMetadataPatch({{li, gi}});
            sendStatus("Set LOD" + std::to_string(li) + " G" + std::to_string(gi) + " surface mode to " + std::string(surfaceModeName(geometry.surfaceMode))); return;
        }
        if (command == "set_material_definition")
        {
            const auto mi = message.value("materialIndex", std::size_t(-1));
            if (mi >= m_asset.materials.size()) throw std::runtime_error("invalid material index");
            auto& material = m_asset.materials[mi];
            const std::string requestedId = message.value("id", material.id);
            if (requestedId.empty()) throw std::runtime_error("material id cannot be empty");
            for (std::size_t other = 0; other < m_asset.materials.size(); ++other)
                if (other != mi && m_asset.materials[other].id == requestedId)
                    throw std::runtime_error("material id must be unique");

            material.id = requestedId;
            if (message.contains("baseColor")) material.baseColor = glm::clamp(jsonVec4(message["baseColor"], material.baseColor), glm::vec4(0.0f), glm::vec4(1.0f));
            if (message.contains("emissiveColor")) material.emissiveColor = glm::max(jsonVec3(message["emissiveColor"], material.emissiveColor), glm::vec3(0.0f));
            material.emissiveStrength = std::max(0.0f, message.value("emissiveStrength", material.emissiveStrength));
            material.metallic = std::clamp(message.value("metallic", material.metallic), 0.0f, 1.0f);
            material.roughness = std::clamp(message.value("roughness", material.roughness), 0.0f, 1.0f);
            material.twoSided = message.value("twoSided", material.twoSided);
            material.baseColorTexture = message.value("baseColorTexture", material.baseColorTexture);
            material.emissiveTexture = message.value("emissiveTexture", material.emissiveTexture);
            markManifestDirty();
            invalidateWizardFrom("surfaces");
            sendAssetMetadata();
            sendStatus("Updated material M" + std::to_string(mi) + ": " + material.id);
            return;
        }
        if (command == "assign_unassigned_material")
        {
            const auto li = message.value("lodIndex", std::size_t(0));
            const auto gi = message.value("geometryIndex", std::size_t(-1));
            const auto mi = message.value("materialIndex", std::size_t(-1));
            if (!ensureLodLoaded(li)) return;
            if (li >= m_asset.renderLods.size() || gi >= m_asset.renderLods[li].geometries.size()) throw std::runtime_error("invalid render geometry index");
            if (mi >= m_asset.materials.size()) throw std::runtime_error("invalid material index");
            auto& geometry = m_asset.renderLods[li].geometries[gi];
            std::size_t changed = 0;
            for (auto& triangle : geometry.mesh.triangles)
                if (triangle.materialIndex == NoIndex)
                {
                    triangle.materialIndex = static_cast<std::int32_t>(mi);
                    ++changed;
                }
            if (changed == 0)
            {
                sendStatus("NO CHANGES: selected geometry has no unassigned triangles");
                return;
            }
            markLodDirty(li);
            invalidateWizardFrom("surfaces");
            sendLodPayload(li);
            sendAssetMetadata();
            sendStatus("Assigned material M" + std::to_string(mi) + " to " + std::to_string(changed) +
                " previously unassigned triangles in LOD" + std::to_string(li) + " G" + std::to_string(gi));
            return;
        }
        if (command == "set_edge_render_mask")
        {
            const auto gi = message.value("geometryIndex", std::size_t(-1)), li = message.value("lodIndex", std::size_t(0)), ei = message.value("edgeIndex", std::size_t(-1));
            if (!ensureLodLoaded(li)) return;
            if (li >= m_asset.renderLods.size() || gi >= m_asset.renderLods[li].geometries.size() || ei >= m_asset.renderLods[li].geometries[gi].mesh.edges.size()) throw std::runtime_error("invalid edge index");
            const auto renderMask = static_cast<std::uint8_t>(message.value("renderMask", 0) & 0xff);
            m_asset.renderLods[li].geometries[gi].mesh.edges[ei].renderMask = renderMask;
            markLodDirty(li);
            invalidateWizardFrom("geometry");
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
            m_asset.collisionVolumes.push_back(std::move(c)); markManifestDirty(); invalidateWizardFrom("physics"); sendAssetMetadata(); sendStatus("Added collision volume: " + createdId); return;
        }
        if (command == "delete_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            const std::string deletedId = m_asset.collisionVolumes[index].id;
            m_asset.collisionVolumes.erase(m_asset.collisionVolumes.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); invalidateWizardFrom("physics"); sendAssetMetadata(); sendStatus("Deleted collision volume: " + deletedId); return;
        }
        if (command == "duplicate_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            auto c = m_asset.collisionVolumes[index]; c.id += "_copy"; const std::string createdId = c.id; m_asset.collisionVolumes.push_back(std::move(c)); markManifestDirty(); invalidateWizardFrom("physics"); sendAssetMetadata(); sendStatus("Duplicated collision volume: " + createdId); return;
        }
        if (command == "set_collision")
        {
            const auto index = message.value("collisionIndex", std::size_t(-1)); if (index >= m_asset.collisionVolumes.size()) throw std::runtime_error("invalid collision index");
            auto& c = m_asset.collisionVolumes[index];
            const bool physicsEdit = message.contains("shape") || message.contains("position") ||
                message.contains("rotationDeg") || message.contains("halfSize") || message.contains("radius") ||
                message.contains("halfHeight") || message.contains("enabled");
            if (message.contains("shape")) c.shape = collisionShapeFromName(message["shape"].get<std::string>());
            if (message.contains("position")) c.localPosition = jsonVec3(message["position"], c.localPosition);
            if (message.contains("rotationDeg")) c.localRotationDeg = jsonVec3(message["rotationDeg"], c.localRotationDeg);
            if (message.contains("halfSize")) c.halfSize = glm::max(jsonVec3(message["halfSize"], c.halfSize), glm::vec3(0.001f));
            if (message.contains("radius")) c.radius = std::max(0.001f, message["radius"].get<float>());
            if (message.contains("halfHeight")) c.halfHeight = std::max(0.0f, message["halfHeight"].get<float>());
            if (message.contains("enabled")) c.enabled = message["enabled"].get<bool>();
            if (message.contains("activeStates"))
            {
                const auto states = jsonStrings(message["activeStates"]);
                requireSemanticStates(m_asset, c.parentNodeIndex, states, "collision " + c.id);
                c.activeStates = states;
            }
            markManifestDirty(); invalidateWizardFrom(physicsEdit ? "physics" : "damage");
            sendAssetMetadata(); sendStatus("Updated collision volume: " + c.id); return;
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
            (void)ringAxis; markManifestDirty(); invalidateWizardFrom("physics"); sendAssetMetadata(); sendStatus("Generated " + std::to_string(count) + " radial collision capsules for " + m_asset.nodes[nodeIndex].id); return;
        }
        if (command == "set_physical_size_profile" || command == "apply_physical_size" ||
            command == "calibrate_physical_scale")
        {
            if (!ensureAllLodsLoaded()) return;
            auto& profile = m_asset.physicalSize;
            if (profile.geometrySpace == PhysicalGeometrySpace::LegacyUnknown)
                throw std::runtime_error("legacy destructive scale must be migrated with RELOAD ALL SOURCE MESHES before calibration");
            if (profile.geometrySpace != PhysicalGeometrySpace::Authoring)
                throw std::runtime_error("physical scale can only be calibrated in authoring space");
            if (message.contains("axis")) profile.axis = physicalSizeAxisFromName(message.value("axis", std::string("z")));
            if (message.contains("targetMeters")) profile.targetMeters = message.value("targetMeters", profile.targetMeters);
            if (!std::isfinite(profile.targetMeters) || profile.targetMeters <= 0.0f)
                throw std::runtime_error("physical target size must be > 0 meters");
            const float sourceExtent = authoringAxisExtent(m_asset, profile.axis);
            if (!std::isfinite(sourceExtent) || sourceExtent <= 1.0e-6f)
                throw std::runtime_error("cannot calibrate asset with zero/invalid authoring extent");
            profile.enabled = true;
            profile.sourceExtent = sourceExtent;
            profile.sourceToMeters = profile.targetMeters / sourceExtent;
            profile.autoApplyOnSourceImport = false;
            profile.geometrySpace = PhysicalGeometrySpace::Authoring;
            markManifestDirty();
            // Geometry does not change. Only physical interpretation and any
            // density-derived rigid-body evidence depend on the calibration.
            invalidateWizardFrom("physics");
            sendAssetMetadata();
            sendStatus("Set physical scale contract: " + std::string(physicalSizeAxisName(profile.axis)) +
                " " + std::to_string(profile.sourceExtent) + " authoring units -> " +
                std::to_string(profile.targetMeters) + " m; sourceToMeters=" +
                std::to_string(profile.sourceToMeters) + ". WORKING geometry was not resized.");
            return;
        }

        if (command == "add_socket")
        {
            Socket s; s.id = message.value("id", std::string("socket.new")); s.kind = message.value("kind", std::string("generic")); s.moduleId = message.value("moduleId", std::string()); s.parentNodeIndex = message.value("parentNodeIndex", NoIndex);
            if (s.parentNodeIndex < NoIndex || s.parentNodeIndex >= static_cast<std::int32_t>(m_asset.nodes.size())) throw std::runtime_error("invalid socket parent node");
            s.localPosition = jsonVec3(message.value("position", json::array()), glm::vec3(0.0f)); s.localRotationDeg = jsonVec3(message.value("rotationDeg", json::array()), glm::vec3(0.0f));
            s.interfaceProfile = message.value("interfaceProfile", std::string());
            s.previewFovDeg = std::clamp(message.value("previewFovDeg", 75.0f), 1.01f, 178.99f);
            s.activeStates = jsonStrings(message.value("activeStates", json::array()));
            if (s.kind == "light" || s.kind == "light_point") s.light.type = LightType::Point; else if (s.kind == "light_spot") s.light.type = LightType::Spot;
            const std::string createdId = s.id;
            m_asset.sockets.push_back(std::move(s)); markManifestDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata(); sendStatus("Added socket: " + createdId); return;
        }
        if (command == "delete_socket")
        {
            const auto index = message.value("socketIndex", std::size_t(-1)); if (index >= m_asset.sockets.size()) throw std::runtime_error("invalid socket index");
            const std::string deletedId = m_asset.sockets[index].id;
            m_asset.sockets.erase(m_asset.sockets.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata(); sendStatus("Deleted socket: " + deletedId); return;
        }
        if (command == "set_socket")
        {
            const auto index = message.value("socketIndex", std::size_t(-1)); if (index >= m_asset.sockets.size()) throw std::runtime_error("invalid socket index");
            auto& s = m_asset.sockets[index];
            const bool semanticEdit = message.contains("position") || message.contains("rotationDeg") ||
                message.contains("enabled") || message.contains("lightType") || message.contains("lightColor") ||
                message.contains("lightIntensity") || message.contains("lightRangeMeters") ||
                message.contains("lightOuterConeDeg");
            if (message.contains("position")) s.localPosition = jsonVec3(message["position"], s.localPosition);
            if (message.contains("rotationDeg")) s.localRotationDeg = jsonVec3(message["rotationDeg"], s.localRotationDeg);
            if (message.contains("enabled")) s.enabled = message["enabled"].get<bool>();
            if (message.contains("interfaceProfile")) s.interfaceProfile = message.value("interfaceProfile", s.interfaceProfile);
            if (message.contains("previewFovDeg")) s.previewFovDeg = std::clamp(message.value("previewFovDeg", s.previewFovDeg), 1.01f, 178.99f);
            if (message.contains("activeStates")) { const auto states = jsonStrings(message["activeStates"]); requireSemanticStates(m_asset, s.parentNodeIndex, states, "socket " + s.id); s.activeStates = states; }
            if (message.contains("lightType")) s.light.type = lightTypeFromName(message["lightType"].get<std::string>());
            if (message.contains("lightColor")) s.light.color = jsonVec3(message["lightColor"], s.light.color);
            if (message.contains("lightIntensity")) s.light.intensity = message["lightIntensity"].get<float>();
            if (message.contains("lightRangeMeters")) s.light.rangeMeters = message["lightRangeMeters"].get<float>();
            if (message.contains("lightOuterConeDeg")) s.light.outerConeDeg = message["lightOuterConeDeg"].get<float>();
            markManifestDirty(); invalidateWizardFrom(semanticEdit ? "semantics" : "damage");
            sendAssetMetadata(); sendStatus("Updated socket: " + s.id); return;
        }
        if (command == "add_structural_link")
        {
            if (!ensureLodLoaded(message.value("lodIndex", std::size_t(0)))) return;
            StructuralLinkDefinition link;
            link.id = message.value("id", std::string("link.new"));
            link.nodeAIndex = message.value("nodeAIndex", NoIndex);
            link.nodeBIndex = message.value("nodeBIndex", NoIndex);
            link.kind = structuralLinkKindFromName(message.value("kind", std::string("fixed_mount")));
            link.loadBearing = message.value("loadBearing", true);
            link.commandable = message.value("commandable", link.kind == StructuralLinkKind::ControlledLock);
            if (link.nodeAIndex < 0 || link.nodeBIndex < 0 || link.nodeAIndex == link.nodeBIndex ||
                link.nodeAIndex >= static_cast<std::int32_t>(m_asset.nodes.size()) || link.nodeBIndex >= static_cast<std::int32_t>(m_asset.nodes.size()))
                throw std::runtime_error("structural link requires two distinct semantic nodes");
            if (link.id.empty()) throw std::runtime_error("structural link id cannot be empty");
            for (const auto& existing : m_asset.structuralLinks) if (existing.id == link.id) throw std::runtime_error("structural link id already exists");
            if (message.value("autoProxy", true))
            {
                const auto lodIndex = message.value("lodIndex", std::size_t(0));
                if (lodIndex >= m_asset.renderLods.size()) throw std::runtime_error("invalid structural-link LOD");
                link.damageProxies.push_back(makeStructuralProxySeed(m_asset, m_asset.renderLods[lodIndex], link));
            }
            m_asset.structuralLinks.push_back(std::move(link));
            markManifestDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata(); sendStatus("Added structural link: " + m_asset.structuralLinks.back().id); return;
        }
        if (command == "set_structural_link")
        {
            const auto index = message.value("linkIndex", std::size_t(-1));
            if (index >= m_asset.structuralLinks.size()) throw std::runtime_error("invalid structural link index");
            auto& link = m_asset.structuralLinks[index];
            if (message.contains("kind")) link.kind = structuralLinkKindFromName(message.value("kind", std::string(structuralLinkKindName(link.kind))));
            if (message.contains("loadBearing")) link.loadBearing = message.value("loadBearing", link.loadBearing);
            if (message.contains("commandable")) link.commandable = message.value("commandable", link.commandable);
            if (message.contains("breakForceN")) link.breakForceN = std::max(0.0f, message.value("breakForceN", link.breakForceN));
            if (message.contains("breakTorqueNm")) link.breakTorqueNm = std::max(0.0f, message.value("breakTorqueNm", link.breakTorqueNm));
            if (message.contains("enabled")) link.enabled = message.value("enabled", link.enabled);
            markManifestDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata(); sendStatus("Updated structural link: " + link.id); return;
        }
        if (command == "delete_structural_link")
        {
            const auto index = message.value("linkIndex", std::size_t(-1));
            if (index >= m_asset.structuralLinks.size()) throw std::runtime_error("invalid structural link index");
            const std::string id = m_asset.structuralLinks[index].id;
            m_asset.structuralLinks.erase(m_asset.structuralLinks.begin() + static_cast<std::ptrdiff_t>(index));
            markManifestDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata(); sendStatus("Deleted structural link: " + id); return;
        }
        if (command == "set_structural_proxy")
        {
            const auto linkIndex = message.value("linkIndex", std::size_t(-1));
            const auto proxyIndex = message.value("proxyIndex", std::size_t(-1));
            if (linkIndex >= m_asset.structuralLinks.size() || proxyIndex >= m_asset.structuralLinks[linkIndex].damageProxies.size())
                throw std::runtime_error("invalid structural proxy index");
            auto& proxy = m_asset.structuralLinks[linkIndex].damageProxies[proxyIndex];
            if (message.contains("position")) proxy.localPosition = jsonVec3(message["position"], proxy.localPosition);
            if (message.contains("rotationDeg")) proxy.localRotationDeg = jsonVec3(message["rotationDeg"], proxy.localRotationDeg);
            if (message.contains("halfSize")) proxy.halfSize = glm::max(jsonVec3(message["halfSize"], proxy.halfSize), glm::vec3(0.001f));
            if (message.contains("radius")) proxy.radius = std::max(0.001f, message.value("radius", proxy.radius));
            if (message.contains("halfHeight")) proxy.halfHeight = std::max(0.001f, message.value("halfHeight", proxy.halfHeight));
            if (message.contains("enabled")) proxy.enabled = message.value("enabled", proxy.enabled);
            markManifestDirty(); invalidateWizardFrom("semantics"); sendAssetMetadata(); sendStatus("Updated structural damage proxy: " + proxy.id); return;
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
            m_asset.hitRegions.push_back(std::move(hit)); markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Added state-scoped hit region"); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Updated hit region: " + hit.id); return;
        }
        if (command == "delete_hit_region")
        {
            const auto index = message.value("hitRegionIndex", std::size_t(-1)); if (index >= m_asset.hitRegions.size()) throw std::runtime_error("invalid hit-region index");
            const auto id = m_asset.hitRegions[index].id; m_asset.hitRegions.erase(m_asset.hitRegions.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Deleted hit region: " + id); return;
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
            m_asset.openings.push_back(std::move(opening)); markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Added state-scoped opening"); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Updated opening: " + opening.id); return;
        }
        if (command == "delete_opening")
        {
            const auto index = message.value("openingIndex", std::size_t(-1)); if (index >= m_asset.openings.size()) throw std::runtime_error("invalid opening index");
            const auto id = m_asset.openings[index].id; m_asset.openings.erase(m_asset.openings.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Deleted opening: " + id); return;
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
            m_asset.repairTargets.push_back(std::move(target)); markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Added repair target"); return;
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
            markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Updated repair target: " + target.id); return;
        }
        if (command == "delete_repair_target")
        {
            const auto index = message.value("repairTargetIndex", std::size_t(-1)); if (index >= m_asset.repairTargets.size()) throw std::runtime_error("invalid repair target index");
            const auto id = m_asset.repairTargets[index].id; m_asset.repairTargets.erase(m_asset.repairTargets.begin() + static_cast<std::ptrdiff_t>(index)); markManifestDirty(); invalidateWizardFrom("damage"); sendAssetMetadata(); sendStatus("Deleted repair target: " + id); return;
        }

        sendStatus("Unknown editor command: " + command, true);
    }
    catch (const std::exception& ex)
    {
        sendStatus(std::string("Editor command failed: ") + ex.what(), true);
    }
}

} // namespace elite::model_asset::editor
