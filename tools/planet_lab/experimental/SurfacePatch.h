#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "tools/planet_lab/experimental/CubeSphere.h"
#include "src/world/celestial/surface/SurfaceField.h"

namespace world::celestial::surface
{
/*
    One cube-sphere patch of a SurfaceField: a (n+1) x (n+1) vertex grid
    plus skirts (edge vertices dropped along the radius) that hide the
    T-junction gaps between patches of different levels.

    The patch is a pure function of (field, key, resolution): its band
    limit is twice its vertex spacing, its vertices are the field at the
    grid directions, its normals come from a one-vertex border ring of the
    same field (identical on both sides of a shared edge). No neighbour
    data, no generation order.
*/
struct PatchVertex
{
    glm::vec3 position {0.0f};   // relative to the patch origin
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec3 albedo {0.5f};
    glm::vec4 detail {0.0f};     // rock, loose (sand/regolith/soil/salt), ice+snow, vegetation
    float emission = 0.0f;
    // Signed distance-like coast value: < 0 water, > 0 land (macro height
    // relative to the water level, scaled) - interpolated per pixel the
    // coastline is smooth and exactly the macro field's.
    float coast = 1.0f;
    glm::vec3 waterAlbedo {0.02f, 0.06f, 0.12f};
};

struct SurfacePatchData
{
    PatchKey key;
    int resolution = 32;
    glm::dvec3 origin {0.0};     // planet-local metres
    double spacingM = 0.0;
    double minWavelengthM = 0.0;
    double skirtDepthM = 0.0;
    double boundingRadiusM = 0.0;
    double minHeightM = 0.0;
    double maxHeightM = 0.0;

    std::vector<PatchVertex> vertices;   // grid, then skirts
    std::vector<std::uint32_t> indices;

    // Per grid vertex (row-major, (n+1)^2): for checks and collision.
    std::vector<double> heights;
    std::vector<double> macroHeights;
    std::vector<std::uint8_t> water;
    std::vector<std::uint64_t> featureIds;

    std::uint64_t checksum = 0;          // everything a patch shows
    std::uint64_t macroChecksum = 0;     // LOD-independent fields only
    double generationMs = 0.0;
};

double patchSpacingM(const SurfaceField& field, const PatchKey& key, int resolution);

SurfacePatchData generateSurfacePatch(const SurfaceField& field, const PatchKey& key, int resolution = 32);

// Deepest level whose vertex spacing stays above the policy's finest.
int maximumPatchLevel(double radiusM, double finestVertexSpacingM, int resolution);

/*
    Streaming cache: patches are generated on worker threads in priority
    order and kept until evicted. The cache is a pure cache - an evicted
    patch regenerates identically.
*/
class SurfacePatchStreamer
{
public:
    struct Stats
    {
        std::uint64_t generated = 0;
        double generationMsTotal = 0.0;
        double generationMsMax = 0.0;
        std::size_t pending = 0;
        std::size_t resident = 0;
    };

    SurfacePatchStreamer(std::shared_ptr<const SurfaceField> field, int resolution, int threads);
    ~SurfacePatchStreamer();

    std::shared_ptr<const SurfacePatchData> find(const PatchKey& key) const;

    // Replaces the not-yet-started requests (stale ones are dropped); the
    // list is in priority order.
    void setRequests(const std::vector<PatchKey>& keys);

    // Patches finished since the last call (for GPU upload).
    std::vector<std::shared_ptr<const SurfacePatchData>> takeCompleted();

    // Drops resident patches not in `keep`.
    void retain(const std::unordered_set<PatchKey, PatchKeyHash>& keep);

    void waitIdle();
    Stats stats() const;

private:
    void worker();

    std::shared_ptr<const SurfaceField> m_field;
    int m_resolution = 32;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<PatchKey> m_queue;
    std::unordered_set<PatchKey, PatchKeyHash> m_inFlight;
    std::unordered_map<PatchKey, std::shared_ptr<const SurfacePatchData>, PatchKeyHash> m_resident;
    std::vector<std::shared_ptr<const SurfacePatchData>> m_completed;
    std::vector<std::thread> m_threads;
    bool m_stop = false;
    Stats m_stats;
};
} // namespace world::celestial::surface
