#include "src/model_asset/HubCityAssembly.h"

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetLodError.h"
#include "src/model_asset/ModelAssetPresentation.h"

#include <nlohmann/json.hpp>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace elite::model_asset
{
namespace
{
using json = nlohmann::json;

constexpr int CityLods = 3;   // component LODs 0..2 from the modules; LOD 3 = the module proxies (node groups)

glm::mat3 rotationXYZ(const glm::vec3& deg)
{
    // R = Rx * Ry * Rz (the assembler's and three.js 'XYZ' convention)
    const glm::vec3 r = glm::radians(deg);
    const glm::mat3 rx(1, 0, 0, 0, std::cos(r.x), std::sin(r.x), 0, -std::sin(r.x), std::cos(r.x));
    const glm::mat3 ry(std::cos(r.y), 0, -std::sin(r.y), 0, 1, 0, std::sin(r.y), 0, std::cos(r.y));
    const glm::mat3 rz(std::cos(r.z), std::sin(r.z), 0, -std::sin(r.z), std::cos(r.z), 0, 0, 0, 1);
    return rx * ry * rz;
}

glm::vec3 eulerXYZ(const glm::mat3& m)
{
    // inverse of rotationXYZ: R02 = sin b, R12 = -sin a cos b, R22 = cos a cos b, R01 = -cos b sin c, R00 = cos b cos c
    // (glm is column-major: R(row, col) = m[col][row])
    const float sb = std::clamp(m[2][0], -1.0f, 1.0f);
    const float b = std::asin(sb);
    float a, c;
    if (std::abs(sb) < 0.99999f)
    {
        a = std::atan2(-m[2][1], m[2][2]);
        c = std::atan2(-m[1][0], m[0][0]);
    }
    else
    {
        a = std::atan2(m[1][2], m[1][1]);
        c = 0.0f;
    }
    return glm::degrees(glm::vec3(a, b, c));
}

bool activeIntact(const std::vector<std::string>& states)
{
    return states.empty() || std::find(states.begin(), states.end(), "intact") != states.end();
}

// one module package, reduced to what a city needs: its geometry per city LOD merged into one mesh (module frame),
// its collision volumes and its materials
struct ModuleKit
{
    std::string folder;
    std::vector<MaterialDefinition> materials;
    MeshLod lods[CityLods];
    bool twoSided[CityLods] = {};
    float lodErrorM[CityLods] = {};          // the two-sided deviation of LOD l from LOD 0 (metres)
    std::vector<CollisionVolume> collision;  // module frame
};

void appendMesh(MeshLod& dst, const MeshLod& src, const glm::mat4& transform, const std::vector<std::int32_t>& materialMap)
{
    const std::uint32_t base = static_cast<std::uint32_t>(dst.vertices.size());
    const std::int32_t triBase = static_cast<std::int32_t>(dst.triangles.size());
    const glm::mat3 normalM = glm::inverseTranspose(glm::mat3(transform));
    for (const auto& v : src.vertices)
    {
        Vertex w = v;
        w.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
        const glm::vec3 n = normalM * v.normal;
        const float len = glm::length(n);
        w.normal = len > 1e-12f ? n / len : v.normal;
        dst.vertices.push_back(w);
    }
    for (const auto& t : src.triangles)
    {
        Triangle u = t;
        u.a += base;
        u.b += base;
        u.c += base;
        if (u.materialIndex >= 0 && static_cast<std::size_t>(u.materialIndex) < materialMap.size())
            u.materialIndex = materialMap[static_cast<std::size_t>(u.materialIndex)];
        dst.triangles.push_back(u);
    }
    for (const auto& e : src.edges)
    {
        Edge f = e;
        f.a += base;
        f.b += base;
        if (f.triangleA >= 0) f.triangleA += triBase;
        if (f.triangleB >= 0) f.triangleB += triBase;
        dst.edges.push_back(f);
    }
}

void recomputeBounds(MeshLod& m)
{
    if (m.vertices.empty())
    {
        m.minBounds = m.maxBounds = glm::vec3(0.0f);
        return;
    }
    m.minBounds = m.maxBounds = m.vertices.front().position;
    for (const auto& v : m.vertices)
    {
        m.minBounds = glm::min(m.minBounds, v.position);
        m.maxBounds = glm::max(m.maxBounds, v.position);
    }
}

std::shared_ptr<const ModuleKit> loadModule(const std::string& moduleRoot, const std::string& folder, std::string* error)
{
    static std::mutex mutex;
    static std::unordered_map<std::string, std::shared_ptr<const ModuleKit>> cache;
    const std::string path = moduleRoot + "/" + folder + "/" + folder + ".elmodel";
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (const auto it = cache.find(path); it != cache.end())
            return it->second;
    }
    ModelAsset asset;
    std::string err;
    if (!std::filesystem::exists(path) || !ModelAssetBinary::load(path, asset, &err))
    {
        if (error) *error = "module package " + path + (err.empty() ? " missing" : " failed: " + err);
        return nullptr;
    }
    auto kit = std::make_shared<ModuleKit>();
    kit->folder = folder;
    kit->materials = asset.materials;
    const presentation::Index index = presentation::buildIndex(asset);
    std::vector<std::int32_t> identity(asset.materials.size());
    for (std::size_t i = 0; i < identity.size(); ++i) identity[i] = static_cast<std::int32_t>(i);
    for (int l = 0; l < CityLods; ++l)
    {
        if (asset.renderLods.empty()) break;
        // a module package: LOD0 = the source LOD0, LOD1 = its copy (role proxy), LOD2 / LOD3 = the source LOD1 / LOD2 -
        // the city LOD l takes the module LOD {0, 2, 3}[l] (what the per-hub packages had), the last one when missing
        static const std::size_t moduleLodOf[CityLods] = {0, 2, 3};
        const std::size_t src = std::min<std::size_t>(moduleLodOf[l], asset.renderLods.size() - 1);
        const RenderLod& lod = asset.renderLods[src];
        bool allTwoSided = !lod.nodes.empty();
        for (std::size_t rn = 0; rn < lod.nodes.size(); ++rn)
        {
            const RenderNode& node = lod.nodes[rn];
            if (!node.enabled || node.geometryIndex < 0 || static_cast<std::size_t>(node.geometryIndex) >= lod.geometries.size() ||
                !activeIntact(node.activeStates))
                continue;
            const auto& g = lod.geometries[static_cast<std::size_t>(node.geometryIndex)];
            allTwoSided = allTwoSided && g.surfaceMode != SurfaceMode::Closed;
            const glm::mat4 t = src < index.renderNodes.size() && rn < index.renderNodes[src].size()
                                    ? index.renderNodes[src][rn].assetTransform
                                    : glm::mat4(1.0f);
            appendMesh(kit->lods[l], g.mesh, t, identity);
        }
        kit->twoSided[l] = allTwoSided;
        recomputeBounds(kit->lods[l]);
    }
    for (int l = 1; l < CityLods; ++l)
        kit->lodErrorM[l] = std::max(kit->lodErrorM[l - 1], std::max(0.0f, meshDeviation(kit->lods[l], kit->lods[0])));
    for (const auto& c : asset.collisionVolumes)
        if (c.enabled && activeIntact(c.activeStates))
            kit->collision.push_back(c);
    std::lock_guard<std::mutex> lock(mutex);
    return cache.emplace(path, std::move(kit)).first->second;
}

glm::vec3 vec3Of(const json& a, std::size_t at)
{
    return glm::vec3(a.at(at).get<float>(), a.at(at + 1).get<float>(), a.at(at + 2).get<float>());
}

// a box as 12 triangles (outward normals, flat), appended to `m`
void appendBox(MeshLod& m, const glm::vec3& lo, const glm::vec3& hi, const glm::mat4& t, std::int32_t material)
{
    static const int faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    static const glm::vec3 normals[6] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    glm::vec3 corner[8];
    for (int i = 0; i < 8; ++i)
        corner[i] = glm::vec3(t * glm::vec4((i & 4) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 1) ? hi.z : lo.z, 1.0f));
    const glm::mat3 nm = glm::mat3(t);
    for (int f = 0; f < 6; ++f)
    {
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        const glm::vec3 n = glm::normalize(nm * normals[f]);
        for (int k = 0; k < 4; ++k)
        {
            Vertex v;
            v.position = corner[faces[f][k]];
            v.normal = n;
            m.vertices.push_back(v);
        }
        // winding: make the triangle normal agree with n
        const glm::vec3 a = m.vertices[base].position, b = m.vertices[base + 1].position, c = m.vertices[base + 2].position;
        const bool flip = glm::dot(glm::cross(b - a, c - a), n) < 0.0f;
        Triangle t1 {base, flip ? base + 2 : base + 1, flip ? base + 1 : base + 2, -1, material, 0};
        Triangle t2 {base, flip ? base + 3 : base + 2, flip ? base + 2 : base + 3, -1, material, 0};
        m.triangles.push_back(t1);
        m.triangles.push_back(t2);
    }
}

} // namespace

