#include "src/game/system_map/ProceduralBodyTextureCache.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/core/ThreadPriority.h"
#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/BodySurfaceFieldLoader.h"
#include "src/world/celestial/surface/CloudWeatherField.h"
#include "src/world/celestial/surface/PlanetSurfaceField.h"
#include "src/world/celestial/surface/TextureBlockCompression.h"

namespace game::system_map
{
namespace
{
namespace fs = std::filesystem;

ProceduralBodyTextureCache::Policy policyFromEnv()
{
    const char* v = std::getenv("ELITE_MAP_PROCEDURAL_BODIES");
    if (!v)
        return ProceduralBodyTextureCache::Policy::All;
    const std::string s(v);
    if (s == "0" || s == "off" || s == "OFF")
        return ProceduralBodyTextureCache::Policy::Off;
    if (s == "missing" || s == "MISSING" || s == "1")
        return ProceduralBodyTextureCache::Policy::MissingOnly;
    return ProceduralBodyTextureCache::Policy::All;
}

int systemIdFromBodyId(const std::string& bodyId)
{
    return world::celestial::surface::mapBodySystemId(bodyId);
}
} // namespace

ProceduralBodyTextureCache::ProceduralBodyTextureCache()
    : m_policy(policyFromEnv())
{
    if (const char* v = std::getenv("ELITE_MAP_PROCEDURAL_SIZE"))
        m_width = std::clamp(std::atoi(v) / 2 * 2, 128, 4096);
    if (m_policy == Policy::Off)
        return;
    m_worker = std::thread([this] { workerLoop(); });
    std::cerr << "[MapProceduralBodies] policy "
              << (m_policy == Policy::All ? "all" : "missing-only") << ", " << m_width << "x" << m_width / 2
              << "\n";
}

ProceduralBodyTextureCache::~ProceduralBodyTextureCache()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
        m_queue.clear();
    }
    m_abort.store(true);
    m_wake.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

void ProceduralBodyTextureCache::memoryUsage(std::size_t& gpuBytes, std::size_t& gpuEntries,
                                             std::size_t& cpuBytes) const
{
    gpuBytes = gpuEntries = cpuBytes = 0;
    for (const auto& [key, e] : m_entries)
    {
        const int state = e->state.load(std::memory_order_acquire);
        if (state == 3)
        {
            gpuBytes += e->gpuBytes;
            ++gpuEntries;
        }
        else if (state == 1)
        {
            cpuBytes += e->rgba.size();
            for (const auto& m : e->surface.albedoMips)
                cpuBytes += m.size();
            for (const auto& m : e->surface.normalMips)
                cpuBytes += m.size();
        }
    }
}

void ProceduralBodyTextureCache::beginFrame(std::uint64_t frame)
{
    if (m_bc1Supported.load(std::memory_order_relaxed) < 0)
    {
        GLint count = 0;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
        std::vector<GLint> formats(static_cast<std::size_t>(std::max(0, count)));
        if (count > 0)
            glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats.data());
        constexpr GLint Bc1 = 0x83F0;   // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
        m_bc1Supported.store(std::find(formats.begin(), formats.end(), Bc1) != formats.end() ? 1 : 0);
        if (m_bc1Supported.load() == 0)
            std::cerr << "[MapProceduralBodies] no S3TC: map textures decoded to RGBA8" << std::endl;
    }
    m_uploadsLeft = 2;
    m_frame = frame;
}

bool ProceduralBodyTextureCache::oldestUploaded(std::uint64_t& lastUsedFrame, std::string& key) const
{
    bool any = false;
    for (const auto& [k, e] : m_entries)
        if (e->state.load(std::memory_order_acquire) == 3 && e->texture != 0 && (!any || e->lastUsedFrame < lastUsedFrame))
        {
            lastUsedFrame = e->lastUsedFrame;
            key = k;
            any = true;
        }
    return any;
}

