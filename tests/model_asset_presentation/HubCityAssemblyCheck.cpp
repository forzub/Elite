// EliteHubCityAssemblyCheck <hub_id> [...] (run from a directory with assets/): assembles each hub city from its frozen
// layout (src/model_asset/HubCityAssembly.h) and compares it with the mesh editor's per-hub package when one exists:
// node ids in order, the module folder of every collision volume per node, collision placement, render-node placement
// per component LOD, LOD triangle counts of placed geometry, bounds. Exit 0 = every hub matches.

#include "src/model_asset/HubCityAssembly.h"
#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetPresentation.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

namespace ma = elite::model_asset;

namespace
{
std::size_t placedTriangles(const ma::ModelAsset& a, std::size_t lod)
{
    std::size_t n = 0;
    const auto& l = a.renderLods[lod];
    for (const auto& rn : l.nodes)
        if (rn.geometryIndex >= 0)
            n += l.geometries[static_cast<std::size_t>(rn.geometryIndex)].mesh.triangles.size();
    return n;
}

float far(const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); }

std::string folderOf(const std::string& id)
{
    const auto a = id.find('.'), b = id.rfind('.');
    return a == std::string::npos || b <= a ? std::string() : id.substr(a + 1, b - a - 1);
}
} // namespace

int main(int argc, char** argv)
{
    int bad = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string hub = argv[i];
        ma::ModelAsset city;
        std::string error;
        if (!ma::assembleHubCity(hub, "assets/compiled/hub_modules", city, &error))
        {
            std::printf("%s: ASSEMBLY FAILED: %s\n", hub.c_str(), error.c_str());
            ++bad;
            continue;
        }
        const ma::presentation::Index index = ma::presentation::buildIndex(city);
        std::printf("%s: assembled %zu nodes, %zu collision, %zu materials, LOD tris placed %zu / %zu / %zu, proxies %zu (%zu tris), "
                    "groups %zu, size (%.0f %.0f %.0f)\n",
                    hub.c_str(), city.nodes.size(), city.collisionVolumes.size(), city.materials.size(), placedTriangles(city, 0),
                    placedTriangles(city, 1), placedTriangles(city, 2), city.renderLods[3].nodes.size(), placedTriangles(city, 3),
                    index.groups.size(), city.maxBounds.x - city.minBounds.x, city.maxBounds.y - city.minBounds.y,
                    city.maxBounds.z - city.minBounds.z);
        const std::string pkg = "assets/compiled/hub_modules/hub_" + hub + "/hub_" + hub + ".elmodel";
        if (!std::filesystem::exists(pkg))
        {
            std::printf("  (no package to compare)\n");
            continue;
        }
        ma::ModelAsset ref;
        if (!ma::ModelAssetBinary::load(pkg, ref, &error))
        {
            std::printf("  package load failed: %s\n", error.c_str());
            ++bad;
            continue;
        }
        int diff = 0;
        const auto note = [&](const char* what, const std::string& detail) {
            if (++diff <= 6) std::printf("  DIFF %s: %s\n", what, detail.c_str());
        };
        if (ref.nodes.size() != city.nodes.size())
            note("node count", std::to_string(city.nodes.size()) + " vs " + std::to_string(ref.nodes.size()));
        for (std::size_t n = 0; n < std::min(ref.nodes.size(), city.nodes.size()); ++n)
            if (ref.nodes[n].id != city.nodes[n].id || ref.nodes[n].moduleId != city.nodes[n].moduleId)
                note("node", city.nodes[n].id + "/" + city.nodes[n].moduleId + " vs " + ref.nodes[n].id + "/" + ref.nodes[n].moduleId);
        // collision: per id (same ids expected), placement within 5 cm
        if (std::getenv("ELITE_CITY_CHECK_GEOMETRY"))
            for (std::size_t l = 0; l < 3 && l < ref.renderLods.size(); ++l)
            {
                std::map<std::string, std::size_t> refTris;
                for (const auto& g : ref.renderLods[l].geometries) refTris[g.id] = g.mesh.triangles.size();
                for (const auto& g : city.renderLods[l].geometries)
                {
                    const auto it = refTris.find(g.id);
                    const std::size_t r = it == refTris.end() ? 0 : it->second;
                    if (r != g.mesh.triangles.size())
                        std::printf("  geometry %s: %zu vs package %zu\n", g.id.c_str(), g.mesh.triangles.size(), r);
                }
            }
        std::map<std::string, const ma::CollisionVolume*> refC;
        for (const auto& c : ref.collisionVolumes) refC[c.id] = &c;
        std::size_t matched = 0;
        for (const auto& c : city.collisionVolumes)
        {
            const auto it = refC.find(c.id);
            if (it == refC.end()) { note("collision missing in package", c.id); continue; }
            ++matched;
            const auto& r = *it->second;
            if (r.parentNodeIndex != c.parentNodeIndex || far(r.localPosition, c.localPosition) > 0.05f ||
                far(r.halfSize, c.halfSize) > 0.05f || folderOf(r.id) != folderOf(c.id))
                note("collision", c.id);
        }
        if (matched != ref.collisionVolumes.size())
            note("collision count", std::to_string(city.collisionVolumes.size()) + " vs " + std::to_string(ref.collisionVolumes.size()));
        // component LODs: render nodes per semantic node at the same place, same placed triangle count
        const ma::presentation::Index refIndex = ma::presentation::buildIndex(ref);
        for (std::size_t l = 0; l < 3 && l < ref.renderLods.size(); ++l)
        {
            const std::size_t a = placedTriangles(city, l), b = placedTriangles(ref, l);
            if (a != b) note("placed triangles", "LOD" + std::to_string(l) + " " + std::to_string(a) + " vs " + std::to_string(b));
            for (std::size_t n = 0; n < std::min(index.components.size(), refIndex.components.size()); ++n)
            {
                const auto& ca = index.components[n].renderNodes, &cb = refIndex.components[n].renderNodes;
                if (l >= ca.size() || l >= cb.size() || ca[l].empty() || cb[l].empty()) continue;
                const glm::vec3 pa(index.renderNodes[l][static_cast<std::size_t>(ca[l].front())].assetTransform[3]);
                const glm::vec3 pb(refIndex.renderNodes[l][static_cast<std::size_t>(cb[l].front())].assetTransform[3]);
                if (far(pa, pb) > 0.05f) { note("render node placement", city.nodes[n].id); break; }
            }
        }
        if (far(index.minBounds, refIndex.minBounds) > 1.0f || far(index.maxBounds, refIndex.maxBounds) > 1.0f)
            note("bounds", "");
        if (index.groups.size() != refIndex.groups.size())
            note("groups", std::to_string(index.groups.size()) + " vs " + std::to_string(refIndex.groups.size()));
        std::printf("  package: %zu nodes, %zu collision, LOD tris placed %zu / %zu / %zu, proxies %zu (%zu tris) -> %s\n",
                    ref.nodes.size(), ref.collisionVolumes.size(), placedTriangles(ref, 0), placedTriangles(ref, 1), placedTriangles(ref, 2),
                    ref.renderLods.size() > 3 ? ref.renderLods[3].nodes.size() : 0, ref.renderLods.size() > 3 ? placedTriangles(ref, 3) : 0,
                    diff ? (std::to_string(diff) + " differences").c_str() : "SAME");
        if (diff) ++bad;
    }
    return bad ? 1 : 0;
}