std::string hubCityLayoutPath(const std::string& hubId)
{
    return "assets/data/navigation/hub_cities/" + hubId + ".json";
}

bool assembleHubCity(const std::string& hubId, const std::string& moduleRoot, ModelAsset& out, std::string* error)
{
    const auto t0 = std::chrono::steady_clock::now();
    const std::string path = hubCityLayoutPath(hubId);
    std::ifstream in(path);
    if (!in)
    {
        if (error) *error = "no city layout " + path;
        return false;
    }
    json doc;
    try
    {
        in >> doc;
    }
    catch (const std::exception& e)
    {
        if (error) *error = path + ": " + e.what();
        return false;
    }
    if (doc.value("schema", std::string()) != "elite.hub_city_layout.v1")
    {
        if (error) *error = path + ": unknown schema";
        return false;
    }

    // modules: their kits, the city's material table (union by id), each module's material map
    const auto& jm = doc.at("modules");
    std::vector<std::shared_ptr<const ModuleKit>> kits;
    std::vector<glm::vec3> boxLo, boxHi;
    std::vector<std::string> proxyMaterial;
    std::map<std::string, std::int32_t> materialOf;
    std::vector<std::vector<std::int32_t>> materialMap;
    out = ModelAsset {};
    const auto materialIndex = [&](const MaterialDefinition& m) {
        const auto [it, inserted] = materialOf.emplace(m.id, static_cast<std::int32_t>(out.materials.size()));
        if (inserted) out.materials.push_back(m);
        return it->second;
    };
    for (const auto& m : jm)
    {
        const std::string folder = m.at("folder").get<std::string>();
        auto kit = loadModule(moduleRoot, folder, error);
        if (!kit)
            return false;
        std::vector<std::int32_t> map;
        for (const auto& mat : kit->materials) map.push_back(materialIndex(mat));
        materialMap.push_back(std::move(map));
        kits.push_back(std::move(kit));
        boxLo.push_back(vec3Of(m.at("lo"), 0));
        boxHi.push_back(vec3Of(m.at("hi"), 0));
        proxyMaterial.push_back(m.value("proxy_material", std::string("default")));
    }
    std::vector<std::int32_t> proxyMat(proxyMaterial.size());
    for (std::size_t i = 0; i < proxyMaterial.size(); ++i)
    {
        const auto it = materialOf.find(proxyMaterial[i]);
        if (it != materialOf.end())
            proxyMat[i] = it->second;
        else
        {
            MaterialDefinition d;
            d.id = proxyMaterial[i];
            d.sourceName = d.id;
            proxyMat[i] = materialIndex(d);
        }
    }

    out.assetId = "hub_" + hubId;
    out.displayName = "Город хаба «" + hubId + "» (сборка по раскладке)";
    out.physicalSize.enabled = true;
    out.physicalSize.sourceToMeters = 1.0f;
    out.physicalSize.geometrySpace = PhysicalGeometrySpace::Meters;

    // the component LODs: one geometry per module (shared by all its nodes)
    for (int l = 0; l < CityLods; ++l)
    {
        RenderLod lod;
        lod.level = static_cast<std::uint32_t>(l);
        lod.sourceKind = "generated";
        lod.role = RenderLodRole::Component;
        lod.relativeGeometricError = l == 0 ? 0.0f : 0.004f * static_cast<float>(l * l);
        for (std::size_t k = 0; k < kits.size(); ++k)
        {
            RenderGeometryDefinition g;
            g.id = kits[k]->folder + ".lod" + std::to_string(l);
            g.surfaceMode = kits[k]->twoSided[l] ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
            g.mesh.minBounds = kits[k]->lods[l].minBounds;
            g.mesh.maxBounds = kits[k]->lods[l].maxBounds;
            appendMesh(g.mesh, kits[k]->lods[l], glm::mat4(1.0f), materialMap[k]);
            g.mesh.minBounds = kits[k]->lods[l].minBounds;
            g.mesh.maxBounds = kits[k]->lods[l].maxBounds;
            lod.geometries.push_back(std::move(g));
        }
        out.renderLods.push_back(std::move(lod));
    }

    // nodes: semantic node at the origin (asset-space placement lives in the render nodes, as the package had it)
    const auto& groups = doc.at("groups");
    const auto& jn = doc.at("nodes");
    std::vector<MeshLod> proxies(groups.size());
    std::vector<std::int32_t> proxyRep(groups.size(), NoIndex);
    bool firstLod[CityLods] = {true, true, true};
    bool first = true;
    for (std::size_t i = 0; i < jn.size(); ++i)
    {
        const auto& n = jn[i];
        const std::string id = n.at(0).get<std::string>();
        const std::size_t mi = n.at(1).get<std::size_t>();
        const std::size_t gi = n.at(2).get<std::size_t>();
        if (mi >= kits.size() || gi >= groups.size())
        {
            if (error) *error = path + ": node " + id + " references a missing module / group";
            return false;
        }
        const glm::vec3 pos = vec3Of(n, 3), rot = vec3Of(n, 6);
        const glm::mat3 R = rotationXYZ(rot);
        glm::mat4 T(R);
        T[3] = glm::vec4(pos, 1.0f);
        const std::int32_t nodeIndex = static_cast<std::int32_t>(out.nodes.size());
        Node node;
        node.id = id;
        node.moduleId = groups[gi].get<std::string>();
        out.nodes.push_back(std::move(node));
        const ModuleKit& kit = *kits[mi];
        for (int l = 0; l < CityLods; ++l)
        {
            RenderNode rn;
            rn.id = id;
            rn.geometryIndex = static_cast<std::int32_t>(mi);
            rn.semanticNodeIndex = nodeIndex;
            rn.localPosition = pos;
            rn.localRotationDeg = rot;
            auto& lod = out.renderLods[static_cast<std::size_t>(l)];
            lod.nodes.push_back(std::move(rn));
            lod.nodeGeometricErrorM.push_back(kit.lodErrorM[l]);
            const MeshLod& gm = kit.lods[l];
            if (gm.vertices.empty()) continue;
            for (int c = 0; c < 8; ++c)
            {
                const glm::vec3 p = R * glm::vec3((c & 1) ? gm.maxBounds.x : gm.minBounds.x, (c & 2) ? gm.maxBounds.y : gm.minBounds.y,
                                                  (c & 4) ? gm.maxBounds.z : gm.minBounds.z) + pos;
                if (firstLod[l]) { lod.minBounds = lod.maxBounds = p; firstLod[l] = false; }
                if (l == 0 && first) { out.minBounds = out.maxBounds = p; first = false; }
                lod.minBounds = glm::min(lod.minBounds, p);
                lod.maxBounds = glm::max(lod.maxBounds, p);
            }
        }
        out.minBounds = glm::min(out.minBounds, out.renderLods[0].minBounds);
        out.maxBounds = glm::max(out.maxBounds, out.renderLods[0].maxBounds);
        // collision: the module's volumes placed by the node; id "<node>.<folder>.<k>" (city damage reads the folder)
        int k = 0;
        for (const auto& c : kit.collision)
        {
            CollisionVolume v = c;
            // the module volume id is "<part>.<k>" (its hit_volumes.json part, e.g. hubk_hab for hubk_hab_a): kept
            v.id = id + "." + (c.id.find('.') != std::string::npos ? c.id : kit.folder + "." + std::to_string(k + 1));
            ++k;
            v.moduleId = out.nodes.back().moduleId;
            v.parentNodeIndex = nodeIndex;
            v.localPosition = R * c.localPosition + pos;
            v.localRotationDeg = eulerXYZ(R * rotationXYZ(c.localRotationDeg));
            out.collisionVolumes.push_back(std::move(v));
        }
        // the far proxy of the node's group: its OBB as a box (a girder as a thinner bar, as the generator did)
        glm::vec3 lo = boxLo[mi], hi = boxHi[mi];
        if (hi.x > lo.x)
        {
            if (kit.folder == "hubb_girder")
            {
                const glm::vec3 c = 0.5f * (lo + hi), h = 0.5f * (hi - lo);
                lo = glm::vec3(c.x - h.x * 0.6f, c.y - h.y * 0.6f, lo.z);
                hi = glm::vec3(c.x + h.x * 0.6f, c.y + h.y * 0.6f, hi.z);
            }
            if (groups[gi].get<std::string>() != "core")
            {
                appendBox(proxies[gi], lo, hi, T, proxyMat[mi]);
                if (proxyRep[gi] == NoIndex) proxyRep[gi] = nodeIndex;
            }
        }
    }

    // the module-proxy LOD: one mesh per group in asset space, bound to the group's first node
    RenderLod proxyLod;
    proxyLod.level = CityLods;
    proxyLod.sourceKind = "generated";
    proxyLod.role = RenderLodRole::ModuleProxy;
    proxyLod.relativeGeometricError = 0.004f * static_cast<float>(CityLods * CityLods);
    bool proxyFirst = true;
    for (std::size_t g = 0; g < groups.size(); ++g)
    {
        if (proxyRep[g] == NoIndex || proxies[g].vertices.empty()) continue;
        recomputeBounds(proxies[g]);
        RenderGeometryDefinition geo;
        geo.id = "proxy." + groups[g].get<std::string>();
        geo.mesh = std::move(proxies[g]);
        if (proxyFirst) { proxyLod.minBounds = geo.mesh.minBounds; proxyLod.maxBounds = geo.mesh.maxBounds; proxyFirst = false; }
        proxyLod.minBounds = glm::min(proxyLod.minBounds, geo.mesh.minBounds);
        proxyLod.maxBounds = glm::max(proxyLod.maxBounds, geo.mesh.maxBounds);
        RenderNode rn;
        rn.id = geo.id;
        rn.geometryIndex = static_cast<std::int32_t>(proxyLod.geometries.size());
        rn.semanticNodeIndex = proxyRep[g];
        proxyLod.geometries.push_back(std::move(geo));
        proxyLod.nodes.push_back(std::move(rn));
    }
    out.renderLods.push_back(std::move(proxyLod));
    for (auto& lod : out.renderLods)
    {
        lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
        lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
    }
    Socket hlod;
    hlod.id = "hlod";
    hlod.kind = "hlod";
    hlod.extent = glm::vec3(doc.value("proxy_error_m", 12.0f), 0.0f, 0.0f);
    out.sockets.push_back(hlod);
    out.physicalSize.axis = PhysicalSizeAxis::Z;
    out.physicalSize.sourceExtent = out.maxBounds.z - out.minBounds.z;
    out.physicalSize.targetMeters = out.physicalSize.sourceExtent;

    std::string err;
    if (!ModelAssetBinary::validate(out, &err))
    {
        if (error) *error = "assembled city " + hubId + " invalid: " + err;
        return false;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cerr << "[HubCityAssembly] " << hubId << ": " << out.nodes.size() << " nodes, " << kits.size() << " modules, "
              << out.collisionVolumes.size() << " collision, " << out.renderLods.back().nodes.size() << " group proxies, "
              << ms << " ms\n";
    return true;
}

const ModelAsset* hubCityAsset(const std::string& hubId)
{
    static std::mutex mutex;
    static std::unordered_map<std::string, std::unique_ptr<ModelAsset>> cache;
    std::lock_guard<std::mutex> lock(mutex);
    const auto [it, inserted] = cache.try_emplace(hubId);
    if (inserted)
    {
        auto asset = std::make_unique<ModelAsset>();
        std::string error;
        if (assembleHubCity(hubId, "assets/compiled/hub_modules", *asset, &error))
            it->second = std::move(asset);
        else
            std::cerr << "[HubCityAssembly] " << hubId << ": " << error << "\n";
    }
    return it->second.get();
}

} // namespace elite::model_asset
