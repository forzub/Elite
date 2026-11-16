#include "tools/mesh_editor/MeshEditorAssembly.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "tools/mesh_editor/MeshEditorCatalog.h"
#include "tools/ship_asset_assemble/ShipAssetAssemble.h"

namespace elite::mesh_editor
{
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
using V3 = std::array<double, 3>;

json readJson(const fs::path& p)
{
    std::ifstream in(p);
    if (!in)
        return json();
    json j = json::parse(in, nullptr, false);
    return j.is_discarded() ? json() : j;
}

void writeJson(const fs::path& p, const json& j)
{
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream(p, std::ios::binary) << j.dump(1) << "\n";
}

double r3(double x) { return std::round(x * 1000.0) / 1000.0; }
json arr(const V3& v) { return json::array({r3(v[0]), r3(v[1]), r3(v[2])}); }

std::string today()
{
    const std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&t));
    return buf;
}

bool has(const std::string& s, std::initializer_list<const char*> words)
{
    std::string l = s;
    std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const char* w : words)
        if (l.find(w) != std::string::npos)
            return true;
    return false;
}

// a part's role by its name (the author changes it in "Состав")
std::string roleOf(const std::string& key)
{
    if (has(key, {"frame", "spine", "shaft", "truss", "keel"}))
        return "frame";
    if (has(key, {"hull", "core"}))
        return "hull";
    if (has(key, {"panel", "plate", "cover", "armour", "armor", "skin"}))
        return "plate";
    if (has(key, {"cockpit", "bridge", "capsule"}))
        return "cockpit";
    if (has(key, {"service_reactor", "sr150", "s60", "s200"}))
        return "service_reactor";
    if (has(key, {"reactor"}))
        return "reactor";
    if (has(key, {"engine", "drive", "vector", "thruster", "rcs", "nozzle"}))
        return "engine";
    if (has(key, {"jumptank", "jump_core", "jump"}))
        return "jump";
    if (has(key, {"tank", "fuel"}))
        return "tank";
    if (has(key, {"laser", "gun", "turret", "miss", "launcher", "rack", "pd1", "vls"}))
        return "weapon";
    if (has(key, {"radar", "comm", "transponder", "sensor", "mast", "antenna"}))
        return "sensor";
    if (has(key, {"control", "computer", "mcc"}))
        return "control";
    if (has(key, {"coolant", "radiator"}))
        return "cooling";
    if (has(key, {"life", "o2", "habitat", "solar"}))
        return has(key, {"solar"}) ? "power_panel" : "life_support";
    if (has(key, {"drone", "pod", "boat"}))
        return "craft";
    if (has(key, {"c20", "pad", "collar", "skid"}))
        return "structure";
    return "equipment";
}

bool isStructure(const std::string& role) { return role == "frame" || role == "hull" || role == "structure"; }
bool isSystem(const std::string& role)
{
    return !isStructure(role) && role != "plate";
}

struct Box
{
    V3 lo {1e30, 1e30, 1e30}, hi {-1e30, -1e30, -1e30};
    bool empty() const { return lo[0] > hi[0]; }
    V3 centre() const { return {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2}; }
    V3 half() const { return {(hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2}; }
    double volume() const { return empty() ? 0 : (hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]); }
    void add(const V3& p)
    {
        for (int k = 0; k < 3; ++k)
        {
            lo[k] = std::min(lo[k], p[k]);
            hi[k] = std::max(hi[k], p[k]);
        }
    }
};

Box grow(Box b, double d)
{
    for (int k = 0; k < 3; ++k)
    {
        b.lo[k] -= d;
        b.hi[k] += d;
    }
    return b;
}

Box intersect(const Box& a, const Box& b)
{
    Box r;
    for (int k = 0; k < 3; ++k)
    {
        r.lo[k] = std::max(a.lo[k], b.lo[k]);
        r.hi[k] = std::min(a.hi[k], b.hi[k]);
    }
    return r;
}

double gap(const Box& a, const Box& b)
{
    double d2 = 0;
    for (int k = 0; k < 3; ++k)
    {
        const double d = std::max({0.0, a.lo[k] - b.hi[k], b.lo[k] - a.hi[k]});
        d2 += d * d;
    }
    return std::sqrt(d2);
}

// rotation (degrees, R = Rx*Ry*Rz as the assembler and three.js 'XYZ') that turns local X onto d
V3 alongX(V3 d)
{
    const double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l <= 0)
        return {0, 0, 0};
    for (auto& c : d)
        c /= l;
    const double a = std::asin(std::clamp(d[1], -1.0, 1.0));
    const double b = std::atan2(-d[2], d[0]);
    return {0, b * 180.0 / M_PI, a * 180.0 / M_PI};
}

