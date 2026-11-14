#include "tools/mesh_editor/MeshEditorShell.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <thread>

namespace elite::mesh_editor
{
namespace fs = std::filesystem;

namespace
{
using V = std::array<double, 3>;

V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const V& a, const V& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V cross(const V& a, const V& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
V norm(V a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 0 ? V {a[0] / l, a[1] / l, a[2] / l} : V {0, 0, 1};
}
} // namespace

std::vector<ShellLink> Shell::rig(const fs::path& modelFolder, const RepairedGroup& lod)
{
    std::vector<ShellLink> links;
    std::ifstream in(modelFolder / "rig.json");
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    if (j.is_object() && j.contains("links") && j["links"].is_array())
        for (const auto& l : j["links"])
        {
            ShellLink s;
            s.id = l.value("id", std::string());
            if (s.id.empty())
                continue;
            s.parts = l.value("parts", std::vector<std::string> {});
            s.parent = l.value("parent", std::string());
            s.joint = l.value("joint", nlohmann::json());
            links.push_back(s);
        }
    if (links.empty())
    {
        ShellLink body;
        body.id = "body";
        for (const auto& p : lod.parts)
            body.parts.push_back(p.key);
        links.push_back(body);
    }
    return links;
}

std::vector<std::vector<char>> Shell::visible(const std::vector<const RepairedPart*>& parts, const std::vector<std::vector<char>>* clear,
                                              int directions, int resolution)
{
    struct Tri
    {
        V p[3];
        int part, index;
    };
    std::vector<Tri> tris;
    std::size_t firstClear = static_cast<std::size_t>(-1);
    V lo {1e30, 1e30, 1e30}, hi {-1e30, -1e30, -1e30};
    // opaque triangles first: when the see-through ones are tested, the depth buffer is final
    for (int pass = 0; pass < 2; ++pass)
    for (std::size_t pi = 0; pi < parts.size(); ++pi)
        for (std::size_t i = 0; i < parts[pi]->t.size(); ++i)
        {
            const bool isClear = clear && (*clear)[pi][i];
            if (isClear != (pass == 1))
                continue;
            if (isClear && firstClear == static_cast<std::size_t>(-1))
                firstClear = tris.size();
            Tri t;
            for (int k = 0; k < 3; ++k)
            {
                const auto& v = parts[pi]->v[static_cast<std::size_t>(parts[pi]->t[i][k])];
                t.p[k] = {v[0], v[1], v[2]};
                for (int c = 0; c < 3; ++c)
                {
                    lo[c] = std::min(lo[c], t.p[k][c]);
                    hi[c] = std::max(hi[c], t.p[k][c]);
                }
            }
            t.part = static_cast<int>(pi);
            t.index = static_cast<int>(i);
            tris.push_back(t);
        }
    std::vector<std::vector<char>> out(parts.size());
    for (std::size_t pi = 0; pi < parts.size(); ++pi)
        out[pi].assign(parts[pi]->t.size(), 0);
    if (tris.empty())
        return out;
    const V centre {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
    double radius = 0;
    for (const auto& t : tris)
        for (const auto& p : t.p)
            radius = std::max(radius, std::sqrt(dot(sub(p, centre), sub(p, centre))));
    radius = std::max(radius * 1.01, 1e-6);
    const int R = resolution;
    const double px = 2 * radius / R;
    const double eps = 0.5 * px;   // a sub-pixel triangle's centre on the nearest surface within half a pixel

    const unsigned nThreads = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
    std::vector<std::vector<char>> seen(nThreads, std::vector<char>(tris.size(), 0));
    auto work = [&](unsigned tid) {
        std::vector<float> depth(static_cast<std::size_t>(R) * R);
        std::vector<int> id(static_cast<std::size_t>(R) * R);
        std::vector<std::array<double, 9>> proj(tris.size());
        auto& vis = seen[tid];
        for (int k = static_cast<int>(tid); k < directions; k += static_cast<int>(nThreads))
        {
            // Fibonacci sphere: even directions all around
            const double z = 1 - 2 * (k + 0.5) / directions;
            const double a = k * M_PI * (3 - std::sqrt(5.0));
            const V d {std::sqrt(1 - z * z) * std::cos(a), std::sqrt(1 - z * z) * std::sin(a), z};
            const V u = norm(cross(d, std::fabs(d[0]) < 0.9 ? V {1, 0, 0} : V {0, 1, 0}));
            const V w = cross(d, u);
            std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::max());
            std::fill(id.begin(), id.end(), -1);
            for (std::size_t ti = 0; ti < tris.size(); ++ti)
            {
                auto& q = proj[ti];
                for (int c = 0; c < 3; ++c)
                {
                    const V r = sub(tris[ti].p[c], centre);
                    q[c * 3 + 0] = (dot(r, u) + radius) / px;
                    q[c * 3 + 1] = (dot(r, w) + radius) / px;
                    q[c * 3 + 2] = dot(r, d);
                }
                const double x0 = q[0], y0 = q[1], x1 = q[3], y1 = q[4], x2 = q[6], y2 = q[7];
                double area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
                if (std::fabs(area) < 1e-12)
                    continue;   // edge-on from here
                const int ix0 = std::max(0, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
                const int ix1 = std::min(R - 1, static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
                const int iy0 = std::max(0, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
                const int iy1 = std::min(R - 1, static_cast<int>(std::ceil(std::max({y0, y1, y2}))));
                if (ix0 > ix1 || iy0 > iy1)
                    continue;
                // edge functions e_i(x, y) = A_i x + B_i y + C_i (>= 0 inside after the sign fix), stepped per pixel;
                // depth is a plane z = zA x + zB y + zC
                const double s = area > 0 ? 1.0 : -1.0;
                area *= s;
                const double A0 = s * (y1 - y2), B0 = s * (x2 - x1), C0 = s * (x1 * y2 - x2 * y1);
                const double A1 = s * (y2 - y0), B1 = s * (x0 - x2), C1 = s * (x2 * y0 - x0 * y2);
                const double A2 = s * (y0 - y1), B2 = s * (x1 - x0), C2 = s * (x0 * y1 - x1 * y0);
                const double zA = (A0 * q[2] + A1 * q[5] + A2 * q[8]) / area;
                const double zB = (B0 * q[2] + B1 * q[5] + B2 * q[8]) / area;
                const double zC = (C0 * q[2] + C1 * q[5] + C2 * q[8]) / area;
                const double tol = -1e-9 * area;
                const double As[3] = {A0, A1, A2}, Bs[3] = {B0, B1, B2}, Cs[3] = {C0, C1, C2};
                for (int y = iy0; y <= iy1; ++y)
                {
                    // the row's covered span from the three half-planes (a thin diagonal triangle's box is mostly empty)
                    const double cy = y + 0.5;
                    double lo = ix0 + 0.5, hi = ix1 + 0.5;
                    for (int e = 0; e < 3 && lo <= hi; ++e)
                    {
                        const double r = Bs[e] * cy + Cs[e] - tol;   // A x + r >= 0
                        if (As[e] > 1e-12)
                            lo = std::max(lo, -r / As[e]);
                        else if (As[e] < -1e-12)
                            hi = std::min(hi, -r / As[e]);
                        else if (r < 0)
                            hi = lo - 1;
                    }
                    if (lo > hi)
                        continue;
                    const int jx0 = std::max(ix0, static_cast<int>(std::ceil(lo - 0.5)) - 1);
                    const int jx1 = std::min(ix1, static_cast<int>(std::floor(hi - 0.5)) + 1);
                    const double sy = cy, sx = jx0 + 0.5;
                    const bool seeThrough = ti >= firstClear;
                    double e0 = A0 * sx + B0 * sy + C0, e1 = A1 * sx + B1 * sy + C1, e2 = A2 * sx + B2 * sy + C2;
                    double zz = zA * sx + zB * sy + zC;
                    float* drow = depth.data() + static_cast<std::size_t>(y) * R;
                    int* irow = id.data() + static_cast<std::size_t>(y) * R;
                    for (int x = jx0; x <= jx1; ++x, e0 += A0, e1 += A1, e2 += A2, zz += zA)
                        if (e0 >= tol && e1 >= tol && e2 >= tol && static_cast<float>(zz) < drow[x])
                        {
                            if (seeThrough)
                            {
                                vis[ti] = 1;   // seen, but writes no depth
                                break;
                            }
                            drow[x] = static_cast<float>(zz);
                            irow[x] = static_cast<int>(ti);
                        }
                }
            }
            for (const int t : id)
                if (t >= 0)
                    vis[static_cast<std::size_t>(t)] = 1;
            // triangles smaller than a pixel: their centre on the nearest surface
            for (std::size_t ti = 0; ti < tris.size(); ++ti)
            {
                if (vis[ti])
                    continue;
                const auto& q = proj[ti];
                // only triangles too small to own a pixel; larger ones were rasterized fairly
                const double area = std::fabs((q[3] - q[0]) * (q[7] - q[1]) - (q[6] - q[0]) * (q[4] - q[1])) / 2;
                if (area > 1.0)
                    continue;
                const double cx = (q[0] + q[3] + q[6]) / 3, cy = (q[1] + q[4] + q[7]) / 3, cz = (q[2] + q[5] + q[8]) / 3;
                const int x = static_cast<int>(cx), y = static_cast<int>(cy);
                if (x < 0 || y < 0 || x >= R || y >= R)
                    continue;
                if (depth[static_cast<std::size_t>(y) * R + x] >= cz - eps)
                    vis[ti] = 1;
            }
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < nThreads; ++t)
        pool.emplace_back(work, t);
    for (auto& t : pool)
        t.join();
    for (std::size_t ti = 0; ti < tris.size(); ++ti)
        for (const auto& s : seen)
            if (s[ti])
            {
                out[static_cast<std::size_t>(tris[ti].part)][static_cast<std::size_t>(tris[ti].index)] = 1;
                break;
            }
    return out;
}

RepairedGroup Shell::build(const RepairedGroup& lod, const std::vector<ShellLink>& links, const MaterialOf& material,
                           const SeeThrough& seeThrough, ShellStats& stats)
{
    std::vector<const RepairedPart*> parts;
    for (const auto& p : lod.parts)
        parts.push_back(&p);
    std::map<std::string, std::string> materialCache;
    auto libraryId = [&](const RepairedPart& p, std::size_t i) -> const std::string& {
        const std::string src = p.tm[i] >= 0 ? p.mtl[static_cast<std::size_t>(p.tm[i])] : std::string();
        std::string& mid = materialCache[p.key + '\n' + src];   // the lookup copies the document: once per pair
        if (mid.empty())
            mid = material(p.key, src);
        return mid;
    };
    std::vector<std::vector<char>> clear(parts.size());
    bool anyClear = false;
    for (std::size_t pi = 0; pi < parts.size(); ++pi)
    {
        clear[pi].assign(parts[pi]->t.size(), 0);
        for (std::size_t i = 0; i < parts[pi]->t.size(); ++i)
            if (seeThrough && seeThrough(libraryId(*parts[pi], i)))
                anyClear = clear[pi][i] = 1;
    }
    const auto vis = visible(parts, anyClear ? &clear : nullptr);
    std::map<std::string, std::size_t> linkOf;
    for (std::size_t li = 0; li < links.size(); ++li)
        for (const auto& k : links[li].parts)
            linkOf[k] = li;
    RepairedGroup out;
    out.name = "скорлупа " + lod.name;
    out.lod = false;
    out.shell = true;
    out.parts.resize(links.size());
    std::vector<std::map<std::string, int>> mtlIndex(links.size());
    for (std::size_t li = 0; li < links.size(); ++li)
    {
        out.parts[li].key = links[li].id;
        out.parts[li].kind = "sheet";
        out.parts[li].sides = 2;
    }
    stats.group = lod.name;
    for (std::size_t pi = 0; pi < parts.size(); ++pi)
    {
        const RepairedPart& p = *parts[pi];
        const auto it = linkOf.find(p.key);
        const std::size_t li = it != linkOf.end() ? it->second : 0;   // an element no link names goes with the root
        RepairedPart& s = out.parts[li];
        std::vector<int> remap(p.v.size(), -1);
        stats.trisIn += p.t.size();
        for (std::size_t i = 0; i < p.t.size(); ++i)
        {
            if (!vis[pi][i])
                continue;
            std::array<int, 3> t {};
            for (int k = 0; k < 3; ++k)
            {
                int& r = remap[static_cast<std::size_t>(p.t[i][k])];
                if (r < 0)
                {
                    r = static_cast<int>(s.v.size());
                    s.v.push_back(p.v[static_cast<std::size_t>(p.t[i][k])]);
                }
                t[k] = r;
            }
            s.t.push_back(t);
            const std::string& mid = libraryId(p, i);
            auto m = mtlIndex[li].find(mid);
            if (m == mtlIndex[li].end())
            {
                m = mtlIndex[li].emplace(mid, static_cast<int>(s.mtl.size())).first;
                s.mtl.push_back(mid);
            }
            s.tm.push_back(m->second);
            ++stats.trisOut;
        }
    }
    for (auto& s : out.parts)
    {
        s.lo = {1e30f, 1e30f, 1e30f};
        s.hi = {-1e30f, -1e30f, -1e30f};
        for (const auto& v : s.v)
            for (int c = 0; c < 3; ++c)
            {
                s.lo[c] = std::min(s.lo[c], v[c]);
                s.hi[c] = std::max(s.hi[c], v[c]);
            }
    }
    return out;
}
} // namespace elite::mesh_editor
