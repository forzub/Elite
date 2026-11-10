// Ship model-asset ASSEMBLE (step 3, slice S2; author 2026-11-09: "the hull in compartments, interiors drawn only
// for a breached compartment").
//
//   EliteShipAssetAssemble <source dir with ship_asset.json> <production.elmodel>
//
// Reads the SOURCE written by tools/ships/ship_asset_source.py (semantic nodes, breachable plate / pad states,
// compartments with interior nodes, openings, exterior portals, collision, render LODs whose geometries are OBJ
// files shared by instances), builds a WORKING ModelAsset in metres (explicit 1:1 calibration), then runs the
// engine's own BUILD (makeProductionCopy: metric copy + per-node LOD error) and validation, and writes the package.

#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetBuild.h"

using namespace elite::model_asset;
using json = nlohmann::json;

namespace
{

glm::vec3 vec3(const json& j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

std::vector<std::string> strings(const json& j)
{
    std::vector<std::string> out;
    for (const auto& s : j) out.push_back(s.get<std::string>());
    return out;
}

MeshLod readObj(const std::string& path, const std::map<std::string, std::int32_t>& materials)
{
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<glm::vec3> v, n;
    MeshLod m;
    std::int32_t mat = NoIndex;
    std::string line;
    while (std::getline(in, line))
    {
        if (line.size() < 2) continue;
        std::istringstream s(line);
        std::string tag;
        s >> tag;
        if (tag == "v") { glm::vec3 p; s >> p.x >> p.y >> p.z; v.push_back(p); }
        else if (tag == "vn") { glm::vec3 p; s >> p.x >> p.y >> p.z; n.push_back(p); }
        else if (tag == "usemtl")
        {
            std::string name;
            s >> name;
            const auto it = materials.find(name);
            mat = it == materials.end() ? NoIndex : it->second;
        }
        else if (tag == "f")
        {
            std::vector<std::uint32_t> idx;
            std::string tok;
            while (s >> tok)
            {
                const auto vi = static_cast<std::uint32_t>(std::stoul(tok.substr(0, tok.find('/'))) - 1);
                const auto ni = tok.rfind('/') == std::string::npos ? vi
                                : static_cast<std::uint32_t>(std::stoul(tok.substr(tok.rfind('/') + 1)) - 1);
                Vertex vx;
                vx.position = v.at(vi);
                vx.normal = ni < n.size() ? n[ni] : glm::vec3(0, 1, 0);
                m.vertices.push_back(vx);
                idx.push_back(static_cast<std::uint32_t>(m.vertices.size() - 1));
            }
            for (std::size_t k = 1; k + 1 < idx.size(); ++k)
            {
                Triangle t;
                t.a = idx[0];
                t.b = idx[k];
                t.c = idx[k + 1];
                t.materialIndex = mat;
                m.triangles.push_back(t);
            }
        }
    }
    if (m.vertices.empty()) throw std::runtime_error("empty mesh " + path);
    m.minBounds = m.maxBounds = m.vertices[0].position;
    for (const auto& vx : m.vertices)
    {
        m.minBounds = glm::min(m.minBounds, vx.position);
        m.maxBounds = glm::max(m.maxBounds, vx.position);
    }
    return m;
}

glm::mat3 rotationXYZ(glm::vec3 deg)
{
    const glm::vec3 r = glm::radians(deg);
    const glm::mat3 rx(1, 0, 0, 0, std::cos(r.x), std::sin(r.x), 0, -std::sin(r.x), std::cos(r.x));
    const glm::mat3 ry(std::cos(r.y), 0, -std::sin(r.y), 0, 1, 0, std::sin(r.y), 0, std::cos(r.y));
    const glm::mat3 rz(std::cos(r.z), std::sin(r.z), 0, -std::sin(r.z), std::cos(r.z), 0, 0, 0, 1);
    return rx * ry * rz;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: EliteShipAssetAssemble <source dir> <production.elmodel>\n");
        return 2;
    }
    const std::string dir = argv[1];
    try
    {
        std::ifstream f(dir + "/ship_asset.json");
        if (!f) throw std::runtime_error("cannot open " + dir + "/ship_asset.json");
        const json src = json::parse(f);
        ModelAsset a;
        a.assetId = src.at("asset_id").get<std::string>();
        a.displayName = src.at("display_name").get<std::string>();

        std::map<std::string, std::int32_t> mat;
        for (const auto& [name, kd] : src.at("materials").items())
        {
            MaterialDefinition m;
            m.id = name;
            m.sourceName = name;
            m.baseColor = glm::vec4(vec3(kd), 1.0f);
            mat[name] = static_cast<std::int32_t>(a.materials.size());
            a.materials.push_back(m);
        }

        std::map<std::string, std::int32_t> node;
        for (const auto& j : src.at("nodes"))
        {
            Node n;
            n.id = j.at("id").get<std::string>();
            n.moduleId = j.value("module", std::string());
            node[n.id] = static_cast<std::int32_t>(a.nodes.size());
            a.nodes.push_back(n);
        }
        auto nodeOf = [&](const json& j) -> std::int32_t {
            if (j.is_null()) return NoIndex;
            const auto it = node.find(j.get<std::string>());
            if (it == node.end()) throw std::runtime_error("unknown node " + j.get<std::string>());
            return it->second;
        };
        for (const auto& j : src.at("states"))
            a.stateVariants.push_back({j.at("id").get<std::string>(), j.at("display").get<std::string>(), nodeOf(j.at("node"))});

        std::map<std::string, std::int32_t> comp;
        for (const auto& j : src.at("compartments"))
        {
            Compartment c;
            c.id = j.at("id").get<std::string>();
            c.role = j.at("role").get<std::string>();
            c.localPosition = vec3(j.at("center"));
            c.halfSize = vec3(j.at("half_size"));
            for (const auto& n : j.at("interior_nodes")) c.interiorNodeIndices.push_back(nodeOf(n));
            comp[c.id] = static_cast<std::int32_t>(a.compartments.size());
            a.compartments.push_back(c);
        }
        std::map<std::string, std::int32_t> opening;
        for (const auto& j : src.at("openings"))
        {
            Opening o;
            o.id = j.at("id").get<std::string>();
            o.parentNodeIndex = nodeOf(j.at("node"));  // the breachable node scopes the state (nodes sit at the origin)
            o.activeStates = strings(j.at("active_states"));
            o.localPosition = vec3(j.at("position"));
            o.localRotationDeg = vec3(j.at("rotation_deg"));
            o.halfSize = vec3(j.at("half_size"));
            opening[o.id] = static_cast<std::int32_t>(a.openings.size());
            a.openings.push_back(o);
        }
        for (const auto& j : src.at("portals"))
        {
            CompartmentPortal p;
            p.id = j.at("id").get<std::string>();
            p.compartmentA = comp.at(j.at("a").get<std::string>());
            p.compartmentB = j.at("b").is_null() ? NoIndex : comp.at(j.at("b").get<std::string>());
            p.parentNodeIndex = nodeOf(j.at("node"));
            p.localPosition = vec3(j.at("position"));
            p.localRotationDeg = vec3(j.at("rotation_deg"));
            p.halfSize = vec3(j.at("half_size"));
            p.openingIndex = opening.at(j.at("opening").get<std::string>());
            a.portals.push_back(p);
        }
        for (const auto& j : src.at("collision"))
        {
            CollisionVolume c;
            c.id = j.at("id").get<std::string>();
            c.moduleId = j.value("module", std::string());
            c.parentNodeIndex = j.contains("node") ? nodeOf(j.at("node")) : NoIndex;
            const std::string shape = j.at("shape").get<std::string>();
            c.shape = shape == "cylinder" ? CollisionShape::Cylinder : CollisionShape::Box;
            c.localPosition = vec3(j.at("position"));
            c.localRotationDeg = vec3(j.at("rotation_deg"));
            if (c.shape == CollisionShape::Box) c.halfSize = vec3(j.at("half_size"));
            else { c.radius = j.at("radius").get<float>(); c.halfHeight = j.at("half_height").get<float>(); }
            c.activeStates = strings(j.at("active_states"));
            a.collisionVolumes.push_back(c);
        }

        bool first = true;
        for (const auto& jl : src.at("render_lods"))
        {
            RenderLod lod;
            lod.level = jl.at("level").get<std::uint32_t>();
            lod.sourceKind = "generated";
            const std::string role = jl.at("role").get<std::string>();
            lod.role = role == "shell" ? RenderLodRole::Shell : role == "proxy" ? RenderLodRole::Proxy : RenderLodRole::Component;
            // LOD0 error is 0 by definition; coarser levels get a positive placeholder, BUILD measures per node
            lod.relativeGeometricError = lod.level == 0 ? 0.0f : 0.004f * static_cast<float>(lod.level * lod.level);
            std::map<std::string, std::int32_t> gidx;
            for (const auto& jg : jl.at("geometries"))
            {
                RenderGeometryDefinition g;
                g.id = jg.at("id").get<std::string>();
                g.sourcePath = jg.at("obj").get<std::string>();
                g.surfaceMode = jg.value("two_sided", false) ? SurfaceMode::ThinTwoSided : SurfaceMode::Closed;
                g.mesh = readObj(dir + "/" + g.sourcePath, mat);
                gidx[g.id] = static_cast<std::int32_t>(lod.geometries.size());
                lod.geometries.push_back(std::move(g));
            }
            bool lodFirst = true;
            for (const auto& jn : jl.at("nodes"))
            {
                RenderNode rn;
                rn.id = jn.at("id").get<std::string>();
                rn.geometryIndex = gidx.at(jn.at("geometry").get<std::string>());
                rn.semanticNodeIndex = nodeOf(jn.at("node"));
                rn.activeStates = strings(jn.at("active_states"));
                rn.localPosition = vec3(jn.at("position"));
                rn.localRotationDeg = vec3(jn.at("rotation_deg"));
                const auto& gm = lod.geometries[static_cast<std::size_t>(rn.geometryIndex)].mesh;
                const glm::mat3 R = rotationXYZ(rn.localRotationDeg);
                for (int cx = 0; cx < 2; ++cx)
                    for (int cy = 0; cy < 2; ++cy)
                        for (int cz = 0; cz < 2; ++cz)
                        {
                            const glm::vec3 c(cx ? gm.maxBounds.x : gm.minBounds.x, cy ? gm.maxBounds.y : gm.minBounds.y,
                                              cz ? gm.maxBounds.z : gm.minBounds.z);
                            const glm::vec3 p = R * c + rn.localPosition;
                            if (lodFirst) { lod.minBounds = lod.maxBounds = p; lodFirst = false; }
                            lod.minBounds = glm::min(lod.minBounds, p);
                            lod.maxBounds = glm::max(lod.maxBounds, p);
                        }
                lod.nodes.push_back(rn);
            }
            lod.declaredGeometryCount = static_cast<std::uint32_t>(lod.geometries.size());
            lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
            if (first) { a.minBounds = lod.minBounds; a.maxBounds = lod.maxBounds; first = false; }
            a.minBounds = glm::min(a.minBounds, lod.minBounds);
            a.maxBounds = glm::max(a.maxBounds, lod.maxBounds);
            a.renderLods.push_back(std::move(lod));
        }

        // explicit 1:1 calibration (the source is already in metres), then the engine's BUILD
        a.physicalSize.enabled = true;
        a.physicalSize.axis = PhysicalSizeAxis::Z;
        a.physicalSize.sourceExtent = a.maxBounds.z - a.minBounds.z;
        a.physicalSize.targetMeters = a.physicalSize.sourceExtent;
        a.physicalSize.sourceToMeters = 1.0f;
        a.physicalSize.geometrySpace = PhysicalGeometrySpace::Authoring;
        std::string error;
        if (!ModelAssetBinary::validate(a, &error)) throw std::runtime_error("working asset invalid: " + error);
        ModelAsset production = makeProductionCopy(a);
        if (!ModelAssetBinary::validate(production, &error)) throw std::runtime_error("production invalid: " + error);
        if (!ModelAssetBinary::save(argv[2], production, &error)) throw std::runtime_error("save failed: " + error);
        std::size_t tris = 0;
        for (const auto& g : production.renderLods[0].geometries) tris += g.mesh.triangles.size();
        std::printf("assembled %s: %zu nodes, %zu states, %zu compartments, %zu portals, %zu collision, %zu LODs "
                    "(LOD0 %zu unique-geometry triangles) -> %s\n",
                    a.assetId.c_str(), a.nodes.size(), a.stateVariants.size(), a.compartments.size(), a.portals.size(),
                    a.collisionVolumes.size(), a.renderLods.size(), tris, argv[2]);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "ASSEMBLE failed: %s\n", e.what());
        return 1;
    }
}
