#include "tools/model_asset_editor/CanonicalMeshBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace elite::model_asset::editor
{
namespace
{
constexpr double RelativeTriangleAreaEpsilon = 1.0e-14;
constexpr float CreaseCos = 0.906307787f; // cos(25 deg), same policy as NativeObjImporter.

using EdgeKey = std::uint64_t;

EdgeKey edgeKey(std::size_t a, std::size_t b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
           static_cast<std::uint64_t>(static_cast<std::uint32_t>(b));
}

std::array<std::size_t, 2> edgeVertices(EdgeKey key)
{
    return {
        static_cast<std::size_t>(static_cast<std::uint32_t>(key >> 32u)),
        static_cast<std::size_t>(static_cast<std::uint32_t>(key))
    };
}

using QuantizedPosition = std::array<std::int64_t, 3>;
using PositionEdgeKey = std::pair<QuantizedPosition, QuantizedPosition>;

QuantizedPosition quantizedPosition(const glm::vec3& p)
{
    return {
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.x) / CanonicalMeshWeldEpsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.y) / CanonicalMeshWeldEpsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.z) / CanonicalMeshWeldEpsilon))
    };
}

PositionEdgeKey positionEdgeKey(const glm::vec3& a, const glm::vec3& b)
{
    auto qa = quantizedPosition(a);
    auto qb = quantizedPosition(b);
    if (qb < qa) std::swap(qa, qb);
    return {qa, qb};
}

struct DisjointSet
{
    explicit DisjointSet(std::size_t count) : parent(count), rank(count, 0)
    {
        std::iota(parent.begin(), parent.end(), std::size_t(0));
    }

    std::size_t find(std::size_t value)
    {
        if (parent[value] != value) parent[value] = find(parent[value]);
        return parent[value];
    }

    void unite(std::size_t a, std::size_t b)
    {
        a = find(a); b = find(b);
        if (a == b) return;
        if (rank[a] < rank[b]) std::swap(a, b);
        parent[b] = a;
        if (rank[a] == rank[b]) ++rank[a];
    }

    std::vector<std::size_t> parent;
    std::vector<std::uint8_t> rank;
};

struct CanonicalPointMap
{
    std::vector<std::size_t> pointForVertex;
    std::vector<std::size_t> representativeVertex;
};

CanonicalPointMap buildPointMap(const MeshLod& mesh)
{
    CanonicalPointMap out;
    out.pointForVertex.resize(mesh.vertices.size(), 0);
    std::map<std::array<std::int64_t, 3>, std::size_t> byPosition;
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi)
    {
        const auto& p = mesh.vertices[vi].position;
        const auto key = quantizedPosition(p);
        const auto [it, inserted] = byPosition.emplace(key, byPosition.size());
        out.pointForVertex[vi] = it->second;
        if (inserted) out.representativeVertex.push_back(vi);
    }
    return out;
}

bool finiteVec3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool usableTriangle(const MeshLod& mesh, const Triangle& t)
{
    if (t.a >= mesh.vertices.size() || t.b >= mesh.vertices.size() || t.c >= mesh.vertices.size()) return false;
    const glm::dvec3 a(mesh.vertices[t.a].position);
    const glm::dvec3 b(mesh.vertices[t.b].position);
    const glm::dvec3 c(mesh.vertices[t.c].position);
    const glm::dvec3 ab = b - a;
    const glm::dvec3 ac = c - a;
    const glm::dvec3 bc = c - b;
    const double maxEdge2 = std::max({glm::dot(ab, ab), glm::dot(ac, ac), glm::dot(bc, bc)});
    if (!std::isfinite(maxEdge2) || maxEdge2 <= 0.0) return false;
    const glm::dvec3 area = glm::cross(ab, ac);
    const double area2 = glm::dot(area, area);
    return std::isfinite(area2) && area2 > maxEdge2 * maxEdge2 * RelativeTriangleAreaEpsilon;
}

