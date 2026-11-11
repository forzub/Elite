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

    // rotation of p about a unit axis by angle a (Rodrigues)
    glm::dvec3 rotateAbout(const glm::dvec3& p, const glm::dvec3& axis, double a)
    {
        const double c = std::cos(a);
        return p * c + glm::cross(axis, p) * std::sin(a) + axis * glm::dot(axis, p) * (1.0 - c);
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
    // weather systems ~ a fifth of the radius (Earth: fronts / lows ~1000-2000 km)
    m_pattern = OctaveSpec {body.radiusM * 0.30, 1.92, 0.50, 6, 1.5};
}

float CloudWeatherField::tendency(const glm::dvec3& dir) const
{
    // The climate's cloudiness by latitude - a tendency the weather rides on, not the picture itself
    // (2026-11-11: narrow strong belts drew three cloud rings with clear subtropics and poles).
    const double latDeg = std::abs(std::asin(std::clamp(dir.y, -1.0, 1.0))) * 180.0 / Pi;
    const double t = 0.50 + 0.16 * gauss(latDeg, 0.0, 10.0) - 0.20 * gauss(latDeg, 24.0, 9.0) +
                     0.14 * gauss(latDeg, 55.0, 14.0) + 0.04 * gauss(latDeg, 90.0, 20.0);
    return static_cast<float>(std::clamp(t * (0.55 + 0.75 * m_body.cloudPotential), 0.0, 1.0));
}

double CloudWeatherField::zonalWind(std::size_t layer, double lat) const
{
    if (m_layers[layer].kind == "deck")
        return 1.6 * m_windRadPerSecond * std::cos(lat);   // one super-rotating deck
    // easterly trades at the equator, westerlies at ~45 deg, polar easterlies - one smooth profile
    const double s = std::sin(2.0 * lat);
    const double u = -std::cos(4.0 * lat) * (1.0 + 0.3 * s * s);
    return u * m_windRadPerSecond * (layer == 0 ? 1.0 : 1.8);
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
                // Storm tracks: tropical cyclones 8-25 deg (small, tight), mid-latitude lows 40-65 (large,
                // loosely wound: the comma of a front)
                const double tropical = 0.6 * gauss(latDeg, 16.0, 7.0);
                const double midLatitude = 0.8 * gauss(latDeg, 52.0, 10.0);
                if (unitFromHash(h, 0) > (tropical + midLatitude) * 0.55)
                    continue;
                const bool low = midLatitude > tropical;
                const double radius = low ? 0.10 + 0.12 * unitFromHash(h, 4) : 0.04 + 0.05 * unitFromHash(h, 4);
                const glm::dvec3 rel = dir - centre * glm::dot(dir, centre);
                const double d = glm::length(rel) / radius;
                if (d > 1.4)
                    continue;
                // Spiral arms: the angle around the centre wound with distance.
                const glm::dvec3 e1 = glm::normalize(glm::cross(centre, glm::dvec3(0, 1, 0.01)));
                const glm::dvec3 e2 = glm::cross(centre, e1);
                const double angle = std::atan2(glm::dot(rel, e2), glm::dot(rel, e1));
                const double hemisphere = centre.y >= 0.0 ? 1.0 : -1.0;
                const double arms = low ? 0.5 + 0.5 * std::cos(angle + hemisphere * d * 3.0)
                                        : 0.5 + 0.5 * std::cos(2.0 * (angle + hemisphere * d * 5.0));
                const double core = 1.0 - smoothstep(0.0, 1.4, d);
                const double eye = low ? 1.0 : smoothstep(0.03, 0.10, d);
                best = std::max(best, core * (0.45 + 0.55 * arms) * eye);
            }
    return static_cast<float>(best);
}

double CloudWeatherField::deckClosure(std::size_t layer) const
{
    // a dense deck has (almost) no holes: its weather shows as shading of the deck (structure), not as gaps
    return m_layers[layer].kind == "deck" ? 0.45 : 0.0;
}

float CloudWeatherField::coverage(std::size_t layer, const glm::dvec3& dir, double t) const
{
    if (layer >= m_layers.size())
        return 0.0f;
    return static_cast<float>(smoothstep(0.0, 0.28, rawValue(layer, dir, t) + deckClosure(layer)) *
                              m_layers[layer].opacity);
}

float CloudWeatherField::structure(std::size_t layer, const glm::dvec3& dir, double t) const
{
    if (layer >= m_layers.size())
        return 0.0f;
    // the unsaturated value of a deck lies ~0..1 (measured: Venus p95 0.45, Alpha Centauri A h p95 0.95)
    return static_cast<float>(std::clamp(rawValue(layer, dir, t), 0.0, 1.0));
}

