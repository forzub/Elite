#include "tools/model_asset_editor/NativeObjImporter.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <map>
#include <limits>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "render/tiny_obj_loader.h"

namespace elite::model_asset::editor
{
namespace
{
constexpr float Epsilon = 1.0e-7f;
constexpr float CreaseCos = 0.906307787f; // cos(25 deg)

struct CornerKey
{
    int v = -1;
    int n = -1;
    int uv = -1;
    bool operator==(const CornerKey& o) const noexcept
    {
        return v == o.v && n == o.n && uv == o.uv;
    }
};
struct CornerHash
{
    std::size_t operator()(const CornerKey& k) const noexcept
    {
        std::size_t h = std::hash<int>{}(k.v);
        h ^= std::hash<int>{}(k.n) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        h ^= std::hash<int>{}(k.uv) + 0x9e3779b9u + (h << 6u) + (h >> 2u);
        return h;
    }
};
struct SourceEdge
{
    int a = -1;
    int b = -1;
    bool operator<(const SourceEdge& o) const noexcept
    {
        return a < o.a || (a == o.a && b < o.b);
    }
};
SourceEdge edgeKey(int a, int b)
{
    if (a > b) std::swap(a, b);
    return {a, b};
}

std::string semanticId(std::string name)
{
    if (name.empty()) name = "material.default";
    for (char& c : name)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u)) c = static_cast<char>(std::tolower(u));
        else c = '_';
    }
    while (name.find("__") != std::string::npos)
        name.erase(name.find("__"), 1);
    if (!name.empty() && name.front() == '_') name.erase(name.begin());
    if (!name.empty() && name.back() == '_') name.pop_back();
    return name.empty() ? "material.default" : name;
}

std::int32_t materialIndexFor(ModelAsset& asset, const tinyobj::material_t& source)
{
    const std::string id = semanticId(source.name);
    for (std::size_t i = 0; i < asset.materials.size(); ++i)
        if (asset.materials[i].id == id)
            return static_cast<std::int32_t>(i);

    MaterialDefinition m;
    m.id = id;
    m.sourceName = source.name;
    m.baseColor = glm::vec4(source.diffuse[0], source.diffuse[1], source.diffuse[2], source.dissolve);
    m.emissiveColor = glm::vec3(source.emission[0], source.emission[1], source.emission[2]);
    m.emissiveStrength = glm::length(m.emissiveColor) > Epsilon ? 1.0f : 0.0f;
    m.metallic = std::clamp(static_cast<float>(source.metallic), 0.0f, 1.0f);
    m.roughness = source.roughness > 0.0f
        ? std::clamp(static_cast<float>(source.roughness), 0.0f, 1.0f)
        : 0.65f;
    m.baseColorTexture = source.diffuse_texname;
    m.emissiveTexture = source.emissive_texname;
    asset.materials.push_back(std::move(m));
    return static_cast<std::int32_t>(asset.materials.size() - 1);
}

void setError(std::string* error, const std::string& value)
{
    if (error) *error = value;
}
}