struct WorkingTriangle
{
    Triangle triangle;
    std::array<std::size_t, 3> point {0, 0, 0};
};

struct EdgeUse
{
    std::size_t triangleIndex = 0;
    int direction = 0;
};

struct WorkingTopology
{
    std::map<EdgeKey, std::vector<EdgeUse>> edgeUses;
};

WorkingTopology buildTopology(const std::vector<WorkingTriangle>& triangles)
{
    WorkingTopology out;
    for (std::size_t ti = 0; ti < triangles.size(); ++ti)
    {
        const auto& p = triangles[ti].point;
        for (int e = 0; e < 3; ++e)
        {
            const auto a = p[e], b = p[(e + 1) % 3];
            if (a == b) continue;
            out.edgeUses[edgeKey(a, b)].push_back({ti, a < b ? 1 : -1});
        }
    }
    return out;
}

std::array<std::size_t, 3> sortedTriangleKey(const std::array<std::size_t, 3>& p)
{
    auto sorted = p;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

struct PreparedWorkingSet
{
    CanonicalPointMap points;
    std::vector<WorkingTriangle> triangles;
    std::size_t invalidTriangles = 0;
    std::size_t degenerateTriangles = 0;
    std::size_t duplicateTriangles = 0;
};

PreparedWorkingSet buildWorkingSet(const MeshLod& mesh)
{
    PreparedWorkingSet out;
    out.points = buildPointMap(mesh);
    std::set<std::tuple<std::size_t, std::size_t, std::size_t, std::int32_t>> duplicateKeys;
    out.triangles.reserve(mesh.triangles.size());
    for (const auto& triangle : mesh.triangles)
    {
        if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() || triangle.c >= mesh.vertices.size())
        {
            ++out.invalidTriangles;
            continue;
        }
        const std::array<std::size_t, 3> points = {
            out.points.pointForVertex[triangle.a],
            out.points.pointForVertex[triangle.b],
            out.points.pointForVertex[triangle.c]
        };
        if (points[0] == points[1] || points[1] == points[2] || points[2] == points[0] || !usableTriangle(mesh, triangle))
        {
            ++out.degenerateTriangles;
            continue;
        }
        const auto sorted = sortedTriangleKey(points);
        const auto key = std::make_tuple(sorted[0], sorted[1], sorted[2], triangle.materialIndex);
        if (!duplicateKeys.insert(key).second)
        {
            ++out.duplicateTriangles;
            continue;
        }
        out.triangles.push_back({triangle, points});
    }
    return out;
}

std::vector<glm::dvec3> faceNormals(const MeshLod& mesh, const std::vector<WorkingTriangle>& triangles)
{
    std::vector<glm::dvec3> out(triangles.size(), glm::dvec3(0.0));
    for (std::size_t ti = 0; ti < triangles.size(); ++ti)
    {
        const auto& t = triangles[ti].triangle;
        const glm::dvec3 a(mesh.vertices[t.a].position);
        const glm::dvec3 b(mesh.vertices[t.b].position);
        const glm::dvec3 c(mesh.vertices[t.c].position);
        const glm::dvec3 n = glm::cross(b - a, c - a);
        const double len = glm::length(n);
        if (len > 1.0e-18 && std::isfinite(len)) out[ti] = n / len;
    }
    return out;
}

struct OrientationSolution
{
    std::vector<int> parity;
    std::size_t flipsRequired = 0;
    std::size_t conflicts = 0;
};

