#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "src/game/geometry/ObjectAssembly.h"
#include "src/game/ship/ShipDescriptor.h"

// Placeholder geometry of a catalog ship without a mesh: its passport
// primitive blocks (ShipDescriptor::primitiveBlocks - hull sections and one
// box per installed position, at their real size and place) as an ordinary
// CPU ObjectAssembly, so the existing ship renderer, LOD proxy and module
// visibility (a destroyed module's box disappears) draw it unchanged.
// One assembly per passport id, built on first use, never destroyed.
namespace game::ship::geometry
{
namespace catalog_assembly_detail
{
// Category colours (linear albedo), the same palette as the designer drawings.
[[nodiscard]] inline glm::vec3 colourOf(const std::string& category)
{
    static const std::map<std::string, glm::vec3> palette = {
        {"hull", {0.42f, 0.45f, 0.50f}},          {"drive", {0.82f, 0.30f, 0.06f}},
        {"reactor", {0.66f, 0.10f, 0.10f}},       {"service_reactor", {0.55f, 0.22f, 0.22f}},
        {"coolant_loop", {0.05f, 0.27f, 0.66f}},  {"radiator", {0.22f, 0.48f, 0.79f}},
        {"jump_core", {0.33f, 0.10f, 0.63f}},     {"tank", {0.43f, 0.25f, 0.67f}},
        {"rcs", {0.67f, 0.43f, 0.05f}},           {"computer", {0.05f, 0.52f, 0.33f}},
        {"radar", {0.03f, 0.38f, 0.25f}},         {"comm", {0.09f, 0.57f, 0.43f}},
        {"transponder", {0.09f, 0.57f, 0.43f}},   {"life_support", {0.10f, 0.36f, 0.06f}},
        {"escape_capsule", {0.26f, 0.56f, 0.16f}}, {"lifeboat", {0.26f, 0.56f, 0.16f}},
        {"container_rack", {0.26f, 0.26f, 0.26f}}, {"train_module", {0.26f, 0.26f, 0.26f}},
        {"passenger_module", {0.36f, 0.35f, 0.19f}}, {"weapon", {0.52f, 0.04f, 0.03f}},
        {"armour", {0.11f, 0.15f, 0.20f}},        {"docking_port", {0.63f, 0.63f, 0.63f}},
    };
    const auto it = palette.find(category);
    return it == palette.end() ? glm::vec3(0.50f, 0.50f, 0.52f) : it->second;
}

// 24 vertices (flat face normals), 12 triangles, 12 feature edges.
inline void appendBox(MeshData& mesh, const glm::vec3& c, const glm::vec3& h, const glm::vec3& colour)
{
    static const glm::vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int f = 0; f < 6; ++f)
    {
        const glm::vec3 n = normals[f];
        const glm::vec3 u = std::abs(n.x) > 0.5f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        const glm::vec3 v = glm::cross(n, u);
        const int base = static_cast<int>(mesh.vertices.size());
        const glm::vec3 corners[4] = {n - u - v, n + u - v, n + u + v, n - u + v};
        for (const auto& k : corners)
        {
            MeshVertex vert;
            vert.position = c + k * h;
            vert.normal = n;
            vert.bary = glm::vec3(0.0f);
            vert.debugColor = colour;
            mesh.vertices.push_back(vert);
        }
        // winding: counter-clockwise seen from outside
        const bool flip = glm::dot(glm::cross(corners[1] - corners[0], corners[2] - corners[0]), n) < 0.0f;
        const int tri[2][3] = {{0, 1, 2}, {0, 2, 3}};
        for (const auto& t : tri)
        {
            MeshTriangle triangle;
            triangle.v0 = base + t[0];
            triangle.v1 = base + (flip ? t[2] : t[1]);
            triangle.v2 = base + (flip ? t[1] : t[2]);
            triangle.faceId = static_cast<int>(mesh.triangles.size());
            triangle.baseColor = colour;
            triangle.hasMaterialColor = true;
            mesh.triangles.push_back(triangle);
            mesh.triangleNormals.push_back(n);
        }
        MeshFace face;
        face.vertices = {base, base + 1, base + 2, base + 3};
        mesh.faces.push_back(face);
    }
    const glm::vec3 lo = c - h, hi = c + h;
    const glm::vec3 p[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                            {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    const int e[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& k : e)
        mesh.edges.push_back({p[k[0]], p[k[1]], 1.0f});
}

inline void finish(MeshData& mesh)
{
    mesh.computeBounds();
    mesh.computeBoundingSphere();
}

[[nodiscard]] inline ObjectAssembly build(const ShipDescriptor& d)
{
    ObjectAssembly a;
    a.typeId = ObjectType::None;
    a.lodSwitchDistance = std::max(2500.0f, d.logicalDimensionsValue.length * 40.0f);
    std::map<std::string, std::size_t> moduleIndex;
    MeshData proxy;
    for (const auto& b : d.primitiveBlocks)
    {
        if (b.moduleId.empty())
            continue;
        auto [it, inserted] = moduleIndex.emplace(b.moduleId, a.modules.size());
        if (inserted)
        {
            AssemblyModule m;
            m.id = b.moduleId;
            m.subsystemId = b.category;
            a.modules.push_back(std::move(m));
        }
        auto& module = a.modules[it->second];
        AssemblyMeshPart part;
        part.id = b.moduleId + "#" + std::to_string(module.meshes.size());
        const glm::vec3 colour = colourOf(b.category);
        appendBox(part.lod0Mesh, b.centerMeters, b.halfSizeMeters, colour);
        finish(part.lod0Mesh);
        part.lod1Mesh = part.lod0Mesh;
        part.minBounds = part.rawMinBounds = part.lod0Mesh.minBounds;
        part.maxBounds = part.rawMaxBounds = part.lod0Mesh.maxBounds;
        part.boundCenter = part.rawCenter = (part.minBounds + part.maxBounds) * 0.5f;
        part.boundHalfSize = (part.maxBounds - part.minBounds) * 0.5f;
        appendBox(proxy, b.centerMeters, b.halfSizeMeters, colour);
        module.meshes.push_back(std::move(part));
    }
    for (auto& m : a.modules)
    {
        glm::vec3 lo(1e30f), hi(-1e30f);
        for (const auto& p : m.meshes)
            lo = glm::min(lo, p.minBounds), hi = glm::max(hi, p.maxBounds);
        m.minBounds = lo;
        m.maxBounds = hi;
        m.boundCenter = (lo + hi) * 0.5f;
        m.boundHalfSize = (hi - lo) * 0.5f;
    }
    finish(proxy);
    a.minBounds = proxy.minBounds;
    a.maxBounds = proxy.maxBounds;
    a.boundCenter = proxy.boundCenter;
    a.boundRadius = proxy.boundRadius;
    a.hasWholeShipProxy = true;
    a.wholeShipProxyMesh = std::move(proxy);
    a.wholeShipProxyBoundCenter = a.boundCenter;
    a.wholeShipProxyBoundRadius = a.boundRadius;
    return a;
}
} // namespace catalog_assembly_detail

[[nodiscard]] inline const ObjectAssembly& catalogShipAssembly(const ShipDescriptor& d)
{
    static std::mutex mutex;
    static auto* cache = new std::map<std::string, std::unique_ptr<const ObjectAssembly>>();   // never destroyed
    std::lock_guard lock(mutex);
    auto& slot = (*cache)[d.catalog.passportId];
    if (!slot)
        slot = std::make_unique<const ObjectAssembly>(catalog_assembly_detail::build(d));
    return *slot;
}
} // namespace game::ship::geometry
