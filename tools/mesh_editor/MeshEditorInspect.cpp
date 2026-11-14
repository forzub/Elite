#include "tools/mesh_editor/MeshEditorInspect.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

#include "tools/mesh_editor/MeshEditorCatalog.h"

namespace elite::mesh_editor
{
namespace fs = std::filesystem;

namespace
{
constexpr double WeldEpsilon = 1e-4;   // 0.1 mm (the source is in metres)

using Vec3 = std::array<double, 3>;
using Tri = std::array<int, 3>;

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 cross(const Vec3& a, const Vec3& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

struct Group
{
    std::string name;          // LOD0 / LOD1 ... / "скорлупа traf"
    bool lod = true;           // a LOD of the model (shells are not compared between LODs)
    std::vector<std::pair<std::string, fs::path>> files;   // part key, file
};

struct RawMesh
{
    std::vector<Vec3> v;
    std::vector<Tri> t;
    std::vector<int> tm;              // material per triangle (index in mtl, -1 none)
    std::vector<std::string> mtl;     // usemtl names in order of first use
    bool material = false;
    std::uint64_t hash = 1469598103934665603ull;
};

RawMesh readObj(const fs::path& file)
{
    RawMesh m;
    int cur = -1;
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line))
    {
        for (unsigned char ch : line)
            m.hash = (m.hash ^ ch) * 1099511628211ull;
        if (line.size() > 2 && line[0] == 'v' && line[1] == ' ')
        {
            Vec3 p {};
            std::sscanf(line.c_str() + 2, "%lf %lf %lf", &p[0], &p[1], &p[2]);
            m.v.push_back(p);
        }
        else if (line.size() > 2 && line[0] == 'f' && line[1] == ' ')
        {
            std::istringstream s(line.substr(2));
            std::string tok;
            std::vector<int> poly;
            while (s >> tok)
            {
                const int i = std::atoi(tok.c_str());
                poly.push_back(i < 0 ? static_cast<int>(m.v.size()) + i : i - 1);
            }
            for (std::size_t k = 1; k + 1 < poly.size(); ++k)
            {
                m.t.push_back({poly[0], poly[k], poly[k + 1]});
                m.tm.push_back(cur);
            }
        }
        else if (line.rfind("usemtl", 0) == 0)
        {
            std::string name = line.substr(6);
            name.erase(0, name.find_first_not_of(" \t"));
            while (!name.empty() && (name.back() == '\r' || name.back() == ' '))
                name.pop_back();
            const auto it = std::find(m.mtl.begin(), m.mtl.end(), name);
            cur = static_cast<int>(it - m.mtl.begin());
            if (it == m.mtl.end())
                m.mtl.push_back(name);
            m.material = true;
        }
        else if (line.rfind("mtllib", 0) == 0)
            m.material = true;
    }
    return m;
}

// one part in one group after the recipe
struct PartGeo
{
    std::vector<Vec3> v;
    std::vector<Tri> t;
    std::vector<int> tm;              // material per triangle (index in mtl; caps: the "eq_hole" entry)
    std::vector<std::string> mtl;
    int rawVerts = 0, rawTris = 0;
    int welded = 0, degenerate = 0, repeated = 0, flipped = 0;
    int split = 0, reoriented = 0, capped = 0;   // touching pieces parted, faces turned to their neighbours, holes closed
    int membrane = 0;                            // back-to-back faces between two touching volumes, removed
    std::vector<std::array<Vec3, 2>> openEdges, badEdges;
    int boundary = 0, nonManifold = 0, inconsistent = 0, edgeCount = 0;
    int holes = 0;            // simple boundary loops left open
    double holeMax = 0.0;     // the biggest open loop, metres
    int pieces = 0, closedPieces = 0;
    bool material = false;
    Vec3 lo {1e30, 1e30, 1e30}, hi {-1e30, -1e30, -1e30};
    std::uint64_t hash = 0;
};

struct RepairOptions
{
    bool capHoles = false;        // the author's "Закрыть дыры": every simple open loop gets a fan cap
    Vec3 modelCentre {0, 0, 0};   // open pieces face away from it (hull plates look outward)
};

using EdgeKey = std::pair<int, int>;
EdgeKey edgeKey(int a, int b) { return {std::min(a, b), std::max(a, b)}; }

std::map<EdgeKey, std::vector<int>> edgeFaces(const std::vector<Tri>& t)
{
    std::map<EdgeKey, std::vector<int>> m;
    for (int i = 0; i < static_cast<int>(t.size()); ++i)
        for (int k = 0; k < 3; ++k)
            m[edgeKey(t[i][k], t[i][(k + 1) % 3])].push_back(i);
    return m;
}

bool hasDirected(const Tri& t, int a, int b);

// an edge and the face pairs that meet across it: two faces - one pair; 4, 6... faces (pieces that touch along the
// edge: a washer under a bolt head, two boxes sharing an edge) - neighbours by angle around the edge, paired so that
// each pair runs the edge both ways; an odd count or no such pairing - no pairs (a real 3+ face edge)
struct EdgeInfo
{
    std::vector<int> faces;
    std::vector<std::pair<int, int>> pairs;
};

std::map<EdgeKey, EdgeInfo> edgeTable(const std::vector<Vec3>& v, const std::vector<Tri>& t)
{
    std::map<EdgeKey, EdgeInfo> m;
    for (int i = 0; i < static_cast<int>(t.size()); ++i)
        for (int k = 0; k < 3; ++k)
            m[edgeKey(t[i][k], t[i][(k + 1) % 3])].faces.push_back(i);
    for (auto& [e, info] : m)
    {
        const auto& f = info.faces;
        if (f.size() == 2)
        {
            info.pairs.push_back({f[0], f[1]});
            continue;
        }
        if (f.size() < 4 || f.size() % 2 != 0)
            continue;
        const Vec3 a = v[e.first], d0 = sub(v[e.second], a);
        const double len = std::sqrt(dot(d0, d0));
        if (len <= 0.0)
            continue;
        const Vec3 d {d0[0] / len, d0[1] / len, d0[2] / len};
        Vec3 u1 = std::abs(d[0]) < 0.9 ? Vec3 {1, 0, 0} : Vec3 {0, 1, 0};
        const double pr = dot(u1, d);
        u1 = {u1[0] - d[0] * pr, u1[1] - d[1] * pr, u1[2] - d[2] * pr};
        const Vec3 u2 = cross(d, u1);
        std::vector<std::pair<double, int>> around;
        for (int fi : f)
        {
            int c = t[fi][0];
            for (int k = 0; k < 3; ++k)
                if (t[fi][k] != e.first && t[fi][k] != e.second)
                    c = t[fi][k];
            const Vec3 w = sub(v[c], a);
            around.push_back({std::atan2(dot(w, u2), dot(w, u1)), fi});
        }
        std::sort(around.begin(), around.end());
        const std::size_t n = around.size();
        for (std::size_t offset = 0; offset < 2 && info.pairs.empty(); ++offset)
        {
            std::vector<std::pair<int, int>> pairs;
            for (std::size_t i = 0; i < n; i += 2)
            {
                const int x = around[(i + offset) % n].second, y = around[(i + offset + 1) % n].second;
                if (hasDirected(t[x], e.first, e.second) == hasDirected(t[y], e.first, e.second))
                    break;
                pairs.push_back({x, y});
            }
            if (pairs.size() == n / 2)
                info.pairs = pairs;
        }
        if (info.pairs.empty())   // windings not settled yet: neighbours by angle, the winding follows in orient()
            for (std::size_t i = 0; i < n; i += 2)
                info.pairs.push_back({around[i].second, around[i + 1].second});
    }
    return m;
}

// a pairing that a settled winding agrees with (each pair runs the edge both ways)
bool pairedWell(const EdgeInfo& info, const std::vector<Tri>& t, const EdgeKey& e)
{
    if (info.pairs.empty() || info.pairs.size() * 2 != info.faces.size())
        return false;
    for (const auto& [x, y] : info.pairs)
        if (hasDirected(t[x], e.first, e.second) == hasDirected(t[y], e.first, e.second))
            return false;
    return true;
}

// pieces: triangles joined through the face pairs of their edges
std::vector<int> pieceOf(const std::vector<Tri>& t, const std::map<EdgeKey, EdgeInfo>& et, int& count)
{
    std::vector<int> parent(t.size());
    for (std::size_t i = 0; i < t.size(); ++i)
        parent[i] = static_cast<int>(i);
    std::function<int(int)> root = [&](int x) { return parent[x] == x ? x : parent[x] = root(parent[x]); };
    for (const auto& [e, info] : et)
        for (const auto& [x, y] : info.pairs)
            parent[root(x)] = root(y);
    std::map<int, int> ids;
    std::vector<int> piece(t.size());
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        const auto it = ids.emplace(root(static_cast<int>(i)), static_cast<int>(ids.size())).first;
        piece[i] = it->second;
    }
    count = static_cast<int>(ids.size());
    return piece;
}