void CloudWeatherField::sample(std::size_t layer, const glm::dvec3& dir, double t, float& cover, float& shape) const
{
    if (layer >= m_layers.size())
    {
        cover = shape = 0.0f;
        return;
    }
    const double raw = rawValue(layer, dir, t);
    cover = static_cast<float>(smoothstep(0.0, 0.28, raw + deckClosure(layer)) * m_layers[layer].opacity);
    shape = static_cast<float>(std::clamp(raw, 0.0, 1.0));
}

double CloudWeatherField::pattern(std::size_t layer, std::uint64_t seed, const glm::dvec3& dir, double age) const
{
    const CloudLayer& L = m_layers[layer];
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    glm::dvec3 p = rotateY(dir, -zonalWind(layer, lat) * age);
    // the pattern also turns slowly about a tilted axis while it lives (development, not transport)
    p = rotateAbout(p, glm::normalize(glm::dvec3(0.31, 0.77, 0.56)), age / 86400.0 * 0.12);
    if (L.kind == "cirrus")
        p = glm::normalize(glm::dvec3(p.x, p.y * 2.5, p.z));   // streaks
    else if (L.kind == "deck")
        p = glm::normalize(glm::dvec3(p.x, p.y * 3.0, p.z));   // super-rotation: zonal streaks and chevrons
    // a large, slow swirl of the flow (domain warp along east / north): fronts and eddies, not blobs
    glm::dvec3 east = glm::cross(glm::dvec3(0.0, 1.0, 0.0), p);
    const double el = glm::length(east);
    east = el > 1.0e-6 ? east / el : glm::dvec3(1.0, 0.0, 0.0);
    const glm::dvec3 north = glm::cross(p, east);
    const OctaveSpec warpSpec {m_body.radiusM * 0.6, 1.92, 0.5, 3, 1.0};
    const double wx = fbm(seed + 7, p, m_body.radiusM, warpSpec, 0.0, 0.0).macro;
    const double wy = fbm(seed + 8, p, m_body.radiusM, warpSpec, 0.0, 0.0).macro;
    const glm::dvec3 q = glm::normalize(p + (east * wx + north * wy) * 0.25);
    // normalised to a unit standard deviation (the two sums measured: std 0.123 and 0.135)
    return (fbm(seed, q, m_body.radiusM, m_pattern, 0.0, 0.0).macro +
            0.35 * fbm(seed + 1, q, m_body.radiusM, OctaveSpec {m_body.radiusM * 0.05, 1.92, 0.5, 4, 1.0}, 0.0, 0.0)
                       .macro) /
           0.131;
}

double CloudWeatherField::rawValue(std::size_t layer, const glm::dvec3& dir, double t) const
{
    const CloudLayer& L = m_layers[layer];
    const std::uint64_t layerSeed = deriveSeed(m_seed, "layer", layer);

    // The weather: one pattern per advection cycle, advected from the moment it fades in (a quarter cycle
    // before its own cycle) and cross-faded into the next over the last quarter - bounded shear, no seams.
    const double phase = t / AdvectionCycleSeconds;
    const auto cycle = static_cast<std::int64_t>(std::floor(phase));
    const double f = phase - static_cast<double>(cycle);
    const double blend = smoothstep(0.75, 1.0, f);
    const auto cycleSeed = [&](std::int64_t c) { return deriveSeed(layerSeed, "cycle", static_cast<std::uint64_t>(c)); };
    double weather = pattern(layer, cycleSeed(cycle), dir, (f + 0.25) * AdvectionCycleSeconds);
    if (blend > 0.0)
    {
        const double next = pattern(layer, cycleSeed(cycle + 1), dir, (f - 0.75) * AdvectionCycleSeconds);
        // two independent fields mixed keep their contrast (no grey mid-fade)
        weather = ((1.0 - blend) * weather + blend * next) / std::sqrt((1.0 - blend) * (1.0 - blend) + blend * blend);
    }

    // Cyclones: each lives one epoch, advected from its epoch start, blended into the next epoch.
    const double epochF = t / EpochSeconds;
    const auto epoch = static_cast<std::int64_t>(std::floor(epochF));
    const double eBlend = smoothstep(0.75, 1.0, epochF - static_cast<double>(epoch));
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    const auto stormsOf = [&](std::int64_t e) {
        const double age = t - static_cast<double>(e) * EpochSeconds;
        return static_cast<double>(cyclones(rotateY(dir, -zonalWind(0, lat) * age), e));
    };
    double storms = 0.0;
    if (L.kind != "deck")
    {
        storms = (1.0 - eBlend) * stormsOf(epoch);
        if (eBlend > 0.0)
            storms += eBlend * stormsOf(epoch + 1);
    }

    // the climate leans, the weather decides: the belts shift the odds, the pattern draws the clouds
    return 0.7 * (tendency(dir) - 0.5) + L.coverageBias + 0.08 + 0.18 * weather +
           0.7 * storms * (layer == 0 ? 1.0 : 0.4);
}
} // namespace world::celestial::surface