// a LOD file named after its LOD0 part: "lod1_station_Core_Front", "hull_lod2", "hull.lod2" -> the LOD0 key
std::string baseKey(const std::string& key)
{
    std::string k = key;
    if (k.size() > 5 && k.rfind("lod", 0) == 0 && std::isdigit(static_cast<unsigned char>(k[3])) && k[4] == '_')
        k = k.substr(5);
    for (const char* sep : {"_lod", ".lod", "_LOD"})
    {
        const auto at = k.rfind(sep);
        if (at != std::string::npos && at + 4 < k.size() && std::isdigit(static_cast<unsigned char>(k[at + 4])))
            k = k.substr(0, at);
    }
    return k;
}

const RepairedGroup* firstLod(const std::vector<RepairedGroup>& groups)
{
    const RepairedGroup* best = nullptr;
    for (const auto& g : groups)
        if (g.lod && (!best || g.parts.size() > best->parts.size()))
            best = &g;
    return best ? best : (groups.empty() ? nullptr : &groups.front());
}

Box partBox(const RepairedPart& p)
{
    Box b;
    b.add({p.lo[0], p.lo[1], p.lo[2]});
    b.add({p.hi[0], p.hi[1], p.hi[2]});
    return b;
}

// automatic seed: one hit volume per node, seams where plates touch plates / the frame, mounts of systems to the
// structure, buses by role
void seedAuto(json& doc, const std::map<std::string, Box>& boxes, bool station)
{
    std::map<std::string, std::string> role;
    for (const auto& n : doc["nodes"])
        role[n["id"]] = n.value("role", "equipment");
    // hit volumes
    for (const auto& n : doc["nodes"])
    {
        const auto it = boxes.find(n["id"]);
        if (it == boxes.end() || it->second.empty())
            continue;
        V3 h = it->second.half();
        for (auto& c : h)
            c = std::max(c, 0.02);
        doc["hit_volumes"].push_back({{"id", n["id"].get<std::string>() + ".hit"}, {"node", n["id"]}, {"shape", "box"},
                                      {"position", arr(it->second.centre())}, {"rotation_deg", {0, 0, 0}}, {"half_size", arr(h)},
                                      {"source", "auto"}});
    }
    // seams ("колбаски") and mounts from contacts of bounding boxes
    std::vector<std::string> ids;
    for (const auto& [id, b] : boxes)
        ids.push_back(id);
    int seamN = 0, mountN = 0;
    for (std::size_t i = 0; i < ids.size(); ++i)
        for (std::size_t j = i + 1; j < ids.size(); ++j)
        {
            const std::string &a = ids[i], &b = ids[j];
            const std::string ra = role[a], rb = role[b];
            const bool plateJoint = (ra == "plate" && (rb == "plate" || isStructure(rb))) || (rb == "plate" && isStructure(ra));
            if (!plateJoint)
                continue;
            const Box x = intersect(grow(boxes.at(a), 0.02), grow(boxes.at(b), 0.02));
            if (x.empty())
                continue;
            V3 size {x.hi[0] - x.lo[0], x.hi[1] - x.lo[1], x.hi[2] - x.lo[2]};
            std::array<int, 3> ord {0, 1, 2};
            std::sort(ord.begin(), ord.end(), [&](int p, int q) { return size[p] < size[q]; });
            if (size[ord[1]] > 0.3 || size[ord[2]] < 0.2)
                continue;   // an overlap, not an edge to edge joint
            V3 dir {0, 0, 0};
            dir[ord[2]] = 1.0;
            const std::string plate = ra == "plate" ? a : b, other = plate == a ? b : a;
            doc["seams"].push_back({{"id", "seam_" + std::to_string(seamN++)}, {"a", other}, {"b", plate}, {"source", "auto"},
                                    {"proxies", json::array({{{"id", "seam_" + std::to_string(seamN - 1) + ".p"}, {"position", arr(x.centre())},
                                                              {"rotation_deg", arr(alongX(dir))},
                                                              {"half_size", arr({size[ord[2]] / 2, 0.04, 0.04})}}})}});
        }
    for (const auto& id : ids)
    {
        if (!isSystem(role[id]))
            continue;
        std::string best;
        double bestGap = 0.5;   // a system farther than 0.5 m from any structure is not mounted to it
        for (const auto& other : ids)
            if (other != id && isStructure(role[other]))
            {
                const double g = gap(boxes.at(id), boxes.at(other));
                if (g < bestGap)
                {
                    bestGap = g;
                    best = other;
                }
            }
        if (best.empty())
            continue;
        Box x = intersect(grow(boxes.at(id), 0.05), grow(boxes.at(best), 0.05));
        const V3 c = x.empty() ? boxes.at(id).centre() : x.centre();
        doc["mounts"].push_back({{"id", "mount_" + std::to_string(mountN++)}, {"a", best}, {"b", id}, {"kind", "equipment_mount"},
                                 {"position", arr(c)}, {"rotation_deg", {0, 0, 0}}, {"half_size", {0.08, 0.08, 0.08}}, {"source", "auto"}});
    }
    // buses: a reactor feeds the drives (propulsion) and the systems (service), a control unit commands, tanks feed
    auto nearest = [&](const std::string& from, const std::vector<std::string>& to) {
        std::string best;
        double d = 1e30;
        for (const auto& t : to)
        {
            const V3 a = boxes.at(from).centre(), b = boxes.at(t).centre();
            const double x = std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
            if (x < d)
            {
                d = x;
                best = t;
            }
        }
        return best;
    };
    std::vector<std::string> reactors, service, controls, tanks, coolers;
    for (const auto& id : ids)
    {
        const std::string& r = role[id];
        if (r == "reactor")
            reactors.push_back(id);
        if (r == "service_reactor")
            service.push_back(id);
        if (r == "control")
            controls.push_back(id);
        if (r == "tank")
            tanks.push_back(id);
        if (r == "cooling")
            coolers.push_back(id);
    }
    if (service.empty())
        service = reactors;
    int connN = 0;
    auto connect = [&](const std::string& from, const std::string& to, const char* bus) {
        if (from.empty() || from == to)
            return;
        doc["connections"].push_back({{"id", "c" + std::to_string(connN++)}, {"from", from}, {"to", to}, {"bus", bus}, {"source", "auto"}});
    };
    for (const auto& id : ids)
    {
        const std::string& r = role[id];
        if (r == "engine" && !reactors.empty())
            connect(nearest(id, reactors), id, "propulsion_power");
        if ((r == "weapon" || r == "sensor" || r == "control" || r == "life_support" || r == "cockpit" || r == "cooling" || r == "jump") &&
            !service.empty())
            connect(nearest(id, service), id, "service_power");
        if ((r == "engine" || r == "weapon" || r == "sensor") && !controls.empty())
            connect(nearest(id, controls), id, "data");
        if ((r == "engine" || r == "reactor") && !tanks.empty())
            connect(nearest(id, tanks), id, "fuel");
        if ((r == "reactor" || r == "service_reactor") && !coolers.empty())
            connect(nearest(id, coolers), id, "coolant");
    }
    // a station keeps traffic out of its hull: a red keep-out box around it (the author adds corridors and preferences)
    if (station)
    {
        Box all;
        for (const auto& [id, b] : boxes)
            if (!b.empty())
            {
                all.add(b.lo);
                all.add(b.hi);
            }
        if (!all.empty())
        {
            V3 h = all.half();
            for (auto& c : h)
                c *= 1.15;
            doc["zones"].push_back({{"id", "keep_out_hull"}, {"kind", "keep_out"}, {"shape", "box"}, {"position", arr(all.centre())},
                                    {"rotation_deg", {0, 0, 0}}, {"half_size", arr(h)}, {"source", "auto"}});
        }
    }
    for (const auto& n : doc["nodes"])
        if (role[n["id"]] == "plate")
            doc["damage"]["detachable"].push_back(n["id"]);
}

