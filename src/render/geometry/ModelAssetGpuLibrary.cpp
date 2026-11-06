#include "src/render/geometry/ModelAssetGpuLibrary.h"

#include <chrono>
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

// Each triangle carries its MaterialDefinition.baseColor (the model's own
// colour); every renderer style interprets it.
game::ship::geometry::MeshData toMeshData(const ma::MeshLod& mesh, const std::vector<ma::MaterialDefinition>& materials)
{
    game::ship::geometry::MeshData out;
    out.vertices.reserve(mesh.vertices.size());
    for (const auto& v : mesh.vertices)
        out.vertices.push_back({v.position, v.normal, glm::vec3(0.0f), glm::vec3(1.0f)});
    out.triangles.reserve(mesh.triangles.size());
    for (const auto& t : mesh.triangles)
    {
        game::ship::geometry::MeshTriangle tri {static_cast<int>(t.a), static_cast<int>(t.b), static_cast<int>(t.c),
                                                t.sourcePolygonId};
        if (t.materialIndex >= 0 && static_cast<std::size_t>(t.materialIndex) < materials.size())
        {
            tri.baseColor = glm::vec3(materials[static_cast<std::size_t>(t.materialIndex)].baseColor);
            tri.hasMaterialColor = true;
        }
        out.triangles.push_back(tri);
    }
    const auto faceNormal = [&](std::int32_t t) {
        const auto& tri = mesh.triangles[static_cast<std::size_t>(t)];
        const glm::vec3 n = glm::cross(mesh.vertices[tri.b].position - mesh.vertices[tri.a].position,
                                       mesh.vertices[tri.c].position - mesh.vertices[tri.a].position);
        const float len = glm::length(n);
        return len > 1.0e-12f ? n / len : glm::vec3(0.0f, 0.0f, 1.0f);
    };
    const auto validTri = [&](std::int32_t t) {
        return t >= 0 && static_cast<std::size_t>(t) < mesh.triangles.size();
    };
    for (const auto& e : mesh.edges)
        if (lineEdge(e) && e.a < mesh.vertices.size() && e.b < mesh.vertices.size())
        {
            game::ship::geometry::MeshEdge edge {mesh.vertices[e.a].position, mesh.vertices[e.b].position};
            if ((e.flags & (ma::EdgeBoundary | ma::EdgeAuthored)) || !validTri(e.triangleA) || !validTri(e.triangleB))
                edge.weight = 1.0f;
            else if (mesh.triangles[static_cast<std::size_t>(e.triangleA)].materialIndex !=
                     mesh.triangles[static_cast<std::size_t>(e.triangleB)].materialIndex)
                edge.weight = 0.85f;
            else
                edge.weight = game::ship::geometry::creaseEdgeWeight(
                    glm::clamp(glm::dot(faceNormal(e.triangleA), faceNormal(e.triangleB)), -1.0f, 1.0f));
            out.edges.push_back(edge);
        }
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
    gpu->startPrewarm();
    return gpu;
}
} // namespace

ModelAssetGpu::~ModelAssetGpu()
{
    m_stop = true;
    if (m_worker.joinable())
        m_worker.join();
}

void ModelAssetGpu::ensureSlots(std::int32_t lod) const
{
    if (m_gpu.size() != asset.renderLods.size())
    {
        m_gpu.resize(asset.renderLods.size());
        m_complete.resize(asset.renderLods.size());
    }
    const auto n = asset.renderLods[static_cast<std::size_t>(lod)].geometries.size();
    auto& slots = m_gpu[static_cast<std::size_t>(lod)];
    if (slots.size() != n)
    {
        slots.resize(n);
        m_complete[static_cast<std::size_t>(lod)].resize(n, 0);
    }
}

void ModelAssetGpu::countUpload(std::size_t verts) const
{
    m_uploadedTriangles += verts / 3;
    m_uploadedBytes += verts * sizeof(game::ship::geometry::MeshVertex);
    ++m_uploadedMeshes;
}

void ModelAssetGpu::startPrewarm() const
{
    if (m_worker.joinable())
        return;
    m_worker = std::thread([this] {
        // coarse LODs first: the far view is complete soonest; LOD0 (the largest) last
        for (std::int32_t lod = static_cast<std::int32_t>(asset.renderLods.size()) - 1; lod >= 0 && !m_stop; --lod)
        {
            const auto& lodData = asset.renderLods[static_cast<std::size_t>(lod)];
            for (std::size_t g = 0; g < lodData.geometries.size() && !m_stop; ++g)
            {
                Piece piece;
                piece.lod = lod;
                piece.geometry = static_cast<std::int32_t>(g);
                piece.prepared = std::make_unique<MeshGPU::Prepared>(
                    MeshGPU::prepare(toMeshData(lodData.geometries[g].mesh, asset.materials)));
                std::lock_guard<std::mutex> lock(m_mutex);
                m_ready.push_back(std::move(piece));
            }
        }
    });
}

