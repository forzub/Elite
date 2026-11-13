#include "tools/mesh_editor/MeshEditorInspect.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
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
    bool material = false;
    std::uint64_t hash = 1469598103934665603ull;
};

RawMesh readObj(const fs::path& file)
{
    RawMesh m;
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
                m.t.push_back({poly[0], poly[k], poly[k + 1]});
        }
        else if (line.rfind("usemtl", 0) == 0 || line.rfind("mtllib", 0) == 0)
            m.material = true;
    }
    return m;
}

// one part in one group after the recipe
struct PartGeo
{
    std::vector<Vec3> v;
    std::vector<Tri> t;
    int rawVerts = 0, rawTris = 0;
    int welded = 0, degenerate = 0, repeated = 0, flipped = 0;
    std::vector<std::array<Vec3, 2>> openEdges, badEdges;
    int boundary = 0, nonManifold = 0, inconsistent = 0, edgeCount = 0;
    bool material = false;
    Vec3 lo {1e30, 1e30, 1e30}, hi {-1e30, -1e30, -1e30};
    std::uint64_t hash = 0;
};

PartGeo repair(const RawMesh& raw)
{
    PartGeo g;
    g.rawVerts = static_cast<int>(raw.v.size());
    g.rawTris = static_cast<int>(raw.t.size());
    g.material = raw.material;
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
    std::set<std::array<int, 3>> seen;
    for (const auto& t0 : raw.t)
    {
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
        if (!seen.insert(key).second)
        {
            ++g.repeated;
            continue;
        }
        g.t.push_back(t);
    }
    // edges
    struct Use
    {
        int count = 0, fwd = 0;
    };
    std::map<std::pair<int, int>, Use> edges;
    for (const auto& t : g.t)
        for (int k = 0; k < 3; ++k)
        {
            const int a = t[k], b = t[(k + 1) % 3];
            auto& u = edges[{std::min(a, b), std::max(a, b)}];
            ++u.count;
            u.fwd += a < b ? 1 : 0;
        }
    g.edgeCount = static_cast<int>(edges.size());
    for (const auto& [e, u] : edges)
    {
        const std::array<Vec3, 2> seg {g.v[e.first], g.v[e.second]};
        if (u.count == 1)
        {
            ++g.boundary;
            g.openEdges.push_back(seg);
        }
        else if (u.count > 2)
        {
            ++g.nonManifold;
            g.badEdges.push_back(seg);
        }
        else if (u.fwd != 1)
        {
            ++g.inconsistent;
            g.badEdges.push_back(seg);
        }
    }
    // a closed, consistently wound part that is inside out as a whole: turn it outward
    if (g.boundary == 0 && g.nonManifold == 0 && g.inconsistent == 0 && !g.t.empty())
    {
        double vol = 0.0;
        for (const auto& t : g.t)
            vol += dot(g.v[t[0]], cross(g.v[t[1]], g.v[t[2]]));
        if (vol < 0.0)
        {
            for (auto& t : g.t)
                std::swap(t[1], t[2]);
            g.flipped = 1;
        }
    }
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
} // namespace

nlohmann::json Inspector::inspect(const ModelCard& card, const fs::path& webRoot, const fs::path& stateDir, bool commitCheck)
{
    const std::vector<Group> groups = collectGroups(card);
    nlohmann::json state = readJson(stateDir / "check.json");
    if (!state.contains("kinds"))
        state["kinds"] = nlohmann::json::object();
    const bool everChecked = state.contains("parts");

    struct PartInfo
    {
        std::string key;
        std::vector<std::pair<int, PartGeo>> geo;   // group index, geometry
    };
    std::vector<PartInfo> parts;
    auto partOf = [&](const std::string& key) -> PartInfo& {
        for (auto& p : parts)
            if (p.key == key)
                return p;
        parts.push_back({key, {}});
        return parts.back();
    };
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
        for (const auto& [key, file] : groups[gi].files)
        {
            PartInfo& p = partOf(key);
            PartGeo g = repair(readObj(file));
            // two files with one key in a group (rare): merge into one part
            auto same = std::find_if(p.geo.begin(), p.geo.end(), [&](const auto& x) { return x.first == static_cast<int>(gi); });
            if (same == p.geo.end())
                p.geo.push_back({static_cast<int>(gi), std::move(g)});
            else
            {
                PartGeo& a = same->second;
                const int base = static_cast<int>(a.v.size());
                a.v.insert(a.v.end(), g.v.begin(), g.v.end());
                for (auto t : g.t)
                    a.t.push_back({t[0] + base, t[1] + base, t[2] + base});
                a.openEdges.insert(a.openEdges.end(), g.openEdges.begin(), g.openEdges.end());
                a.badEdges.insert(a.badEdges.end(), g.badEdges.begin(), g.badEdges.end());
                a.boundary += g.boundary;
                a.nonManifold += g.nonManifold;
                a.inconsistent += g.inconsistent;
                a.welded += g.welded;
                a.degenerate += g.degenerate;
                a.repeated += g.repeated;
                a.flipped += g.flipped;
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
        const bool openMuch = first.boundary >= 0.2 * std::max(first.edgeCount, 1);
        const std::string autoKind = first.boundary == 0 ? "volume" : (openMuch || minExt <= 0.05 * maxExt || minExt < 0.02 ? "sheet" : "volume");
        const std::string kind = state["kinds"].value(p.key, autoKind);

        nlohmann::json issues = nlohmann::json::array();
        nlohmann::json lods = nlohmann::json::object();
        nlohmann::json fixed = {{"welded", 0}, {"degenerate", 0}, {"repeated", 0}, {"flipped", 0}};
        bool error = false;
        std::uint64_t hash = 1469598103934665603ull;
        const bool several = p.geo.size() > 1;
        for (const auto& [gi, g] : p.geo)
        {
            const std::string at = several ? groups[gi].name + ": " : std::string();
            lods[groups[gi].name] = {{"tris", g.t.size()}, {"verts", g.v.size()}};
            fixed["welded"] = fixed["welded"].get<int>() + g.welded;
            fixed["degenerate"] = fixed["degenerate"].get<int>() + g.degenerate;
            fixed["repeated"] = fixed["repeated"].get<int>() + g.repeated;
            fixed["flipped"] = fixed["flipped"].get<int>() + g.flipped;
            hash = (hash ^ g.hash) * 1099511628211ull;
            if (kind == "volume" && g.boundary > 0)
            {
                issues.push_back({{"sev", "error"}, {"text", at + "дыры: " + std::to_string(g.boundary) + " открытых рёбер"}});
                error = true;
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
        const int fixedTotal = fixed["welded"].get<int>() + fixed["degenerate"].get<int>() + fixed["repeated"].get<int>() + fixed["flipped"].get<int>();

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
                            {"lods", lods},
                            {"size", {ext[0], ext[1], ext[2]}}});
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
} // namespace elite::mesh_editor