// the mock-up (tools/ships/ship_asset_source.py): nodes, instances, collision, seams / mounts, states are already there
void seedMockup(json& doc, const json& src, const Assembly::LinkResolver& links)
{
    std::map<std::string, json> place;   // node -> its LOD0 render instance
    for (const auto& rn : src["render_lods"][0]["nodes"])
        place[rn["node"]] = rn;
    for (const auto& n : src["nodes"])
    {
        const std::string id = n["id"];
        const auto it = place.find(id);
        if (it == place.end())
            continue;
        const std::string part = it->second["geometry"];
        json node = {{"id", id}, {"part", part}, {"parent", id == "hull" ? json() : json("hull")}, {"role", roleOf(part)},
                     {"module", n.value("module", std::string())}, {"position", it->second["position"]},
                     {"rotation_deg", it->second["rotation_deg"]}};
        const std::string ref = links(part);
        if (!ref.empty())
            node["ref"] = ref;
        doc["nodes"].push_back(node);
    }
    for (const auto& c : src.value("collision", json::array()))
    {
        json h = c;
        h["source"] = "mock-up";
        doc["hit_volumes"].push_back(h);
    }
    for (const auto& l : src.value("structural_links", json::array()))
    {
        json x = l;
        x["source"] = "mock-up";
        if (l.value("kind", std::string()) == "weld_seam")
            doc["seams"].push_back(x);
        else
        {
            // a mount link: the proxy boxes are its mount points (a link without points stays, e.g. a pad fixed to its reactor)
            if (l["proxies"].empty())
                doc["mounts"].push_back({{"id", l["id"]}, {"a", l["a"]}, {"b", l["b"]}, {"kind", l["kind"]}, {"link", l["id"]},
                                         {"load_bearing", l.value("load_bearing", true)}, {"position", nullptr}, {"source", "mock-up"}});
            for (const auto& p : l["proxies"])
                doc["mounts"].push_back({{"id", p["id"]}, {"a", l["a"]}, {"b", l["b"]}, {"kind", l["kind"]}, {"link", l["id"]},
                                         {"load_bearing", l.value("load_bearing", true)}, {"position", p["position"]},
                                         {"rotation_deg", p["rotation_deg"]}, {"half_size", p["half_size"]}, {"source", "mock-up"}});
        }
    }
    for (const auto& s : src.value("states", json::array()))
        doc["damage"]["states"].push_back(s);
    for (const auto& r : src.value("release_rules", json::object()).value("plates", json::array()))
        doc["damage"]["detachable"].push_back(r.value("plate", std::string()));
    if (src.contains("object_frame"))
        doc["object_frame"] = src["object_frame"];
    for (const auto& [name, kd] : src["materials"].items())
        doc["materials"][name] = name;
}

