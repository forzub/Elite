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
    Map textures of bodies from their generated physical descriptor
    (elite.generated_body.v1): the equirect albedo is a bake of the SAME
    surface field the flight renderer draws (makeSurfaceField), so the map
    and the flight show one planet. Bakes run on a background worker
    (below normal priority); textureFor() returns 0 until the bake is
    uploaded, the caller draws its fallback meanwhile.

    Texture layout matches the cached generated albedo as TextureLoader
    uploads it: longitude u = atan2(z, x) / 2pi + 0.5, GL row 0 = south pole.

    Policy (env ELITE_MAP_PROCEDURAL_BODIES):
      unset / "missing"  procedural only for bodies without a cached texture
                         (every system except Sol / Tau Ceti)
      "all"              procedural for every body (compare against the cache)
      "0" / "off"        never (old behaviour)
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

    // GL thread, once per frame: resets the upload budget (two textures).
    void beginFrame();

    // GL thread. DEV memory ledger: uploaded GPU bytes (RGBA8 + mips) and
    // baked-but-not-uploaded CPU bytes.
    void memoryUsage(std::size_t& gpuBytes, std::size_t& gpuEntries, std::size_t& cpuBytes) const;

    // GL thread. systemId < 0: taken from the body id prefix "system_<N>.".
    // 0 while the bake is queued / running, or when the body has no
    // generated descriptor (stars, belts, ships).
    GLuint textureFor(int systemId, const std::string& bodyId);

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
    };

    void workerLoop();
    void bake(Entry& entry);
    std::string descriptorPathFor(int systemId, const std::string& bodyId);
    static bool upload(Entry& entry);

    Policy m_policy = Policy::MissingOnly;
    int m_width = 1024;

    std::unordered_map<std::string, std::shared_ptr<Entry>> m_entries; // GL thread only
    int m_uploadsLeft = 2;

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::shared_ptr<Entry>> m_queue;
    bool m_stop = false;
    std::atomic<bool> m_abort {false};
    std::thread m_worker;

    // worker only: systemId -> (body id -> descriptor path)
    std::unordered_map<int, std::unordered_map<std::string, std::string>> m_descriptorIndex;
    std::string m_systemsRoot;
};
} // namespace game::system_map