bool hasDirected(const Tri& t, int a, int b)
{
    for (int k = 0; k < 3; ++k)
        if (t[k] == a && t[(k + 1) % 3] == b)
            return true;
    return false;
}

// every piece wound one way (faces follow their neighbours); a closed piece then faces outward (positive volume),
// an open one away from the model centre. Returns faces whose final winding differs from the source.
void orient(PartGeo& g, const Vec3& centre)
{
    const auto et = edgeTable(g.v, g.t);
    int count = 0;
    const std::vector<int> piece = pieceOf(g.t, et, count);
    const std::vector<Tri> before = g.t;
    std::vector<char> done(g.t.size(), 0);
    std::vector<std::vector<int>> members(count);
    for (std::size_t i = 0; i < g.t.size(); ++i)
        members[piece[i]].push_back(static_cast<int>(i));
    g.pieces = count;
    g.closedPieces = 0;
    for (int p = 0; p < count; ++p)
    {
        // breadth-first from the first face: a neighbour across a shared edge must run it the other way
        std::vector<int> queue {members[p][0]};
        done[members[p][0]] = 1;
        for (std::size_t q = 0; q < queue.size(); ++q)
        {
            const Tri& a = g.t[queue[q]];
            for (int k = 0; k < 3; ++k)
            {
                const int u = a[k], v = a[(k + 1) % 3];
                int n = -1;
                for (const auto& [x, y] : et.at(edgeKey(u, v)).pairs)
                    n = x == queue[q] ? y : y == queue[q] ? x : n;
                if (n < 0 || done[n])
                    continue;
                if (hasDirected(g.t[n], u, v))
                    std::swap(g.t[n][1], g.t[n][2]);
                done[n] = 1;
                queue.push_back(n);
            }
        }
        bool closed = true;
        double vol = 0.0, facing = 0.0;
        for (int i : members[p])
        {
            const Tri& t = g.t[i];
            for (int k = 0; k < 3; ++k)
            {
                const auto& info = et.at(edgeKey(t[k], t[(k + 1) % 3]));
                closed = closed && !info.pairs.empty() && info.pairs.size() * 2 == info.faces.size();
            }
            vol += dot(g.v[t[0]], cross(g.v[t[1]], g.v[t[2]]));
            const Vec3 n = cross(sub(g.v[t[1]], g.v[t[0]]), sub(g.v[t[2]], g.v[t[0]]));
            const Vec3 c {(g.v[t[0]][0] + g.v[t[1]][0] + g.v[t[2]][0]) / 3 - centre[0], (g.v[t[0]][1] + g.v[t[1]][1] + g.v[t[2]][1]) / 3 - centre[1],
                          (g.v[t[0]][2] + g.v[t[1]][2] + g.v[t[2]][2]) / 3 - centre[2]};
            facing += dot(n, c);
        }
        g.closedPieces += closed ? 1 : 0;
        if (closed ? vol < 0.0 : facing < 0.0)
            for (int i : members[p])
                std::swap(g.t[i][1], g.t[i][2]);
        int changed = 0;
        for (int i : members[p])
            changed += hasDirected(g.t[i], before[i][0], before[i][1]) ? 0 : 1;
        if (changed == static_cast<int>(members[p].size()) && closed)
            ++g.flipped;   // a closed piece inside out as a whole
        else
            g.reoriented += changed;
    }
}

