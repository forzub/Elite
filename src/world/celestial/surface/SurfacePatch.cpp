#include "src/world/celestial/surface/SurfacePatch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
double patchSpacingM(const SurfaceField& field, const PatchKey& key, int resolution)
{
    return patchEdgeLengthM(key.level, field.referenceRadiusM()) / static_cast<double>(resolution);
}

int maximumPatchLevel(double radiusM, double finestVertexSpacingM, int resolution)
{
    int level = 0;
    while (level < 28 && patchEdgeLengthM(level + 1, radiusM) / resolution >= finestVertexSpacingM)
        ++level;
    return level;
}

SurfacePatchData generateSurfacePatch(const SurfaceField& field, const PatchKey& key, int n)
{
    const auto start = std::chrono::steady_clock::now();
    SurfacePatchData out;
    out.key = key;
    out.resolution = n;
    out.spacingM = patchSpacingM(field, key, n);
    out.minWavelengthM = out.spacingM * 2.0;
    const double radius = field.referenceRadiusM();
    out.origin = patchDirection(key, n / 2, n / 2, n) * radius;

    // Border ring (-1 .. n+1) for normals; the grid is 0 .. n.
    const int w = n + 3;
    std::vector<glm::dvec3> pos(static_cast<std::size_t>(w * w));
    std::vector<glm::dvec3> dirs(pos.size());
    std::vector<SurfaceSample> samples(pos.size());
    for (int j = -1; j <= n + 1; ++j)
        for (int i = -1; i <= n + 1; ++i)
        {
            const std::size_t k = static_cast<std::size_t>((j + 1) * w + (i + 1));
            const glm::dvec3 d = patchDirection(key, i, j, n);
            dirs[k] = d;
            samples[k] = field.sample(d, out.minWavelengthM);
            pos[k] = d * (radius + samples[k].heightM);
        }

    const std::size_t gridCount = static_cast<std::size_t>((n + 1) * (n + 1));
    out.vertices.reserve(gridCount + static_cast<std::size_t>(4 * (n + 1)));
    out.heights.reserve(gridCount);
    out.macroHeights.reserve(gridCount);
    out.water.reserve(gridCount);
    out.featureIds.reserve(gridCount);
    out.minHeightM = 1.0e30;
    out.maxHeightM = -1.0e30;
    Checksum all;
    Checksum macro;
    all.add(GeneratorVersion);
    macro.add(GeneratorVersion);
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i)
        {
            const std::size_t k = static_cast<std::size_t>((j + 1) * w + (i + 1));
            const SurfaceSample& s = samples[k];
            const glm::dvec3 dU = pos[k + 1] - pos[k - 1];
            const glm::dvec3 dV = pos[k + static_cast<std::size_t>(w)] - pos[k - static_cast<std::size_t>(w)];
            glm::dvec3 normal = glm::normalize(glm::cross(dU, dV));
            if (glm::dot(normal, dirs[k]) < 0.0)
                normal = -normal;
            if (s.water)
                normal = dirs[k];
            const double slope = 1.0 - glm::dot(normal, dirs[k]);
            const SurfaceAppearance a = field.appearance(s, dirs[k], slope);

            PatchVertex v;
            v.position = glm::vec3(pos[k] - out.origin);
            v.normal = glm::vec3(normal);
            v.albedo = a.albedo;
            v.emission = a.emission;
            v.water = s.water ? 1.0f : 0.0f;
            const auto wt = [&](SurfaceMaterial m) { return a.weights[static_cast<std::size_t>(m)]; };
            v.detail = glm::vec4(wt(SurfaceMaterial::Rock) + wt(SurfaceMaterial::Basalt),
                                 wt(SurfaceMaterial::Regolith) + wt(SurfaceMaterial::Sand) +
                                     wt(SurfaceMaterial::Soil) + wt(SurfaceMaterial::Salt),
                                 wt(SurfaceMaterial::Ice) + wt(SurfaceMaterial::Snow),
                                 wt(SurfaceMaterial::Vegetation));
            out.vertices.push_back(v);
            out.heights.push_back(s.heightM);
            out.macroHeights.push_back(s.macroHeightM);
            out.water.push_back(s.water ? 1 : 0);
            out.featureIds.push_back(s.featureId);
            out.minHeightM = std::min(out.minHeightM, s.heightM);
            out.maxHeightM = std::max(out.maxHeightM, s.heightM);

            all.add(s.heightM);
            all.add(a.albedo);
            all.add(s.water);
            all.add(s.featureId);
            macro.add(s.macroHeightM);
            macro.add(s.water);
            macro.add(s.featureId);
            macro.add(static_cast<std::uint8_t>(s.biome));
        }
    out.checksum = all.value;
    out.macroChecksum = macro.value;

    // Grid triangles (CCW seen from outside: face u x v is outward).
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
        {
            const auto a = static_cast<std::uint32_t>(j * (n + 1) + i);
            const auto b = a + 1;
            const auto c = a + static_cast<std::uint32_t>(n + 1);
            const auto d = c + 1;
            out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
        }

    // Skirts: deep enough for the largest gap to a coarser neighbour (its
    // edge interpolates over two of our cells, plus the octaves it lacks).
    out.skirtDepthM = out.spacingM * 1.5 + std::min(field.detailAmplitudeBoundM(), out.spacingM * 4.0) + 1.0;
    const auto addSkirt = [&](int i0, int j0, int di, int dj) {
        std::uint32_t previousTop = 0;
        std::uint32_t previousBottom = 0;
        for (int s = 0; s <= n; ++s)
        {
            const int i = i0 + di * s;
            const int j = j0 + dj * s;
            const auto top = static_cast<std::uint32_t>(j * (n + 1) + i);
            PatchVertex v = out.vertices[top];
            const glm::dvec3 d = dirs[static_cast<std::size_t>((j + 1) * w + (i + 1))];
            v.position = glm::vec3(glm::dvec3(v.position) - d * out.skirtDepthM);
            const auto bottom = static_cast<std::uint32_t>(out.vertices.size());
            out.vertices.push_back(v);
            if (s > 0)
                out.indices.insert(out.indices.end(),
                                   {previousTop, top, bottom, previousTop, bottom, previousBottom});
            previousTop = top;
            previousBottom = bottom;
        }
    };
    addSkirt(0, 0, 1, 0);
    addSkirt(n, 0, 0, 1);
    addSkirt(n, n, -1, 0);
    addSkirt(0, n, 0, -1);

    double r2 = 0.0;
    for (const PatchVertex& v : out.vertices)
        r2 = std::max(r2, static_cast<double>(glm::dot(v.position, v.position)));
    out.boundingRadiusM = std::sqrt(r2);
    out.generationMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return out;
}

