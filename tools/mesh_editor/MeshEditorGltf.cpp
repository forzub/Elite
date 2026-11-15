#include "tools/mesh_editor/MeshEditorGltf.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "tools/mesh_editor/MeshEditorCatalog.h"

namespace elite::mesh_editor
{
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
using M4 = std::array<double, 16>;   // column-major like glTF

M4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

M4 mul(const M4& a, const M4& b)
{
    M4 r {};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
        {
            double s = 0;
            for (int k = 0; k < 4; ++k)
                s += a[k * 4 + rr] * b[c * 4 + k];
            r[c * 4 + rr] = s;
        }
    return r;
}

M4 localMatrix(const json& n)
{
    if (n.contains("matrix"))
    {
        M4 m {};
        for (int i = 0; i < 16; ++i)
            m[i] = n["matrix"][i].get<double>();
        return m;
    }
    const auto t = n.value("translation", std::vector<double> {0, 0, 0});
    const auto q = n.value("rotation", std::vector<double> {0, 0, 0, 1});
    const auto s = n.value("scale", std::vector<double> {1, 1, 1});
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const double r[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
                         2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                         2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};   // columns
    M4 m {};
    for (int c = 0; c < 3; ++c)
        for (int rr = 0; rr < 3; ++rr)
            m[c * 4 + rr] = r[c * 3 + rr] * s[static_cast<std::size_t>(c)];
    m[12] = t[0];
    m[13] = t[1];
    m[14] = t[2];
    m[15] = 1;
    return m;
}

std::array<double, 3> apply(const M4& m, const std::array<double, 3>& p)
{
    return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
            m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}

double r3(double x) { return std::round(x * 10000.0) / 10000.0; }

// position, Euler XYZ degrees (R = Rx*Ry*Rz, the assembler's and three.js 'XYZ'), scale
void decompose(const M4& m, json& pos, json& rot, json& scale)
{
    double s[3];
    for (int c = 0; c < 3; ++c)
        s[c] = std::sqrt(m[c * 4] * m[c * 4] + m[c * 4 + 1] * m[c * 4 + 1] + m[c * 4 + 2] * m[c * 4 + 2]);
    auto R = [&](int row, int col) { return s[col] > 0 ? m[col * 4 + row] / s[col] : 0.0; };   // 0-based
    const double m13 = std::clamp(R(0, 2), -1.0, 1.0);
    double x, y = std::asin(m13), z;
    if (std::abs(m13) < 0.9999999)
    {
        x = std::atan2(-R(1, 2), R(2, 2));
        z = std::atan2(-R(0, 1), R(0, 0));
    }
    else
    {
        x = std::atan2(R(2, 1), R(1, 1));
        z = 0;
    }
    const double d = 180.0 / M_PI;
    pos = {r3(m[12]), r3(m[13]), r3(m[14])};
    rot = {r3(x * d), r3(y * d), r3(z * d)};
    scale = {r3(s[0]), r3(s[1]), r3(s[2])};
}

struct Buffers
{
    std::vector<std::vector<unsigned char>> data;
};

std::vector<unsigned char> decodeBase64(const std::string& in)
{
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : in)
    {
        const auto p = chars.find(static_cast<char>(c));
        if (p == std::string::npos)
            break;
        val = (val << 6) + static_cast<int>(p);
        bits += 6;
        if (bits >= 0)
        {
            out.push_back(static_cast<unsigned char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// the elements of an accessor as doubles (count x components)
std::vector<double> readAccessor(const json& g, const Buffers& b, int index, int& components)
{
    const json& a = g["accessors"][static_cast<std::size_t>(index)];
    const json& v = g["bufferViews"][a.value("bufferView", 0)];
    const auto& buf = b.data[v.value("buffer", 0)];
    const std::string type = a["type"];
    components = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : 4;
    const int ct = a["componentType"];
    const int size = ct == 5126 || ct == 5125 ? 4 : ct == 5123 || ct == 5122 ? 2 : 1;
    const std::size_t stride = v.value("byteStride", static_cast<std::size_t>(size * components));
    const std::size_t base = v.value("byteOffset", static_cast<std::size_t>(0)) + a.value("byteOffset", static_cast<std::size_t>(0));
    const std::size_t count = a["count"];
    std::vector<double> out;
    out.reserve(count * static_cast<std::size_t>(components));
    for (std::size_t i = 0; i < count; ++i)
        for (int c = 0; c < components; ++c)
        {
            const std::size_t at = base + i * stride + static_cast<std::size_t>(c * size);
            if (at + static_cast<std::size_t>(size) > buf.size())
                return out;
            const unsigned char* p = buf.data() + at;
            double x = 0;
            if (ct == 5126)
            {
                float f;
                std::memcpy(&f, p, 4);
                x = f;
            }
            else if (ct == 5125)
            {
                std::uint32_t u;
                std::memcpy(&u, p, 4);
                x = u;
            }
            else if (ct == 5123)
            {
                std::uint16_t u;
                std::memcpy(&u, p, 2);
                x = u;
            }
            else
                x = *p;
            out.push_back(x);
        }
    return out;
}

std::pair<std::string, int> splitLod(const std::string& name)
{
    const auto at = name.rfind("__lod");
    if (at != std::string::npos && at + 5 < name.size() && std::isdigit(static_cast<unsigned char>(name[at + 5])))
        return {name.substr(0, at), std::atoi(name.c_str() + at + 5)};
    return {name, 0};
}

std::pair<std::string, std::string> pairOf(const std::string& s)
{
    const auto c = s.find(':');
    return c == std::string::npos ? std::make_pair(s, std::string()) : std::make_pair(s.substr(0, c), s.substr(c + 1));
}
} // namespace

fs::path GltfSource::find(const fs::path& folder)
{
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(folder, ec))
        if (e.path().extension() == ".gltf")
            return e.path();
    return {};
}

json GltfSource::scene(const fs::path& workDir)
{
    std::ifstream in(workDir / "gltf" / "scene.json");
    if (!in)
        return json();
    json j = json::parse(in, nullptr, false);
    return j.is_discarded() ? json() : j;
}

fs::path GltfSource::convert(const ModelCard& card, const fs::path& workDir, std::string& error)
{
    const fs::path src = find(card.folder);
    if (src.empty())
    {
        error = "no .gltf";
        return {};
    }
    const fs::path out = workDir / "gltf";
    std::error_code ec;
    // fresh when the scene file is newer than the .gltf and every buffer
    auto stamp = fs::last_write_time(src, ec);
    std::ifstream in(src);
    json g = json::parse(in, nullptr, false);
    if (g.is_discarded())
    {
        error = "bad glTF " + src.u8string();
        return {};
    }
    Buffers b;
    for (const auto& buf : g.value("buffers", json::array()))
    {
        const std::string uri = buf.value("uri", std::string());
        if (uri.rfind("data:", 0) == 0)
            b.data.push_back(decodeBase64(uri.substr(uri.find(',') + 1)));
        else
        {
            const fs::path bp = src.parent_path() / fs::u8path(uri);
            stamp = std::max(stamp, fs::last_write_time(bp, ec));
            std::ifstream bin(bp, std::ios::binary);
            b.data.emplace_back(std::istreambuf_iterator<char>(bin), std::istreambuf_iterator<char>());
        }
    }
    if (fs::exists(out / "scene.json") && fs::last_write_time(out / "scene.json", ec) > stamp)
        return out;
    fs::remove_all(out, ec);
    fs::create_directories(out, ec);

    json scene = {{"source", src.generic_u8string()}, {"nodes", json::array()}, {"hit_volumes", json::array()}, {"zones", json::array()},
                  {"mounts", json::array()}, {"seams", json::array()}, {"connections", json::array()}, {"kinds", json::object()},
                  {"materials", json::object()}};
    const auto& nodes = g["nodes"];
    std::map<std::string, std::string> parentOf;
    int meshes = 0, seamN = 0, connN = 0;
    std::function<void(int, const M4&, const std::string&)> walk = [&](int ni, const M4& parentM, const std::string& parentName) {
        const json& n = nodes[static_cast<std::size_t>(ni)];
        const M4 m = mul(parentM, localMatrix(n));
        const std::string name = n.value("name", "node" + std::to_string(ni));
        const json ex = n.value("extras", json::object());
        json pos, rot, scl;
        decompose(m, pos, rot, scl);
        if (ex.contains("elite_part"))
        {
            // a library link: its meshes are the module's own, not this model's
            scene["nodes"].push_back({{"id", name}, {"part", name}, {"ref", ex["elite_part"]}, {"parent", parentName.empty() ? json() : json(parentName)},
                                      {"position", pos}, {"rotation_deg", rot}});
            return;
        }
        if (ex.contains("elite_hit"))
        {
            json h = {{"id", name}, {"node", ex["elite_hit"]}, {"shape", ex.value("elite_shape", std::string("box"))}, {"position", pos},
                      {"rotation_deg", rot}, {"source", "blender"}};
            if (h["shape"] == "cylinder")
            {
                h["radius"] = scl[0];
                h["half_height"] = scl[1];   // Blender Z (the cylinder axis) is glTF Y
                if (ex.contains("elite_radius_top"))
                    h["radius_top"] = ex["elite_radius_top"];
            }
            else
                h["half_size"] = scl;
            scene["hit_volumes"].push_back(h);
        }
        else if (ex.contains("elite_zone"))
            scene["zones"].push_back({{"id", name}, {"kind", ex["elite_zone"]}, {"shape", "box"}, {"position", pos}, {"rotation_deg", rot},
                                      {"half_size", scl}, {"source", "blender"}});
        else if (ex.contains("elite_mount"))
        {
            const auto [a, bb] = pairOf(ex["elite_mount"].get<std::string>());
            scene["mounts"].push_back({{"id", name}, {"a", a}, {"b", bb}, {"kind", "equipment_mount"}, {"position", pos}, {"rotation_deg", rot},
                                       {"half_size", scl}, {"source", "blender"}});
        }
        else if (ex.contains("elite_seam"))
        {
            const auto [a, bb] = pairOf(ex["elite_seam"].get<std::string>());
            scene["seams"].push_back({{"id", name}, {"a", a}, {"b", bb}, {"source", "blender"},
                                      {"proxies", json::array({{{"id", name + ".p" + std::to_string(seamN++)}, {"position", pos},
                                                                {"rotation_deg", rot}, {"half_size", scl}}})}});
        }
        if (n.contains("mesh"))
        {
            const auto [key, lod] = splitLod(name);
            const fs::path dir = out / ("LOD" + std::to_string(lod));
            fs::create_directories(dir, ec);
            std::ofstream obj(dir / fs::u8path(key + ".obj"), std::ios::binary);
            obj << "# from " << src.filename().u8string() << " node " << name << " (model space, glTF Y up)\n";
            std::size_t base = 0;
            for (const auto& prim : g["meshes"][n["mesh"].get<std::size_t>()]["primitives"])
            {
                if (prim.value("mode", 4) != 4 || !prim["attributes"].contains("POSITION"))
                    continue;
                int comps = 0;
                const auto p = readAccessor(g, b, prim["attributes"]["POSITION"], comps);
                const std::size_t count = p.size() / 3;
                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto w = apply(m, {p[i * 3], p[i * 3 + 1], p[i * 3 + 2]});
                    obj << "v " << w[0] << " " << w[1] << " " << w[2] << "\n";
                }
                if (prim.contains("material"))
                {
                    const std::string mat = g["materials"][prim["material"].get<std::size_t>()].value("name", std::string("default"));
                    obj << "usemtl " << mat << "\n";
                    scene["materials"][mat] = mat;
                }
                std::vector<double> idx;
                if (prim.contains("indices"))
                    idx = readAccessor(g, b, prim["indices"], comps);
                else
                    for (std::size_t i = 0; i < count; ++i)
                        idx.push_back(static_cast<double>(i));
                for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
                    obj << "f " << base + static_cast<std::size_t>(idx[i]) + 1 << " " << base + static_cast<std::size_t>(idx[i + 1]) + 1 << " "
                        << base + static_cast<std::size_t>(idx[i + 2]) + 1 << "\n";
                base += count;
            }
            ++meshes;
            if (lod == 0)
            {
                json node = {{"id", key}, {"part", key}, {"parent", parentName.empty() ? json() : json(parentName)},
                             {"position", {0, 0, 0}}, {"rotation_deg", {0, 0, 0}}};
                if (ex.contains("elite_role"))
                    node["role"] = ex["elite_role"];
                if (ex.contains("elite_material"))
                    node["material"] = ex["elite_material"];
                scene["nodes"].push_back(node);
                if (ex.contains("elite_kind"))
                    scene["kinds"][key] = ex["elite_kind"];
            }
        }
        if (ex.contains("elite_feeds"))
        {
            // "service_power:a b; data:c"
            std::stringstream groups(ex["elite_feeds"].get<std::string>());
            std::string grp;
            while (std::getline(groups, grp, ';'))
            {
                const auto [bus, list] = pairOf(grp);
                std::stringstream targets(list);
                std::string t;
                std::string busName = bus;
                busName.erase(0, busName.find_first_not_of(' '));
                while (targets >> t)
                    scene["connections"].push_back({{"id", "c" + std::to_string(connN++)}, {"from", splitLod(name).first}, {"to", t},
                                                    {"bus", busName}, {"source", "blender"}});
            }
        }
        const std::string self = n.contains("mesh") ? splitLod(name).first : parentName;
        for (const auto& c : n.value("children", json::array()))
            walk(c.get<int>(), m, self);
    };
    const int sceneIndex = g.value("scene", 0);
    for (const auto& r : g["scenes"][static_cast<std::size_t>(sceneIndex)]["nodes"])
        walk(r.get<int>(), identity(), std::string());
    std::ofstream(out / "scene.json", std::ios::binary) << scene.dump(1) << "\n";
    std::cerr << "[MeshEditor] glTF " << src.filename().u8string() << ": " << meshes << " meshes, " << scene["nodes"].size() << " nodes, "
              << scene["hit_volumes"].size() << " hit volumes, " << scene["zones"].size() << " zones" << std::endl;
    return out;
}
} // namespace elite::mesh_editor