// pieces that only touch (along an edge or at a vertex) get their own vertices, so a shared edge stops being a 3+ face edge
void splitTouching(PartGeo& g)
{
    const auto et = edgeTable(g.v, g.t);
    int count = 0;
    const std::vector<int> piece = pieceOf(g.t, et, count);
    int touching = 0;
    for (const auto& [e, info] : et)
        touching += info.faces.size() > 2 ? 1 : 0;
    g.split = touching;   // edges where pieces touch: parted (own vertices) or paired face to face
    if (count < 2)
        return;
    std::map<std::pair<int, int>, int> copy;   // (piece, vertex) -> new vertex
    std::vector<Vec3> v;
    for (std::size_t i = 0; i < g.t.size(); ++i)
        for (int k = 0; k < 3; ++k)
        {
            const auto key = std::make_pair(piece[i], g.t[i][k]);
            auto it = copy.find(key);
            if (it == copy.end())
            {
                it = copy.emplace(key, static_cast<int>(v.size())).first;
                v.push_back(g.v[g.t[i][k]]);
            }
            g.t[i][k] = it->second;
        }
    g.v = std::move(v);
}

// "Закрыть дыры": a fan from the loop centre for every simple open loop (each loop vertex starts one open edge)
void capHoles(PartGeo& g)
{
    const auto ef = edgeFaces(g.t);
    std::map<int, std::vector<int>> next;   // open half-edge a->b as the face runs it
    for (const auto& [e, f] : ef)
        if (f.size() == 1)
        {
            const Tri& t = g.t[f[0]];
            const bool fwd = hasDirected(t, e.first, e.second);
            next[fwd ? e.first : e.second].push_back(fwd ? e.second : e.first);
        }
    std::set<int> used;
    for (const auto& [start, outs] : next)
    {
        if (used.count(start) || outs.size() != 1)
            continue;
        std::vector<int> loop {start};
        int cur = outs[0];
        bool ok = true;
        while (cur != start)
        {
            const auto it = next.find(cur);
            if (it == next.end() || it->second.size() != 1 || used.count(cur) || loop.size() > 100000)
            {
                ok = false;
                break;
            }
            loop.push_back(cur);
            cur = it->second[0];
        }
        for (int x : loop)
            used.insert(x);
        if (!ok || loop.size() < 3)
            continue;
        Vec3 c {0, 0, 0};
        for (int x : loop)
            for (int k = 0; k < 3; ++k)
                c[k] += g.v[x][k] / static_cast<double>(loop.size());
        const int ci = static_cast<int>(g.v.size());
        g.v.push_back(c);
        auto hole = std::find(g.mtl.begin(), g.mtl.end(), std::string("eq_hole"));
        if (hole == g.mtl.end())
        {
            g.mtl.push_back("eq_hole");
            hole = g.mtl.end() - 1;
        }
        const int holeMat = static_cast<int>(hole - g.mtl.begin());
        for (std::size_t i = 0; i < loop.size(); ++i)
        {
            const int a = loop[i], b = loop[(i + 1) % loop.size()];
            g.t.push_back({b, a, ci});   // the cap runs the open edge the other way
            g.tm.push_back(holeMat);
        }
        ++g.capped;
    }
}