bool importObjNative(
    const std::filesystem::path& path,
    ModelAsset& asset,
    MeshLod& out,
    std::string* error
)
{
    if (error) error->clear();

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    const std::string base = path.parent_path().string() + std::string(1, std::filesystem::path::preferred_separator);
    if (!tinyobj::LoadObj(
            &attrib, &shapes, &materials, &warn, &err,
            path.string().c_str(), base.c_str(), false))
    {
        setError(error, "OBJ load failed: " + path.string() + (err.empty() ? "" : " (" + err + ")"));
        return false;
    }

    out = MeshLod{};
    std::unordered_map<CornerKey, std::uint32_t, CornerHash> vertexMap;
    std::vector<int> sourceNormalForCompiled;
    std::vector<glm::vec3> triangleNormals;

    auto compiledVertex = [&](const tinyobj::index_t& index) -> std::uint32_t
    {
        const CornerKey key {index.vertex_index, index.normal_index, index.texcoord_index};
        const auto found = vertexMap.find(key);
        if (found != vertexMap.end()) return found->second;

        Vertex v;
        if (index.vertex_index >= 0)
        {
            const std::size_t b = static_cast<std::size_t>(index.vertex_index) * 3u;
            v.position = glm::vec3(attrib.vertices[b], attrib.vertices[b + 1], attrib.vertices[b + 2]);
        }
        if (index.normal_index >= 0)
        {
            const std::size_t b = static_cast<std::size_t>(index.normal_index) * 3u;
            v.normal = glm::normalize(glm::vec3(attrib.normals[b], attrib.normals[b + 1], attrib.normals[b + 2]));
        }
        else
        {
            v.normal = glm::vec3(0.0f);
        }
        if (index.texcoord_index >= 0)
        {
            const std::size_t b = static_cast<std::size_t>(index.texcoord_index) * 2u;
            v.uv = glm::vec2(attrib.texcoords[b], attrib.texcoords[b + 1]);
        }

        const auto compiled = static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.push_back(v);
        sourceNormalForCompiled.push_back(index.normal_index);
        vertexMap.emplace(key, compiled);
        return compiled;
    };

    std::int32_t polygonId = 0;
    struct EdgeBuild
    {
        Edge edge;
        std::int32_t polygonA = -1;
        std::int32_t polygonB = -1;
        std::int32_t materialA = NoIndex;
        std::int32_t materialB = NoIndex;
        bool normalSeam = false;
    };
    std::map<SourceEdge, EdgeBuild> edgeBuilds;

    for (const auto& shape : shapes)
    {
        std::size_t offset = 0;
        for (std::size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face, ++polygonId)
        {
            const int fv = shape.mesh.num_face_vertices[face];
            if (fv < 3) { offset += static_cast<std::size_t>(std::max(fv, 0)); continue; }

            const std::uint32_t smoothingGroup = face < shape.mesh.smoothing_group_ids.size()
                ? shape.mesh.smoothing_group_ids[face] : 0u;
            std::int32_t materialIndex = NoIndex;
            const int sourceMaterial = face < shape.mesh.material_ids.size() ? shape.mesh.material_ids[face] : -1;
            if (sourceMaterial >= 0 && sourceMaterial < static_cast<int>(materials.size()))
                materialIndex = materialIndexFor(asset, materials[static_cast<std::size_t>(sourceMaterial)]);

            const tinyobj::index_t i0 = shape.mesh.indices[offset];
            for (int k = 1; k < fv - 1; ++k)
            {
                tinyobj::index_t corners[3] {
                    i0,
                    shape.mesh.indices[offset + static_cast<std::size_t>(k)],
                    shape.mesh.indices[offset + static_cast<std::size_t>(k + 1)]
                };
                std::uint32_t vi[3] {
                    compiledVertex(corners[0]), compiledVertex(corners[1]), compiledVertex(corners[2])
                };
                if (vi[0] == vi[1] || vi[1] == vi[2] || vi[2] == vi[0]) continue;

                const glm::vec3 a = out.vertices[vi[0]].position;
                const glm::vec3 b = out.vertices[vi[1]].position;
                const glm::vec3 c = out.vertices[vi[2]].position;
                const glm::vec3 cross = glm::cross(b - a, c - a);
                if (glm::dot(cross, cross) <= Epsilon) continue;
                const glm::vec3 faceNormal = glm::normalize(cross);

                const std::int32_t triangleIndex = static_cast<std::int32_t>(out.triangles.size());
                out.triangles.push_back({vi[0], vi[1], vi[2], polygonId, materialIndex, smoothingGroup});
                triangleNormals.push_back(faceNormal);

                for (int e = 0; e < 3; ++e)
                {
                    const int next = (e + 1) % 3;
                    const SourceEdge key = edgeKey(corners[e].vertex_index, corners[next].vertex_index);
                    auto& build = edgeBuilds[key];
                    const bool seam = corners[e].normal_index != corners[next].normal_index;
                    if (build.edge.triangleA < 0)
                    {
                        build.edge.a = vi[e]; build.edge.b = vi[next];
                        build.edge.triangleA = triangleIndex;
                        build.polygonA = polygonId;
                        build.materialA = materialIndex;
                        build.normalSeam = seam;
                    }
                    else if (build.edge.triangleB < 0)
                    {
                        build.edge.triangleB = triangleIndex;
                        build.polygonB = polygonId;
                        build.materialB = materialIndex;
                        build.normalSeam = build.normalSeam || seam;
                    }
                }
            }
            offset += static_cast<std::size_t>(fv);
        }
    }

    if (out.vertices.empty() || out.triangles.empty())
    {
        setError(error, "OBJ contains no usable triangles: " + path.string());
        return false;
    }

    // Only synthesize normals for corners that had no authored OBJ normal.
    std::vector<glm::vec3> accumulated(out.vertices.size(), glm::vec3(0.0f));
    for (std::size_t ti = 0; ti < out.triangles.size(); ++ti)
    {
        const auto& t = out.triangles[ti];
        for (const auto vi : {t.a, t.b, t.c})
            if (sourceNormalForCompiled[vi] < 0)
                accumulated[vi] += triangleNormals[ti];
    }
    for (std::size_t i = 0; i < out.vertices.size(); ++i)
    {
        if (sourceNormalForCompiled[i] < 0)
        {
            const float n2 = glm::dot(accumulated[i], accumulated[i]);
            out.vertices[i].normal = n2 > Epsilon ? glm::normalize(accumulated[i]) : glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    out.minBounds = glm::vec3(std::numeric_limits<float>::max());
    out.maxBounds = glm::vec3(-std::numeric_limits<float>::max());
    for (const auto& v : out.vertices)
    {
        out.minBounds = glm::min(out.minBounds, v.position);
        out.maxBounds = glm::max(out.maxBounds, v.position);
    }

    out.edges.reserve(edgeBuilds.size());
    for (auto& [key, build] : edgeBuilds)
    {
        if (build.edge.triangleB < 0)
        {
            build.edge.flags |= EdgeBoundary | EdgePolygonBoundary;
            build.edge.renderMask = EdgeRenderTechnical | EdgeRenderElite;
        }
        else
        {
            if (build.polygonA == build.polygonB)
                build.edge.flags |= EdgeTriangulationInternal;
            else
                build.edge.flags |= EdgePolygonBoundary;

            const glm::vec3 na = triangleNormals[static_cast<std::size_t>(build.edge.triangleA)];
            const glm::vec3 nb = triangleNormals[static_cast<std::size_t>(build.edge.triangleB)];
            const float ndot = std::clamp(glm::dot(na, nb), -1.0f, 1.0f);
            if (ndot < CreaseCos) build.edge.flags |= EdgeCrease;
            if (build.materialA != build.materialB) build.edge.flags |= EdgeMaterialSeam;
            if (build.normalSeam) build.edge.flags |= EdgeNormalSeam;

            build.edge.renderMask = 0;
            if ((build.edge.flags & (EdgeCrease | EdgeMaterialSeam | EdgeNormalSeam)) != 0)
                build.edge.renderMask |= EdgeRenderTechnical;
            if ((build.edge.flags & EdgeTriangulationInternal) == 0)
                build.edge.renderMask |= EdgeRenderElite;
        }
        out.edges.push_back(build.edge);
    }

    return true;
}

} // namespace elite::model_asset::editor