OrientationSolution solveOrientation(const WorkingTopology& topology, std::size_t triangleCount)
{
    OrientationSolution out;
    out.parity.assign(triangleCount, -1);
    std::vector<std::vector<std::pair<std::size_t, bool>>> adjacency(triangleCount);
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        if (uses.size() != 2) continue;
        const bool parityDiff = uses[0].direction == uses[1].direction;
        adjacency[uses[0].triangleIndex].push_back({uses[1].triangleIndex, parityDiff});
        adjacency[uses[1].triangleIndex].push_back({uses[0].triangleIndex, parityDiff});
    }
    for (std::size_t seed = 0; seed < triangleCount; ++seed)
    {
        if (out.parity[seed] != -1) continue;
        std::vector<std::size_t> queue {seed};
        out.parity[seed] = 0;
        bool componentConflict = false;
        for (std::size_t qi = 0; qi < queue.size(); ++qi)
        {
            const auto ti = queue[qi];
            for (const auto& [neighbor, diff] : adjacency[ti])
            {
                const int wanted = out.parity[ti] ^ (diff ? 1 : 0);
                if (out.parity[neighbor] == -1)
                {
                    out.parity[neighbor] = wanted;
                    queue.push_back(neighbor);
                }
                else if (out.parity[neighbor] != wanted)
                {
                    componentConflict = true;
                }
            }
        }
        if (componentConflict) ++out.conflicts;
    }
    out.flipsRequired = static_cast<std::size_t>(std::count(out.parity.begin(), out.parity.end(), 1));
    return out;
}

struct ComponentSummary
{
    std::vector<std::vector<std::size_t>> triangles;
    std::vector<std::size_t> boundaryEdges;
    std::vector<std::size_t> multiUseEdges;
};

ComponentSummary buildComponents(const WorkingTopology& topology, std::size_t triangleCount)
{
    ComponentSummary out;
    if (triangleCount == 0) return out;
    DisjointSet sets(triangleCount);
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        if (uses.size() == 2) sets.unite(uses[0].triangleIndex, uses[1].triangleIndex);
    }
    std::map<std::size_t, std::size_t> componentIndex;
    for (std::size_t ti = 0; ti < triangleCount; ++ti)
    {
        const auto root = sets.find(ti);
        const auto [it, inserted] = componentIndex.emplace(root, componentIndex.size());
        if (inserted)
        {
            out.triangles.emplace_back();
            out.boundaryEdges.push_back(0);
            out.multiUseEdges.push_back(0);
        }
        out.triangles[it->second].push_back(ti);
    }
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        if (uses.empty()) continue;
        std::set<std::size_t> touched;
        for (const auto& use : uses)
        {
            const auto root = sets.find(use.triangleIndex);
            touched.insert(componentIndex[root]);
        }
        if (uses.size() == 1)
        {
            ++out.boundaryEdges[*touched.begin()];
        }
        else if (uses.size() > 2)
        {
            for (const auto ci : touched) ++out.multiUseEdges[ci];
        }
    }
    return out;
}

std::size_t sourceNonManifoldEdgeCount(const MeshLod& mesh)
{
    return static_cast<std::size_t>(std::count_if(mesh.edges.begin(), mesh.edges.end(), [](const Edge& edge) {
        return (edge.flags & EdgeNonManifold) != 0;
    }));
}

void applyParity(std::vector<WorkingTriangle>& triangles, const OrientationSolution& orientation)
{
    for (std::size_t ti = 0; ti < triangles.size() && ti < orientation.parity.size(); ++ti)
    {
        if (orientation.parity[ti] != 1) continue;
        std::swap(triangles[ti].triangle.b, triangles[ti].triangle.c);
        std::swap(triangles[ti].point[1], triangles[ti].point[2]);
    }
}

