#include "tools/model_asset_editor/CanonicalMeshBuilder.h"
#include "src/model_asset/RuntimeMeshNormalizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include <Eigen/Core>
#include <igl/embree/reorient_facets_raycast.h>
#include <igl/split_nonmanifold.h>

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

CanonicalPointMap buildPositionalPointMap(const MeshLod& mesh)
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
    std::size_t sourceTriangleIndex = 0;
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

struct PositionalEdgeUse
{
    std::size_t workingTriangleIndex = 0;
    std::uint32_t renderA = 0;
    std::uint32_t renderB = 0;
    QuantizedPosition positionA {};
    QuantizedPosition positionB {};
};

CanonicalPointMap buildTopologicalPointMap(
    const MeshLod& mesh,
    const std::vector<WorkingTriangle>& triangles)
{
    CanonicalPointMap out;
    out.pointForVertex.resize(mesh.vertices.size(), 0);
    if (mesh.vertices.empty()) return out;

    // A 1e-4 positional match is a weld candidate, not proof of topological
    // identity. Multiple independent panels on the station intentionally touch
    // at exactly the same positions. Globally welding those positions creates
    // artificial 3/4-face edges that did not exist in the authored topology.
    DisjointSet sets(mesh.vertices.size());
    std::map<PositionEdgeKey, std::vector<PositionalEdgeUse>> positionalUses;
    std::map<std::size_t, std::size_t> workingBySourceTriangle;

    for (std::size_t ti = 0; ti < triangles.size(); ++ti)
    {
        workingBySourceTriangle.emplace(triangles[ti].sourceTriangleIndex, ti);
        const auto& t = triangles[ti].triangle;
        const std::uint32_t r[3] = {t.a, t.b, t.c};
        for (int e = 0; e < 3; ++e)
        {
            const int next = (e + 1) % 3;
            if (r[e] >= mesh.vertices.size() || r[next] >= mesh.vertices.size()) continue;
            auto qa = quantizedPosition(mesh.vertices[r[e]].position);
            auto qb = quantizedPosition(mesh.vertices[r[next]].position);
            if (qa == qb) continue;
            auto keyA = qa, keyB = qb;
            std::uint32_t renderA = r[e], renderB = r[next];
            if (keyB < keyA)
            {
                std::swap(keyA, keyB);
                std::swap(renderA, renderB);
            }
            positionalUses[{keyA, keyB}].push_back({ti, renderA, renderB, keyA, keyB});
        }
    }

    auto unionUseEndpoints = [&](const PositionalEdgeUse& a, const PositionalEdgeUse& b)
    {
        if (a.positionA == b.positionA) sets.unite(a.renderA, b.renderA);
        if (a.positionB == b.positionB) sets.unite(a.renderB, b.renderB);
    };

    // Canonical builder output persists its recovered connectivity in MeshLod
    // edges. Once that marker is present, stored triangle adjacency is
    // authoritative and positional coincidence must never reconnect unrelated
    // boundary sheets on a later ANALYZE/reprepare pass. RAW/imported payloads
    // do not have that marker, so the 1e-4 two-use fallback remains available
    // to recover OBJ seams whose source indices differ despite equal positions.
    const bool trustStoredAdjacency = !mesh.edges.empty() && std::all_of(
        mesh.edges.begin(), mesh.edges.end(),
        [](const Edge& edge) { return (edge.flags & EdgeCanonicalTopology) != 0; });

    if (!trustStoredAdjacency)
    {
        for (const auto& [key, uses] : positionalUses)
        {
            (void)key;
            if (uses.size() == 2) unionUseEndpoints(uses[0], uses[1]);
        }
    }

    auto triangleEndpoint = [&](const WorkingTriangle& triangle, const QuantizedPosition& position) -> std::uint32_t
    {
        const std::uint32_t r[3] = {triangle.triangle.a, triangle.triangle.b, triangle.triangle.c};
        for (const auto vi : r)
        {
            if (vi < mesh.vertices.size() && quantizedPosition(mesh.vertices[vi].position) == position)
                return vi;
        }
        return std::numeric_limits<std::uint32_t>::max();
    };

    // NativeObjImporter preserves source-edge triangle adjacency. Use it to
    // reconnect render splits across UV/material/hard-normal seams when a
    // positional edge has more than two coincident uses. Crucially, unrelated
    // coincident edges are not all welded together.
    for (const auto& edge : mesh.edges)
    {
        if (edge.a >= mesh.vertices.size() || edge.b >= mesh.vertices.size() ||
            edge.triangleA < 0 || edge.triangleB < 0)
            continue;
        const auto ita = workingBySourceTriangle.find(static_cast<std::size_t>(edge.triangleA));
        const auto itb = workingBySourceTriangle.find(static_cast<std::size_t>(edge.triangleB));
        if (ita == workingBySourceTriangle.end() || itb == workingBySourceTriangle.end()) continue;

        const auto qa = quantizedPosition(mesh.vertices[edge.a].position);
        const auto qb = quantizedPosition(mesh.vertices[edge.b].position);
        if (qa == qb) continue;
        const auto& ta = triangles[ita->second];
        const auto& tb = triangles[itb->second];
        const auto aa = triangleEndpoint(ta, qa);
        const auto ab = triangleEndpoint(tb, qa);
        const auto ba = triangleEndpoint(ta, qb);
        const auto bb = triangleEndpoint(tb, qb);
        const auto invalid = std::numeric_limits<std::uint32_t>::max();
        if (aa != invalid && ab != invalid) sets.unite(aa, ab);
        if (ba != invalid && bb != invalid) sets.unite(ba, bb);
    }

    std::map<std::size_t, std::size_t> pointByRoot;
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi)
    {
        const auto root = sets.find(vi);
        const auto [it, inserted] = pointByRoot.emplace(root, pointByRoot.size());
        out.pointForVertex[vi] = it->second;
        if (inserted) out.representativeVertex.push_back(vi);
    }
    return out;
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

    // Positional identity is intentionally used first only for garbage cleanup:
    // collapsed and duplicate triangles should disappear even when authored OBJ
    // render splits use different indices. Topological welding happens only
    // after that cleanup so duplicate faces cannot manufacture a 3-use edge.
    const auto positionalPoints = buildPositionalPointMap(mesh);
    std::set<std::tuple<std::size_t, std::size_t, std::size_t, std::int32_t>> duplicateKeys;
    out.triangles.reserve(mesh.triangles.size());
    for (std::size_t sourceTriangleIndex = 0; sourceTriangleIndex < mesh.triangles.size(); ++sourceTriangleIndex)
    {
        const auto& triangle = mesh.triangles[sourceTriangleIndex];
        if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() || triangle.c >= mesh.vertices.size())
        {
            ++out.invalidTriangles;
            continue;
        }
        const std::array<std::size_t, 3> points = {
            positionalPoints.pointForVertex[triangle.a],
            positionalPoints.pointForVertex[triangle.b],
            positionalPoints.pointForVertex[triangle.c]
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
        out.triangles.push_back({triangle, points, sourceTriangleIndex});
    }

    out.points = buildTopologicalPointMap(mesh, out.triangles);
    for (auto& triangle : out.triangles)
    {
        triangle.point = {
            out.points.pointForVertex[triangle.triangle.a],
            out.points.pointForVertex[triangle.triangle.b],
            out.points.pointForVertex[triangle.triangle.c]
        };
    }
    return out;
}

