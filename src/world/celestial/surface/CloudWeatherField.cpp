#include "src/world/celestial/surface/CloudWeatherField.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
    constexpr double Pi = 3.14159265358979323846;

    glm::dvec3 rotateY(const glm::dvec3& d, double a)
    {
        const double c = std::cos(a);
        const double s = std::sin(a);
        return glm::dvec3(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
    }

    double gauss(double x, double mu, double sigma)
    {
        const double t = (x - mu) / sigma;
        return std::exp(-t * t);
    }
}

CloudWeatherField::CloudWeatherField(const BodySurfaceDescriptor& body) : m_body(body)
{
    m_seed = layerSeed(surfaceSeed(body.seed), "weather");
    const double bar = body.surfacePressurePa / 1.0e5;
    const double potential = std::clamp(body.cloudPotential, 0.0, 1.0);
    if (bar > 0.005 && potential > 0.0)
    {
        if (bar > 20.0)
        {
            m_layers.push_back({"deck", 45000.0, 0.95f, 0.45f}); // Venus-like global deck
        }
        else
        {
            const double scale = std::max(1.0, body.surfacePressurePa > 0.0 ? 8000.0 * std::sqrt(bar) : 8000.0);
            m_layers.push_back({"cumulus", std::min(scale * 0.3, 3000.0), 0.85f, 0.0f});
            if (potential > 0.3 && bar > 0.3)
                m_layers.push_back({"cirrus", std::min(scale * 1.2, 11000.0), 0.35f, -0.15f});
        }
    }
    else if (bar > 0.001)
    {
        m_layers.push_back({"haze", 20000.0, 0.15f, -0.2f}); // thin dusty / icy haze
    }
    // Zonal wind speed ~ 20 m/s at the surface scale.
    m_windRadPerSecond = 20.0 / std::max(1.0e5, body.radiusM);
    m_pattern = OctaveSpec {body.radiusM * 0.35, 1.92, 0.55, 6, 1.5};
}

float CloudWeatherField::tendency(const glm::dvec3& dir) const
{
    const double latDeg = std::abs(std::asin(std::clamp(dir.y, -1.0, 1.0))) * 180.0 / Pi;
    const double t = 0.45 + 0.40 * gauss(latDeg, 0.0, 9.0) - 0.32 * gauss(latDeg, 24.0, 8.0) +
                     0.32 * gauss(latDeg, 55.0, 12.0) - 0.10 * gauss(latDeg, 90.0, 15.0);
    return static_cast<float>(std::clamp(t * (0.4 + 0.8 * m_body.cloudPotential), 0.0, 1.0));
}

float CloudWeatherField::cyclones(const glm::dvec3& dir, std::int64_t epoch) const
{
    const std::uint64_t seed = deriveSeed(m_seed, "cyclones", static_cast<std::uint64_t>(epoch));
    const double cell = 0.45;
    const glm::dvec3 base = glm::floor(dir / cell);
    double best = 0.0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const glm::dvec3 c = base + glm::dvec3(dx, dy, dz);
                const std::uint64_t h = latticeHash(seed, static_cast<std::int64_t>(c.x), static_cast<std::int64_t>(c.y),
                                                    static_cast<std::int64_t>(c.z));
                const glm::dvec3 q = (c + glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) * cell;
                if (std::abs(glm::length(q) - 1.0) > cell * 0.5)
                    continue;
                const glm::dvec3 centre = glm::normalize(q);
                const double latDeg = std::abs(std::asin(centre.y)) * 180.0 / Pi;
                // Storm tracks: tropical cyclones 8-25 deg, mid-latitude lows 40-65.
                const double chance = 0.6 * gauss(latDeg, 16.0, 7.0) + 0.8 * gauss(latDeg, 52.0, 10.0);
                if (unitFromHash(h, 0) > chance * 0.55)
                    continue;
                const double radius = 0.06 + 0.10 * unitFromHash(h, 4);
                const glm::dvec3 rel = dir - centre * glm::dot(dir, centre);
                const double d = glm::length(rel) / radius;
                if (d > 1.4)
                    continue;
                // Spiral arms: the angle around the centre wound with distance.
                const glm::dvec3 e1 = glm::normalize(glm::cross(centre, glm::dvec3(0, 1, 0.01)));
                const glm::dvec3 e2 = glm::cross(centre, e1);
                const double angle = std::atan2(glm::dot(rel, e2), glm::dot(rel, e1));
                const double hemisphere = centre.y >= 0.0 ? 1.0 : -1.0;
                const double arms = 0.5 + 0.5 * std::cos(2.0 * (angle + hemisphere * d * 5.0));
                const double core = 1.0 - smoothstep(0.0, 1.4, d);
                const double eye = smoothstep(0.03, 0.10, d);
                best = std::max(best, core * (0.45 + 0.55 * arms) * eye);
            }
    return static_cast<float>(best);
}

float CloudWeatherField::coverage(std::size_t layer, const glm::dvec3& dir, double t) const
{
    if (layer >= m_layers.size())
        return 0.0f;
    const CloudLayer& L = m_layers[layer];
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    const double latDeg = std::abs(lat) * 180.0 / Pi;
    // Trade winds westward below 30 deg, westerlies eastward 30-60.
    const double zonal = latDeg < 30.0 ? -1.0 : latDeg < 60.0 ? 1.3 : -0.4;
    const glm::dvec3 advected = rotateY(dir, -zonal * m_windRadPerSecond * t * (layer == 0 ? 1.0 : 1.8));

    // Evolution: the pattern also turns slowly about a tilted axis (a
    // bounded drift through the noise, unlike the zonal advection).
    const glm::dvec3 axis = glm::normalize(glm::dvec3(0.31, 0.77, 0.56));
    const double turn = t / 86400.0 * 0.12;
    const std::uint64_t layerSeed = deriveSeed(m_seed, "layer", layer);
    glm::dvec3 p = advected;
    if (L.kind == "cirrus")
        p = glm::normalize(glm::dvec3(advected.x, advected.y * 2.5, advected.z)); // streaks
    const double pattern =
        fbm(layerSeed,
            p * std::cos(turn) + glm::cross(axis, p) * std::sin(turn) + axis * glm::dot(axis, p) * (1.0 - std::cos(turn)),
            m_body.radiusM, m_pattern, 0.0, 0.0)
            .macro +
        0.35 * fbm(layerSeed + 1, p, m_body.radiusM, OctaveSpec {m_body.radiusM * 0.05, 1.92, 0.5, 4, 1.0}, 0.0, 0.0)
                   .macro;

    // Cyclones: blended between consecutive epochs (no popping).
    const double epochF = t / EpochSeconds;
    const auto epoch = static_cast<std::int64_t>(std::floor(epochF));
    const double blend = smoothstep(0.75, 1.0, epochF - static_cast<double>(epoch));
    const double storms = (1.0 - blend) * cyclones(advected, epoch) + blend * cyclones(advected, epoch + 1);

    const double base = tendency(dir) + L.coverageBias;
    const double value = base - 0.5 + 0.55 * pattern + 0.7 * storms * (layer == 0 ? 1.0 : 0.4);
    return static_cast<float>(smoothstep(0.0, 0.28, value) * L.opacity);
}
} // namespace world::celestial::surface
