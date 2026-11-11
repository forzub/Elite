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
    // "system_9.Epsilon_Eridani.eps_Eri_b"
    if (bodyId.rfind("system_", 0) != 0)
        return -1;
    int id = 0;
    std::size_t i = 7;
    if (i >= bodyId.size() || !std::isdigit(static_cast<unsigned char>(bodyId[i])))
        return -1;
    for (; i < bodyId.size() && std::isdigit(static_cast<unsigned char>(bodyId[i])); ++i)
        id = id * 10 + (bodyId[i] - '0');
    return id;
}

// the folder holding assets/source/celestial (curated maps): the working directory or a parent
std::string findAssetRoot()
{
    const fs::path cwd = fs::current_path();
    std::error_code ec;
    for (const fs::path& base : {cwd, cwd.parent_path(), cwd.parent_path().parent_path()})
        if (fs::is_directory(base / "assets/source/celestial", ec))
            return base.lexically_normal().generic_string();
    return {};
}

std::string findSystemsRoot()
{
    const fs::path cwd = fs::current_path();
    std::error_code ec;
    for (const char* rel : {"assets/data/galaxy_details/systems", "src/assets/data/galaxy_details/systems"})
        for (const fs::path& base : {cwd, cwd.parent_path(), cwd.parent_path().parent_path()})
            if (fs::is_directory(base / rel, ec))
                return (base / rel).lexically_normal().generic_string();
    return {};
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
            gpuBytes += static_cast<std::size_t>(e->width) * e->height * 4 * 4 / 3 * (e->normalTexture ? 2 : 1);
            ++gpuEntries;
        }
        else if (state == 1)
            cpuBytes += e->rgba.size();
    }
}

