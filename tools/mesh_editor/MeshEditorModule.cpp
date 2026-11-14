#include "tools/mesh_editor/MeshEditorModule.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <tuple>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetBuild.h"
#include "tools/mesh_editor/MeshEditorAssembly.h"
#include "tools/ship_asset_assemble/ShipAssetAssemble.h"

namespace elite::mesh_editor
{
namespace fs = std::filesystem;
using json = nlohmann::json;
namespace ma = elite::model_asset;

namespace
{
// one part as an indexed mesh: corners welded where position and normal agree; triangles grouped by material name
struct Indexed
{
    std::vector<std::array<float, 3>> pos, nrm;
    std::map<std::string, std::vector<std::uint32_t>> byMaterial;   // material name -> triangle corner indices
};

Indexed indexed(const RepairedPart& p)
{
    Indexed out;
    const auto normals = Assembly::cornerNormals(p);
    std::map<std::tuple<int, long, long, long>, std::uint32_t> seen;
    for (std::size_t i = 0; i < p.t.size(); ++i)
    {
        const std::string mat = p.tm[i] >= 0 ? p.mtl[static_cast<std::size_t>(p.tm[i])] : std::string("default");
        auto& idx = out.byMaterial[mat];
        for (int k = 0; k < 3; ++k)
        {
            const int vi = p.t[i][k];
            const auto& n = normals[i * 3 + static_cast<std::size_t>(k)];
            const auto key = std::make_tuple(vi, std::lround(n[0] * 1e4), std::lround(n[1] * 1e4), std::lround(n[2] * 1e4));
            auto it = seen.find(key);
            if (it == seen.end())
            {
                it = seen.emplace(key, static_cast<std::uint32_t>(out.pos.size())).first;
                out.pos.push_back(p.v[static_cast<std::size_t>(vi)]);
                out.nrm.push_back(n);
            }
            idx.push_back(it->second);
        }
    }
    return out;
}

json libraryEntry(const json& library, const std::string& id)
{
    for (const auto& m : library.value("materials", json::array()))
        if (m.value("id", std::string()) == id)
            return m;
    return {{"id", id}, {"baseColor", {0.6, 0.62, 0.65}}, {"metallic", 0.1}, {"roughness", 0.7}};
}

std::set<std::string> usedMaterials(const ModuleShell& m)
{
    std::set<std::string> used;
    for (const auto& g : m.lods)
        for (const auto& p : g.parts)
            for (std::size_t i = 0; i < p.t.size(); ++i)
                used.insert(p.tm[i] >= 0 ? p.mtl[static_cast<std::size_t>(p.tm[i])] : std::string("default"));
    return used;
}

// the rig link holding an element (hit volumes belong to elements); the first link when none names it
std::size_t linkOf(const std::vector<ShellLink>& links, const std::string& element)
{
    for (std::size_t i = 0; i < links.size(); ++i)
        for (const auto& p : links[i].parts)
            if (p == element || p.rfind(element + ".", 0) == 0 || element.rfind(p + ".", 0) == 0)
                return i;
    return 0;
}

glm::vec3 vec3(const json& j, glm::vec3 fallback = glm::vec3(0.0f))
{
    if (!j.is_array() || j.size() < 3)
        return fallback;
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

// Rx * Ry * Rz (the assembler's rotation order), as a glTF quaternion x y z w
std::array<double, 4> quatXYZ(const json& deg)
{
    const glm::vec3 r = glm::radians(vec3(deg));
    auto q = [](double ang, int axis) {
        std::array<double, 4> o {0, 0, 0, std::cos(ang / 2)};
        o[static_cast<std::size_t>(axis)] = std::sin(ang / 2);
        return o;
    };
    auto mul = [](const std::array<double, 4>& a, const std::array<double, 4>& b) {
        return std::array<double, 4> {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                                      a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                                      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                                      a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    };
    return mul(mul(q(r.x, 0), q(r.y, 1)), q(r.z, 2));
}

int lodLevel(const std::string& name, int fallback)
{
    return name.rfind("LOD", 0) == 0 ? std::atoi(name.c_str() + 3) : fallback;
}
} // namespace

bool ModuleFiles::writePackage(const ModuleShell& m, const fs::path& elmodel, std::string& report)
{
    try
    {
        ma::ModelAsset a;
        a.assetId = m.id;
        a.displayName = m.title.empty() ? m.id : m.title;
        std::map<std::string, std::int32_t> mat;
        for (const auto& id : usedMaterials(m))
        {
            const json e = libraryEntry(m.library, id);
            ma::MaterialDefinition d;
            d.id = id;
            d.sourceName = id;
            d.baseColor = glm::vec4(vec3(e.value("baseColor", json()), glm::vec3(0.6f)), 1.0f);
            d.metallic = e.value("metallic", 0.0f);
            d.roughness = e.value("roughness", 0.65f);
            d.emissiveColor = vec3(e.value("emissive", json()));
            d.emissiveStrength = glm::length(d.emissiveColor) > 0.0f ? 1.0f : 0.0f;
            d.twoSided = true;   // a shell is thin sheets
            mat[id] = static_cast<std::int32_t>(a.materials.size());
            a.materials.push_back(d);
        }

        // one semantic node per rig link: parent, joint (rig.json: hinge / slide, pivot, axis, limits)
        std::map<std::string, std::int32_t> node;
        for (const auto& l : m.links)
        {
            node[l.id] = static_cast<std::int32_t>(a.nodes.size());
            ma::Node n;
            n.id = l.id;
            n.moduleId = m.id;
            if (l.joint.is_object())
            {
                const std::string type = l.joint.value("type", std::string());
                n.joint.type = type == "hinge" ? ma::JointType::Revolute : type == "slide" ? ma::JointType::Prismatic : ma::JointType::Fixed;
                n.joint.pivot = vec3(l.joint.value("pivot", json()));
                n.joint.axis = vec3(l.joint.value("axis", json()), glm::vec3(0, 1, 0));
                n.pivot = n.joint.pivot;
                const json lim = l.joint.value("limits", json());
                if (lim.is_array() && lim.size() == 2)
                {
                    if (n.joint.type == ma::JointType::Prismatic)
                    {
                        n.joint.minTravel = lim[0].get<float>();
                        n.joint.maxTravel = lim[1].get<float>();
                    }
                    else
                    {
                        n.joint.minAngleDeg = lim[0].get<float>();
                        n.joint.maxAngleDeg = lim[1].get<float>();
                    }
                }
            }
            a.nodes.push_back(n);
        }
        for (std::size_t i = 0; i < m.links.size(); ++i)
            if (const auto it = node.find(m.links[i].parent); it != node.end())
                a.nodes[i].parentIndex = it->second;

        bool first = true;
        int fallbackLevel = 0;
        for (const auto& g : m.lods)
        {
            ma::RenderLod lod;
            lod.level = static_cast<std::uint32_t>(lodLevel(g.name, fallbackLevel++));
            lod.sourceKind = "generated";
            lod.role = ma::RenderLodRole::Component;
            lod.relativeGeometricError = lod.level == 0 ? 0.0f : 0.004f * static_cast<float>(lod.level * lod.level);
            bool lodFirst = true;
            for (const auto& p : g.parts)
            {
                if (p.t.empty() || !node.count(p.key))
                    continue;
                const Indexed ix = indexed(p);
                ma::RenderGeometryDefinition geo;
                geo.id = p.key + ".lod" + std::to_string(lod.level);
                geo.surfaceMode = ma::SurfaceMode::ThinTwoSided;
                for (std::size_t i = 0; i < ix.pos.size(); ++i)
                {
                    ma::Vertex v;
                    v.position = {ix.pos[i][0], ix.pos[i][1], ix.pos[i][2]};
                    v.normal = {ix.nrm[i][0], ix.nrm[i][1], ix.nrm[i][2]};
                    geo.mesh.vertices.push_back(v);
                }
                for (const auto& [name, idx] : ix.byMaterial)
                    for (std::size_t k = 0; k + 2 < idx.size(); k += 3)
                    {
                        ma::Triangle t;
                        t.a = idx[k];
                        t.b = idx[k + 1];
                        t.c = idx[k + 2];
                        t.materialIndex = mat.at(name);
                        geo.mesh.triangles.push_back(t);
                    }
                elite::ship_asset::buildLineEdges(geo.mesh);
                geo.mesh.minBounds = geo.mesh.maxBounds = geo.mesh.vertices[0].position;
                for (const auto& v : geo.mesh.vertices)
                {
                    geo.mesh.minBounds = glm::min(geo.mesh.minBounds, v.position);
                    geo.mesh.maxBounds = glm::max(geo.mesh.maxBounds, v.position);
                }
                if (lodFirst)
                {
                    lod.minBounds = geo.mesh.minBounds;
                    lod.maxBounds = geo.mesh.maxBounds;
                    lodFirst = false;
                }
                lod.minBounds = glm::min(lod.minBounds, geo.mesh.minBounds);
                lod.maxBounds = glm::max(lod.maxBounds, geo.mesh.maxBounds);
                ma::RenderNode rn;
                rn.id = p.key + "@" + std::to_string(lod.level);
                rn.geometryIndex = static_cast<std::int32_t>(lod.geometries.size());
                rn.semanticNodeIndex = node.at(p.key);
                lod.geometries.push_back(std::move(geo));
                lod.nodes.push_back(rn);
            }
            if (lod.nodes.empty())
                continue;
            lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
            lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
            if (first)
            {
                a.minBounds = lod.minBounds;
                a.maxBounds = lod.maxBounds;
                first = false;
            }
            a.minBounds = glm::min(a.minBounds, lod.minBounds);
            a.maxBounds = glm::max(a.maxBounds, lod.maxBounds);
            a.renderLods.push_back(std::move(lod));
        }
        if (a.renderLods.empty())
            throw std::runtime_error("no shell geometry");

        for (const auto& h : m.hitVolumes)
        {
            ma::CollisionVolume c;
            c.id = h.value("id", h.value("node", std::string("hit")));
            c.moduleId = h.value("node", std::string());
            c.parentNodeIndex = static_cast<std::int32_t>(linkOf(m.links, c.moduleId));
            c.shape = h.value("shape", std::string("box")) == "cylinder" ? ma::CollisionShape::Cylinder : ma::CollisionShape::Box;
            c.localPosition = vec3(h.value("position", json()));
            c.localRotationDeg = vec3(h.value("rotation_deg", json()));
            if (c.shape == ma::CollisionShape::Cylinder)
            {
                c.radius = h.value("radius", 0.5f);
                c.halfHeight = h.value("half_height", 0.5f);
            }
            else
                c.halfSize = vec3(h.value("half_size", json()), glm::vec3(0.5f));
            a.collisionVolumes.push_back(c);
        }
        // the mounting point: the module's origin (the generators put the flange there)
        ma::Socket mount;
        mount.id = "mount";
        mount.kind = "equipment_mount";
        mount.moduleId = m.id;
        mount.parentNodeIndex = 0;
        mount.interfaceProfile = m.category + "/" + m.id;
        a.sockets.push_back(mount);

        a.physicalSize.enabled = true;   // the generators model in metres: 1:1
        a.physicalSize.axis = ma::PhysicalSizeAxis::Z;
        a.physicalSize.sourceExtent = std::max(a.maxBounds.z - a.minBounds.z, 1e-3f);
        a.physicalSize.targetMeters = a.physicalSize.sourceExtent;
        a.physicalSize.sourceToMeters = 1.0f;
        a.physicalSize.geometrySpace = ma::PhysicalGeometrySpace::Authoring;
        std::string error;
        if (!ma::ModelAssetBinary::validate(a, &error))
            throw std::runtime_error("working asset invalid: " + error);
        const ma::ModelAsset production = ma::makeProductionCopy(a);
        if (!ma::ModelAssetBinary::validate(production, &error))
            throw std::runtime_error("production invalid: " + error);
        std::error_code ec;
        fs::create_directories(elmodel.parent_path(), ec);
        if (!ma::ModelAssetBinary::save(elmodel.string(), production, &error))
            throw std::runtime_error("save failed: " + error);
        ma::ModelAssetBinary::pruneStaleLods(elmodel.string(), production, &error);
        std::size_t tris = 0;
        for (const auto& g : production.renderLods[0].geometries)
            tris += g.mesh.triangles.size();
        char buf[256];
        std::snprintf(buf, sizeof buf, "%zu links, %zu LODs (LOD0 %zu triangles), %zu hit volumes, %zu materials", a.nodes.size(),
                      a.renderLods.size(), tris, a.collisionVolumes.size(), a.materials.size());
        report = buf;
        return true;
    }
    catch (const std::exception& e)
    {
        report = std::string("package failed: ") + e.what();
        return false;
    }
}

void ModuleFiles::writeGltf(const ModuleShell& m, const fs::path& gltf)
{
    std::vector<char> bin;
    json views = json::array(), accessors = json::array(), meshes = json::array(), nodes = json::array(), materials = json::array();
    auto view = [&](const void* data, std::size_t bytes, int target) {
        while (bin.size() % 4)
            bin.push_back(0);
        views.push_back({{"buffer", 0}, {"byteOffset", bin.size()}, {"byteLength", bytes}, {"target", target}});
        bin.insert(bin.end(), static_cast<const char*>(data), static_cast<const char*>(data) + bytes);
        return static_cast<int>(views.size() - 1);
    };
    std::map<std::string, int> matIndex;
    for (const auto& id : usedMaterials(m))
    {
        const json e = libraryEntry(m.library, id);
        const glm::vec3 c = vec3(e.value("baseColor", json()), glm::vec3(0.6f));
        json pbr = {{"baseColorFactor", {c.x, c.y, c.z, 1.0}}, {"metallicFactor", e.value("metallic", 0.0)},
                    {"roughnessFactor", e.value("roughness", 0.65)}};
        json mj = {{"name", id}, {"pbrMetallicRoughness", pbr}, {"doubleSided", true}, {"extras", {{"elite_material", id}}}};
        const glm::vec3 em = vec3(e.value("emissive", json()));
        if (glm::length(em) > 0.0f)
            mj["emissiveFactor"] = {em.x, em.y, em.z};
        matIndex[id] = static_cast<int>(materials.size());
        materials.push_back(mj);
    }
    json roots = json::array();
    int fallbackLevel = 0;
    for (const auto& g : m.lods)
    {
        const int level = lodLevel(g.name, fallbackLevel++);
        std::map<std::string, int> nodeOfLink;
        for (const auto& p : g.parts)
        {
            if (p.t.empty())
                continue;
            const Indexed ix = indexed(p);
            std::array<float, 3> lo = ix.pos[0], hi = ix.pos[0];
            for (const auto& v : ix.pos)
                for (int k = 0; k < 3; ++k)
                {
                    lo[k] = std::min(lo[k], v[k]);
                    hi[k] = std::max(hi[k], v[k]);
                }
            const int pv = view(ix.pos.data(), ix.pos.size() * 12, 34962);
            accessors.push_back({{"bufferView", pv}, {"componentType", 5126}, {"count", ix.pos.size()}, {"type", "VEC3"},
                                 {"min", {lo[0], lo[1], lo[2]}}, {"max", {hi[0], hi[1], hi[2]}}});
            const int pa = static_cast<int>(accessors.size() - 1);
            const int nv = view(ix.nrm.data(), ix.nrm.size() * 12, 34962);
            accessors.push_back({{"bufferView", nv}, {"componentType", 5126}, {"count", ix.nrm.size()}, {"type", "VEC3"}});
            const int na = static_cast<int>(accessors.size() - 1);
            json prims = json::array();
            for (const auto& [name, idx] : ix.byMaterial)
            {
                const int iv = view(idx.data(), idx.size() * 4, 34963);
                accessors.push_back({{"bufferView", iv}, {"componentType", 5125}, {"count", idx.size()}, {"type", "SCALAR"}});
                prims.push_back({{"attributes", {{"POSITION", pa}, {"NORMAL", na}}}, {"indices", accessors.size() - 1},
                                 {"material", matIndex.at(name)}});
            }
            const std::string name = p.key + "__lod" + std::to_string(level);
            meshes.push_back({{"name", name}, {"primitives", prims}});
            json extras = {{"elite_link", p.key}, {"elite_lod", g.name}};
            for (const auto& l : m.links)
                if (l.id == p.key && l.joint.is_object())
                    extras["elite_joint"] = l.joint.dump();
            nodes.push_back({{"name", name}, {"mesh", meshes.size() - 1}, {"extras", extras}});
            nodeOfLink[p.key] = static_cast<int>(nodes.size() - 1);
        }
        // the rig hierarchy inside one LOD (link transforms are identity: the shells are in the module frame)
        for (const auto& [link, ni] : nodeOfLink)
        {
            std::string parent;
            for (const auto& l : m.links)
                if (l.id == link)
                    parent = l.parent;
            if (const auto it = nodeOfLink.find(parent); it != nodeOfLink.end())
                nodes[static_cast<std::size_t>(it->second)]["children"].push_back(ni);
            else
                roots.push_back(ni);
        }
    }
    for (const auto& h : m.hitVolumes)
    {
        const bool cyl = h.value("shape", std::string("box")) == "cylinder";
        const auto q = quatXYZ(h.value("rotation_deg", json()));
        json scale = cyl ? json {h.value("radius", 0.5), h.value("half_height", 0.5), h.value("radius", 0.5)}
                         : h.value("half_size", json {0.5, 0.5, 0.5});
        nodes.push_back({{"name", "hit." + h.value("id", h.value("node", std::string()))},
                         {"translation", h.value("position", json {0, 0, 0})},
                         {"rotation", {q[0], q[1], q[2], q[3]}},
                         {"scale", scale},
                         {"extras", {{"elite_hit", h.value("node", std::string())}, {"elite_shape", cyl ? "cylinder" : "box"}}}});
        roots.push_back(nodes.size() - 1);
    }
    nodes.push_back({{"name", m.id + ".mount"}, {"extras", {{"elite_mount_point", true}}}});
    roots.push_back(nodes.size() - 1);

    const fs::path binPath = fs::path(gltf).replace_extension(".bin");
    json doc = {{"asset", {{"version", "2.0"}, {"generator", "Elite mesh editor (MeshEditorModule)"}}},
                {"scene", 0},
                {"scenes", {{{"name", m.id}, {"nodes", roots}, {"extras", {{"elite_module", m.category + "/" + m.id}}}}}},
                {"nodes", nodes},
                {"meshes", meshes},
                {"materials", materials},
                {"accessors", accessors},
                {"bufferViews", views},
                {"buffers", {{{"uri", binPath.filename().u8string()}, {"byteLength", bin.size()}}}}};
    std::error_code ec;
    fs::create_directories(gltf.parent_path(), ec);
    std::ofstream(binPath, std::ios::binary).write(bin.data(), static_cast<std::streamsize>(bin.size()));
    std::ofstream(gltf) << doc.dump();
}
} // namespace elite::mesh_editor
