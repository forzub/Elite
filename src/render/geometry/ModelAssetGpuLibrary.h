#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/geometry/MeshGPU.h"
#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/ModelAssetPresentation.h"
#include "src/world/types/ObjectType.h"

namespace render::geometry
{

// Client-side GPU sidecar of a compiled ModelAsset v4 production package
// (assets/compiled/models/<id>/<id>.elmodel). The CPU asset + presentation
// index are loaded once; each (LOD, geometry) mesh is uploaded lazily the
// first time the presentation selects it, so a far object never pays for
// LOD0. Simulation / collision do NOT read this (they stay on the legacy
// assembly until the hit / collision migration).
struct ModelAssetGpu
{
    elite::model_asset::ModelAsset asset;
    elite::model_asset::presentation::Index index;
    // Asset (production metres, authoring basis) -> object logical frame:
    // the descriptor mesh basis and the descriptor logical size, i.e. the same
    // frame the legacy assembly, its hit volumes and docking ports use.
    glm::mat4 assetToLogical {1.0f};
    float assetToLogicalScale = 1.0f;
    // Semantic node -> legacy assembly module id (rotating modules), "" = none.
    std::vector<std::string> nodeModule;
    std::string path;

    ~ModelAssetGpu();
    const MeshGPU* gpuFor(std::int32_t lod, std::int32_t geometry) const;
    // Prewarm (no first-sight stalls): a loader thread prepares every (LOD, geometry) on the CPU,
    // coarse LODs first; pumpPrewarm() puts them on the GPU in chunks within a per-frame budget,
    // before the presentation selects them. A mesh needed earlier is finished synchronously.
    void startPrewarm() const;
    void pumpPrewarm(double budgetMs) const;
    std::size_t uploadedTriangles() const { return m_uploadedTriangles; }
    std::size_t uploadedBytes() const { return m_uploadedBytes; }
    std::size_t uploadedMeshes() const { return m_uploadedMeshes; }

private:
    struct Piece
    {
        std::int32_t lod = 0;
        std::int32_t geometry = 0;
        std::unique_ptr<MeshGPU::Prepared> prepared;
        std::size_t offset = 0;
    };
    void ensureSlots(std::int32_t lod) const;
    void finishCurrent() const;
    void countUpload(std::size_t verts) const;
    mutable std::vector<std::vector<std::unique_ptr<MeshGPU>>> m_gpu; // [lod][geometry]
    mutable std::vector<std::vector<char>> m_complete;              // [lod][geometry] fully on the GPU
    mutable std::mutex m_mutex;
    mutable std::deque<Piece> m_ready;        // prepared by the loader thread, not yet uploaded
    mutable std::unique_ptr<Piece> m_current; // uploading in chunks
    mutable std::thread m_worker;
    mutable std::atomic<bool> m_stop {false};
    mutable std::size_t m_uploadedTriangles = 0;
    mutable std::size_t m_uploadedBytes = 0;
    mutable std::size_t m_uploadedMeshes = 0;
};

class ModelAssetGpuLibrary
{
public:
    // nullptr when the object type has no production package (or
    // ELITE_LEGACY_MODEL_ASSETS=1): the caller keeps the legacy assembly path.
    static const ModelAssetGpu* get(ObjectType type);
};

} // namespace render::geometry
