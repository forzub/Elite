#include "tools/model_asset_editor/LodMeshSimplifier.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/LU>
#include <igl/COLLAPSE_EDGE_NULL.h>
#include <igl/circulation.h>
#include <igl/connect_boundary_to_infinity.h>
#include <igl/decimate.h>
#include <igl/is_edge_manifold.h>
#include <igl/remove_unreferenced.h>

#include "tools/model_asset_editor/CanonicalMeshBuilder.h"

namespace elite::model_asset::editor
{
namespace
{

constexpr double CreaseCos = 0.906307787; // cos(25 deg): same crease policy as CanonicalMeshBuilder
constexpr double FlipCos = 0.2;           // a collapse may turn a neighbour face by < ~78 deg
constexpr std::size_t MinimumComponentTriangles = 8;
constexpr double Infinity = std::numeric_limits<double>::infinity();

struct Dsu
{
    explicit Dsu(std::size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0u); }
    std::uint32_t find(std::uint32_t v)
    {
        while (parent[v] != v)
        {
            parent[v] = parent[parent[v]];
            v = parent[v];
        }
        return v;
    }
    void unite(std::uint32_t a, std::uint32_t b)
    {
        a = find(a);
        b = find(b);
        if (a != b) parent[std::max(a, b)] = std::min(a, b);
    }
    std::vector<std::uint32_t> parent;
};

// Unweighted plane quadric: x'Ax + 2b'x + c = sum of squared distances.
struct Quadric
{
    Eigen::Matrix3d A = Eigen::Matrix3d::Zero();
    Eigen::Vector3d b = Eigen::Vector3d::Zero();
    double c = 0.0;

    void addPlane(const Eigen::Vector3d& n, const Eigen::Vector3d& point)
    {
        const double d = -n.dot(point);
        A += n * n.transpose();
        b += d * n;
        c += d * d;
    }
    double eval(const Eigen::Vector3d& x) const { return std::max(0.0, x.dot(A * x) + 2.0 * b.dot(x) + c); }
    Quadric operator+(const Quadric& o) const
    {
        Quadric q;
        q.A = A + o.A;
        q.b = b + o.b;
        q.c = c + o.c;
        return q;
    }
};

std::uint64_t pairKey(std::uint32_t a, std::uint32_t b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint64_t>(a) << 32) | b;
}

// Geometric point per render vertex from the canonical edge topology (render
// vertices split by normal seams are re-joined where an authored / canonical
// edge says the two triangles share that edge). Without edges: exact weld.
std::vector<std::uint32_t> geometricPoints(const MeshLod& mesh, std::uint32_t& pointCount)
{
    Dsu dsu(mesh.vertices.size());
    const auto sameCorner = [&](std::uint32_t corner, std::uint32_t vertex) {
        return corner < mesh.vertices.size() && vertex < mesh.vertices.size() &&
               mesh.vertices[corner].position == mesh.vertices[vertex].position;
    };
    if (!mesh.edges.empty())
    {
        for (const auto& edge : mesh.edges)
            for (const std::int32_t ti : {edge.triangleA, edge.triangleB})
            {
                if (ti < 0 || static_cast<std::size_t>(ti) >= mesh.triangles.size()) continue;
                const auto& t = mesh.triangles[static_cast<std::size_t>(ti)];
                for (const std::uint32_t corner : {t.a, t.b, t.c})
                {
                    if (sameCorner(corner, edge.a)) dsu.unite(corner, edge.a);
                    if (sameCorner(corner, edge.b)) dsu.unite(corner, edge.b);
                }
            }
    }
    else
    {
        std::map<std::array<float, 3>, std::uint32_t> byPosition;
        for (std::uint32_t v = 0; v < mesh.vertices.size(); ++v)
        {
            const auto& p = mesh.vertices[v].position;
            const auto [it, inserted] = byPosition.emplace(std::array<float, 3> {p.x, p.y, p.z}, v);
            if (!inserted) dsu.unite(v, it->second);
        }
    }
    std::vector<std::uint32_t> point(mesh.vertices.size(), 0);
    std::vector<std::int64_t> compact(mesh.vertices.size(), -1);
    pointCount = 0;
    for (std::uint32_t v = 0; v < mesh.vertices.size(); ++v)
    {
        const std::uint32_t root = dsu.find(v);
        if (compact[root] < 0) compact[root] = pointCount++;
        point[v] = static_cast<std::uint32_t>(compact[root]);
    }
    return point;
}