std::size_t ProceduralBodyTextureCache::evict(const std::string& key)
{
    const auto it = m_entries.find(key);
    if (it == m_entries.end() || it->second->state.load(std::memory_order_acquire) != 3)
        return 0;
    const std::size_t total = it->second->gpuBytes;
    glDeleteTextures(1, &it->second->texture);
    if (it->second->normalTexture)
        glDeleteTextures(1, &it->second->normalTexture);
    m_entries.erase(it);   // loaded / baked again on its next request
    return total;
}

GLuint ProceduralBodyTextureCache::useEntry(const std::string& key, int systemId, const std::string& bodyId,
                                            bool clouds, double cloudTime, int& state)
{
    auto it = m_entries.find(key);
    if (it == m_entries.end())
    {
        auto entry = std::make_shared<Entry>();
        entry->systemId = systemId;
        entry->bodyId = bodyId;
        entry->clouds = clouds;
        entry->cloudTimeSeconds = cloudTime;
        entry->lastUsedFrame = m_frame;
        m_entries.emplace(key, entry);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(entry);
        }
        m_wake.notify_one();
        state = 0;
        return 0;
    }
    Entry& e = *it->second;
    e.lastUsedFrame = m_frame;
    state = e.state.load(std::memory_order_acquire);
    if (state == 3)
        return e.texture;
    if (state == 1 && m_uploadsLeft > 0)
    {
        --m_uploadsLeft;
        if (upload(e))
        {
            e.state.store(3, std::memory_order_release);
            state = 3;
            return e.texture;
        }
        e.state.store(2, std::memory_order_release);
        state = 2;
    }
    return 0;
}

const std::vector<render::celestial::CelestialShapeTriangle>*
ProceduralBodyTextureCache::shapeFor(int systemId, const std::string& bodyId)
{
    if (m_policy == Policy::Off || bodyId.empty())
        return nullptr;
    if (systemId < 0)
        systemId = systemIdFromBodyId(bodyId);
    const auto it = m_entries.find(std::to_string(systemId) + "|" + bodyId);
    if (it == m_entries.end() || it->second->state.load(std::memory_order_acquire) != 3 || it->second->shape.empty())
        return nullptr;
    it->second->lastUsedFrame = m_frame;
    return &it->second->shape;
}

GLuint ProceduralBodyTextureCache::normalTextureFor(int systemId, const std::string& bodyId)
{
    if (m_policy == Policy::Off || bodyId.empty())
        return 0;
    if (systemId < 0)
        systemId = systemIdFromBodyId(bodyId);
    const auto it = m_entries.find(std::to_string(systemId) + "|" + bodyId);
    if (it == m_entries.end() || it->second->state.load(std::memory_order_acquire) != 3)
        return 0;
    it->second->lastUsedFrame = m_frame;
    return it->second->normalTexture;
}

GLuint ProceduralBodyTextureCache::textureFor(int systemId, const std::string& bodyId)
{
    if (m_policy == Policy::Off || bodyId.empty())
        return 0;
    if (systemId < 0)
        systemId = systemIdFromBodyId(bodyId);
    if (systemId < 0)
        return 0;
    int state = 0;
    return useEntry(std::to_string(systemId) + "|" + bodyId, systemId, bodyId, false, 0.0, state);
}