std::size_t flipInsideOutClosedComponents(
    const MeshLod& mesh,
    std::vector<WorkingTriangle>& triangles,
    const WorkingTopology& topology,
    std::size_t& flippedTriangles)
{
    std::size_t flippedComponents = 0;
    const auto components = buildComponents(topology, triangles.size());
    for (std::size_t ci = 0; ci < components.triangles.size(); ++ci)
    {
        if (components.boundaryEdges[ci] != 0) continue;
        double volume6 = 0.0;
        for (const auto ti : components.triangles[ci])
        {
            const auto& t = triangles[ti].triangle;
            const glm::dvec3 a(mesh.vertices[t.a].position);
            const glm::dvec3 b(mesh.vertices[t.b].position);
            const glm::dvec3 c(mesh.vertices[t.c].position);
            volume6 += glm::dot(a, glm::cross(b, c));
        }
        if (volume6 >= -1.0e-10) continue;
        for (const auto ti : components.triangles[ci])
        {
            std::swap(triangles[ti].triangle.b, triangles[ti].triangle.c);
            std::swap(triangles[ti].point[1], triangles[ti].point[2]);
            ++flippedTriangles;
        }
        ++flippedComponents;
    }
    return flippedComponents;
}

bool edgeIsSmooth(
    const WorkingTriangle& a,
    const WorkingTriangle& b,
    const glm::dvec3& normalA,
    const glm::dvec3& normalB)
{
    if (a.triangle.sourcePolygonId >= 0 && a.triangle.sourcePolygonId == b.triangle.sourcePolygonId)
        return true;
    const auto sa = a.triangle.smoothingGroupId;
    const auto sb = b.triangle.smoothingGroupId;
    if (sa != 0 || sb != 0) return sa != 0 && sa == sb;
    const double d = std::clamp(glm::dot(normalA, normalB), -1.0, 1.0);
    return d >= static_cast<double>(CreaseCos);
}

std::size_t cornerForPoint(const WorkingTriangle& triangle, std::size_t point)
{
    for (std::size_t corner = 0; corner < 3; ++corner)
        if (triangle.point[corner] == point) return corner;
    return 3;
}

struct OldEdgeMetadata
{
    std::uint32_t flags = 0;
    std::uint8_t renderMask = 0;
    bool hasMask = false;
};

std::map<PositionEdgeKey, OldEdgeMetadata> collectOldEdgeMetadata(const MeshLod& mesh)
{
    std::map<PositionEdgeKey, OldEdgeMetadata> out;
    for (const auto& edge : mesh.edges)
    {
        if (edge.a >= mesh.vertices.size() || edge.b >= mesh.vertices.size()) continue;
        const auto key = positionEdgeKey(mesh.vertices[edge.a].position, mesh.vertices[edge.b].position);
        if (key.first == key.second) continue;
        auto& meta = out[key];
        meta.flags |= edge.flags;
        if (!meta.hasMask)
        {
            meta.renderMask = edge.renderMask;
            meta.hasMask = true;
        }
        else
        {
            meta.renderMask |= edge.renderMask;
        }
    }
    return out;
}

struct RebuiltRenderMesh
{
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::size_t normalIslands = 0;
};