// open edges, 3+ face edges and badly wound edges of the final mesh, plus the simple open loops
void analyse(PartGeo& g)
{
    g.openEdges.clear();
    g.badEdges.clear();
    g.boundary = g.nonManifold = g.inconsistent = 0;
    const auto ef = edgeFaces(g.t);
    const auto et = edgeTable(g.v, g.t);
    g.edgeCount = static_cast<int>(ef.size());
    std::map<int, std::vector<int>> adj;
    for (const auto& [e, f] : ef)
    {
        const std::array<Vec3, 2> seg {g.v[e.first], g.v[e.second]};
        if (f.size() == 1)
        {
            ++g.boundary;
            g.openEdges.push_back(seg);
            adj[e.first].push_back(e.second);
            adj[e.second].push_back(e.first);
        }
        else if (f.size() > 2)
        {
            if (!pairedWell(et.at(e), g.t, e))
            {
                ++g.nonManifold;
                g.badEdges.push_back(seg);
            }
        }
        else if (hasDirected(g.t[f[0]], e.first, e.second) == hasDirected(g.t[f[1]], e.first, e.second))
        {
            ++g.inconsistent;
            g.badEdges.push_back(seg);
        }
    }
    g.holes = 0;
    g.holeMax = 0.0;
    std::set<int> seen;
    for (const auto& [s, n] : adj)
    {
        if (seen.count(s))
            continue;
        std::vector<int> stack {s};
        Vec3 lo {1e30, 1e30, 1e30}, hi {-1e30, -1e30, -1e30};
        while (!stack.empty())
        {
            const int x = stack.back();
            stack.pop_back();
            if (!seen.insert(x).second)
                continue;
            for (int k = 0; k < 3; ++k)
            {
                lo[k] = std::min(lo[k], g.v[x][k]);
                hi[k] = std::max(hi[k], g.v[x][k]);
            }
            for (int y : adj[x])
                stack.push_back(y);
        }
        ++g.holes;
        g.holeMax = std::max({g.holeMax, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
    }
}

PartGeo repair(const RawMesh& raw, const RepairOptions& opt)
{
    PartGeo g;
    g.rawVerts = static_cast<int>(raw.v.size());
    g.rawTris = static_cast<int>(raw.t.size());
    g.material = raw.material;
    g.mtl = raw.mtl;
    g.hash = raw.hash;
    // weld
    std::map<std::array<long long, 3>, int> cells;
    std::vector<int> remap(raw.v.size(), -1);
    for (std::size_t i = 0; i < raw.v.size(); ++i)
    {
        const auto& p = raw.v[i];
        const std::array<long long, 3> key {std::llround(p[0] / WeldEpsilon), std::llround(p[1] / WeldEpsilon), std::llround(p[2] / WeldEpsilon)};
        const auto it = cells.find(key);
        if (it != cells.end())
            remap[i] = it->second;
        else
        {
            remap[i] = static_cast<int>(g.v.size());
            cells[key] = remap[i];
            g.v.push_back(p);
            for (int k = 0; k < 3; ++k)
            {
                g.lo[k] = std::min(g.lo[k], p[k]);
                g.hi[k] = std::max(g.hi[k], p[k]);
            }
        }
    }
    g.welded = g.rawVerts - static_cast<int>(g.v.size());
    // degenerate and repeated triangles
    std::map<std::array<int, 3>, int> seenAt;   // sorted corners -> first face
    std::map<int, int> opposite;                // face -> its back-to-back twin
    for (std::size_t ti = 0; ti < raw.t.size(); ++ti)
    {
        const Tri& t0 = raw.t[ti];
        bool bad = false;
        Tri t {};
        for (int k = 0; k < 3; ++k)
        {
            bad = bad || t0[k] < 0 || t0[k] >= static_cast<int>(remap.size());
            t[k] = bad ? 0 : remap[t0[k]];
        }
        if (bad || t[0] == t[1] || t[1] == t[2] || t[0] == t[2])
        {
            ++g.degenerate;
            continue;
        }
        const Vec3 n = cross(sub(g.v[t[1]], g.v[t[0]]), sub(g.v[t[2]], g.v[t[0]]));
        if (dot(n, n) < 1e-18)
        {
            ++g.degenerate;
            continue;
        }
        std::array<int, 3> key = t;
        std::sort(key.begin(), key.end());
        const auto it = seenAt.find(key);
        if (it != seenAt.end())
        {
            if (hasDirected(g.t[it->second], t[1], t[0]) && opposite.count(it->second) == 0)
                opposite[it->second] = static_cast<int>(g.t.size());   // the same face turned the other way
            else
            {
                ++g.repeated;
                continue;
            }
        }
        else
            seenAt[key] = static_cast<int>(g.t.size());
        g.t.push_back(t);
        g.tm.push_back(ti < raw.tm.size() ? raw.tm[ti] : -1);
    }
    // a membrane between two touching volumes (two lathed bodies meeting at a disc: the disc twice, back to back) - both
    // copies go, the volumes become one closed body. A free double-faced plate (its rim edges have no other faces) stays.
    if (!opposite.empty())
    {
        const auto ef = edgeFaces(g.t);
        std::map<EdgeKey, std::vector<int>> repEdges;   // edges of the membrane faces (first copies)
        for (const auto& [a, b] : opposite)
            for (int k = 0; k < 3; ++k)
                repEdges[edgeKey(g.t[a][k], g.t[a][(k + 1) % 3])].push_back(a);
        std::map<int, int> parent;
        for (const auto& [a, b] : opposite)
            parent[a] = a;
        std::function<int(int)> root = [&](int x) { return parent[x] == x ? x : parent[x] = root(parent[x]); };
        for (const auto& [e, f] : repEdges)
            for (std::size_t i = 1; i < f.size(); ++i)
                parent[root(f[0])] = root(f[i]);
        std::map<int, bool> attached;   // membrane -> every rim edge has faces besides the two copies
        for (const auto& [e, f] : repEdges)
            if (f.size() == 1)
            {
                const int r = root(f[0]);
                const bool more = ef.at(e).size() > 2;
                attached[r] = attached.count(r) ? attached[r] && more : more;
            }
        std::vector<char> drop(g.t.size(), 0);
        for (const auto& [a, b] : opposite)
            if (attached.count(root(a)) && attached[root(a)])
                drop[a] = drop[b] = 1;
        std::vector<Tri> kept;
        std::vector<int> keptMat;
        for (std::size_t i = 0; i < g.t.size(); ++i)
            if (drop[i])
                ++g.membrane;
            else
            {
                kept.push_back(g.t[i]);
                keptMat.push_back(g.tm[i]);
            }
        g.t = std::move(kept);
        g.tm = std::move(keptMat);
    }
    if (g.t.empty())
        return g;
    splitTouching(g);
    orient(g, opt.modelCentre);
    if (opt.capHoles)
    {
        capHoles(g);
        const int flipped = g.flipped, reoriented = g.reoriented;
        orient(g, opt.modelCentre);   // the capped pieces are closed now: outward by volume
        g.flipped = flipped;
        g.reoriented = reoriented;
    }
    analyse(g);
    return g;
}

std::vector<Group> collectGroups(const ModelCard& card)
{
    std::vector<Group> groups;
    std::error_code ec;
    auto objsIn = [&](const fs::path& dir, std::vector<std::pair<std::string, fs::path>>& out, const std::string& prefix) {
        std::vector<fs::path> files;
        for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (it->is_directory() && it->path().filename() == "variants")
            {
                it.disable_recursion_pending();   // damaged variants: the "Повреждения" step
                continue;
            }
            if (it->is_regular_file() && it->path().extension() == ".obj")
                files.push_back(it->path());
        }
        std::sort(files.begin(), files.end());
        for (const auto& f : files)
            out.push_back({prefix + f.stem().u8string(), f});
    };
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(card.folder, ec))
        if (e.is_directory() && e.path().filename().string().rfind("LOD", 0) == 0)
            dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& d : dirs)
    {
        Group g;
        g.name = d.filename().u8string();
        objsIn(d, g.files, "");
        groups.push_back(std::move(g));
    }
    if (dirs.empty())
    {
        std::map<std::string, Group> byLod;   // the mock-up: source/geo/<part>.lod<N>.obj
        for (const auto& f : fs::directory_iterator(card.folder / "source" / "geo", ec))
        {
            const std::string stem = f.path().stem().u8string();
            const auto dot = stem.rfind(".lod");
            if (f.path().extension() != ".obj" || dot == std::string::npos)
                continue;
            Group& g = byLod["LOD" + stem.substr(dot + 4)];
            g.name = "LOD" + stem.substr(dot + 4);
            g.files.push_back({stem.substr(0, dot), f.path()});
        }
        for (auto& [name, g] : byLod)
        {
            std::sort(g.files.begin(), g.files.end());
            groups.push_back(std::move(g));
        }
    }
    std::vector<fs::path> shells;
    for (const auto& e : fs::directory_iterator(card.folder / "shells", ec))
        if (e.is_directory())
            shells.push_back(e.path());
    std::sort(shells.begin(), shells.end());
    for (const auto& s : shells)
    {
        Group g;
        g.name = "скорлупа " + s.filename().u8string();
        g.lod = false;
        objsIn(s, g.files, s.filename().u8string() + "/");
        groups.push_back(std::move(g));
    }
    return groups;
}

