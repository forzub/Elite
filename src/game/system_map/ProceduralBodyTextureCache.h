#pragma once

#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>

namespace game::system_map
{
/*
    Map textures of bodies from their surface field: the equirect albedo is
    a bake of the SAME surface the flight renderer draws, taken from the
    process-wide BodySurfaceRegistry (acquireBodySurface): the procedural
    field of the generated descriptor, the curated Sol maps
    (assets/source/celestial) where they exist, appearance passports - so
    the map and the flight show one planet (docs/CELESTIAL_RESOURCE_AUDIT.md
    step 3) and the baked assets/generated cache is not needed for it. Bakes run on a background worker
    (below normal priority); textureFor() returns 0 until the bake is
    uploaded, the caller draws its fallback meanwhile.

    Texture layout matches the cached generated albedo as TextureLoader
    uploads it: longitude u = atan2(z, x) / 2pi + 0.5, GL row 0 = south pole.

    Policy (env ELITE_MAP_PROCEDURAL_BODIES):
      unset / "all"      every body from its surface field (the cached
                         generated texture only while the bake runs)
      "missing"          only bodies without a cached generated texture
                         (the step-1 behaviour: Sol / Tau Ceti from the cache)
      "0" / "off"        never (the old cache-only behaviour)
    ELITE_MAP_PROCEDURAL_SIZE: texture width (height = width / 2), default 1024.
*/
class ProceduralBodyTextureCache
{
public:
    enum class Policy
    {
        Off,
        MissingOnly,
        All
    };

    ProceduralBodyTextureCache();
    ~ProceduralBodyTextureCache();
    ProceduralBodyTextureCache(const ProceduralBodyTextureCache&) = delete;
    ProceduralBodyTextureCache& operator=(const ProceduralBodyTextureCache&) = delete;

    Policy policy() const noexcept { return m_policy; }

    // GL thread, once per frame: resets the upload budget (two textures); frame = the map's frame counter
    // (stamps the entries' last use for the map texture budget).
    void beginFrame(std::uint64_t frame);

    // Map texture budget (MapCelestialRenderResources::enforceMapTextureBudget): the least recently used
    // uploaded texture, and its eviction (GL delete; baked again on its next request). Returns the bytes freed.
    bool oldestUploaded(std::uint64_t& lastUsedFrame, std::string& key) const;
    std::size_t evict(const std::string& key);

    // GL thread. DEV memory ledger: uploaded GPU bytes (RGBA8 + mips) and
    // baked-but-not-uploaded CPU bytes.
    void memoryUsage(std::size_t& gpuBytes, std::size_t& gpuEntries, std::size_t& cpuBytes) const;

    // GL thread. systemId < 0: taken from the body id prefix "system_<N>.".
    // 0 while the bake is queued / running, or when the body has no
    // generated descriptor (stars, belts, ships).
    GLuint textureFor(int systemId, const std::string& bodyId);

    // GL thread. The body's cloud layer from the SAME CloudWeatherField flight draws (one source of
    // clouds, docs/CELESTIAL_RESOURCE_AUDIT.md): RGBA equirect, alpha = the combined cover of its layers at
    // the 20-min world-time step flight uses (the previous step's texture shows while the next bakes).
    // 0 while baking; noClouds = the body has no cloud layer (airless, giant, no cloud potential).
    GLuint cloudTextureFor(int systemId, const std::string& bodyId, double worldTimeSeconds, bool& noClouds);

private:
    struct Entry
    {
        int systemId = -1;
        std::string bodyId;
        std::atomic<int> state {0}; // 0 queued, 1 baked (pixels ready), 2 failed, 3 uploaded
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgba; // GL row order (south first)
        GLuint texture = 0;
        std::uint64_t lastUsedFrame = 0;
        bool clouds = false;          // a cloud bake (cloudTextureFor) instead of the surface albedo
        double cloudTimeSeconds = 0.0;
    };
    GLuint useEntry(const std::string& key, int systemId, const std::string& bodyId, bool clouds, double cloudTime,
                    int& state);
    void bakeClouds(Entry& entry);

    void workerLoop();
    void bake(Entry& entry);
    std::string descriptorPathFor(int systemId, const std::string& bodyId);
    static bool upload(Entry& entry);

    Policy m_policy = Policy::All;
    int m_width = 1024;

    std::unordered_map<std::string, std::shared_ptr<Entry>> m_entries; // GL thread only
    int m_uploadsLeft = 2;
    std::uint64_t m_frame = 0;

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::shared_ptr<Entry>> m_queue;
    bool m_stop = false;
    std::atomic<bool> m_abort {false};
    std::thread m_worker;

    // worker only: systemId -> (body id -> descriptor path)
    std::unordered_map<int, std::unordered_map<std::string, std::string>> m_descriptorIndex;
    std::unordered_map<int, std::string> m_systemFolder;   // "000_sol" -> "sol" (curated maps folder)
    std::string m_systemsRoot;
    std::string m_assetRoot;   // holds assets/source/celestial
};
} // namespace game::system_map