struct Face
{
    std::array<std::uint32_t, 3> p {};
    std::uint32_t triangle = 0;
    Eigen::Vector3d normal = Eigen::Vector3d::Zero();
};

bool smoothAcross(const Triangle& a, const Triangle& b, const Eigen::Vector3d& na, const Eigen::Vector3d& nb)
{
    if (a.materialIndex != b.materialIndex) return false;
    if (a.sourcePolygonId >= 0 && a.sourcePolygonId == b.sourcePolygonId) return true;
    if (a.smoothingGroupId != 0 || b.smoothingGroupId != 0)
        return a.smoothingGroupId != 0 && a.smoothingGroupId == b.smoothingGroupId;
    return na.dot(nb) >= CreaseCos;
}

struct ComponentResult
{
    bool simplified = false;
    bool nonManifold = false; // not edge-manifold once boundaries are closed (left untouched)
    Eigen::MatrixXd U;
    Eigen::MatrixXi G;
    std::vector<std::uint32_t> birthFace;  // per G row: local face index
    std::vector<std::int64_t> birthPoint;  // per U row: local point index
    std::size_t collapses = 0;
};

// One edge-manifold component (local points V, faces F, region per face).
ComponentResult decimateComponent(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& F,
    const std::vector<std::int64_t>& faceRegion,
    const std::vector<std::uint8_t>& pinned,
    double targetError)
{
    ComponentResult result;
    Eigen::MatrixXd VO;
    Eigen::MatrixXi FO;
    igl::connect_boundary_to_infinity(V, F, VO, FO);
    if (!igl::is_edge_manifold(FO))
    {
        if (std::getenv("ELITE_LOD_SIMPLIFY_DEBUG"))
        {
            std::map<std::uint64_t, int> uses;
            for (int f = 0; f < FO.rows(); ++f)
                for (int k = 0; k < 3; ++k)
                    ++uses[pairKey(static_cast<std::uint32_t>(FO(f, k)), static_cast<std::uint32_t>(FO(f, (k + 1) % 3)))];
            std::map<int, int> histogram;
            for (const auto& [key, n] : uses) ++histogram[n];
            std::fprintf(stderr, "[lod-simplify] not edge-manifold: V=%d F=%d FO=%d", static_cast<int>(V.rows()),
                         static_cast<int>(F.rows()), static_cast<int>(FO.rows()));
            for (const auto& [n, count] : histogram) std::fprintf(stderr, " uses%d=%d", n, count);
            std::fprintf(stderr, "\n");
        }
        result.nonManifold = true;
        return result;
    }
    const int infinite = static_cast<int>(V.rows());
    const int originalFaces = static_cast<int>(F.rows());

    // Regions: phony boundary faces are "outside" (-1), so a mesh boundary is
    // a region border like a crease.
    std::vector<std::int64_t> region(static_cast<std::size_t>(FO.rows()), -1);
    for (int f = 0; f < originalFaces; ++f) region[static_cast<std::size_t>(f)] = faceRegion[static_cast<std::size_t>(f)];

    // Quadrics of the original planes + constraint planes along region borders.
    std::vector<Quadric> quadric(static_cast<std::size_t>(VO.rows()));
    std::vector<Eigen::Vector3d> faceNormal(static_cast<std::size_t>(originalFaces), Eigen::Vector3d::Zero());
    for (int f = 0; f < originalFaces; ++f)
    {
        const Eigen::Vector3d a = V.row(F(f, 0)).transpose(), b = V.row(F(f, 1)).transpose(), c = V.row(F(f, 2)).transpose();
        Eigen::Vector3d n = (b - a).cross(c - a);
        const double len = n.norm();
        if (len <= 0.0) continue;
        n /= len;
        faceNormal[static_cast<std::size_t>(f)] = n;
        for (int k = 0; k < 3; ++k) quadric[static_cast<std::size_t>(F(f, k))].addPlane(n, a);
    }
    {
        std::map<std::uint64_t, std::vector<int>> edgeFaces;
        for (int f = 0; f < originalFaces; ++f)
            for (int k = 0; k < 3; ++k)
                edgeFaces[pairKey(static_cast<std::uint32_t>(F(f, k)), static_cast<std::uint32_t>(F(f, (k + 1) % 3)))].push_back(f);
        for (const auto& [key, faces] : edgeFaces)
        {
            const bool border = faces.size() == 1 ||
                                (faces.size() == 2 && region[static_cast<std::size_t>(faces[0])] != region[static_cast<std::size_t>(faces[1])]);
            if (!border) continue;
            const int ia = static_cast<int>(key >> 32), ib = static_cast<int>(key & 0xffffffffu);
            const Eigen::Vector3d pa = V.row(ia).transpose(), pb = V.row(ib).transpose();
            const Eigen::Vector3d dir = (pb - pa).normalized();
            for (const int f : faces)
            {
                Eigen::Vector3d m = dir.cross(faceNormal[static_cast<std::size_t>(f)]);
                if (m.norm() <= 1.0e-12) continue;
                m.normalize();
                quadric[static_cast<std::size_t>(ia)].addPlane(m, pa);
                quadric[static_cast<std::size_t>(ib)].addPlane(m, pa);
            }
        }
    }

    const double target2 = targetError * targetError;
    const auto regionOf = [&](int f) { return f < 0 ? std::int64_t(-1) : region[static_cast<std::size_t>(f)]; };
    // Region transitions around the fan of one endpoint = number of border
    // edges at that vertex (0 interior, 2 on a border line, >2 corner).
    const auto fans = [&](int e, const Eigen::MatrixXi& Fc, const Eigen::VectorXi& EMAP, const Eigen::MatrixXi& EF,
                          const Eigen::MatrixXi& EI, const Eigen::MatrixXi& E, std::vector<int>& sFan,
                          std::vector<int>& dFan) {
        std::vector<int> nv0, nf0, nv1, nf1;
        igl::circulation(e, true, Fc, EMAP, EF, EI, nv0, nf0);
        igl::circulation(e, false, Fc, EMAP, EF, EI, nv1, nf1);
        const int s = E(e, 0);
        bool firstIsS = true;
        for (const int f : nf0)
            if (f >= 0 && Fc(f, 0) != IGL_COLLAPSE_EDGE_NULL && Fc(f, 0) != s && Fc(f, 1) != s && Fc(f, 2) != s)
            {
                firstIsS = false;
                break;
            }
        sFan = firstIsS ? nf0 : nf1;
        dFan = firstIsS ? nf1 : nf0;
    };
    const auto transitions = [&](const std::vector<int>& fan) {
        int count = 0;
        for (std::size_t i = 0; i < fan.size(); ++i)
            if (regionOf(fan[i]) != regionOf(fan[(i + 1) % fan.size()])) ++count;
        return count;
    };

    igl::decimate_cost_and_placement_callback costAndPlacement =
        [&](const int e, const Eigen::MatrixXd& Vc, const Eigen::MatrixXi& Fc, const Eigen::MatrixXi& E,
            const Eigen::VectorXi& EMAP, const Eigen::MatrixXi& EF, const Eigen::MatrixXi& EI, double& cost,
            Eigen::RowVectorXd& p) {
            const int s = E(e, 0), d = E(e, 1);
            p.resize(3);
            if (s == infinite || d == infinite || pinned[static_cast<std::size_t>(s)] || pinned[static_cast<std::size_t>(d)])
            {
                cost = Infinity;
                p = Vc.row(s == infinite ? d : s);
                return;
            }
            std::vector<int> sFan, dFan;
            fans(e, Fc, EMAP, EF, EI, E, sFan, dFan);
            const int ts = transitions(sFan), td = transitions(dFan);
            const bool edgeOnBorder = regionOf(EF(e, 0)) != regionOf(EF(e, 1));
            const Eigen::Vector3d ps = Vc.row(s).transpose(), pd = Vc.row(d).transpose();
            if ((!edgeOnBorder && ts > 0 && td > 0) || (ts > 2 && td > 2))
            {
                cost = Infinity;
                p = ps.transpose();
                return;
            }
            const Quadric q = quadric[static_cast<std::size_t>(s)] + quadric[static_cast<std::size_t>(d)];
            Eigen::Vector3d best;
            if (ts > 2 || (ts > 0 && td == 0)) best = ps;      // corner / border vertex stays put
            else if (td > 2 || (td > 0 && ts == 0)) best = pd;
            else
            {
                const Eigen::Vector3d mid = 0.5 * (ps + pd);
                best = ps;
                double bestCost = q.eval(ps);
                for (const Eigen::Vector3d& candidate : {pd, mid})
                {
                    const double c = q.eval(candidate);
                    if (c < bestCost) { bestCost = c; best = candidate; }
                }
                Eigen::FullPivLU<Eigen::Matrix3d> lu(q.A);
                if (lu.rank() == 3)
                {
                    const Eigen::Vector3d optimal = lu.solve(-q.b);
                    if (optimal.allFinite() && (optimal - mid).norm() <= (pd - ps).norm() && q.eval(optimal) < bestCost)
                        best = optimal;
                }
            }
            cost = q.eval(best);
            if (!(cost <= target2)) cost = Infinity;
            p = best.transpose();
        };

    int v1 = -1, v2 = -1;
    igl::decimate_pre_collapse_callback preCollapse =
        [&](const Eigen::MatrixXd& Vc, const Eigen::MatrixXi& Fc, const Eigen::MatrixXi& E, const Eigen::VectorXi& EMAP,
            const Eigen::MatrixXi& EF, const Eigen::MatrixXi& EI, const igl::min_heap<std::tuple<double, int, int>>&,
            const Eigen::VectorXi&, const Eigen::MatrixXd& C, const int e) -> bool {
            v1 = E(e, 0);
            v2 = E(e, 1);
            std::vector<int> sFan, dFan;
            fans(e, Fc, EMAP, EF, EI, E, sFan, dFan);
            const Eigen::Vector3d target = C.row(e).transpose();
            for (const auto* fan : {&sFan, &dFan})
                for (const int f : *fan)
                {
                    if (f < 0 || Fc(f, 0) == IGL_COLLAPSE_EDGE_NULL) continue;
                    bool hasS = false, hasD = false, hasInf = false;
                    for (int k = 0; k < 3; ++k)
                    {
                        hasS = hasS || Fc(f, k) == v1;
                        hasD = hasD || Fc(f, k) == v2;
                        hasInf = hasInf || Fc(f, k) == infinite;
                    }
                    if ((hasS && hasD) || hasInf) continue;
                    Eigen::Vector3d before[3], after[3];
                    for (int k = 0; k < 3; ++k)
                    {
                        before[k] = Vc.row(Fc(f, k)).transpose();
                        after[k] = Fc(f, k) == v1 || Fc(f, k) == v2 ? target : before[k];
                    }
                    const Eigen::Vector3d n0 = (before[1] - before[0]).cross(before[2] - before[0]);
                    const Eigen::Vector3d n1 = (after[1] - after[0]).cross(after[2] - after[0]);
                    const double l0 = n0.norm(), l1 = n1.norm();
                    if (l1 <= 1.0e-9 * l0 || n0.dot(n1) < FlipCos * l0 * l1) return false;
                }
            return true;
        };
    igl::decimate_post_collapse_callback postCollapse =
        [&](const Eigen::MatrixXd&, const Eigen::MatrixXi&, const Eigen::MatrixXi&, const Eigen::VectorXi&,
            const Eigen::MatrixXi&, const Eigen::MatrixXi&, const igl::min_heap<std::tuple<double, int, int>>&,
            const Eigen::VectorXi&, const Eigen::MatrixXd&, const int, const int, const int, const int, const int,
            const bool collapsed) {
            if (!collapsed) return;
            const int keep = std::min(v1, v2);
            quadric[static_cast<std::size_t>(keep)] = quadric[static_cast<std::size_t>(v1)] + quadric[static_cast<std::size_t>(v2)];
            ++result.collapses;
        };
    igl::decimate_stopping_condition_callback stop =
        [](const Eigen::MatrixXd&, const Eigen::MatrixXi&, const Eigen::MatrixXi&, const Eigen::VectorXi&,
           const Eigen::MatrixXi&, const Eigen::MatrixXi&, const igl::min_heap<std::tuple<double, int, int>>& Q,
           const Eigen::VectorXi&, const Eigen::MatrixXd&, const int, const int, const int, const int, const int) {
            return Q.empty() || std::get<0>(Q.top()) == Infinity;
        };

    Eigen::MatrixXd U;
    Eigen::MatrixXi G;
    Eigen::VectorXi J, I;
    const bool ok = igl::decimate(VO, FO, costAndPlacement, stop, preCollapse, postCollapse, U, G, J, I);
    if (std::getenv("ELITE_LOD_SIMPLIFY_DEBUG"))
        std::fprintf(stderr, "[lod-simplify] V=%d F=%d FO=%d decimate=%d collapses=%zu\n", static_cast<int>(V.rows()),
                     originalFaces, static_cast<int>(FO.rows()), ok ? 1 : 0, result.collapses);
    if (result.collapses == 0) return result;

    // Drop the phony boundary faces, then unreferenced vertices (incl. infinity).
    std::vector<int> keep;
    for (int r = 0; r < G.rows(); ++r)
        if (J(r) < originalFaces) keep.push_back(r);
    Eigen::MatrixXi G2(static_cast<Eigen::Index>(keep.size()), 3);
    Eigen::VectorXi J2(static_cast<Eigen::Index>(keep.size()));
    for (std::size_t i = 0; i < keep.size(); ++i)
    {
        G2.row(static_cast<Eigen::Index>(i)) = G.row(keep[i]);
        J2(static_cast<Eigen::Index>(i)) = J(keep[i]);
    }
    Eigen::VectorXi oldToNew, newToOld;
    igl::remove_unreferenced(U, G2, result.U, result.G, oldToNew, newToOld);
    result.birthFace.resize(static_cast<std::size_t>(result.G.rows()));
    for (Eigen::Index r = 0; r < result.G.rows(); ++r) result.birthFace[static_cast<std::size_t>(r)] = static_cast<std::uint32_t>(J2(r));
    result.birthPoint.resize(static_cast<std::size_t>(result.U.rows()));
    for (Eigen::Index r = 0; r < result.U.rows(); ++r) result.birthPoint[static_cast<std::size_t>(r)] = I(newToOld(r));
    result.simplified = true;
    return result;
}

} // namespace