nlohmann::json readJson(const fs::path& p)
{
    std::ifstream in(p);
    if (!in)
        return nlohmann::json::object();
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    return j.is_object() ? j : nlohmann::json::object();
}

std::string hex(std::uint64_t h)
{
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

std::string cm(double metres)
{
    char buf[32];
    if (metres < 0.01)
        std::snprintf(buf, sizeof buf, "%.1f мм", metres * 1000.0);
    else if (metres < 1.0)
        std::snprintf(buf, sizeof buf, "%.0f см", metres * 100.0);
    else
        std::snprintf(buf, sizeof buf, "%.2f м", metres);
    return buf;
}
struct PartInfo
{
    std::string key;
    std::vector<std::pair<int, PartGeo>> geo;   // group index, geometry
};

// every mesh of every group through the recipe; state: check.json (caps)
std::vector<PartInfo> loadParts(const std::vector<Group>& groups, const nlohmann::json& state)
{
    std::vector<PartInfo> parts;
    auto partOf = [&](const std::string& key) -> PartInfo& {
        for (auto& p : parts)
            if (p.key == key)
                return p;
        parts.push_back({key, {}});
        return parts.back();
    };
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
    {
        std::vector<RawMesh> raws;
        Vec3 lo {1e30, 1e30, 1e30}, hi {-1e30, -1e30, -1e30};
        for (const auto& [key, file] : groups[gi].files)
        {
            raws.push_back(readObj(file));
            for (const auto& v : raws.back().v)
                for (int k = 0; k < 3; ++k)
                {
                    lo[k] = std::min(lo[k], v[k]);
                    hi[k] = std::max(hi[k], v[k]);
                }
        }
        RepairOptions opt;
        opt.modelCentre = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
        for (std::size_t fi = 0; fi < groups[gi].files.size(); ++fi)
        {
            const std::string& key = groups[gi].files[fi].first;
            PartInfo& p = partOf(key);
            opt.capHoles = state["caps"].value(key, false);
            PartGeo g = repair(raws[fi], opt);
            // two files with one key in a group (rare): merge into one part
            auto same = std::find_if(p.geo.begin(), p.geo.end(), [&](const auto& x) { return x.first == static_cast<int>(gi); });
            if (same == p.geo.end())
                p.geo.push_back({static_cast<int>(gi), std::move(g)});
            else
            {
                PartGeo& a = same->second;
                const int base = static_cast<int>(a.v.size());
                a.v.insert(a.v.end(), g.v.begin(), g.v.end());
                for (std::size_t i = 0; i < g.t.size(); ++i)
                {
                    const Tri& t = g.t[i];
                    a.t.push_back({t[0] + base, t[1] + base, t[2] + base});
                    int m = -1;
                    if (g.tm[i] >= 0)
                    {
                        const std::string& name = g.mtl[static_cast<std::size_t>(g.tm[i])];
                        auto it = std::find(a.mtl.begin(), a.mtl.end(), name);
                        if (it == a.mtl.end())
                        {
                            a.mtl.push_back(name);
                            it = a.mtl.end() - 1;
                        }
                        m = static_cast<int>(it - a.mtl.begin());
                    }
                    a.tm.push_back(m);
                }
                a.openEdges.insert(a.openEdges.end(), g.openEdges.begin(), g.openEdges.end());
                a.badEdges.insert(a.badEdges.end(), g.badEdges.begin(), g.badEdges.end());
                a.boundary += g.boundary;
                a.nonManifold += g.nonManifold;
                a.inconsistent += g.inconsistent;
                a.welded += g.welded;
                a.degenerate += g.degenerate;
                a.repeated += g.repeated;
                a.flipped += g.flipped;
                a.split += g.split;
                a.reoriented += g.reoriented;
                a.capped += g.capped;
                a.membrane += g.membrane;
                a.holes += g.holes;
                a.holeMax = std::max(a.holeMax, g.holeMax);
                a.pieces += g.pieces;
                a.closedPieces += g.closedPieces;
                a.edgeCount += g.edgeCount;
                a.rawVerts += g.rawVerts;
                a.rawTris += g.rawTris;
                a.hash = a.hash * 31 + g.hash;
                for (int k = 0; k < 3; ++k)
                {
                    a.lo[k] = std::min(a.lo[k], g.lo[k]);
                    a.hi[k] = std::max(a.hi[k], g.hi[k]);
                }
            }
        }
    }

    return parts;
}

// the automatic kind: closed -> volume; open along much of its edges, or thin -> sheet; else a volume with holes
std::string autoKindOf(const PartGeo& first)
{
    const double ext[3] = {first.hi[0] - first.lo[0], first.hi[1] - first.lo[1], first.hi[2] - first.lo[2]};
    const double minExt = std::min({ext[0], ext[1], ext[2]}), maxExt = std::max({ext[0], ext[1], ext[2]});
    const bool openMuch = first.boundary >= 0.2 * std::max(first.edgeCount, 1);
    return first.boundary == 0 ? "volume" : (openMuch || minExt <= 0.05 * maxExt || minExt < 0.02 ? "sheet" : "volume");
}

} // namespace

