// The ground under a direction from a body's centre (see BodyTerrainProbe.h). Moved from
// GameServer::terrainHeight (2026-11-06): the client's landing autopilot probes the same ground.
#include "src/game/navigation/globalflight/BodyTerrainProbe.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "src/game/system_map/PlanetBodyOrientation.h"
#include "src/world/celestial/surface/BodySurfaceFieldLoader.h"

namespace game::navigation::globalflight
{
namespace
{
struct TerrainField
{
    std::atomic<bool> ready {false};
    world::celestial::surface::LoadedBodySurface loaded;
};
std::unordered_map<std::string, std::shared_ptr<TerrainField>>& cache()
{
    static std::unordered_map<std::string, std::shared_ptr<TerrainField>> fields;
    return fields;
}
std::mutex& cacheMutex()
{
    static std::mutex m;
    return m;
}
} // namespace

bool sampleBodyTerrain(const world::celestial::CelestialSystemSnapshot& snapshot, const std::string& bodyId,
                       const glm::dvec3& worldDirection, BodyTerrainSample& ground)
{
    const world::celestial::CelestialBodyState* body = nullptr;
    for (const auto& b : snapshot.bodies)
        if (b.id == bodyId)
            body = &b;
    if (!body || body->sourcePath.empty())
        return false;

    bool fresh = false;
    std::shared_ptr<TerrainField> slot;
    {
        std::lock_guard<std::mutex> lock(cacheMutex());
        auto& cached = cache()[snapshot.systemName + "|" + body->sourcePath + "|" + bodyId];
        if (!cached)
        {
            cached = std::make_shared<TerrainField>();
            slot = cached;
            fresh = true;
        }
        else
            slot = cached;
    }
    if (fresh)
    {
        // the same source the flight renderer loads: curated maps or the generated body
        world::celestial::surface::BodySurfaceSource source;
        source.id = bodyId;
        source.descriptorPath = body->sourcePath;
        source.radiusM = body->radiusKm * 1000.0;
        world::celestial::surface::findImportedBodyMaps(".", snapshot.systemName, body->sourcePath, source);
        std::thread([field = slot, source] {
            world::celestial::surface::loadBodySurface(source, field->loaded);
            field->ready.store(true, std::memory_order_release);
            std::cerr << "[Terrain] " << source.id << ": surface field "
                      << (field->loaded.field ? "loaded" : "FAILED " + field->loaded.error)
                      << (source.importedHeightPath.empty() ? " (procedural)" : " (curated height map)") << std::endl;
        }).detach();
        return false;
    }
    if (!slot->ready.load(std::memory_order_acquire) || !slot->loaded.field)
        return false;

    // world -> body-fixed direction with the renderer's orientation (prime, north, east)
    const auto o = game::system_map::makePlanetTextureOrientation(
        body->axialTiltDeg, body->axisNodeDeg, body->rotationPhaseRad, body->textureLongitudeOffsetDeg);
    const glm::dvec3 d = glm::normalize(worldDirection);
    const glm::dvec3 dirBody(glm::dot(d, glm::dvec3(o.prime)), glm::dot(d, glm::dvec3(o.north)), glm::dot(d, glm::dvec3(o.east)));
    const auto& field = *slot->loaded.field;
    const glm::dvec3 n = glm::normalize(dirBody);
    // the ground the ship stands on is the RENDERED one: the finest terrain level near the ground
    // shows the field down to ~1 m (small craters, rims, ejecta - CraterLayer to 1.5 m); a 30 m
    // sample smoothed them away and the hull sank into the rendered relief (author 2026-11-06).
    // The slope and the appearance stay on the 30 m relief (a stable, noise-free gradient).
    const auto sample = field.sample(n, 30.0);
    const auto detail = field.sample(n, 1.0);
    // the sea is solid too: its surface over a sea-floor height
    const bool water = sample.water && sample.waterLevelM >= sample.heightM;
    ground.heightM = water ? sample.waterLevelM : detail.heightM;
    ground.smoothHeightM = water ? sample.waterLevelM : sample.heightM;
    // slope under the hull: central differences +-15 m (a 30 m base, the hull's scale). A one-sided
    // 60 m step reached past a settlement pad (flat to 36 m) onto the slope beyond it - a perfect
    // landing on the Shackleton pad read 32.6 deg and counted as tipping over (author 2026-11-06).
    const double radius = std::max(1.0, field.referenceRadiusM());
    constexpr double HalfBaseM = 15.0;
    const double step = HalfBaseM / radius;
    glm::dvec3 t1 = glm::normalize(glm::cross(n, std::abs(n.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0)));
    const glm::dvec3 t2 = glm::cross(n, t1);
    const auto at = [&](const glm::dvec3& t, double s) { return field.sample(glm::normalize(n + t * (s * step)), 30.0).heightM; };
    const double g1 = (at(t1, 1.0) - at(t1, -1.0)) / (2.0 * HalfBaseM);
    const double g2 = (at(t2, 1.0) - at(t2, -1.0)) / (2.0 * HalfBaseM);
    const double gradient = std::sqrt(g1 * g1 + g2 * g2);
    ground.slopeDeg = water ? 0.0 : glm::degrees(std::atan(gradient));
    // the dominant material of the rendered surface there
    if (water)
        ground.material = TerrainWaterMaterial;
    else
    {
        const auto look = field.appearance(sample, n, gradient);
        int best = 0;
        for (int k = 1; k < static_cast<int>(look.weights.size()); ++k)
            if (look.weights[static_cast<std::size_t>(k)] > look.weights[static_cast<std::size_t>(best)])
                best = k;
        ground.material = best;
    }
    ground.feature = static_cast<int>(sample.featureClass);
    return true;
}

} // namespace game::navigation::globalflight