GLuint ProceduralBodyTextureCache::cloudTextureFor(int systemId, const std::string& bodyId, double worldTimeSeconds,
                                                  bool& noClouds)
{
    noClouds = false;
    if (m_policy == Policy::Off || bodyId.empty())
        return 0;
    if (systemId < 0)
        systemId = systemIdFromBodyId(bodyId);
    if (systemId < 0)
        return 0;
    // the flight renderer's weather step (CelestialFlightRenderer::updateClouds): a new state every 20 min
    constexpr double Step = 1200.0;
    const auto step = static_cast<long long>(std::floor(worldTimeSeconds / Step));
    const auto keyFor = [&](long long s) {
        return "clouds|" + std::to_string(systemId) + "|" + bodyId + "|" + std::to_string(s);
    };
    int state = 0;
    const GLuint now = useEntry(keyFor(step), systemId, bodyId, true, static_cast<double>(step) * Step, state);
    if (state == 2)
    {
        noClouds = true;
        return 0;
    }
    if (now != 0)
        return now;
    // while the new step bakes: the previous step's texture, if it is still here
    const auto prev = m_entries.find(keyFor(step - 1));
    if (prev != m_entries.end() && prev->second->state.load(std::memory_order_acquire) == 3)
    {
        prev->second->lastUsedFrame = m_frame;
        return prev->second->texture;
    }
    return 0;
}

namespace
{
void setMapTextureParameters(int levels)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // equirect: periodic in longitude, clamped at the poles
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// a BC1 mip chain as it is, or decoded to RGBA8 when the GPU has no S3TC; returns the GPU bytes
std::size_t uploadBc1(GLuint tex, int width, int height, const std::vector<std::vector<std::uint8_t>>& mips,
                      bool bc1)
{
    namespace surf = world::celestial::surface;
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    std::size_t bytes = 0;
    int w = width, h = height;
    std::vector<std::uint8_t> rgba;
    for (std::size_t level = 0; level < mips.size(); ++level)
    {
        if (bc1)
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), 0x83F0, w, h, 0,
                                   static_cast<GLsizei>(mips[level].size()), mips[level].data());
            bytes += mips[level].size();
        }
        else
        {
            rgba.resize(static_cast<std::size_t>(w) * h * 4);
            surf::decodeBC1(mips[level].data(), w, h, rgba.data());
            glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         rgba.data());
            bytes += rgba.size();
        }
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    setMapTextureParameters(static_cast<int>(mips.size()));
    return bytes;
}
} // namespace

bool ProceduralBodyTextureCache::upload(Entry& e)
{
    GLint previous = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    GLuint tex = 0;
    if (e.clouds)
    {
        if (e.width <= 0 || e.rgba.empty())
            return false;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, e.width, e.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, e.rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        setMapTextureParameters(1 + static_cast<int>(std::floor(std::log2(std::max(e.width, e.height)))));
        e.gpuBytes = e.rgba.size() * 4 / 3;
        std::vector<std::uint8_t>().swap(e.rgba);
    }
    else
    {
        if (e.surface.albedoMips.empty())
            return false;
        const bool bc1 = m_bc1Supported.load(std::memory_order_relaxed) != 0;
        glGenTextures(1, &tex);
        e.gpuBytes = uploadBc1(tex, e.surface.width, e.surface.height, e.surface.albedoMips, bc1);
        if (!e.surface.normalMips.empty())
        {
            GLuint n = 0;
            glGenTextures(1, &n);
            e.gpuBytes += uploadBc1(n, e.surface.normalWidth, e.surface.normalHeight, e.surface.normalMips, bc1);
            e.normalTexture = n;
        }
        // the shape triangles stay (shapeFor), the pixels go
        decltype(e.surface.albedoMips)().swap(e.surface.albedoMips);
        decltype(e.surface.normalMips)().swap(e.surface.normalMips);
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
    e.texture = tex;
    return tex != 0;
}

void ProceduralBodyTextureCache::workerLoop()
{
    core::lowerCurrentThreadPriority();
    for (;;)
    {
        std::shared_ptr<Entry> entry;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this] { return m_stop || !m_queue.empty(); });
            if (m_stop)
                return;
            entry = m_queue.front();
            m_queue.pop_front();
        }
        bake(*entry);
    }
}