nlohmann::json Inspector::inspect(const ModelCard& card, const fs::path& webRoot, const fs::path& stateDir, bool commitCheck)
{
    const std::vector<Group> groups = collectGroups(card);
    nlohmann::json state = readJson(stateDir / "check.json");
    if (!state.contains("kinds"))
        state["kinds"] = nlohmann::json::object();
    const bool everChecked = state.contains("parts");

    if (!state.contains("caps"))
        state["caps"] = nlohmann::json::object();
    std::vector<PartInfo> parts = loadParts(groups, state);

    // LOD groups that are real per-part LODs (a whole-ship proxy of one mesh is not compared part by part)
    std::vector<int> partLods;
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
        if (groups[gi].lod && groups[gi].files.size() > 1)
            partLods.push_back(static_cast<int>(gi));

    nlohmann::json outParts = nlohmann::json::array();
    nlohmann::json newState = state;
    newState["parts"] = nlohmann::json::object();
    int nOk = 0, nErr = 0, nChanged = 0, nNew = 0;
    for (const auto& p : parts)
    {
        const PartGeo& first = p.geo.front().second;
        // kind: closed -> volume; open and thin -> sheet; open and thick -> a volume with holes
        const double ext[3] = {first.hi[0] - first.lo[0], first.hi[1] - first.lo[1], first.hi[2] - first.lo[2]};
        const double minExt = std::min({ext[0], ext[1], ext[2]}), maxExt = std::max({ext[0], ext[1], ext[2]});
        // a sheet: open along much of its edges (a hull plate, bent or flat) or thin; a volume with a few holes stays a volume
        (void)minExt;
        (void)maxExt;
        const std::string autoKind = autoKindOf(first);
        const std::string kind = state["kinds"].value(p.key, autoKind);

        nlohmann::json issues = nlohmann::json::array();
        nlohmann::json lods = nlohmann::json::object();
        nlohmann::json fixed = {{"welded", 0}, {"degenerate", 0}, {"repeated", 0}, {"flipped", 0}, {"split", 0}, {"reoriented", 0}, {"capped", 0}, {"membrane", 0}};
        bool error = false;
        bool openVolume = false;
        int holes = 0;
        std::uint64_t hash = 1469598103934665603ull;
        const bool several = p.geo.size() > 1;
        const bool caps = state["caps"].value(p.key, false);
        for (const auto& [gi, g] : p.geo)
        {
            const std::string at = several ? groups[gi].name + ": " : std::string();
            lods[groups[gi].name] = {{"tris", g.t.size()}, {"verts", g.v.size()}};
            for (const auto& [name, n] : std::initializer_list<std::pair<const char*, int>> {
                     {"welded", g.welded}, {"degenerate", g.degenerate}, {"repeated", g.repeated}, {"flipped", g.flipped},
                     {"split", g.split}, {"reoriented", g.reoriented}, {"capped", g.capped}, {"membrane", g.membrane}})
                fixed[name] = fixed[name].get<int>() + n;
            hash = (hash ^ g.hash) * 1099511628211ull;
            if (kind == "volume" && g.boundary > 0)
            {
                issues.push_back({{"sev", "error"},
                                  {"text", at + "дыры: " + std::to_string(g.holes) + " (" + std::to_string(g.boundary) + " открытых рёбер, самая большая " +
                                               cm(g.holeMax) + ")" + (caps ? " — эти не закрываются сами" : "")}});
                error = true;
                openVolume = true;
                holes += g.holes;
            }
            if (g.nonManifold > 0)
            {
                issues.push_back({{"sev", "error"}, {"text", at + std::to_string(g.nonManifold) + " рёбер, где сходятся 3 и больше граней"}});
                error = true;
            }
            if (g.inconsistent > 0)
            {
                const bool err = kind == "volume";
                issues.push_back({{"sev", err ? "error" : "note"},
                                  {"text", at + "часть граней вывернута: " + std::to_string(g.inconsistent) + " рёбер между гранями разного направления"}});
                error = error || err;
            }
            if (!g.material)
                issues.push_back({{"sev", "note"}, {"text", at + "нет материала"}});
        }
        // between LODs
        std::vector<std::string> absent;
        int prevTris = -1;
        std::string prevName;
        for (int gi : partLods)
        {
            const auto it = std::find_if(p.geo.begin(), p.geo.end(), [&](const auto& x) { return x.first == gi; });
            if (it == p.geo.end())
            {
                if (groups[gi].lod && p.key.find('/') == std::string::npos)
                    absent.push_back(groups[gi].name);
                continue;
            }
            const int tris = static_cast<int>(it->second.t.size());
            if (prevTris >= 0 && tris >= prevTris && tris > 12)
                issues.push_back({{"sev", "note"},
                                  {"text", groups[gi].name + " не легче " + prevName + ": " + std::to_string(tris) + " треуг. против " + std::to_string(prevTris)}});
            prevTris = tris;
            prevName = groups[gi].name;
        }
        const bool inPartLod = std::any_of(p.geo.begin(), p.geo.end(), [&](const auto& x) {
            return std::find(partLods.begin(), partLods.end(), x.first) != partLods.end();
        });   // a whole-ship proxy (its LOD has one mesh) is not compared part by part
        if (!absent.empty() && inPartLod && p.key.find('/') == std::string::npos)
        {
            std::string s;
            for (const auto& a : absent)
                s += (s.empty() ? "" : ", ") + a;
            issues.push_back({{"sev", "note"}, {"text", "нет в " + s}});
        }
        int fixedTotal = 0;
        for (const auto& [name, n] : fixed.items())
            fixedTotal += n.get<int>();
        // how the game draws it: a closed volume with outward normals - one side (back faces culled); a sheet - both sides
        bool consistent = true;
        for (const auto& [gi, g] : p.geo)
            consistent = consistent && g.nonManifold == 0 && g.inconsistent == 0;
        const int sides = kind == "sheet" || openVolume ? 2 : 1;
        std::string render = kind == "sheet" ? "двусторонний: лист, нормали от центра модели"
                                             : openVolume ? "двусторонний, пока есть дыры (иначе сквозь них видна пустота)"
                                                          : "односторонний: замкнут, нормали наружу";
        if (!consistent)
            render += "; грани местами не согласованы — свет может лечь пятнами";

        // against the last check
        const Vec3 centre {(first.lo[0] + first.hi[0]) / 2, (first.lo[1] + first.hi[1]) / 2, (first.lo[2] + first.hi[2]) / 2};
        std::string change = "none", changeText;
        if (everChecked && !commitCheck)
        {
            const auto prev = state["parts"].find(p.key);
            if (prev == state["parts"].end())
                change = "new";
            else if ((*prev).value("hash", std::string()) != hex(hash))
            {
                change = "changed";
                const auto c = (*prev).value("centre", std::vector<double> {0, 0, 0});
                const auto sz = (*prev).value("size", std::vector<double> {0, 0, 0});
                const double shift = c.size() == 3 ? std::sqrt(dot(sub(centre, {c[0], c[1], c[2]}), sub(centre, {c[0], c[1], c[2]}))) : 0.0;
                double grow = 0.0;
                for (int k = 0; k < 3 && sz.size() == 3; ++k)
                    grow = std::max(grow, std::abs(ext[k] - sz[k]));
                if (shift > 0.001)
                    changeText = "сдвинута на " + cm(shift);
                if (grow > 0.001)
                    changeText += (changeText.empty() ? "" : ", ") + std::string("размер изменился на ") + cm(grow);
                if (changeText.empty())
                    changeText = "геометрия изменилась";
            }
        }
        newState["parts"][p.key] = {{"hash", hex(hash)}, {"centre", {centre[0], centre[1], centre[2]}}, {"size", {ext[0], ext[1], ext[2]}},
                                    {"result", error ? "error" : "ok"}};

        nErr += error ? 1 : 0;
        nOk += error ? 0 : 1;
        nChanged += change == "changed" ? 1 : 0;
        nNew += change == "new" ? 1 : 0;
        outParts.push_back({{"key", p.key},
                            {"kind", kind},
                            {"kindAuto", autoKind},
                            {"kindSet", state["kinds"].contains(p.key)},
                            {"result", error ? "error" : "ok"},
                            {"change", change},
                            {"changeText", changeText},
                            {"issues", issues},
                            {"fixed", fixed},
                            {"fixedTotal", fixedTotal},
                            {"sides", sides},
                            {"render", render},
                            {"holes", holes},
                            {"caps", caps},
                            {"canCap", holes > 0 || caps},
                            {"lods", lods},
                            {"size", {ext[0], ext[1], ext[2]}},
                            {"centre", {centre[0], centre[1], centre[2]}}});
    }
    nlohmann::json missing = nlohmann::json::array();
    if (everChecked && !commitCheck)
        for (const auto& [key, v] : state["parts"].items())
            if (std::none_of(parts.begin(), parts.end(), [&](const PartInfo& p) { return p.key == key; }))
                missing.push_back(key);

    // the viewer's binary files: per group, triangles of every part, then open edges, then bad edges
    std::error_code ec;
    const fs::path cacheDir = webRoot / "cache" / card.category / card.id;
    fs::create_directories(cacheDir, ec);
    nlohmann::json outGroups = nlohmann::json::array();
    const auto stamp = std::to_string(fs::file_time_type::clock::now().time_since_epoch().count());
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
    {
        std::vector<float> tris, open, bad;
        nlohmann::json ranges = nlohmann::json::array();
        for (const auto& p : parts)
            for (const auto& [g, geo] : p.geo)
            {
                if (g != static_cast<int>(gi))
                    continue;
                nlohmann::json r;
                r["key"] = p.key;
                r["tri"] = {tris.size() / 9, geo.t.size()};
                for (const auto& t : geo.t)
                    for (int k = 0; k < 3; ++k)
                        for (int c = 0; c < 3; ++c)
                            tris.push_back(static_cast<float>(geo.v[t[k]][c]));
                const bool sheet = state["kinds"].value(p.key, std::string()) == "sheet" ||
                                   (!state["kinds"].contains(p.key) &&
                                    std::any_of(outParts.begin(), outParts.end(), [&](const nlohmann::json& o) { return o["key"] == p.key && o["kind"] == "sheet"; }));
                r["open"] = {open.size() / 6, sheet ? 0 : geo.openEdges.size()};
                for (const auto& o : outParts)
                    if (o["key"] == p.key)
                        r["sides"] = o["sides"];
                if (!sheet)
                    for (const auto& s : geo.openEdges)
                        for (const auto& v : s)
                            for (int c = 0; c < 3; ++c)
                                open.push_back(static_cast<float>(v[c]));
                r["bad"] = {bad.size() / 6, geo.badEdges.size()};
                for (const auto& s : geo.badEdges)
                    for (const auto& v : s)
                        for (int c = 0; c < 3; ++c)
                            bad.push_back(static_cast<float>(v[c]));
                ranges.push_back(r);
            }
        const std::string file = "g" + std::to_string(gi) + ".bin";
        {
            std::ofstream out(cacheDir / file, std::ios::binary);
            out.write(reinterpret_cast<const char*>(tris.data()), static_cast<std::streamsize>(tris.size() * sizeof(float)));
            out.write(reinterpret_cast<const char*>(open.data()), static_cast<std::streamsize>(open.size() * sizeof(float)));
            out.write(reinterpret_cast<const char*>(bad.data()), static_cast<std::streamsize>(bad.size() * sizeof(float)));
        }
        outGroups.push_back({{"name", groups[gi].name},
                             {"url", (fs::path("cache") / card.category / card.id / file).generic_u8string() + "?v=" + stamp},
                             {"tris", tris.size() / 9},
                             {"open", open.size() / 6},
                             {"bad", bad.size() / 6},
                             {"parts", ranges}});
    }

    if (commitCheck)
    {
        newState["checkedAt"] = stamp;
        fs::create_directories(stateDir, ec);
        std::ofstream(stateDir / "check.json") << newState.dump(1);
    }
    nlohmann::json j;
    j["type"] = "inspection";
    j["category"] = card.category;
    j["id"] = card.id;
    j["checked"] = everChecked || commitCheck;
    j["groups"] = outGroups;
    j["parts"] = outParts;
    j["missing"] = missing;
    j["summary"] = {{"parts", parts.size()}, {"ok", nOk}, {"error", nErr}, {"changed", nChanged}, {"new", nNew}, {"missing", missing.size()}};
    std::cerr << "[MeshEditor] inspect " << card.category << "/" << card.id << ": " << parts.size() << " parts, " << nErr << " with errors"
              << (commitCheck ? " (check saved)" : "") << std::endl;
    return j;
}