RebuiltRenderMesh rebuildRenderVertices(
    const MeshLod& source,
    const std::vector<WorkingTriangle>& triangles,
    const WorkingTopology& topology)
{
    RebuiltRenderMesh out;
    if (triangles.empty()) return out;

    const auto normals = faceNormals(source, triangles);
    const std::size_t cornerCount = triangles.size() * 3u;
    DisjointSet normalSets(cornerCount);
    for (const auto& [key, uses] : topology.edgeUses)
    {
        if (uses.size() != 2) continue;
        const auto& ua = uses[0];
        const auto& ub = uses[1];
        if (!edgeIsSmooth(triangles[ua.triangleIndex], triangles[ub.triangleIndex],
                          normals[ua.triangleIndex], normals[ub.triangleIndex]))
            continue;
        const auto endpoints = edgeVertices(key);
        for (const auto point : endpoints)
        {
            const auto ca = cornerForPoint(triangles[ua.triangleIndex], point);
            const auto cb = cornerForPoint(triangles[ub.triangleIndex], point);
            if (ca < 3 && cb < 3)
                normalSets.unite(ua.triangleIndex * 3u + ca, ub.triangleIndex * 3u + cb);
        }
    }

    std::map<std::size_t, glm::dvec3> sums;
    for (std::size_t ti = 0; ti < triangles.size(); ++ti)
    {
        for (std::size_t corner = 0; corner < 3; ++corner)
            sums[normalSets.find(ti * 3u + corner)] += normals[ti];
    }
    std::map<std::size_t, glm::vec3> islandNormal;
    for (const auto& [root, sum] : sums)
    {
        const double len = glm::length(sum);
        islandNormal[root] = len > 1.0e-18
            ? glm::vec3(sum / len)
            : glm::vec3(0.0f, 1.0f, 0.0f);
    }
    out.normalIslands = islandNormal.size();

    using RenderKey = std::pair<std::uint32_t, std::size_t>;
    std::map<RenderKey, std::uint32_t> renderMap;
    out.triangles.reserve(triangles.size());
    for (std::size_t ti = 0; ti < triangles.size(); ++ti)
    {
        Triangle rebuilt = triangles[ti].triangle;
        std::uint32_t* dst[3] = {&rebuilt.a, &rebuilt.b, &rebuilt.c};
        const std::uint32_t src[3] = {
            triangles[ti].triangle.a,
            triangles[ti].triangle.b,
            triangles[ti].triangle.c
        };
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            const auto root = normalSets.find(ti * 3u + corner);
            const RenderKey key {src[corner], root};
            const auto found = renderMap.find(key);
            if (found != renderMap.end())
            {
                *dst[corner] = found->second;
                continue;
            }
            Vertex vertex = source.vertices[src[corner]];
            vertex.normal = islandNormal[root];
            const auto newIndex = static_cast<std::uint32_t>(out.vertices.size());
            out.vertices.push_back(vertex);
            renderMap.emplace(key, newIndex);
            *dst[corner] = newIndex;
        }
        out.triangles.push_back(rebuilt);
    }
    return out;
}

void updateBounds(MeshLod& mesh)
{
    if (mesh.vertices.empty())
    {
        mesh.minBounds = glm::vec3(0.0f);
        mesh.maxBounds = glm::vec3(0.0f);
        return;
    }
    mesh.minBounds = glm::vec3(std::numeric_limits<float>::max());
    mesh.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto& vertex : mesh.vertices)
    {
        mesh.minBounds = glm::min(mesh.minBounds, vertex.position);
        mesh.maxBounds = glm::max(mesh.maxBounds, vertex.position);
    }
}

struct RenderEdgeUse
{
    std::size_t triangleIndex = 0;
    std::size_t pointA = 0;
    std::size_t pointB = 0;
    std::uint32_t renderA = 0;
    std::uint32_t renderB = 0;
};