struct LibiglRepairStats
{
    std::size_t splitTopologyVertices = 0;
    std::size_t raycastPatches = 0;
    std::size_t raycastFlippedTriangles = 0;
};

bool repairTopologyAndOrientationWithLibigl(
    const MeshLod& mesh,
    PreparedWorkingSet& working,
    LibiglRepairStats& stats,
    std::string& error)
{
    if (working.triangles.empty() || working.points.representativeVertex.empty())
    {
        error = "libigl repair received empty topology";
        return false;
    }

    // Compact only points referenced by the cleaned triangles before handing
    // the soup to libigl. Removed/unused OBJ vertices must not influence split
    // statistics or Embree patch orientation.
    std::vector<int> oldPointToCompact(working.points.representativeVertex.size(), -1);
    std::vector<std::size_t> compactToOldPoint;
    compactToOldPoint.reserve(working.points.representativeVertex.size());
    for (const auto& wt : working.triangles)
    {
        for (const auto oldPoint : wt.point)
        {
            if (oldPoint >= oldPointToCompact.size())
            {
                error = "canonical triangle references missing geometric point";
                return false;
            }
            if (oldPointToCompact[oldPoint] >= 0) continue;
            oldPointToCompact[oldPoint] = static_cast<int>(compactToOldPoint.size());
            compactToOldPoint.push_back(oldPoint);
        }
    }
    if (compactToOldPoint.empty())
    {
        error = "libigl repair received no referenced topology vertices";
        return false;
    }

    Eigen::MatrixXd V(static_cast<Eigen::Index>(compactToOldPoint.size()), 3);
    for (std::size_t compact = 0; compact < compactToOldPoint.size(); ++compact)
    {
        const auto oldPoint = compactToOldPoint[compact];
        const auto vi = working.points.representativeVertex[oldPoint];
        if (vi >= mesh.vertices.size())
        {
            error = "canonical point representative outside render vertex array";
            return false;
        }
        const auto& p = mesh.vertices[vi].position;
        V(static_cast<Eigen::Index>(compact), 0) = p.x;
        V(static_cast<Eigen::Index>(compact), 1) = p.y;
        V(static_cast<Eigen::Index>(compact), 2) = p.z;
    }

    Eigen::MatrixXi F(static_cast<Eigen::Index>(working.triangles.size()), 3);
    for (std::size_t ti = 0; ti < working.triangles.size(); ++ti)
    {
        for (int corner = 0; corner < 3; ++corner)
        {
            const int compact = oldPointToCompact[working.triangles[ti].point[corner]];
            if (compact < 0)
            {
                error = "canonical triangle references an unregistered topology vertex";
                return false;
            }
            F(static_cast<Eigen::Index>(ti), corner) = compact;
        }
    }

    Eigen::MatrixXd SV;
    Eigen::MatrixXi SF;
    Eigen::VectorXi SVI;
    igl::split_nonmanifold(V, F, SV, SF, SVI);
    if (SF.rows() != F.rows() || SVI.size() != SV.rows())
    {
        error = "libigl split_nonmanifold returned an incompatible face mapping";
        return false;
    }

    // Keep pre-existing canonical point ids for the first copy of every
    // topology vertex. split_nonmanifold-created copies receive fresh ids, so
    // authored metadata remains stable on every edge that did not need a cut.
    CanonicalPointMap repairedPoints = working.points;
    std::vector<std::size_t> splitPointId(static_cast<std::size_t>(SV.rows()), 0);
    std::vector<bool> claimed(compactToOldPoint.size(), false);
    for (Eigen::Index svi = 0; svi < SVI.size(); ++svi)
    {
        const int originalCompact = SVI(svi);
        if (originalCompact < 0 || static_cast<std::size_t>(originalCompact) >= compactToOldPoint.size())
        {
            error = "libigl split_nonmanifold returned an invalid vertex source index";
            return false;
        }
        const auto compact = static_cast<std::size_t>(originalCompact);
        const auto oldPoint = compactToOldPoint[compact];
        if (!claimed[compact])
        {
            claimed[compact] = true;
            splitPointId[static_cast<std::size_t>(svi)] = oldPoint;
        }
        else
        {
            const auto newPoint = repairedPoints.representativeVertex.size();
            repairedPoints.representativeVertex.push_back(working.points.representativeVertex[oldPoint]);
            splitPointId[static_cast<std::size_t>(svi)] = newPoint;
            ++stats.splitTopologyVertices;
        }
    }

    for (Eigen::Index fi = 0; fi < SF.rows(); ++fi)
    {
        auto& wt = working.triangles[static_cast<std::size_t>(fi)];
        for (int corner = 0; corner < 3; ++corner)
        {
            const int splitIndex = SF(fi, corner);
            if (splitIndex < 0 || static_cast<std::size_t>(splitIndex) >= splitPointId.size())
            {
                error = "libigl split_nonmanifold returned an invalid split vertex index";
                return false;
            }
            wt.point[corner] = splitPointId[static_cast<std::size_t>(splitIndex)];
        }
    }
    working.points = std::move(repairedPoints);

    // Match the station spike budget. libigl's default 100*faces is excessive
    // for an interactive offline editor; this bounded patch-wise budget already
    // produced the visually accepted S3 result under MinGW64.
    const int faceCount = static_cast<int>(SF.rows());
    const int raysTotal = std::clamp(faceCount * 8, 200000, 1000000);
    constexpr int raysMinimum = 16;
    Eigen::VectorXi flips;
    Eigen::VectorXi components;
    try
    {
        igl::embree::reorient_facets_raycast(
            SV, SF,
            raysTotal,
            raysMinimum,
            false,  // patch-wise
            false,  // ambient-occlusion mode accepted by the station spike
            false,  // editor writes its own concise repair log
            flips,
            components);
    }
    catch (const std::exception& ex)
    {
        error = std::string("Embree orientation failed: ") + ex.what();
        return false;
    }

    if (flips.size() != SF.rows() || components.size() != SF.rows())
    {
        error = "Embree orientation returned incompatible result vectors";
        return false;
    }
    stats.raycastPatches = components.size() == 0
        ? 0u
        : static_cast<std::size_t>(components.maxCoeff() + 1);

    for (Eigen::Index fi = 0; fi < flips.size(); ++fi)
    {
        if (flips(fi) == 0) continue;
        auto& wt = working.triangles[static_cast<std::size_t>(fi)];
        std::swap(wt.triangle.b, wt.triangle.c);
        std::swap(wt.point[1], wt.point[2]);
        ++stats.raycastFlippedTriangles;
    }

    const auto repairedTopology = buildTopology(working.triangles);
    for (const auto& [key, uses] : repairedTopology.edgeUses)
    {
        (void)key;
        if (uses.size() > 2)
        {
            error = "split_nonmanifold left a multi-use canonical edge";
            return false;
        }
    }
    return true;
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

glm::dvec3 canonicalPointPosition(
    const MeshLod& mesh,
    const CanonicalPointMap& points,
    std::size_t point)
{
    return glm::dvec3(mesh.vertices[points.representativeVertex[point]].position);
}

std::size_t countInsideOutClosedComponents(
    const MeshLod& mesh,
    const CanonicalPointMap& points,
    const std::vector<WorkingTriangle>& triangles,
    const WorkingTopology& topology)
{
    std::size_t inward = 0;
    const auto components = buildComponents(topology, triangles.size());
    for (std::size_t ci = 0; ci < components.triangles.size(); ++ci)
    {
        if (components.boundaryEdges[ci] != 0) continue;
        double volume6 = 0.0;
        for (const auto ti : components.triangles[ci])
        {
            const auto& t = triangles[ti];
            const auto a = canonicalPointPosition(mesh, points, t.point[0]);
            const auto b = canonicalPointPosition(mesh, points, t.point[1]);
            const auto c = canonicalPointPosition(mesh, points, t.point[2]);
            volume6 += glm::dot(a, glm::cross(b, c));
        }
        if (volume6 < -1.0e-10) ++inward;
    }
    return inward;
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

std::map<EdgeKey, OldEdgeMetadata> collectOldEdgeMetadata(
    const MeshLod& mesh,
    const CanonicalPointMap& points)
{
    std::map<EdgeKey, OldEdgeMetadata> out;
    for (const auto& edge : mesh.edges)
    {
        if (edge.a >= mesh.vertices.size() || edge.b >= mesh.vertices.size()) continue;
        const auto pa = points.pointForVertex[edge.a];
        const auto pb = points.pointForVertex[edge.b];
        if (pa == pb) continue;
        auto& meta = out[edgeKey(pa, pb)];
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
    std::vector<std::size_t> pointForVertex;
    std::size_t normalIslands = 0;
};

RebuiltRenderMesh rebuildRenderVertices(
    const MeshLod& source,
    const std::vector<WorkingTriangle>& triangles,
    const WorkingTopology& topology,
    const CanonicalPointMap& sourcePoints)
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

    // A render vertex is rebuilt from canonical geometric identity plus the
    // render-only splits that actually matter. Source OBJ vertex identity and
    // authored normal index are deliberately NOT part of this key: otherwise a
    // dirty OBJ normal split would survive forever even after normals ceased to
    // be authoritative. UV, material and reconstructed hard-normal islands do
    // remain valid reasons to create multiple GPU vertices at one point.
    auto floatBits = [](float value)
    {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "unexpected float width");
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    };
    using RenderKey = std::tuple<std::size_t, std::uint32_t, std::uint32_t, std::int32_t, std::size_t>;
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
            const auto point = triangles[ti].point[corner];
            const auto& sourceVertex = source.vertices[src[corner]];
            const RenderKey key {
                point,
                floatBits(sourceVertex.uv.x),
                floatBits(sourceVertex.uv.y),
                rebuilt.materialIndex,
                root
            };
            const auto found = renderMap.find(key);
            if (found != renderMap.end())
            {
                *dst[corner] = found->second;
                continue;
            }
            Vertex vertex = sourceVertex;
            // Positional weld is now real in the working render payload: all
            // render splits of one geometric point share one representative
            // canonical position. Only their UV/material/normal identity may
            // remain distinct.
            vertex.position = source.vertices[sourcePoints.representativeVertex[point]].position;
            vertex.normal = islandNormal[root];
            const auto newIndex = static_cast<std::uint32_t>(out.vertices.size());
            out.vertices.push_back(vertex);
            out.pointForVertex.push_back(point);
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
    const std::vector<std::size_t>& pointForVertex,
    const std::map<EdgeKey, OldEdgeMetadata>& oldMetadata)
{
    std::map<EdgeKey, std::vector<RenderEdgeUse>> uses;
    std::vector<glm::dvec3> normals(mesh.triangles.size(), glm::dvec3(0.0));
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
    {
        const auto& t = mesh.triangles[ti];
        const std::uint32_t r[3] = {t.a, t.b, t.c};
        const std::size_t p[3] = {
            pointForVertex[t.a], pointForVertex[t.b], pointForVertex[t.c]
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
        edge.flags |= EdgeCanonicalTopology;
        edge.a = first.renderA;
        edge.b = first.renderB;
        edge.triangleA = static_cast<std::int32_t>(first.triangleIndex);
        if (edgeUses.size() >= 2)
            edge.triangleB = static_cast<std::int32_t>(edgeUses[1].triangleIndex);

        const auto old = oldMetadata.find(key);
        if (old != oldMetadata.end())
        {
            edge.flags |= old->second.flags & EdgeAuthored;
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

            WorkingTriangle wa {a, {pointForVertex[a.a], pointForVertex[a.b], pointForVertex[a.c]}};
            WorkingTriangle wb {b, {pointForVertex[b.a], pointForVertex[b.b], pointForVertex[b.c]}};
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
    std::sort(out.begin(), out.end(), [](const Edge& a, const Edge& b) {
        return std::tie(a.triangleA, a.triangleB, a.a, a.b, a.flags, a.renderMask) <
               std::tie(b.triangleA, b.triangleB, b.a, b.b, b.flags, b.renderMask);
    });
    return out;
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
    // EdgeNonManifold on the decoded/authored payload is diagnostic evidence,
    // not a reason to skip canonicalization. More importantly, coincident
    // positions are no longer assumed to be one topological vertex. The
    // working set reconnects ordinary two-face seams but keeps ambiguous 3+
    // coincident edge uses as independent sheets unless authored adjacency
    // explicitly connects them. Only a multi-use edge that still exists in
    // that topology-aware graph is a genuine canonical non-manifold defect.

    const auto topology = buildTopology(working.triangles);
    for (const auto& [key, uses] : topology.edgeUses)
    {
        (void)key;
        if (uses.size() == 1) ++out.boundaryEdges;
        else if (uses.size() > 2) ++out.canonicalMultiUseEdges;
    }
    if (out.canonicalMultiUseEdges != 0)
    {
        out.structuralInvalid = true;
        out.invalidReason = "canonical topology contains an edge used by more than two faces";
        return out;
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
    }
    out.insideOutClosedComponents =
        countInsideOutClosedComponents(mesh, working.points, oriented, orientedTopology);
    return out;
}

CanonicalMeshBuildResult canonicalizeMesh(MeshLod& mesh)
{
    CanonicalMeshBuildResult result;
    result.before.renderVertices = mesh.vertices.size();
    result.before.triangles = mesh.triangles.size();
    result.before.sourceNonManifoldEdges = sourceNonManifoldEdgeCount(mesh);

    // PREPARE repairs authored topology. Only payload defects that make the
    // mesh unreadable are blockers before libigl gets a chance to repair it.
    for (const auto& vertex : mesh.vertices)
    {
        if (!finiteVec3(vertex.position))
        {
            result.before.finitePositions = false;
            result.before.structuralInvalid = true;
            result.before.invalidReason = "non-finite vertex position";
            result.repairStatus = "FAILED";
            result.error = result.before.invalidReason;
            return result;
        }
    }

    const auto beforeFingerprint = canonicalMeshFingerprint(mesh);
    auto working = buildWorkingSet(mesh);
    result.before.geometricPoints = working.points.representativeVertex.size();
    result.before.invalidTriangles = working.invalidTriangles;
    result.before.degenerateTriangles = working.degenerateTriangles;
    result.before.duplicateTriangles = working.duplicateTriangles;
    result.removedDegenerateTriangles = working.degenerateTriangles;
    result.removedDuplicateTriangles = working.duplicateTriangles;

    if (working.invalidTriangles != 0)
    {
        result.before.structuralInvalid = true;
        result.before.invalidReason = "triangle index outside render vertex array";
        result.repairStatus = "FAILED";
        result.error = result.before.invalidReason;
        return result;
    }
    if (working.triangles.empty())
    {
        result.before.structuralInvalid = true;
        result.before.invalidReason = "mesh has no usable triangles after canonical cleanup";
        result.repairStatus = "FAILED";
        result.error = result.before.invalidReason;
        return result;
    }

    const auto originalTopology = buildTopology(working.triangles);
    for (const auto& [key, uses] : originalTopology.edgeUses)
    {
        (void)key;
        if (uses.size() == 1) ++result.before.boundaryEdges;
        else if (uses.size() > 2) ++result.before.canonicalMultiUseEdges;
    }
    const auto beforeOrientation = solveOrientation(originalTopology, working.triangles.size());
    result.before.windingFlipsRequired = beforeOrientation.flipsRequired;
    result.before.windingConflicts = beforeOrientation.conflicts;

    // Preserve authored edge masks/metadata before topology repair. New point
    // copies created by split_nonmanifold intentionally have no inherited edge
    // identity unless rebuild can map them back unambiguously.
    const auto oldMetadata = collectOldEdgeMetadata(mesh, working.points);

    // Production topology/orientation authority. The previous v7 radial
    // envelope/open-component heuristic is deliberately gone from PREPARE.
    LibiglRepairStats libiglStats;
    if (!repairTopologyAndOrientationWithLibigl(mesh, working, libiglStats, result.error))
    {
        result.repairStatus = "LIBIGL_FAILED";
        return result;
    }
    result.splitTopologyVertices = libiglStats.splitTopologyVertices;
    result.raycastPatches = libiglStats.raycastPatches;
    result.raycastFlippedTriangles = libiglStats.raycastFlippedTriangles;
    result.flippedTriangles = libiglStats.raycastFlippedTriangles;

    auto topology = buildTopology(working.triangles);
    const auto finalOrientation = solveOrientation(topology, working.triangles.size());
    result.after.windingFlipsRequired = finalOrientation.flipsRequired;
    result.after.windingConflicts = finalOrientation.conflicts;
    const auto components = buildComponents(topology, working.triangles.size());
    result.after.components = components.triangles.size();
    for (std::size_t ci = 0; ci < components.triangles.size(); ++ci)
    {
        result.after.boundaryEdges += components.boundaryEdges[ci];
        result.after.canonicalMultiUseEdges += components.multiUseEdges[ci];
        if (components.boundaryEdges[ci] == 0) ++result.after.closedComponents;
        else ++result.after.openComponents;
    }
    result.after.insideOutClosedComponents =
        countInsideOutClosedComponents(mesh, working.points, working.triangles, topology);

    if (result.after.canonicalMultiUseEdges != 0 ||
        result.after.windingConflicts != 0 || result.after.windingFlipsRequired != 0 ||
        result.after.insideOutClosedComponents != 0)
    {
        result.repairStatus = "LIBIGL_FAILED";
        result.error = "libigl/Embree repair did not satisfy canonical topology/orientation invariants";
        return result;
    }

    // Everything below this line is editor-owned reconstruction. libigl never
    // owns UVs, materials, smoothing metadata, hard-normal islands or .elmesh
    // identity.
    auto rebuilt = rebuildRenderVertices(mesh, working.triangles, topology, working.points);
    if (rebuilt.vertices.empty() || rebuilt.triangles.empty())
    {
        result.repairStatus = "FAILED";
        result.error = "canonical builder produced an empty mesh";
        return result;
    }

    MeshLod candidate;
    candidate.vertices = std::move(rebuilt.vertices);
    candidate.triangles = std::move(rebuilt.triangles);
    updateBounds(candidate);
    candidate.edges = rebuildEdges(candidate, rebuilt.pointForVertex, oldMetadata);
    result.normalIslands = rebuilt.normalIslands;

    // Preserve v7's cheap render-projection stabilization. It may recover
    // additional seam adjacency from freshly rebuilt canonical edges, but it
    // never reruns a heuristic orientation policy. Any topology/orientation
    // regression aborts transactionally instead of silently guessing.
    std::size_t expectedGeometricPoints = working.points.representativeVertex.size();
    PreparedWorkingSet finalWorking;
    WorkingTopology finalTopology;
    bool projectionStable = false;
    for (std::size_t projectionPass = 0; projectionPass < 3; ++projectionPass)
    {
        auto candidateWorking = buildWorkingSet(candidate);
        if (candidateWorking.invalidTriangles != 0 || candidateWorking.triangles.empty())
        {
            result.repairStatus = "FAILED";
            result.error = "canonical render projection became unreadable";
            return result;
        }
        auto candidateTopology = buildTopology(candidateWorking.triangles);
        bool multiUse = false;
        for (const auto& [key, uses] : candidateTopology.edgeUses)
        {
            (void)key;
            if (uses.size() > 2) { multiUse = true; break; }
        }
        const auto candidateOrientation = solveOrientation(candidateTopology, candidateWorking.triangles.size());
        if (multiUse || candidateOrientation.conflicts != 0 || candidateOrientation.flipsRequired != 0)
        {
            result.repairStatus = "FAILED";
            result.error = "canonical render projection changed repaired topology";
            return result;
        }

        const auto currentGeometricPoints = candidateWorking.points.representativeVertex.size();
        if (currentGeometricPoints == expectedGeometricPoints)
        {
            finalWorking = std::move(candidateWorking);
            finalTopology = std::move(candidateTopology);
            projectionStable = true;
            break;
        }

        expectedGeometricPoints = currentGeometricPoints;
        const auto projectionMetadata = collectOldEdgeMetadata(candidate, candidateWorking.points);
        auto projected = rebuildRenderVertices(
            candidate, candidateWorking.triangles, candidateTopology, candidateWorking.points);
        if (projected.vertices.empty() || projected.triangles.empty())
        {
            result.repairStatus = "FAILED";
            result.error = "canonical topology stabilization produced an empty mesh";
            return result;
        }
        MeshLod stabilized;
        stabilized.vertices = std::move(projected.vertices);
        stabilized.triangles = std::move(projected.triangles);
        updateBounds(stabilized);
        stabilized.edges = rebuildEdges(stabilized, projected.pointForVertex, projectionMetadata);
        candidate = std::move(stabilized);
        result.normalIslands = projected.normalIslands;
        ++result.topologyStabilizationPasses;
    }

    if (!projectionStable)
    {
        result.repairStatus = "FAILED";
        result.error = "canonical topology projection did not stabilize";
        return result;
    }

    // Measure the exact payload that will be committed. This final gate is
    // cheap and deterministic; it does not invoke the removed radial heuristic.
    CanonicalMeshAnalysis measured;
    measured.renderVertices = candidate.vertices.size();
    measured.geometricPoints = finalWorking.points.representativeVertex.size();
    measured.triangles = candidate.triangles.size();
    measured.sourceNonManifoldEdges = sourceNonManifoldEdgeCount(candidate);
    measured.finitePositions = true;
    for (const auto& [key, uses] : finalTopology.edgeUses)
    {
        (void)key;
        if (uses.size() == 1) ++measured.boundaryEdges;
        else if (uses.size() > 2) ++measured.canonicalMultiUseEdges;
    }
    const auto finalComponents = buildComponents(finalTopology, finalWorking.triangles.size());
    measured.components = finalComponents.triangles.size();
    for (std::size_t ci = 0; ci < finalComponents.triangles.size(); ++ci)
    {
        if (finalComponents.boundaryEdges[ci] == 0) ++measured.closedComponents;
        else ++measured.openComponents;
    }
    const auto measuredOrientation = solveOrientation(finalTopology, finalWorking.triangles.size());
    measured.windingFlipsRequired = measuredOrientation.flipsRequired;
    measured.windingConflicts = measuredOrientation.conflicts;
    measured.insideOutClosedComponents =
        countInsideOutClosedComponents(candidate, finalWorking.points, finalWorking.triangles, finalTopology);
    result.after = measured;
    result.rebuiltRenderVertices = candidate.vertices.size();
    result.rebuiltEdges = candidate.edges.size();

    if (measured.canonicalMultiUseEdges != 0 || measured.windingConflicts != 0 ||
        measured.windingFlipsRequired != 0 || measured.insideOutClosedComponents != 0)
    {
        result.repairStatus = "FAILED";
        result.error = "repaired candidate does not satisfy render mesh contract";
        return result;
    }

    const auto afterFingerprint = canonicalMeshFingerprint(candidate);
    result.changed = beforeFingerprint != afterFingerprint;
    mesh = std::move(candidate);
    result.success = true;
    result.repairStatus = "GOOD_ENOUGH";
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
    // Edge renderMask is intentionally authorable after the SOURCE boundary and
    // does not invalidate canonical geometry. Structural edge identity/adjacency
    // does: a stale or externally rewritten edge topology must cross the boundary again.
    for (const auto& edge : mesh.edges)
    {
        hashValue(hash, edge.a); hashValue(hash, edge.b);
        hashValue(hash, edge.triangleA); hashValue(hash, edge.triangleB);
        hashValue(hash, edge.flags);
    }
    return hash;
}

} // namespace elite::model_asset::editor