MeshLod simplifyLodSurface(const MeshLod& source, double targetError, LodSimplifyStats* stats)
{
    LodSimplifyStats local;
    local.inputTriangles = source.triangles.size();
    local.targetError = targetError;
    local.outputTriangles = source.triangles.size();
    if (source.triangles.empty() || !(targetError > 0.0) || !std::isfinite(targetError))
    {
        if (stats) *stats = local;
        return source;
    }

    std::uint32_t pointCount = 0;
    const std::vector<std::uint32_t> pointOf = geometricPoints(source, pointCount);
    std::vector<Eigen::Vector3d> pointPosition(pointCount);
    for (std::uint32_t v = 0; v < source.vertices.size(); ++v)
        pointPosition[pointOf[v]] = source.vertices[v].position.x * Eigen::Vector3d::UnitX() +
                                    source.vertices[v].position.y * Eigen::Vector3d::UnitY() +
                                    source.vertices[v].position.z * Eigen::Vector3d::UnitZ();

    // Faces in point space (degenerate after re-joining: left out of the
    // simplifier, kept verbatim with their component).
    std::vector<Face> faces;
    faces.reserve(source.triangles.size());
    for (std::uint32_t ti = 0; ti < source.triangles.size(); ++ti)
    {
        const auto& t = source.triangles[ti];
        if (t.a >= source.vertices.size() || t.b >= source.vertices.size() || t.c >= source.vertices.size()) continue;
        Face f;
        f.p = {pointOf[t.a], pointOf[t.b], pointOf[t.c]};
        if (f.p[0] == f.p[1] || f.p[1] == f.p[2] || f.p[0] == f.p[2]) continue;
        f.triangle = ti;
        const Eigen::Vector3d n = (pointPosition[f.p[1]] - pointPosition[f.p[0]]).cross(pointPosition[f.p[2]] - pointPosition[f.p[0]]);
        if (n.norm() > 0.0) f.normal = n.normalized();
        faces.push_back(f);
    }

    // Edge -> faces: connectivity (components), smoothing regions, manifoldness.
    std::vector<std::pair<std::uint64_t, std::uint32_t>> edgeUses;
    edgeUses.reserve(faces.size() * 3);
    for (std::uint32_t fi = 0; fi < faces.size(); ++fi)
        for (int k = 0; k < 3; ++k) edgeUses.emplace_back(pairKey(faces[fi].p[k], faces[fi].p[(k + 1) % 3]), fi);
    std::sort(edgeUses.begin(), edgeUses.end());
    Dsu component(faces.size()), region(faces.size());
    std::vector<std::uint8_t> nonManifold(faces.size(), 0);
    for (std::size_t i = 0; i < edgeUses.size();)
    {
        std::size_t j = i;
        while (j < edgeUses.size() && edgeUses[j].first == edgeUses[i].first) ++j;
        for (std::size_t k = i + 1; k < j; ++k) component.unite(edgeUses[i].second, edgeUses[k].second);
        if (j - i == 2)
        {
            const Face& a = faces[edgeUses[i].second];
            const Face& b = faces[edgeUses[i + 1].second];
            if (smoothAcross(source.triangles[a.triangle], source.triangles[b.triangle], a.normal, b.normal))
                region.unite(edgeUses[i].second, edgeUses[i + 1].second);
        }
        else if (j - i > 2)
        {
            for (std::size_t k = i; k < j; ++k) nonManifold[edgeUses[k].second] = 1;
        }
        i = j;
    }

    // Pinned points: authored edges keep their exact vertices.
    std::vector<std::uint8_t> pinnedPoint(pointCount, 0);
    for (const auto& edge : source.edges)
        if ((edge.flags & EdgeAuthored) != 0 && edge.a < source.vertices.size() && edge.b < source.vertices.size())
            pinnedPoint[pointOf[edge.a]] = pinnedPoint[pointOf[edge.b]] = 1;
    local.pinnedVertices = static_cast<std::size_t>(std::count(pinnedPoint.begin(), pinnedPoint.end(), 1));

    std::map<std::uint32_t, std::vector<std::uint32_t>> facesByComponent;
    for (std::uint32_t fi = 0; fi < faces.size(); ++fi) facesByComponent[component.find(fi)].push_back(fi);
    local.components = facesByComponent.size();

    // Output assembly: untouched triangles keep their render vertices.
    MeshLod out;
    std::vector<std::int64_t> keptVertex(source.vertices.size(), -1);
    std::vector<std::uint8_t> triangleKept(source.triangles.size(), 1);
    std::vector<std::int64_t> pointOutVertex(pointCount, -1); // pinned points of simplified components
    const auto keepTriangle = [&](std::uint32_t ti) {
        Triangle t = source.triangles[ti];
        for (std::uint32_t* corner : {&t.a, &t.b, &t.c})
        {
            if (keptVertex[*corner] < 0)
            {
                keptVertex[*corner] = static_cast<std::int64_t>(out.vertices.size());
                out.vertices.push_back(source.vertices[*corner]);
            }
            *corner = static_cast<std::uint32_t>(keptVertex[*corner]);
        }
        out.triangles.push_back(t);
    };

    std::vector<std::uint8_t> faceSimplified(faces.size(), 0);
    for (const auto& [root, members] : facesByComponent)
    {
        (void)root;
        bool skip = members.size() < MinimumComponentTriangles;
        if (skip) ++local.skippedSmallComponents;
        bool manifold = true, textured = false;
        for (const std::uint32_t fi : members)
        {
            manifold = manifold && !nonManifold[fi];
            const auto& t = source.triangles[faces[fi].triangle];
            for (const std::uint32_t v : {t.a, t.b, t.c})
                textured = textured || source.vertices[v].uv != glm::vec2(0.0f);
        }
        if (!skip && !manifold) { skip = true; ++local.skippedNonManifoldComponents; }
        if (!skip && textured) { skip = true; ++local.skippedTexturedComponents; }
        if (skip) continue;

        // Local arrays.
        std::map<std::uint32_t, int> localPoint;
        std::vector<std::uint32_t> pointOfLocal;
        for (const std::uint32_t fi : members)
            for (const std::uint32_t p : faces[fi].p)
                if (localPoint.emplace(p, static_cast<int>(pointOfLocal.size())).second) pointOfLocal.push_back(p);
        Eigen::MatrixXd V(static_cast<Eigen::Index>(pointOfLocal.size()), 3);
        std::vector<std::uint8_t> pinned(pointOfLocal.size() + 1, 0);
        for (std::size_t i = 0; i < pointOfLocal.size(); ++i)
        {
            V.row(static_cast<Eigen::Index>(i)) = pointPosition[pointOfLocal[i]].transpose();
            pinned[i] = pinnedPoint[pointOfLocal[i]];
        }
        Eigen::MatrixXi F(static_cast<Eigen::Index>(members.size()), 3);
        std::vector<std::int64_t> faceRegion(members.size());
        for (std::size_t i = 0; i < members.size(); ++i)
        {
            for (int k = 0; k < 3; ++k) F(static_cast<Eigen::Index>(i), k) = localPoint[faces[members[i]].p[k]];
            faceRegion[i] = region.find(members[i]);
        }

        const auto decimateStart = std::chrono::steady_clock::now();
        ComponentResult r = decimateComponent(V, F, faceRegion, pinned, targetError);
        local.decimateSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - decimateStart).count();
        if (!r.simplified)
        {
            if (r.nonManifold) ++local.skippedNonManifoldComponents;
            continue;
        }
        ++local.simplifiedComponents;
        local.collapses += r.collapses;
        for (const std::uint32_t fi : members) faceSimplified[fi] = 1;

        const std::size_t base = out.vertices.size();
        for (Eigen::Index v = 0; v < r.U.rows(); ++v)
        {
            Vertex vertex;
            vertex.position = glm::vec3(static_cast<float>(r.U(v, 0)), static_cast<float>(r.U(v, 1)), static_cast<float>(r.U(v, 2)));
            vertex.normal = glm::vec3(0.0f);
            out.vertices.push_back(vertex);
            const std::int64_t birth = r.birthPoint[static_cast<std::size_t>(v)];
            if (birth >= 0 && static_cast<std::size_t>(birth) < pointOfLocal.size() && pinned[static_cast<std::size_t>(birth)])
                pointOutVertex[pointOfLocal[static_cast<std::size_t>(birth)]] = static_cast<std::int64_t>(base + static_cast<std::size_t>(v));
        }
        for (Eigen::Index f = 0; f < r.G.rows(); ++f)
        {
            Triangle t = source.triangles[faces[members[r.birthFace[static_cast<std::size_t>(f)]]].triangle];
            t.a = static_cast<std::uint32_t>(base + static_cast<std::size_t>(r.G(f, 0)));
            t.b = static_cast<std::uint32_t>(base + static_cast<std::size_t>(r.G(f, 1)));
            t.c = static_cast<std::uint32_t>(base + static_cast<std::size_t>(r.G(f, 2)));
            out.triangles.push_back(t);
        }
    }
    if (local.simplifiedComponents == 0)
    {
        if (stats) *stats = local;
        return source;
    }

    // Everything not simplified stays verbatim (incl. faces that degenerated
    // in point space).
    for (std::uint32_t fi = 0; fi < faces.size(); ++fi)
        if (faceSimplified[fi]) triangleKept[faces[fi].triangle] = 0;
    for (std::uint32_t ti = 0; ti < source.triangles.size(); ++ti)
    {
        const auto& t = source.triangles[ti];
        if (t.a >= source.vertices.size() || t.b >= source.vertices.size() || t.c >= source.vertices.size()) continue;
        if (triangleKept[ti]) keepTriangle(ti);
    }

    // Edges: untouched parts keep theirs (authored flags / render masks);
    // simplified parts keep only authored edges between pinned vertices.
    for (const auto& edge : source.edges)
    {
        std::int64_t a = edge.a < keptVertex.size() ? keptVertex[edge.a] : -1;
        std::int64_t b = edge.b < keptVertex.size() ? keptVertex[edge.b] : -1;
        if ((a < 0 || b < 0) && (edge.flags & EdgeAuthored) != 0 && edge.a < pointOf.size() && edge.b < pointOf.size())
        {
            a = pointOutVertex[pointOf[edge.a]];
            b = pointOutVertex[pointOf[edge.b]];
        }
        if (a < 0 || b < 0 || a == b) continue;
        Edge mapped = edge;
        mapped.a = static_cast<std::uint32_t>(a);
        mapped.b = static_cast<std::uint32_t>(b);
        mapped.triangleA = mapped.triangleB = -1;
        out.edges.push_back(mapped);
    }

    const auto canonicalStart = std::chrono::steady_clock::now();
    const CanonicalMeshBuildResult canonical = canonicalizeMesh(out);
    local.canonicalSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - canonicalStart).count();
    if (!canonical.success)
    {
        local.canonical = false;
        local.error = canonical.error.empty() ? canonical.repairStatus : canonical.error;
        local.outputTriangles = source.triangles.size();
        if (stats) *stats = local;
        return source;
    }
    local.outputTriangles = out.triangles.size();
    if (stats) *stats = local;
    return out;
}

} // namespace elite::model_asset::editor