std::vector<Edge> rebuildEdges(
    const MeshLod& mesh,
    const CanonicalPointMap& points,
    const std::map<PositionEdgeKey, OldEdgeMetadata>& oldMetadata)
{
    std::map<EdgeKey, std::vector<RenderEdgeUse>> uses;
    std::vector<glm::dvec3> normals(mesh.triangles.size(), glm::dvec3(0.0));
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
    {
        const auto& t = mesh.triangles[ti];
        const std::uint32_t r[3] = {t.a, t.b, t.c};
        const std::size_t p[3] = {
            points.pointForVertex[t.a], points.pointForVertex[t.b], points.pointForVertex[t.c]
        };
        const glm::dvec3 a(mesh.vertices[t.a].position);
        const glm::dvec3 b(mesh.vertices[t.b].position);
        const glm::dvec3 c(mesh.vertices[t.c].position);
        const auto n = glm::cross(b - a, c - a);
        const auto len = glm::length(n);
        if (len > 1.0e-18) normals[ti] = n / len;
        for (int e = 0; e < 3; ++e)
        {
            const int next = (e + 1) % 3;
            uses[edgeKey(p[e], p[next])].push_back({ti, p[e], p[next], r[e], r[next]});
        }
    }

    std::vector<Edge> out;
    out.reserve(uses.size());
    for (const auto& [key, edgeUses] : uses)
    {
        (void)key;
        if (edgeUses.empty()) continue;
        const auto& first = edgeUses.front();
        Edge edge;
        edge.a = first.renderA;
        edge.b = first.renderB;
        edge.triangleA = static_cast<std::int32_t>(first.triangleIndex);
        if (edgeUses.size() >= 2)
            edge.triangleB = static_cast<std::int32_t>(edgeUses[1].triangleIndex);

        const auto oldKey = positionEdgeKey(mesh.vertices[first.renderA].position, mesh.vertices[first.renderB].position);
        const auto old = oldMetadata.find(oldKey);
        if (old != oldMetadata.end())
        {
            edge.flags |= old->second.flags & (EdgeAuthored | EdgeNonManifold);
            if (old->second.hasMask) edge.renderMask = old->second.renderMask;
        }

        if (edgeUses.size() == 1)
        {
            edge.flags |= EdgeBoundary | EdgePolygonBoundary;
            if (old == oldMetadata.end() || !old->second.hasMask)
                edge.renderMask = EdgeRenderTechnical | EdgeRenderElite;
        }
        else
        {
            const auto& a = mesh.triangles[edgeUses[0].triangleIndex];
            const auto& b = mesh.triangles[edgeUses[1].triangleIndex];
            if (a.sourcePolygonId >= 0 && a.sourcePolygonId == b.sourcePolygonId)
                edge.flags |= EdgeTriangulationInternal;
            else
                edge.flags |= EdgePolygonBoundary;
            const double dot = std::clamp(glm::dot(normals[edgeUses[0].triangleIndex], normals[edgeUses[1].triangleIndex]), -1.0, 1.0);
            if (dot < static_cast<double>(CreaseCos)) edge.flags |= EdgeCrease;
            if (a.materialIndex != b.materialIndex) edge.flags |= EdgeMaterialSeam;

            WorkingTriangle wa {a, {points.pointForVertex[a.a], points.pointForVertex[a.b], points.pointForVertex[a.c]}};
            WorkingTriangle wb {b, {points.pointForVertex[b.a], points.pointForVertex[b.b], points.pointForVertex[b.c]}};
            if (!edgeIsSmooth(wa, wb, normals[edgeUses[0].triangleIndex], normals[edgeUses[1].triangleIndex]))
                edge.flags |= EdgeNormalSeam;

            if (old == oldMetadata.end() || !old->second.hasMask)
            {
                edge.renderMask = 0;
                if ((edge.flags & (EdgeCrease | EdgeMaterialSeam | EdgeNormalSeam)) != 0)
                    edge.renderMask |= EdgeRenderTechnical;
                if ((edge.flags & EdgeTriangulationInternal) == 0)
                    edge.renderMask |= EdgeRenderElite;
            }
        }
        out.push_back(edge);
    }
    return out;
}

bool sameVec2(const glm::vec2& a, const glm::vec2& b)
{
    return a.x == b.x && a.y == b.y;
}

bool sameVec3(const glm::vec3& a, const glm::vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool meshPayloadEqual(const MeshLod& a, const MeshLod& b)
{
    if (a.vertices.size() != b.vertices.size() || a.triangles.size() != b.triangles.size() || a.edges.size() != b.edges.size()) return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
    {
        const auto& av = a.vertices[i]; const auto& bv = b.vertices[i];
        if (!sameVec3(av.position, bv.position) || !sameVec3(av.normal, bv.normal) || !sameVec2(av.uv, bv.uv)) return false;
    }
    for (std::size_t i = 0; i < a.triangles.size(); ++i)
    {
        const auto& x = a.triangles[i]; const auto& y = b.triangles[i];
        if (x.a != y.a || x.b != y.b || x.c != y.c || x.sourcePolygonId != y.sourcePolygonId ||
            x.materialIndex != y.materialIndex || x.smoothingGroupId != y.smoothingGroupId) return false;
    }
    for (std::size_t i = 0; i < a.edges.size(); ++i)
    {
        const auto& x = a.edges[i]; const auto& y = b.edges[i];
        if (x.a != y.a || x.b != y.b || x.triangleA != y.triangleA || x.triangleB != y.triangleB ||
            x.flags != y.flags || x.renderMask != y.renderMask) return false;
    }
    return sameVec3(a.minBounds, b.minBounds) && sameVec3(a.maxBounds, b.maxBounds);
}

void hashBytes(std::uint64_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
}

template <class T>
void hashValue(std::uint64_t& hash, const T& value)
{
    hashBytes(hash, &value, sizeof(value));
}

} // namespace