void ProceduralBodyTextureCache::beginFrame(std::uint64_t frame)
{
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
    const std::size_t bytes = static_cast<std::size_t>(it->second->width) * it->second->height * 4 * 4 / 3;
    glDeleteTextures(1, &it->second->texture);
    std::size_t total = bytes;
    if (it->second->normalTexture)
    {
        glDeleteTextures(1, &it->second->normalTexture);
        total += bytes;
    }
    m_entries.erase(it);   // baked again on its next request
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

bool ProceduralBodyTextureCache::upload(Entry& e)
{
    if (e.width <= 0 || e.rgba.empty())
        return false;
    GLint previous = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, e.width, e.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, e.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // equirect: periodic in longitude, clamped at the poles
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
    e.texture = tex;
    std::vector<std::uint8_t>().swap(e.rgba);
    if (!e.normalRgba.empty())
    {
        GLuint n = 0;
        glGenTextures(1, &n);
        glBindTexture(GL_TEXTURE_2D, n);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, e.width, e.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, e.normalRgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        e.normalTexture = n;
        std::vector<std::uint8_t>().swap(e.normalRgba);
    }
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

std::string ProceduralBodyTextureCache::descriptorPathFor(int systemId, const std::string& bodyId)
{
    auto sys = m_descriptorIndex.find(systemId);
    if (sys == m_descriptorIndex.end())
    {
        if (m_systemsRoot.empty())
            m_systemsRoot = findSystemsRoot();
        std::unordered_map<std::string, std::string> index;
        std::error_code ec;
        // the local systems and the distant quest / route targets (galaxy_details/distant_systems, e.g.
        // 100062_kepler_62): findSystem() serves both, so their maps can open
        std::vector<fs::path> roots;
        if (!m_systemsRoot.empty())
        {
            roots.push_back(fs::path(m_systemsRoot));
            roots.push_back(fs::path(m_systemsRoot).parent_path() / "distant_systems");
        }
        bool found = false;
        for (const fs::path& root : roots)
        {
            if (found)
                break;
            for (const auto& dir : fs::directory_iterator(root, ec))
            {
                const std::string name = dir.path().filename().string();
                const std::size_t us = name.find('_');
                if (us == std::string::npos || us == 0 || name.find_first_not_of("0123456789") != us ||
                    std::atoi(name.substr(0, us).c_str()) != systemId)
                    continue;
                // "000_sol" -> "sol": the curated maps folder (assets/source/celestial/<system>)
                m_systemFolder[systemId] = name.substr(us + 1);
                const fs::path bodies = dir.path() / "generated" / "bodies";
                for (const auto& f : fs::directory_iterator(bodies, ec))
                {
                    if (f.path().extension() != ".json")
                        continue;
                    try
                    {
                        std::ifstream in(f.path());
                        const nlohmann::json j = nlohmann::json::parse(in);
                        if (j.value("schema", std::string()) == "elite.generated_body.v1")
                            index[j.value("id", std::string())] = f.path().generic_string();
                    }
                    catch (const std::exception&)
                    {
                    }
                }
                found = true;
                break;
            }
        }
        sys = m_descriptorIndex.emplace(systemId, std::move(index)).first;
    }
    const auto b = sys->second.find(bodyId);
    return b == sys->second.end() ? std::string() : b->second;
}

void ProceduralBodyTextureCache::bakeClouds(Entry& e)
{
    namespace surf = world::celestial::surface;
    const std::string path = descriptorPathFor(e.systemId, e.bodyId);
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

void ProceduralBodyTextureCache::bake(Entry& e)
{
    namespace surf = world::celestial::surface;
    if (e.clouds)
    {
        bakeClouds(e);
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const std::string path = descriptorPathFor(e.systemId, e.bodyId);
    surf::BodySurfaceDescriptor body;
    std::string error;
    if (path.empty() || !surf::loadGeneratedBody(path, body, &error))
    {
        // stars, belts, stations: no generated body - silently no texture
        if (!path.empty())
            std::cerr << "[MapProceduralBodies] " << e.bodyId << ": " << error << "\n";
        e.state.store(2, std::memory_order_release);
        return;
    }
    // the flight renderer's surface (curated maps, passports) from the shared registry: no second copy
    // while the body is also in flight; released after the bake when nobody else holds it
    surf::BodySurfaceSource source;
    source.id = e.bodyId;
    source.descriptorPath = path;
    source.radiusM = body.radiusM;
    if (m_assetRoot.empty())
        m_assetRoot = findAssetRoot();
    if (!m_assetRoot.empty())
        surf::findImportedBodyMaps(m_assetRoot, m_systemFolder[e.systemId], path, source);
    surf::LoadedBodySurface loaded;
    if (!surf::acquireBodySurface(source, loaded) || !loaded.field)
    {
        std::cerr << "[MapProceduralBodies] " << e.bodyId << ": surface not loaded " << loaded.error << "\n";
        e.state.store(2, std::memory_order_release);
        return;
    }
    const std::shared_ptr<surf::SurfaceField> field = loaded.field;
    const int w = m_width, h = m_width / 2;
    const double pi = 3.14159265358979323846;
    const double texelM = 2.0 * pi * field->referenceRadiusM() / w;
    const auto dirAt = [&](int x, int row) { // row 0 = north
        const double lon = ((x + 0.5) / w - 0.5) * 2.0 * pi;
        const double lat = (0.5 - (row + 0.5) / h) * pi;
        return glm::dvec3(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
    };
    std::vector<surf::SurfaceSample> samples(static_cast<std::size_t>(w) * h);
    std::vector<std::uint8_t> rgba(samples.size() * 4);
    const int threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    const auto parallelRows = [&](auto&& fn) {
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&, t] {
                core::lowerCurrentThreadPriority();
                for (int row = t; row < h && !m_abort.load(std::memory_order_relaxed); row += threads)
                    fn(row);
            });
        for (auto& th : pool)
            th.join();
    };
    parallelRows([&](int row) {
        for (int x = 0; x < w; ++x)
            samples[static_cast<std::size_t>(row) * w + x] = field->sample(dirAt(x, row), texelM * 2.0);
    });
    parallelRows([&](int row) {
        for (int x = 0; x < w; ++x)
        {
            const auto& s = samples[static_cast<std::size_t>(row) * w + x];
            const auto& east = samples[static_cast<std::size_t>(row) * w + (x + 1) % w];
            const auto& north = samples[static_cast<std::size_t>(std::max(0, row - 1)) * w + x];
            const double slope =
                std::min(1.0, std::hypot(east.heightM - s.heightM, north.heightM - s.heightM) / texelM);
            const glm::vec3 a =
                field->appearance(s, dirAt(x, row), 1.0 - 1.0 / std::sqrt(1.0 + slope * slope)).albedo;
            const glm::vec3 c = glm::pow(glm::clamp(a, glm::vec3(0.0f), glm::vec3(1.0f)), glm::vec3(1.0f / 2.2f));
            // GL row order: south first (as TextureLoader flips the cached TGA)
            const std::size_t o = (static_cast<std::size_t>(h - 1 - row) * w + x) * 4;
            rgba[o + 0] = static_cast<std::uint8_t>(c.r * 255.0f + 0.5f);
            rgba[o + 1] = static_cast<std::uint8_t>(c.g * 255.0f + 0.5f);
            rgba[o + 2] = static_cast<std::uint8_t>(c.b * 255.0f + 0.5f);
            rgba[o + 3] = 255;
        }
    });
    if (m_abort.load())
        return;
    // relief normal map (the baker's buildNormalFromHeightImage on the height normalised to the body's range)
    {
        double minH = 1.0e30, maxH = -1.0e30;
        for (const auto& s : samples)
        {
            minH = std::min(minH, s.heightM);
            maxH = std::max(maxH, s.heightM);
        }
        const double range = std::max(1.0, maxH - minH);
        const auto h01 = [&](int x, int row) {   // row 0 = north, wrapped in x, clamped in y
            x = (x % w + w) % w;
            row = std::clamp(row, 0, h - 1);
            return static_cast<float>((samples[static_cast<std::size_t>(row) * w + x].heightM - minH) / range);
        };
        constexpr float Strength = 8.0f;
        std::vector<std::uint8_t> normal(static_cast<std::size_t>(w) * h * 4);
        for (int row = 0; row < h; ++row)
            for (int x = 0; x < w; ++x)
            {
                const glm::vec3 n = glm::normalize(glm::vec3(-(h01(x + 1, row) - h01(x - 1, row)) * Strength,
                                                             -(h01(x, row + 1) - h01(x, row - 1)) * Strength, 1.0f));
                const glm::vec3 enc = n * 0.5f + glm::vec3(0.5f);
                const std::size_t o = (static_cast<std::size_t>(h - 1 - row) * w + x) * 4;   // GL rows: south first
                normal[o + 0] = static_cast<std::uint8_t>(enc.x * 255.0f + 0.5f);
                normal[o + 1] = static_cast<std::uint8_t>(enc.y * 255.0f + 0.5f);
                normal[o + 2] = static_cast<std::uint8_t>(enc.z * 255.0f + 0.5f);
                normal[o + 3] = 255;
            }
        e.normalRgba = std::move(normal);
    }
    e.width = w;
    e.height = h;
    e.rgba = std::move(rgba);
    e.state.store(1, std::memory_order_release);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cerr << "[MapProceduralBodies] baked " << e.bodyId << " ("
              << (loaded.descriptor.source == surf::SurfaceSourceKind::ImportedRealData ? "curated maps, " : "") << surf::archetypeName(surf::surfaceArchetype(body))
              << ") " << w << "x" << h << " in " << static_cast<int>(ms) << " ms\n";
}
} // namespace game::system_map