void Inspector::setKind(const fs::path& stateDir, const std::string& part, const std::string& kind)
{
    nlohmann::json state = readJson(stateDir / "check.json");
    if (!state.contains("kinds"))
        state["kinds"] = nlohmann::json::object();
    if (kind.empty())
        state["kinds"].erase(part);
    else
        state["kinds"][part] = kind;
    std::error_code ec;
    fs::create_directories(stateDir, ec);
    std::ofstream(stateDir / "check.json") << state.dump(1);
}
void Inspector::setCaps(const fs::path& stateDir, const std::string& part, bool on)
{
    nlohmann::json state = readJson(stateDir / "check.json");
    if (!state.contains("caps"))
        state["caps"] = nlohmann::json::object();
    if (on)
        state["caps"][part] = true;
    else
        state["caps"].erase(part);
    std::error_code ec;
    fs::create_directories(stateDir, ec);
    std::ofstream(stateDir / "check.json") << state.dump(1);
}

std::vector<RepairedGroup> Inspector::repaired(const ModelCard& card, const fs::path& stateDir)
{
    const std::vector<Group> groups = collectGroups(card);
    nlohmann::json state = readJson(stateDir / "check.json");
    if (!state.contains("caps"))
        state["caps"] = nlohmann::json::object();
    if (!state.contains("kinds"))
        state["kinds"] = nlohmann::json::object();
    const std::vector<PartInfo> parts = loadParts(groups, state);
    std::vector<RepairedGroup> out(groups.size());
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
    {
        out[gi].name = groups[gi].name;
        out[gi].lod = groups[gi].lod;
        out[gi].shell = !groups[gi].lod;
    }
    for (const auto& p : parts)
    {
        const std::string kind = state["kinds"].value(p.key, autoKindOf(p.geo.front().second));
        for (const auto& [gi, g] : p.geo)
        {
            RepairedPart r;
            r.key = p.key;
            r.kind = kind;
            r.sides = kind == "sheet" || g.boundary > 0 ? 2 : 1;
            for (const auto& v : g.v)
                r.v.push_back({static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2])});
            r.t = g.t;
            r.tm = g.tm;
            r.mtl = g.mtl;
            for (int k = 0; k < 3; ++k)
            {
                r.lo[k] = static_cast<float>(g.lo[k]);
                r.hi[k] = static_cast<float>(g.hi[k]);
            }
            for (const auto& e : g.openEdges)
                r.open.push_back({static_cast<float>(e[0][0]), static_cast<float>(e[0][1]), static_cast<float>(e[0][2]),
                                  static_cast<float>(e[1][0]), static_cast<float>(e[1][1]), static_cast<float>(e[1][2])});
            out[static_cast<std::size_t>(gi)].parts.push_back(std::move(r));
        }
    }
    return out;
}
} // namespace elite::mesh_editor