CanonicalMeshAnalysis analyzeCanonicalMesh(const MeshLod& mesh)
{
    CanonicalMeshAnalysis out;
    out.renderVertices = mesh.vertices.size();
    out.triangles = mesh.triangles.size();
    out.sourceNonManifoldEdges = sourceNonManifoldEdgeCount(mesh);

    for (const auto& vertex : mesh.vertices)
    {
        if (!finiteVec3(vertex.position)) out.finitePositions = false;
    }
    if (!out.finitePositions)
    {
        out.structuralInvalid = true;
        out.invalidReason = "non-finite vertex position";
        return out;
    }

    const auto working = buildWorkingSet(mesh);
    out.geometricPoints = working.points.representativeVertex.size();
    out.invalidTriangles = working.invalidTriangles;
    out.degenerateTriangles = working.degenerateTriangles;
    out.duplicateTriangles = working.duplicateTriangles;
    if (out.invalidTriangles != 0)
    {
        out.structuralInvalid = true;
        out.invalidReason = "triangle index outside render vertex array";
        return out;
    }
    if (working.triangles.empty())
    {
        out.structuralInvalid = true;
        out.invalidReason = "mesh has no usable triangles after canonical cleanup";
        return out;
    }
    if (out.sourceNonManifoldEdges != 0)
    {
        out.structuralInvalid = true;
        out.invalidReason = "source topology contains an edge used by more than two faces";
    }

    const auto topology = buildTopology(working.triangles);
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        if (uses.size() == 1) ++out.boundaryEdges;
        else if (uses.size() > 2) ++out.canonicalMultiUseEdges;
    }
    const auto orientation = solveOrientation(topology, working.triangles.size());
    out.windingFlipsRequired = orientation.flipsRequired;
    out.windingConflicts = orientation.conflicts;
    if (orientation.conflicts != 0)
    {
        out.structuralInvalid = true;
        if (out.invalidReason.empty()) out.invalidReason = "topology cannot be oriented consistently";
    }

    auto oriented = working.triangles;
    applyParity(oriented, orientation);
    const auto orientedTopology = buildTopology(oriented);
    const auto components = buildComponents(orientedTopology, oriented.size());
    out.components = components.triangles.size();
    for (std::size_t ci = 0; ci < components.triangles.size(); ++ci)
    {
        const bool closed = components.boundaryEdges[ci] == 0;
        if (closed) ++out.closedComponents; else ++out.openComponents;
        if (!closed) continue;
        double volume6 = 0.0;
        for (const auto ti : components.triangles[ci])
        {
            const auto& t = oriented[ti].triangle;
            const glm::dvec3 a(mesh.vertices[t.a].position);
            const glm::dvec3 b(mesh.vertices[t.b].position);
            const glm::dvec3 c(mesh.vertices[t.c].position);
            volume6 += glm::dot(a, glm::cross(b, c));
        }
        if (volume6 < -1.0e-10) ++out.insideOutClosedComponents;
    }
    return out;
}