void ModelAssetGpu::finishCurrent() const
{
    if (!m_current)
        return;
    auto& slot = m_gpu[static_cast<std::size_t>(m_current->lod)][static_cast<std::size_t>(m_current->geometry)];
    slot->uploadChunk(*m_current->prepared, m_current->offset, m_current->prepared->verts.size());
    m_complete[static_cast<std::size_t>(m_current->lod)][static_cast<std::size_t>(m_current->geometry)] = 1;
    countUpload(m_current->prepared->verts.size());
    m_current.reset();
}

void ModelAssetGpu::pumpPrewarm(double budgetMs) const
{
    const auto t0 = std::chrono::steady_clock::now();
    const auto elapsed = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); };
    constexpr std::size_t ChunkVerts = 32768;   // ~1.5 MB of MeshVertex per glBufferSubData
    while (elapsed() < budgetMs)
    {
        if (m_current)
        {
            auto& slot = m_gpu[static_cast<std::size_t>(m_current->lod)][static_cast<std::size_t>(m_current->geometry)];
            m_current->offset = slot->uploadChunk(*m_current->prepared, m_current->offset, ChunkVerts);
            if (m_current->offset >= m_current->prepared->verts.size())
            {
                m_complete[static_cast<std::size_t>(m_current->lod)][static_cast<std::size_t>(m_current->geometry)] = 1;
                countUpload(m_current->prepared->verts.size());
                m_current.reset();
            }
            continue;
        }
        Piece piece;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_ready.empty())
                return;
            piece = std::move(m_ready.front());
            m_ready.pop_front();
        }
        ensureSlots(piece.lod);
        auto& slot = m_gpu[static_cast<std::size_t>(piece.lod)][static_cast<std::size_t>(piece.geometry)];
        if (slot)
            continue;   // already uploaded synchronously (needed before the prewarm reached it)
        slot = std::make_unique<MeshGPU>();
        slot->allocate(*piece.prepared, false);
        m_current = std::make_unique<Piece>(std::move(piece));
    }
}

const MeshGPU* ModelAssetGpu::gpuFor(std::int32_t lod, std::int32_t geometry) const
{
    if (lod < 0 || static_cast<std::size_t>(lod) >= asset.renderLods.size()) return nullptr;
    const auto& lodData = asset.renderLods[static_cast<std::size_t>(lod)];
    if (geometry < 0 || static_cast<std::size_t>(geometry) >= lodData.geometries.size()) return nullptr;
    ensureSlots(lod);
    auto& slot = m_gpu[static_cast<std::size_t>(lod)][static_cast<std::size_t>(geometry)];
    if (slot && m_complete[static_cast<std::size_t>(lod)][static_cast<std::size_t>(geometry)])
        return slot.get();
    if (slot)
    {
        finishCurrent();   // the one being uploaded in chunks: finish it now
        return slot.get();
    }
    // prepared by the loader thread already? upload it whole; else build it here (the old path)
    std::unique_ptr<MeshGPU::Prepared> prepared;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_ready.begin(); it != m_ready.end(); ++it)
            if (it->lod == lod && it->geometry == geometry)
            {
                prepared = std::move(it->prepared);
                m_ready.erase(it);
                break;
            }
    }
    const auto t0 = std::chrono::steady_clock::now();
    const bool fromLoader = prepared != nullptr;
    if (!prepared)
        prepared = std::make_unique<MeshGPU::Prepared>(
            MeshGPU::prepare(toMeshData(lodData.geometries[static_cast<std::size_t>(geometry)].mesh, asset.materials)));
    slot = std::make_unique<MeshGPU>();
    slot->allocate(*prepared, true);
    m_complete[static_cast<std::size_t>(lod)][static_cast<std::size_t>(geometry)] = 1;
    countUpload(prepared->verts.size());
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (ms > 2.0)
        std::cerr << "[ModelAssetGpu] synchronous upload lod " << lod << " geometry " << geometry << ": "
                  << prepared->verts.size() / 3 << " tris in " << ms << " ms ("
                  << (fromLoader ? "prepared by the loader" : "built here") << ")" << std::endl;
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