void ProceduralBodyTextureCache::bakeClouds(Entry& e)
{
    namespace surf = world::celestial::surface;
    const std::string path = m_catalog.descriptorPath(e.systemId, e.bodyId);
    surf::BodySurfaceDescriptor body;
    std::string error;
    // the flight's rule (CelestialFlightRenderer::startLoad): a solid body with an atmosphere that can cloud
    if (path.empty() || !surf::loadGeneratedBody(path, body, &error) || !surf::hasSolidSurface(body) ||
        body.surfacePressurePa <= 50.0 || body.cloudPotential <= 0.0)
    {
        e.state.store(2, std::memory_order_release);
        return;
    }
    const surf::CloudWeatherField clouds(body);
    if (clouds.layers().empty())
    {
        e.state.store(2, std::memory_order_release);
        return;
    }
    const int w = 512, h = 256;
    const double pi = 3.14159265358979323846;
    // a hot dense deck reads yellow-white (sulphuric acid, Venus ~740 K), a cold deck and water clouds
    // white-grey
    const glm::vec3 colour = clouds.hasDeck() && body.meanTemperatureK > 500.0 ? glm::vec3(0.93f, 0.87f, 0.70f)
                             : clouds.hasDeck()                               ? glm::vec3(0.90f, 0.91f, 0.93f)
                                                                              : glm::vec3(0.97f, 0.97f, 0.98f);
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4);
    for (int row = 0; row < h && !m_abort.load(std::memory_order_relaxed); ++row)
        for (int x = 0; x < w; ++x)
        {
            const double lon = ((x + 0.5) / w - 0.5) * 2.0 * pi;
            const double lat = (0.5 - (row + 0.5) / h) * pi;
            const glm::dvec3 dir(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
            double clear = 1.0;
            for (std::size_t l = 0; l < clouds.layers().size(); ++l)
                clear *= 1.0 - std::clamp(static_cast<double>(clouds.coverage(l, dir, e.cloudTimeSeconds)), 0.0, 1.0);
            const std::size_t o = (static_cast<std::size_t>(h - 1 - row) * w + x) * 4;   // GL row order: south first
            rgba[o + 0] = static_cast<std::uint8_t>(colour.r * 255.0f);
            rgba[o + 1] = static_cast<std::uint8_t>(colour.g * 255.0f);
            rgba[o + 2] = static_cast<std::uint8_t>(colour.b * 255.0f);
            rgba[o + 3] = static_cast<std::uint8_t>(std::clamp(1.0 - clear, 0.0, 1.0) * 255.0 + 0.5);
        }
    if (m_abort.load())
        return;
    e.width = w;
    e.height = h;
    e.rgba = std::move(rgba);
    e.state.store(1, std::memory_order_release);
}

bool ProceduralBodyTextureCache::loadStored(Entry& e, std::uint64_t fingerprint)
{
    namespace surf = world::celestial::surface;
    for (const std::string& dir : {m_prebakeDir, m_cacheDir})
    {
        if (dir.empty())
            continue;
        surf::MapBodyPrebake stored;
        if (!surf::readMapBodyPrebake(surf::mapBodyPrebakePath(dir, e.systemId, e.bodyId), fingerprint, stored))
            continue;
        // the file's probe against the surface field as the generator makes it NOW
        surf::MapBodyProbe probe;
        std::string error;
        if (!surf::mapBodyProbe(m_catalog, e.systemId, e.bodyId, m_width, probe, error) ||
            !surf::mapBodyProbesMatch(probe, stored.probe))
        {
            std::cerr << "[MapProceduralBodies] " << e.bodyId << ": stored bake in " << dir
                      << " is stale (the surface generator changed), baking again" << std::endl;
            continue;
        }
        e.surface = std::move(stored);
        return true;
    }
    return false;
}