CanonicalMeshBuildResult canonicalizeMesh(MeshLod& mesh)
{
    CanonicalMeshBuildResult result;
    result.before = analyzeCanonicalMesh(mesh);
    if (result.before.structuralInvalid)
    {
        result.error = result.before.invalidReason;
        return result;
    }

    const MeshLod original = mesh;
    auto working = buildWorkingSet(mesh);
    result.removedDegenerateTriangles = working.degenerateTriangles;
    result.removedDuplicateTriangles = working.duplicateTriangles;

    auto topology = buildTopology(working.triangles);
    const auto orientation = solveOrientation(topology, working.triangles.size());
    if (orientation.conflicts != 0)
    {
        result.error = "topology cannot be oriented consistently";
        return result;
    }
    result.flippedTriangles += orientation.flipsRequired;
    applyParity(working.triangles, orientation);

    topology = buildTopology(working.triangles);
    result.flippedClosedComponents = flipInsideOutClosedComponents(mesh, working.triangles, topology, result.flippedTriangles);
    topology = buildTopology(working.triangles);

    const auto oldMetadata = collectOldEdgeMetadata(mesh);
    auto rebuilt = rebuildRenderVertices(mesh, working.triangles, topology);
    if (rebuilt.vertices.empty() || rebuilt.triangles.empty())
    {
        result.error = "canonical builder produced an empty mesh";
        return result;
    }
    result.normalIslands = rebuilt.normalIslands;
    result.rebuiltRenderVertices = rebuilt.vertices.size();

    MeshLod candidate;
    candidate.vertices = std::move(rebuilt.vertices);
    candidate.triangles = std::move(rebuilt.triangles);
    updateBounds(candidate);
    const auto rebuiltPoints = buildPointMap(candidate);
    candidate.edges = rebuildEdges(candidate, rebuiltPoints, oldMetadata);
    result.rebuiltEdges = candidate.edges.size();

    result.after = analyzeCanonicalMesh(candidate);
    if (result.after.structuralInvalid || result.after.degenerateTriangles != 0 || result.after.duplicateTriangles != 0 ||
        result.after.windingConflicts != 0 || result.after.insideOutClosedComponents != 0)
    {
        result.error = result.after.invalidReason.empty() ? "canonical mesh validation failed" : result.after.invalidReason;
        return result;
    }

    result.changed = !meshPayloadEqual(original, candidate);
    mesh = std::move(candidate);
    result.success = true;
    return result;
}

std::uint64_t canonicalMeshFingerprint(const MeshLod& mesh)
{
    std::uint64_t hash = 1469598103934665603ull;
    const std::uint64_t vertexCount = mesh.vertices.size();
    const std::uint64_t triangleCount = mesh.triangles.size();
    const std::uint64_t edgeCount = mesh.edges.size();
    hashValue(hash, vertexCount);
    hashValue(hash, triangleCount);
    hashValue(hash, edgeCount);
    for (const auto& vertex : mesh.vertices)
    {
        hashValue(hash, vertex.position.x); hashValue(hash, vertex.position.y); hashValue(hash, vertex.position.z);
        hashValue(hash, vertex.normal.x); hashValue(hash, vertex.normal.y); hashValue(hash, vertex.normal.z);
        hashValue(hash, vertex.uv.x); hashValue(hash, vertex.uv.y);
    }
    for (const auto& triangle : mesh.triangles)
    {
        hashValue(hash, triangle.a); hashValue(hash, triangle.b); hashValue(hash, triangle.c);
        hashValue(hash, triangle.sourcePolygonId); hashValue(hash, triangle.materialIndex); hashValue(hash, triangle.smoothingGroupId);
    }
    // Edge renderMask is intentionally authorable after preparation and does
    // not invalidate canonical geometry. Structural edge identity/adjacency
    // does: a stale or externally rewritten edge topology must be prepared again.
    for (const auto& edge : mesh.edges)
    {
        hashValue(hash, edge.a); hashValue(hash, edge.b);
        hashValue(hash, edge.triangleA); hashValue(hash, edge.triangleB);
        hashValue(hash, edge.flags);
    }
    return hash;
}

} // namespace elite::model_asset::editor