// smooth normals inside a crease angle (35 deg), hard across it
std::vector<std::array<float, 3>> cornerNormals(const RepairedPart& p)
{
    std::vector<std::array<double, 3>> fn(p.t.size());
    std::vector<std::vector<int>> at(p.v.size());
    for (std::size_t i = 0; i < p.t.size(); ++i)
    {
        const auto& t = p.t[i];
        const auto& a = p.v[t[0]];
        const auto& b = p.v[t[1]];
        const auto& c = p.v[t[2]];
        const std::array<double, 3> u {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        std::array<double, 3> n {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
        const double l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (l > 0)
            for (auto& x : n)
                x /= l;
        fn[i] = n;
        for (int k = 0; k < 3; ++k)
            at[t[k]].push_back(static_cast<int>(i));
    }
    const double crease = std::cos(35.0 * M_PI / 180.0);
    std::vector<std::array<float, 3>> out;
    for (std::size_t i = 0; i < p.t.size(); ++i)
        for (int k = 0; k < 3; ++k)
        {
            std::array<double, 3> s {0, 0, 0};
            for (int f : at[p.t[i][k]])
                if (fn[f][0] * fn[i][0] + fn[f][1] * fn[i][1] + fn[f][2] * fn[i][2] >= crease)
                    for (int c = 0; c < 3; ++c)
                        s[c] += fn[f][c];
            const double l = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
            out.push_back(l > 0 ? std::array<float, 3> {static_cast<float>(s[0] / l), static_cast<float>(s[1] / l), static_cast<float>(s[2] / l)}
                                : std::array<float, 3> {0, 1, 0});
        }
    return out;
}

// the library id of a source material
std::string libraryId(const json& doc, const json& library, const std::string& partKey, const std::string& source)
{
    const std::string own = doc["parts"].value(partKey, json::object()).value("material", std::string());
    if (!own.empty())
        return own;
    const std::string mapped = doc.value("materials", json::object()).value(source, std::string());
    if (!mapped.empty())
        return mapped;
    for (const auto& m : library["materials"])
        if (m.value("id", std::string()) == source)
            return source;
    return "default";
}

// one OBJ: the parts given (merged), materials as library ids
void writeObj(const fs::path& file, const std::vector<const RepairedPart*>& parts, const json& doc, const json& library,
              std::set<std::string>& used)
{
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    out << "# mesh editor build (MESH_EDITOR_SPEC 16): repaired, wound outward, library materials\n";
    std::size_t base = 0, nbase = 0;
    for (const RepairedPart* p : parts)
    {
        for (const auto& v : p->v)
            out << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";
        const auto normals = cornerNormals(*p);
        for (const auto& n : normals)
            out << "vn " << n[0] << " " << n[1] << " " << n[2] << "\n";
        std::string current = "\x01";
        for (std::size_t i = 0; i < p->t.size(); ++i)
        {
            const std::string src = p->tm[i] >= 0 ? p->mtl[static_cast<std::size_t>(p->tm[i])] : std::string();
            const std::string id = libraryId(doc, library, p->key, src);
            if (id != current)
            {
                out << "usemtl " << id << "\n";
                current = id;
                used.insert(id);
            }
            out << "f";
            for (int k = 0; k < 3; ++k)
                out << " " << base + static_cast<std::size_t>(p->t[i][k]) + 1 << "//" << nbase + i * 3 + static_cast<std::size_t>(k) + 1;
            out << "\n";
        }
        base += p->v.size();
        nbase += normals.size();
    }
}

json materialJson(const json& library, const std::string& id)
{
    for (const auto& m : library["materials"])
        if (m.value("id", std::string()) == id)
            return {{"baseColor", m["baseColor"]}, {"metallic", m.value("metallic", 0.0)}, {"roughness", m.value("roughness", 0.65)},
                    {"emissive", m.value("emissive", json::array({0, 0, 0}))}, {"twoSided", m.value("twoSided", false)}};
    return {{"baseColor", {0.6, 0.62, 0.65}}, {"metallic", 0.1}, {"roughness", 0.7}};
}

json proxyList(const json& proxies)
{
    json out = json::array();
    for (const auto& p : proxies)
        out.push_back({{"id", p["id"]}, {"position", p["position"]}, {"rotation_deg", p.value("rotation_deg", json::array({0, 0, 0}))},
                       {"half_size", p["half_size"]}});
    return out;
}
} // namespace

fs::path Assembly::file(const ModelCard& card) { return card.folder / "assembly.json"; }

fs::path Assembly::libraryFile(const fs::path& sourceTree) { return sourceTree / "assets/materials/material_library.json"; }

std::string Assembly::materialId(const json& doc, const json& library, const std::string& partKey, const std::string& source)
{
    json d = doc.is_object() ? doc : json::object();
    if (!d.contains("parts"))
        d["parts"] = json::object();
    return libraryId(d, library, partKey, source);
}

std::vector<std::array<float, 3>> Assembly::cornerNormals(const RepairedPart& p) { return elite::mesh_editor::cornerNormals(p); }

void Assembly::writeMeshObj(const fs::path& file, const std::vector<const RepairedPart*>& parts, const json& library)
{
    std::set<std::string> used;
    writeObj(file, parts, json {{"parts", json::object()}, {"materials", json::object()}}, library, used);
}

json Assembly::loadLibrary(const fs::path& sourceTree)
{
    json j = readJson(libraryFile(sourceTree));
    if (!j.is_object() || !j.contains("materials"))
        j = {{"schema", "elite.material_library.v1"}, {"materials", json::array()}};
    return j;
}

void Assembly::saveLibrary(const fs::path& sourceTree, const json& lib) { writeJson(libraryFile(sourceTree), lib); }

json Assembly::load(const ModelCard& card, const std::vector<RepairedGroup>& groups, const LinkResolver& links, const json& seed)
{
    json doc = readJson(file(card));
    const RepairedGroup* g0 = firstLod(groups);
    std::map<std::string, Box> partBoxes;
    if (g0)
        for (const auto& p : g0->parts)
            partBoxes[p.key] = partBox(p);
    const bool fresh = !doc.is_object() || doc.value("schema", std::string()) != "elite.assembly.v1";
    if (fresh)
    {
        doc = {{"schema", "elite.assembly.v1"},
               {"model", card.category + "/" + card.id},
               {"seeded", true},
               {"seededFrom", json::array()},
               {"parts", json::object()},
               {"nodes", json::array()},
               {"materials", json::object()},
               {"hit_volumes", json::array()},
               {"zones", json::array()},
               {"mounts", json::array()},
               {"seams", json::array()},
               {"connections", json::array()},
               {"damage", {{"states", json::array()}, {"detachable", json::array()}}}};
        const json mock = readJson(card.folder / "source" / "ship_asset.json");
        if (seed.is_object() && seed.contains("calibration"))
        {
            doc["calibration"] = seed["calibration"];
            doc["seededFrom"].push_back(seed["calibration"].value("from", std::string("calibration")));
        }
        if (seed.is_object() && seed.contains("nodes"))
        {
            // a Blender scene (MeshEditorGltf): the author's hierarchy, links, volumes, zones, mounts, seams, buses
            for (json n : seed["nodes"])
            {
                if (!n.contains("role"))
                    n["role"] = n.contains("ref") ? roleOf(n["ref"].get<std::string>()) : roleOf(n["id"].get<std::string>());
                if (n.contains("material"))
                {
                    doc["parts"][n["part"].get<std::string>()]["material"] = n["material"];
                    n.erase("material");
                }
                doc["nodes"].push_back(n);
            }
            for (const char* k : {"hit_volumes", "zones", "mounts", "seams", "connections"})
                for (const auto& x : seed.value(k, json::array()))
                    doc[k].push_back(x);
            const json seedMaterials = seed.value("materials", json::object());
            for (const auto& [m, id] : seedMaterials.items())
                doc["materials"][m] = id;
            doc["seededFrom"].push_back("blender glTF");
        }
        else if (mock.is_object() && mock.contains("render_lods"))
        {
            seedMockup(doc, mock, links);
            doc["seededFrom"].push_back("source/ship_asset.json");
        }
        else
        {
            for (const auto& [key, b] : partBoxes)
            {
                json node = {{"id", key}, {"part", key}, {"parent", json()}, {"role", roleOf(key)}, {"position", {0, 0, 0}}, {"rotation_deg", {0, 0, 0}}};
                const std::string ref = links(key);
                if (!ref.empty())
                    node["ref"] = ref;
                doc["nodes"].push_back(node);
            }
            // the hierarchy: the biggest structure part is the root, everything else hangs on it
            std::string root;
            double vol = -1;
            for (const auto& n : doc["nodes"])
                if (isStructure(n["role"]) && partBoxes[n["id"]].volume() > vol)
                {
                    vol = partBoxes[n["id"]].volume();
                    root = n["id"];
                }
            for (auto& n : doc["nodes"])
                if (!root.empty() && n["id"] != root)
                    n["parent"] = root;
            const json hv = readJson(card.folder / "hit_volumes.json");
            if (hv.is_object() && hv.contains("volumes"))
            {
                for (const auto& v : hv["volumes"])
                {
                    json h = v;
                    h["node"] = v.value("part", std::string());
                    h["source"] = "hit_volumes.json";
                    doc["hit_volumes"].push_back(h);
                }
                doc["seededFrom"].push_back("hit_volumes.json");
                std::map<std::string, Box> none;
                json saved = doc["hit_volumes"];
                seedAuto(doc, partBoxes, false);
                doc["hit_volumes"] = saved;   // the generator's volumes, not boxes per part
            }
            else
                seedAuto(doc, partBoxes, card.category == "stations");
            doc["seededFrom"].push_back("auto");
        }
        // every source material name gets a library id: itself when the library has it, else default
        if (g0)
            for (const auto& p : g0->parts)
                for (const auto& m : p.mtl)
                    if (!doc["materials"].contains(m))
                        doc["materials"][m] = m;
        doc["seededAt"] = today();
    }
    // parts: refreshed from the meshes (kind, box, materials); the author's role / material stay
    for (const auto& [key, b] : partBoxes)
    {
        json& p = doc["parts"][key];
        if (!p.is_object())
            p = json::object();
        p["centre"] = arr(b.centre());
        p["size"] = arr({b.hi[0] - b.lo[0], b.hi[1] - b.lo[1], b.hi[2] - b.lo[2]});
        for (const auto& rp : g0->parts)
            if (rp.key == key)
            {
                p["kind"] = rp.kind;
                p["sourceMaterials"] = rp.mtl;
            }
        if (!p.contains("material"))
            p["material"] = "";
    }
    return doc;
}

void Assembly::save(const ModelCard& card, json doc)
{
    doc.erase("seeded");
    doc["savedAt"] = today();
    writeJson(file(card), doc);
}

json Assembly::build(const ModelCard& card, const json& doc, const std::vector<RepairedGroup>& groups, const fs::path& outputRoot,
                     const json& library, const LinkGeometry& linkGeometry)
{
    const fs::path out = outputRoot / card.category / card.id;
    const fs::path geo = out / "geo";
    std::error_code ec;
    fs::remove_all(geo, ec);   // only this model's own build folder
    fs::create_directories(geo, ec);
    std::set<std::string> used;
    json files = json::array();
    json src;
    const json mock = readJson(card.folder / "source" / "ship_asset.json");
    std::map<std::string, std::string> refOfPart;
    for (const auto& n : doc["nodes"])
        if (n.contains("ref"))
            refOfPart[n.value("part", std::string())] = n["ref"];
    std::map<std::string, std::vector<RepairedGroup>> libCache;
    auto libGroups = [&](const std::string& ref) -> const std::vector<RepairedGroup>& {
        auto it = libCache.find(ref);
        if (it == libCache.end())
            it = libCache.emplace(ref, linkGeometry(ref)).first;
        return it->second;
    };
    // the mesh of a part in LOD level L: a library model's own repaired LOD (merged) for a link, else the part itself
    auto writePart = [&](const std::string& key, int level, bool shell, const std::string& objName) -> bool {
        std::vector<const RepairedPart*> parts;
        const auto ref = refOfPart.find(key);
        if (ref != refOfPart.end())
        {
            const auto& lg = libGroups(ref->second);
            std::vector<const RepairedGroup*> lods;
            for (const auto& g : lg)
                if (g.lod)
                    lods.push_back(&g);
            // the mock-up's rule (ship_asset_source.py): component LODs 0..2 use the module's LOD1..3, the shell LOD3
            const int want = shell ? 3 : level + 1;
            const int pick = lods.empty() ? -1 : std::min(want, static_cast<int>(lods.size()) - 1);
            if (pick >= 0)
                for (const auto& p : lods[static_cast<std::size_t>(pick)]->parts)
                    parts.push_back(&p);
        }
        if (parts.empty())
        {
            std::string name = shell ? std::string() : "LOD" + std::to_string(level);
            for (const auto& g : groups)
                if ((shell && g.shell) || (!shell && g.name == name))
                    for (const auto& p : g.parts)
                        if (p.key == key || baseKey(p.key) == key || (shell && p.key.substr(p.key.find('/') + 1) == key))
                            parts.push_back(&p);
        }
        if (parts.empty())
            return false;
        writeObj(geo / objName, parts, doc, library, used);
        return true;
    };

    if (mock.is_object() && mock.contains("render_lods"))
    {
        // the mock-up: its own schema stays the base; meshes are rebuilt, the editable lists replaced
        src = mock;
        for (auto& lod : src["render_lods"])
        {
            const int level = lod["level"];
            const bool shell = lod.value("role", std::string()) == "shell";
            for (auto& g : lod["geometries"])
            {
                const std::string key = g["id"];
                const std::string obj = key + ".lod" + std::to_string(level) + ".obj";
                if (!writePart(key, shell ? 3 : level, false, obj))
                {
                    // nothing repaired for it (should not happen): keep the source file
                    fs::copy_file(card.folder / "source" / g["obj"].get<std::string>(), geo / obj, fs::copy_options::overwrite_existing, ec);
                }
                g["obj"] = "geo/" + obj;
                const json part = doc["parts"].value(key, json::object());
                g["two_sided"] = part.value("kind", std::string()) == "sheet";
            }
        }
    }
    else
    {
        src = {{"schema", "elite.ship_asset_source.v1"}, {"asset_id", card.id}, {"display_name", card.title},
               {"generator", "tools/mesh_editor (MeshEditorAssembly)"}, {"frame", "source frame, metres"}};
        json nodes = json::array();
        bool needWhole = false;
        for (const auto& n : doc["nodes"])
            nodes.push_back({{"id", n["id"]}, {"module", n.value("module", n["id"].get<std::string>())}});
        json lods = json::array();
        int level = 0;
        for (const auto& g : groups)
        {
            if (g.parts.empty())
                continue;
            json lod;
            const bool whole = g.lod && g.parts.size() == 1 && level > 0;
            lod["level"] = level;
            lod["role"] = g.shell ? "shell" : whole ? "proxy" : "component";
            lod["geometries"] = json::array();
            lod["nodes"] = json::array();
            if (g.shell || whole)
            {
                // one mesh for the whole model (a far proxy, a traffic shell): a "whole" node carries it
                needWhole = true;
                const std::string id = (g.shell ? "shell_" : "whole_") + std::to_string(level);
                std::vector<const RepairedPart*> parts;
                for (const auto& p : g.parts)
                    parts.push_back(&p);
                writeObj(geo / (id + ".obj"), parts, doc, library, used);
                lod["geometries"].push_back({{"id", id}, {"obj", "geo/" + id + ".obj"}, {"two_sided", true}});
                lod["nodes"].push_back({{"id", id}, {"geometry", id}, {"node", "whole"}, {"position", {0, 0, 0}}, {"rotation_deg", {0, 0, 0}}});
            }
            else
            {
                std::set<std::string> written;
                for (const auto& n : doc["nodes"])
                {
                    const std::string key = n.value("part", n["id"].get<std::string>());
                    const std::string gid = key + ".lod" + std::to_string(level);
                    if (!written.count(gid))
                    {
                        if (!writePart(key, level, false, gid + ".obj"))
                            continue;
                        written.insert(gid);
                        const json part = doc["parts"].value(key, json::object());
                        lod["geometries"].push_back({{"id", gid}, {"obj", "geo/" + gid + ".obj"}, {"two_sided", part.value("kind", std::string()) == "sheet"}});
                    }
                    lod["nodes"].push_back({{"id", n["id"].get<std::string>() + "@" + std::to_string(level)}, {"geometry", gid}, {"node", n["id"]},
                                            {"position", n.value("position", json::array({0, 0, 0}))},
                                            {"rotation_deg", n.value("rotation_deg", json::array({0, 0, 0}))}});
                }
                // parts of this LOD that no node is (a coarser LOD merging several LOD0 parts): carried by "whole"
                std::set<std::string> nodeParts;
                for (const auto& n : doc["nodes"])
                    nodeParts.insert(n.value("part", n["id"].get<std::string>()));
                for (const auto& p : g.parts)
                    if (!nodeParts.count(p.key) && !nodeParts.count(baseKey(p.key)))
                    {
                        needWhole = true;
                        const std::string gid = "whole." + p.key + ".lod" + std::to_string(level);
                        writeObj(geo / (gid + ".obj"), {&p}, doc, library, used);
                        lod["geometries"].push_back({{"id", gid}, {"obj", "geo/" + gid + ".obj"}, {"two_sided", p.sides == 2}});
                        lod["nodes"].push_back({{"id", gid}, {"geometry", gid}, {"node", "whole"}, {"position", {0, 0, 0}}, {"rotation_deg", {0, 0, 0}}});
                    }
            }
            if (!lod["nodes"].empty())
            {
                lods.push_back(lod);
                ++level;
            }
        }
        if (needWhole)
            nodes.push_back({{"id", "whole"}, {"module", "whole"}});
        src["nodes"] = nodes;
        src["render_lods"] = lods;
        if (doc.contains("object_frame"))
            src["object_frame"] = doc["object_frame"];
    }
    if (doc.contains("calibration") && doc["calibration"].is_object() && doc["calibration"].value("targetMeters", 0.0) > 0.0)
        src["calibration"] = doc["calibration"];
    // the editable lists (both paths)
    std::set<std::string> nodeIds;
    for (const auto& n : src["nodes"])
        nodeIds.insert(n["id"]);
    json collision = json::array();
    for (const auto& h : doc["hit_volumes"])
    {
        json c = {{"id", h["id"]}, {"shape", h.value("shape", std::string("box"))}, {"position", h["position"]},
                  {"rotation_deg", h.value("rotation_deg", json::array({0, 0, 0}))}, {"active_states", h.value("active_states", json::array())},
                  {"module", h.value("module", h.value("node", std::string()))}};
        if (nodeIds.count(h.value("node", std::string())))
            c["node"] = h["node"];
        if (c["shape"] == "cylinder")
        {
            c["radius"] = h["radius"];
            c["half_height"] = h["half_height"];
            if (h.contains("radius_top"))
                c["radius_top"] = h["radius_top"];
        }
        else
        {
            c["half_size"] = h["half_size"];
            if (h.contains("hex_top"))
                c["hex_top"] = h["hex_top"];
        }
        collision.push_back(c);
    }
    src["collision"] = collision;
    json links = json::array();
    for (const auto& s : doc["seams"])
        if (nodeIds.count(s.value("a", std::string())) && nodeIds.count(s.value("b", std::string())))
            links.push_back({{"id", s["id"]}, {"a", s["a"]}, {"b", s["b"]}, {"kind", "weld_seam"}, {"load_bearing", s.value("load_bearing", false)},
                             {"proxies", proxyList(s["proxies"])}});
    std::map<std::string, json> mountLinks;   // mounts grouped by their link (a mock-up mount link has 4 points)
    for (const auto& m : doc["mounts"])
    {
        if (!nodeIds.count(m.value("a", std::string())) || !nodeIds.count(m.value("b", std::string())))
            continue;
        const std::string lid = m.value("link", m["id"].get<std::string>());
        json& l = mountLinks[lid];
        if (l.is_null())
            l = {{"id", lid}, {"a", m["a"]}, {"b", m["b"]}, {"kind", m.value("kind", std::string("equipment_mount"))},
                 {"load_bearing", m.value("load_bearing", true)}, {"proxies", json::array()}};
        if (m.contains("position") && m["position"].is_array())
            l["proxies"].push_back({{"id", m["id"]}, {"position", m["position"]}, {"rotation_deg", m.value("rotation_deg", json::array({0, 0, 0}))},
                                    {"half_size", m["half_size"]}});
    }
    for (auto& [id, l] : mountLinks)
        links.push_back(l);
    src["structural_links"] = links;
    json states = json::array();
    for (const auto& s : doc["damage"].value("states", json::array()))
        if (nodeIds.count(s.value("node", std::string())))
            states.push_back(s);
    src["states"] = states;
    json sockets = json::array();
    for (const auto& n : doc["nodes"])
        if (n.contains("ref"))
            sockets.push_back({{"id", "link." + n["id"].get<std::string>()}, {"kind", "link"}, {"profile", n["ref"]}, {"node", n["id"]},
                               {"position", n.value("position", json::array({0, 0, 0}))}, {"rotation_deg", n.value("rotation_deg", json::array({0, 0, 0}))}});
    for (const auto& z : doc["zones"])
        sockets.push_back({{"id", z["id"]}, {"kind", "nav_zone:" + z.value("kind", std::string("keep_out"))}, {"position", z["position"]},
                           {"rotation_deg", z.value("rotation_deg", json::array({0, 0, 0}))}, {"extent", z["half_size"]}});
    src["sockets"] = sockets;
    // materials: the library's values for every id the meshes use
    json mats = json::object();
    for (const auto& id : used)
        mats[id] = materialJson(library, id);
    src["materials"] = mats;
    writeJson(out / "ship_asset.json", src);

    std::string report;
    const std::string elmodel = (out / (card.id + ".elmodel")).generic_u8string();
    const bool ok = elite::ship_asset::assembleShipAsset(out.generic_u8string(), elmodel, std::string(), report);
    // what .elmodel v4 has no chunk for (links keep their reference, buses, zones, mounts): beside it
    json side = {{"schema", "elite.assembly_sidecar.v1"},
                 {"model", card.category + "/" + card.id},
                 {"builtAt", today()},
                 {"links", json::array()},
                 {"connections", doc["connections"]},
                 {"zones", doc["zones"]},
                 {"mounts", doc["mounts"]},
                 {"seams", doc["seams"].size()},
                 {"detachable", doc["damage"].value("detachable", json::array())},
                 {"materials", json::array()}};
    for (const auto& n : doc["nodes"])
        if (n.contains("ref"))
            side["links"].push_back({{"node", n["id"]}, {"ref", n["ref"]}, {"position", n.value("position", json::array({0, 0, 0}))},
                                     {"rotation_deg", n.value("rotation_deg", json::array({0, 0, 0}))}});
    for (const auto& id : used)
        side["materials"].push_back(id);
    writeJson(out / (card.id + ".elassembly.json"), side);
    for (const auto& e : fs::directory_iterator(out, ec))
        if (e.is_regular_file())
            files.push_back({{"name", e.path().filename().u8string()}, {"bytes", e.file_size()}});
    std::cerr << "[MeshEditor] build " << card.category << "/" << card.id << ": " << report << std::endl;
    return {{"type", "built"}, {"category", card.category}, {"id", card.id}, {"ok", ok}, {"report", report}, {"output", out.generic_u8string()},
            {"files", files}, {"materials", used.size()}};
}
} // namespace elite::mesh_editor
