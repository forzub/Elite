#include "tools/planet_lab/PlanetLabBodies.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/PlanetSurfaceField.h"
#include "src/world/celestial/surface/SmallBodyField.h"

namespace planet_lab
{
namespace
{
    glm::dvec3 fibonacci(int i, int n)
    {
        const double y = 1.0 - 2.0 * (i + 0.5) / n;
        const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        const double phi = i * 2.399963229728653;
        return glm::dvec3(r * std::cos(phi), y, r * std::sin(phi));
    }

    // The viewpoints look at a landmark where LOD detail and landmark
    // stability are both visible: on a world with oceans a mountain range
    // of moderate height near a coast in the temperate belt (not the
    // snow-buried top), elsewhere the highest relief (crater rim, volcano).
    void chooseFocus(LabBody& body)
    {
        const SurfaceField& field = *body.field;
        double best = -1.0e30;
        const int n = 6000;
        for (int i = 0; i < n; ++i)
        {
            const glm::dvec3 d = fibonacci(i, n);
            if (std::abs(d.y) > 0.8)
                continue;
            const SurfaceSample s = field.sample(d, field.referenceRadiusM() * 0.01);
            double score = s.water ? -1.0e30 : s.macroHeightM;
            if (!body.isSmallBody && body.descriptor.oceanCoverage > 0.0 && field.solidSurface())
            {
                const double lat = std::abs(std::asin(d.y)) * 57.29577951308232;
                const bool range = s.featureClass == FeatureClass::MountainRange;
                const bool green = s.biome == Biome::TemperateForest || s.biome == Biome::Grassland ||
                                   s.biome == Biome::BorealForest;
                // Local relief: the height range in a +-15 km stencil.
                double lo = s.macroHeightM, hi = s.macroHeightM;
                if (!s.water && green)
                {
                    const glm::dvec3 e = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), d) + glm::dvec3(1e-9, 0, 0));
                    const glm::dvec3 n = glm::cross(d, e);
                    const double step = 15000.0 / field.referenceRadiusM();
                    for (const glm::dvec3& o : {e, -e, n, -n})
                    {
                        const double hh = field.sample(glm::normalize(d + o * step), 2000.0).macroHeightM;
                        lo = std::min(lo, hh);
                        hi = std::max(hi, hh);
                    }
                }
                score = s.water || lat < 25.0 || lat > 55.0 || !green ? -1.0e30
                                                                   : (hi - lo) + (range ? 300.0 : 0.0);
            }
            if (!field.solidSurface())
                score = s.featureClass == FeatureClass::Storm ? 1.0 - std::abs(d.y) : -std::abs(d.y);
            if (score > best)
            {
                best = score;
                body.focusDir = d;
                body.focusLabel = featureLabel(s.featureClass, s.featureId);
            }
        }
    }

    void finish(LabBody& body)
    {
        if (body.isSmallBody)
        {
            body.policy.solidSurface = true;
            body.policy.groundReachable = true;
            body.policy.finestVertexSpacingM = 0.1;
            body.policy.reason = "small body: reachable";
        }
        else
        {
            body.field = makeSurfaceField(body.descriptor);
            body.policy = surfaceDetailPolicy(body.descriptor);
            auto clouds = std::make_shared<CloudWeatherField>(body.descriptor);
            if (!clouds->layers().empty() && hasSolidSurface(body.descriptor))
                body.clouds = clouds;
        }
        chooseFocus(body);
    }
}

bool loadLabBodyFromFile(const std::string& path, LabBody& out, std::string& error)
{
    out = LabBody {};
    out.sourcePath = path;
    if (!loadGeneratedBody(path, out.descriptor, &error))
        return false;
    out.label = out.descriptor.id;
    finish(out);
    return true;
}

bool loadLabBodies(const std::string& root, std::vector<LabBody>& out, std::string& error)
{
    const std::string sol = root + "/src/assets/data/galaxy_details/systems/000_sol/generated/";
    const std::string aca = root + "/src/assets/data/galaxy_details/systems/001_alpha_centauri_a_b/generated/";
    const struct
    {
        const char* label;
        std::string path;
    } planets[] = {
        {"earth_like", sol + "bodies/planet_earth.json"},
        {"rocky", sol + "bodies/planet_mercury.json"},
        {"gas_giant", sol + "bodies/planet_jupiter.json"},
        {"dry", sol + "bodies/planet_mars.json"},
        {"hostile", aca + "bodies/planet_alpha_centauri_a_i.json"},
    };
    out.clear();
    for (const auto& p : planets)
    {
        LabBody body;
        if (!loadLabBodyFromFile(p.path, body, error))
            return false;
        body.label = p.label;
        out.push_back(std::move(body));
    }

    // Asteroid: Sol main belt - the first body of >= 1.5 km radius in a
    // fixed run of cells (a deterministic search, no size override).
    ReservoirSurfaceDescriptor belt;
    if (!loadGeneratedReservoir(sol + "reservoirs/sol_asteroid_belt.json", belt, &error))
        return false;
    LabBody asteroid;
    asteroid.label = "asteroid";
    asteroid.sourcePath = sol + "reservoirs/sol_asteroid_belt.json";
    asteroid.isSmallBody = true;
    for (int x = 271; x < 271 + 4000 && asteroid.smallBody.meanRadiusM < 1500.0; ++x)
    {
        const glm::ivec3 cell(x, -3, 118);
        for (int c = 0; c < asteroidCandidatesInCell(belt, cell); ++c)
        {
            asteroid.smallBody = asteroidInReservoir(belt, cell, c);
            if (asteroid.smallBody.meanRadiusM >= 1500.0)
                break;
        }
    }
    asteroid.field = std::make_shared<SmallBodyField>(asteroid.smallBody);
    finish(asteroid);
    out.insert(out.begin() + 2, std::move(asteroid));
    return true;
}
} // namespace planet_lab
