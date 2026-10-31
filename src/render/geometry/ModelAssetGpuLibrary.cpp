#include "src/render/geometry/ModelAssetGpuLibrary.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"

namespace render::geometry
{
namespace
{
namespace ma = elite::model_asset;

// Elite / Technical edge lines: authored, boundary and crease edges only (the
// triangulation interior and smooth polygon seams are not lines).
bool lineEdge(const ma::Edge& e)
{
    if (!(e.renderMask & ma::EdgeRenderElite)) return false;
    if (e.flags & ma::EdgeTriangulationInternal) return false;
    return (e.flags & (ma::EdgeBoundary | ma::EdgeCrease | ma::EdgeAuthored)) != 0;
}

game::ship::geometry::MeshData toMeshData(const ma::MeshLod& mesh)
{
    game::ship::geometry::MeshData out;
    out.vertices.reserve(mesh.vertices.size());
    for (const auto& v : mesh.vertices)
        out.vertices.push_back({v.position, v.normal, glm::vec3(0.0f), glm::vec3(1.0f)});
    out.triangles.reserve(mesh.triangles.size());
    for (const auto& t : mesh.triangles)
        out.triangles.push_back({static_cast<int>(t.a), static_cast<int>(t.b), static_cast<int>(t.c), t.sourcePolygonId});
    for (const auto& e : mesh.edges)
        if (lineEdge(e) && e.a < mesh.vertices.size() && e.b < mesh.vertices.size())
            out.edges.push_back({mesh.vertices[e.a].position, mesh.vertices[e.b].position});
    out.minBounds = mesh.minBounds;
    out.maxBounds = mesh.maxBounds;
    out.boundCenter = 0.5f * (mesh.minBounds + mesh.maxBounds);
    out.boundRadius = 0.5f * glm::length(mesh.maxBounds - mesh.minBounds);
    return out;
}

const char* packageFor(ObjectType type)
{
    switch (type)
    {
        case ObjectType::Station: return "assets/compiled/models/station/station.elmodel";
        default: return nullptr;
    }
}

// Legacy rotating module that carries a semantic node (by authored part name):
// keeps the habitat ring spinning exactly like the legacy assembly.
std::string legacyModuleFor(ObjectType type, const std::string& nodeId)
{
    if (type != ObjectType::Station) return {};
    if (nodeId.find("Habitat") != std::string::npos || nodeId.find("Core_Front") != std::string::npos)
        return "station_habitat_ring";
    if (nodeId.find("Core_Rear") != std::string::npos) return "station_core";
    return "station_solar_panels";
}

std::unique_ptr<ModelAssetGpu> load(ObjectType type)
{
    if (const char* legacy = std::getenv("ELITE_LEGACY_MODEL_ASSETS"); legacy && *legacy && *legacy != '0')
        return nullptr;
    const char* path = packageFor(type);
    if (!path || !std::filesystem::exists(path))
    {
        if (path)
            std::cerr << "[ModelAssetGpu] no production package " << path << " - legacy assembly path\n";
        return nullptr;
    }
    auto gpu = std::make_unique<ModelAssetGpu>();
    std::string error;
    if (!ma::ModelAssetBinary::load(path, gpu->asset, &error))
    {
        std::cerr << "[ModelAssetGpu] load failed " << path << ": " << error << " - legacy assembly path\n";
        return nullptr;
    }
    if (gpu->asset.physicalSize.geometrySpace != ma::PhysicalGeometrySpace::Meters)
    {
        std::cerr << "[ModelAssetGpu] " << path << " is not a metric BUILD package - legacy assembly path\n";
        return nullptr;
    }
    gpu->path = path;
    gpu->index = ma::presentation::buildIndex(gpu->asset);

    // Same frame as the legacy assembly: descriptor mesh basis, then the
    // descriptor logical size measured along its scale reference axis.
    const auto& desc = ObjectDescriptorRegistry::get(type);
    const glm::mat4 basis = desc.meshToLogicalBasis4();
    glm::vec3 mn(1.0e30f), mx(-1.0e30f);
    for (int i = 0; i < 8; ++i)
    {
        const glm::vec3 c((i & 1) ? gpu->index.maxBounds.x : gpu->index.minBounds.x,
                          (i & 2) ? gpu->index.maxBounds.y : gpu->index.minBounds.y,
                          (i & 4) ? gpu->index.maxBounds.z : gpu->index.minBounds.z);
        const glm::vec3 p = glm::vec3(basis * glm::vec4(c, 1.0f));
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }
    const glm::vec3 size = mx - mn;
    const auto& dims = desc.logicalDimensions();
    float scale = 1.0f;
    if (dims.enabled)
    {
        switch (dims.scaleReference)
        {
            case ScaleReference::Length: scale = size.z > 1e-6f ? dims.length / size.z : 1.0f; break;
            case ScaleReference::Width: scale = size.x > 1e-6f ? dims.width / size.x : 1.0f; break;
            case ScaleReference::Height: scale = size.y > 1e-6f ? dims.height / size.y : 1.0f; break;
            case ScaleReference::None: break;
        }
    }
    gpu->assetToLogicalScale = scale;
    gpu->assetToLogical = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * basis;
    for (const auto& node : gpu->asset.nodes)
        gpu->nodeModule.push_back(legacyModuleFor(type, node.id));

    std::size_t tris = 0;
    for (const auto& lod : gpu->asset.renderLods)
        for (const auto& g : lod.geometries) tris += g.mesh.triangles.size();
    std::cerr << "[ModelAssetGpu] " << path << " components=" << gpu->asset.nodes.size()
              << " lods=" << gpu->asset.renderLods.size() << " tris(all lods)=" << tris
              << " asset->logical scale=" << scale << " logical size=(" << size.x * scale << ", " << size.y * scale
              << ", " << size.z * scale << ") m\n";
    return gpu;
}
} // namespace

const MeshGPU* ModelAssetGpu::gpuFor(std::int32_t lod, std::int32_t geometry) const
{
    if (lod < 0 || static_cast<std::size_t>(lod) >= asset.renderLods.size()) return nullptr;
    const auto& lodData = asset.renderLods[static_cast<std::size_t>(lod)];
    if (geometry < 0 || static_cast<std::size_t>(geometry) >= lodData.geometries.size()) return nullptr;
    if (m_gpu.size() != asset.renderLods.size()) m_gpu.resize(asset.renderLods.size());
    auto& slots = m_gpu[static_cast<std::size_t>(lod)];
    if (slots.size() != lodData.geometries.size()) slots.resize(lodData.geometries.size());
    auto& slot = slots[static_cast<std::size_t>(geometry)];
    if (!slot)
    {
        const auto& mesh = lodData.geometries[static_cast<std::size_t>(geometry)].mesh;
        slot = std::make_unique<MeshGPU>();
        slot->upload(toMeshData(mesh));
        m_uploadedTriangles += mesh.triangles.size();
        // MeshGPU expands triangles: 3 MeshVertex per triangle + edge lines.
        m_uploadedBytes += mesh.triangles.size() * 3 * sizeof(game::ship::geometry::MeshVertex);
        ++m_uploadedMeshes;
    }
    return slot.get();
}

const ModelAssetGpu* ModelAssetGpuLibrary::get(ObjectType type)
{
    static std::unordered_map<std::uint16_t, std::unique_ptr<ModelAssetGpu>> cache;
    static std::unordered_map<std::uint16_t, bool> tried;
    const auto key = static_cast<std::uint16_t>(type);
    if (!tried[key])
    {
        tried[key] = true;
        cache[key] = load(type);
    }
    const auto it = cache.find(key);
    return it == cache.end() ? nullptr : it->second.get();
}

} // namespace render::geometry
