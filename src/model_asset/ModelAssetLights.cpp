#include "src/model_asset/ModelAssetLights.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace elite::model_asset
{
namespace
{
AssetLightKind kindOf(const MaterialDefinition& m)
{
    const std::string& a = m.id;
    const std::string& b = m.sourceName;
    const auto has = [&](const char* s) { return a.find(s) != std::string::npos || b.find(s) != std::string::npos; };
    if (has("beacon")) return AssetLightKind::Beacon;
    if (has("flood")) return AssetLightKind::Flood;
    if (has("flare")) return AssetLightKind::Flare;
    if (has("nav")) return AssetLightKind::Nav;
    if (has("window")) return AssetLightKind::Window;
    return AssetLightKind::Other;
}

bool isFacade(const MaterialDefinition& m)
{
    return m.id.rfind("eq_hub_facade", 0) == 0 || m.sourceName.rfind("eq_hub_facade", 0) == 0;
}

// how bright a square metre of each kind is against a lit window (beacons and floodlights are lamps, not panes)
float brightnessOf(AssetLightKind k)
{
    switch (k)
    {
        case AssetLightKind::Beacon: return 25.0f;
        case AssetLightKind::Flood: return 30.0f;
        case AssetLightKind::Flare: return 8.0f;
        case AssetLightKind::Nav: return 15.0f;
        case AssetLightKind::Window: return 1.0f;
        case AssetLightKind::Facade: return 0.12f;   // ~10-15 % of the panes lit on average (large_object_fill.frag)
        default: return 2.0f;
    }
}

struct Cell
{
    glm::dvec3 weighted {0.0};
    double area = 0.0;
};
} // namespace

std::vector<AssetLight> extractGeometryLights(const MeshLod& mesh, const std::vector<MaterialDefinition>& materials,
                                              float cellMeters, float facadeCellMeters)
{
    // key: material index, cell (x, y, z)
    struct Key
    {
        std::int32_t mat;
        std::int32_t x, y, z;
        bool operator==(const Key& o) const { return mat == o.mat && x == o.x && y == o.y && z == o.z; }
    };
    struct KeyHash
    {
        std::size_t operator()(const Key& k) const
        {
            std::size_t h = static_cast<std::size_t>(k.mat) * 0x9E3779B97F4A7C15ull;
            h ^= static_cast<std::size_t>(k.x) * 0x85EBCA77u + (h << 6) + (h >> 2);
            h ^= static_cast<std::size_t>(k.y) * 0xC2B2AE3Du + (h << 6) + (h >> 2);
            h ^= static_cast<std::size_t>(k.z) * 0x27D4EB2Fu + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<Key, Cell, KeyHash> cells;
    std::vector<std::int8_t> glow(materials.size(), 0);   // 0 none, 1 emissive, 2 facade
    for (std::size_t i = 0; i < materials.size(); ++i)
    {
        const auto& m = materials[i];
        if (m.emissiveStrength > 0.0f && glm::length(m.emissiveColor) > 0.0f)
            glow[i] = 1;
        else if (isFacade(m))
            glow[i] = 2;
    }
    for (const auto& t : mesh.triangles)
    {
        if (t.materialIndex < 0 || static_cast<std::size_t>(t.materialIndex) >= materials.size())
            continue;
        const std::int8_t g = glow[static_cast<std::size_t>(t.materialIndex)];
        if (!g || t.a >= mesh.vertices.size() || t.b >= mesh.vertices.size() || t.c >= mesh.vertices.size())
            continue;
        const glm::dvec3 a(mesh.vertices[t.a].position), b(mesh.vertices[t.b].position), c(mesh.vertices[t.c].position);
        const double area = 0.5 * glm::length(glm::cross(b - a, c - a));
        if (area <= 1.0e-6)
            continue;
        const glm::dvec3 centre = (a + b + c) / 3.0;
        const double cell = g == 2 ? facadeCellMeters : cellMeters;
        const Key k {t.materialIndex, static_cast<std::int32_t>(std::floor(centre.x / cell)),
                     static_cast<std::int32_t>(std::floor(centre.y / cell)), static_cast<std::int32_t>(std::floor(centre.z / cell))};
        auto& e = cells[k];
        e.weighted += centre * area;
        e.area += area;
    }
    std::vector<AssetLight> out;
    out.reserve(cells.size());
    for (const auto& [k, e] : cells)
    {
        const auto& m = materials[static_cast<std::size_t>(k.mat)];
        AssetLight l;
        l.position = glm::vec3(e.weighted / e.area);
        if (glow[static_cast<std::size_t>(k.mat)] == 2)
        {
            l.kind = AssetLightKind::Facade;
            l.color = glm::vec3(1.0f, 0.78f, 0.50f);
        }
        else
        {
            l.kind = kindOf(m);
            const glm::vec3 c = m.emissiveColor;
            const float mx = std::max({c.x, c.y, c.z, 1.0e-6f});
            l.color = c / mx;
        }
        // a closed glowing box shows about half of its surface to any one side
        l.areaM2 = static_cast<float>(0.5 * e.area);
        l.power = l.areaM2 * brightnessOf(l.kind);
        out.push_back(l);
    }
    return out;
}

std::vector<AssetLight> extractAssetLights(const ModelAsset& asset, const presentation::Index& index)
{
    std::vector<AssetLight> out;
    // the most detailed component LOD
    std::int32_t lod0 = -1;
    std::uint32_t best = 0xffffffffu;
    for (std::size_t li = 0; li < asset.renderLods.size(); ++li)
        if (asset.renderLods[li].role == RenderLodRole::Component && asset.renderLods[li].level < best)
        {
            best = asset.renderLods[li].level;
            lod0 = static_cast<std::int32_t>(li);
        }
    if (lod0 < 0 || static_cast<std::size_t>(lod0) >= index.renderNodes.size())
        return out;
    const auto& lod = asset.renderLods[static_cast<std::size_t>(lod0)];
    const auto& infos = index.renderNodes[static_cast<std::size_t>(lod0)];
    std::vector<std::vector<AssetLight>> perGeometry(lod.geometries.size());
    std::vector<char> done(lod.geometries.size(), 0);
    for (std::size_t n = 0; n < lod.nodes.size() && n < infos.size(); ++n)
    {
        const auto& rn = lod.nodes[n];
        // state-bound render nodes (damaged / breached variants) are not the standing lights
        if (!rn.enabled || !rn.activeStates.empty() || rn.geometryIndex < 0 ||
            static_cast<std::size_t>(rn.geometryIndex) >= lod.geometries.size())
            continue;
        const auto g = static_cast<std::size_t>(rn.geometryIndex);
        if (!done[g])
        {
            perGeometry[g] = extractGeometryLights(lod.geometries[g].mesh, asset.materials);
            done[g] = 1;
        }
        const glm::mat4& T = infos[n].assetTransform;
        for (AssetLight l : perGeometry[g])
        {
            l.position = glm::vec3(T * glm::vec4(l.position, 1.0f));
            out.push_back(l);
        }
    }
    return out;
}
} // namespace elite::model_asset