// ---- streamer -------------------------------------------------------------

SurfacePatchStreamer::SurfacePatchStreamer(std::shared_ptr<const SurfaceField> field, int resolution, int threads)
    : m_field(std::move(field)), m_resolution(resolution)
{
    for (int i = 0; i < std::max(1, threads); ++i)
        m_threads.emplace_back([this] { worker(); });
}

SurfacePatchStreamer::~SurfacePatchStreamer()
{
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
        m_queue.clear();
    }
    m_wake.notify_all();
    for (auto& t : m_threads)
        t.join();
}

std::shared_ptr<const SurfacePatchData> SurfacePatchStreamer::find(const PatchKey& key) const
{
    std::lock_guard lock(m_mutex);
    const auto it = m_resident.find(key);
    return it == m_resident.end() ? nullptr : it->second;
}

void SurfacePatchStreamer::setRequests(const std::vector<PatchKey>& keys)
{
    {
        std::lock_guard lock(m_mutex);
        m_queue.clear();
        for (const PatchKey& k : keys)
            if (!m_resident.count(k) && !m_inFlight.count(k))
                m_queue.push_back(k);
        m_stats.pending = m_queue.size() + m_inFlight.size();
    }
    m_wake.notify_all();
}

std::vector<std::shared_ptr<const SurfacePatchData>> SurfacePatchStreamer::takeCompleted()
{
    std::lock_guard lock(m_mutex);
    std::vector<std::shared_ptr<const SurfacePatchData>> out;
    out.swap(m_completed);
    return out;
}

void SurfacePatchStreamer::retain(const std::unordered_set<PatchKey, PatchKeyHash>& keep)
{
    std::lock_guard lock(m_mutex);
    for (auto it = m_resident.begin(); it != m_resident.end();)
        it = keep.count(it->first) ? std::next(it) : m_resident.erase(it);
    m_stats.resident = m_resident.size();
}

void SurfacePatchStreamer::waitIdle()
{
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [this] { return m_queue.empty() && m_inFlight.empty(); });
}

SurfacePatchStreamer::Stats SurfacePatchStreamer::stats() const
{
    std::lock_guard lock(m_mutex);
    Stats s = m_stats;
    s.pending = m_queue.size() + m_inFlight.size();
    s.resident = m_resident.size();
    return s;
}

void SurfacePatchStreamer::worker()
{
    for (;;)
    {
        PatchKey key;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [this] { return m_stop || !m_queue.empty(); });
            if (m_stop)
                return;
            key = m_queue.front();
            m_queue.pop_front();
            m_inFlight.insert(key);
        }
        auto patch = std::make_shared<SurfacePatchData>(generateSurfacePatch(*m_field, key, m_resolution));
        {
            std::lock_guard lock(m_mutex);
            m_inFlight.erase(key);
            m_stats.generated += 1;
            m_stats.generationMsTotal += patch->generationMs;
            m_stats.generationMsMax = std::max(m_stats.generationMsMax, patch->generationMs);
            m_resident[key] = patch;
            m_completed.push_back(patch);
        }
        m_idle.notify_all();
    }
}
} // namespace world::celestial::surface