namespace
{
std::vector<render::celestial::CelestialShapeTriangle> shapeTriangles(
    const world::celestial::surface::MapBodyPrebake& p)
{
    std::vector<render::celestial::CelestialShapeTriangle> out;
    const int NLon = p.shapeLon, NLat = p.shapeLat;
    if (NLon <= 0 || NLat < 2 || p.shapeGrid.size() != static_cast<std::size_t>(NLon + 1) * NLat)
        return out;
    const auto vertex = [&](int i, int j) {
        render::celestial::CelestialShapeVertex v;
        v.pos = p.shapeGrid[static_cast<std::size_t>(i) * (NLon + 1) + j];
        v.uv = glm::vec2(static_cast<float>(j) / NLon, 1.0f - static_cast<float>(i) / (NLat - 1));
        return v;
    };
    out.reserve(static_cast<std::size_t>(NLon) * (NLat - 1) * 2);
    for (int i = 0; i + 1 < NLat; ++i)
        for (int j = 0; j < NLon; ++j)
        {
            const auto a = vertex(i, j), b = vertex(i, j + 1), c = vertex(i + 1, j), d = vertex(i + 1, j + 1);
            out.push_back({a, c, b});
            out.push_back({b, c, d});
        }
    return out;
}
} // namespace

void ProceduralBodyTextureCache::bake(Entry& e)
{
    namespace surf = world::celestial::surface;
    if (e.clouds)
    {
        bakeClouds(e);
        return;
    }
    if (!m_dirsResolved)
    {
        m_dirsResolved = true;
        m_prebakeDir = surf::mapBodyPrebakeDir(m_catalog.assetRoot());
        m_cacheDir = surf::mapBodyRuntimeCacheDir(m_catalog.assetRoot());
        std::cerr << "[MapProceduralBodies] prebake " << (m_prebakeDir.empty() ? "-" : m_prebakeDir) << ", cache "
                  << (m_cacheDir.empty() ? "- (not writable)" : m_cacheDir) << std::endl;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const auto msSince = [&] {
        return static_cast<int>(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    };
    const std::uint64_t fingerprint = surf::mapBodyFingerprint(m_catalog, e.systemId, e.bodyId, m_width);
    if (fingerprint == 0)
    {
        // stars, belts, stations: no generated body - silently no texture
        e.state.store(2, std::memory_order_release);
        return;
    }
    // a prebaked or cached file of the same inputs: no generation at all
    if (loadStored(e, fingerprint))
    {
        e.shape = shapeTriangles(e.surface);
        e.width = e.surface.width;
        e.height = e.surface.height;
        e.state.store(1, std::memory_order_release);
        std::cerr << "[MapProceduralBodies] loaded " << e.bodyId << " in " << msSince() << " ms" << std::endl;
        return;
    }
    surf::MapBodyBake baked;
    std::string error;
    const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    if (!surf::bakeMapBody(m_catalog, e.systemId, e.bodyId, m_width, threads, &m_abort, baked, error))
    {
        if (m_abort.load())
            return;
        if (!error.empty())
            std::cerr << "[MapProceduralBodies] " << e.bodyId << ": " << error << std::endl;
        e.state.store(2, std::memory_order_release);
        return;
    }
    const int bakeMs = msSince();
    surf::MapBodyPrebake compressed = surf::compressMapBodyBake(baked, fingerprint);
    // into the runtime cache: the next session loads it
    const bool saved = !m_cacheDir.empty() &&
                       surf::writeMapBodyPrebake(surf::mapBodyPrebakePath(m_cacheDir, e.systemId, e.bodyId), compressed);
    e.surface = std::move(compressed);
    e.shape = shapeTriangles(e.surface);
    e.width = e.surface.width;
    e.height = e.surface.height;
    e.state.store(1, std::memory_order_release);
    std::cerr << "[MapProceduralBodies] baked " << e.bodyId << " (" << baked.description << ") " << baked.width
              << "x" << baked.height << " in " << bakeMs << " ms, compressed + " << (saved ? "cached" : "NOT cached")
              << " in " << (msSince() - bakeMs) << " ms" << std::endl;
}
} // namespace game::system_map
