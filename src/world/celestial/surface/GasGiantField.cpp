#include "src/world/celestial/surface/GasGiantField.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
    constexpr double Pi = 3.14159265358979323846;

    double fraction(const BodySurfaceDescriptor& body, const char* gas)
    {
        for (const auto& [name, value] : body.atmosphereComposition)
            if (name == gas)
                return value;
        return 0.0;
    }
}

GasGiantField::GasGiantField(const BodySurfaceDescriptor& body, double epochSeconds)
    : m_body(body), m_epoch(epochSeconds)
{
    m_iceGiant = surfaceArchetype(body) == SurfaceArchetype::IceGiant;
    m_radius = std::max(1.0e6, body.radiusM);
    m_surfaceSeed = surfaceSeed(body.seed);
    m_seedBands = layerSeed(m_surfaceSeed, "zonal_bands");
    m_seedEddies = layerSeed(m_surfaceSeed, "eddies");
    m_seedStorms = layerSeed(m_surfaceSeed, "storms");

    // More jets on a large fast rotator (Rhines scale ~ sqrt(U / beta)).
    const double size = std::clamp(m_radius / 7.0e7, 0.3, 2.0);
    const double spin = std::clamp(10.0 / std::max(1.0, body.rotationPeriodHours), 0.2, 2.5);
    m_jets = m_iceGiant ? 2.0 + 1.0 * spin : 3.5 + 2.5 * std::sqrt(size * spin);
    m_jets += unitFromHash(m_seedBands) * 0.8;
    m_contrast = m_iceGiant ? 0.35 : 1.0;

    // Palette from the composition and temperature (cloud chemistry).
    const double methane = fraction(body, "CH4");
    if (body.meanTemperatureK > 900.0)
    {
        m_zone = {0.42f, 0.30f, 0.24f}; // hot: alkali / silicate haze
        m_belt = {0.20f, 0.12f, 0.10f};
    }
    else if (m_iceGiant || methane > 0.015)
    {
        m_zone = {0.62f, 0.84f, 0.90f}; // methane absorbs red
        m_belt = {0.44f, 0.66f, 0.82f};
    }
    else
    {
        m_zone = {0.93f, 0.88f, 0.76f}; // ammonia ice zones
        m_belt = {0.66f, 0.47f, 0.32f}; // tholin / sulfur belts
    }
    const double tint = unitFromHash(m_seedBands, 3) - 0.5;
    m_belt *= glm::vec3(static_cast<float>(1.0 + 0.12 * tint), 1.0f, static_cast<float>(1.0 - 0.12 * tint));
}

glm::dvec3 GasGiantField::drifted(const glm::dvec3& dir) const
{
    // Differential rotation: alternating jets drift relative to each other.
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    const double speed = 2.0e-6 * std::cos(lat * m_jets * 2.0) * std::cos(lat);
    const double a = speed * m_epoch;
    const double c = std::cos(a);
    const double s = std::sin(a);
    return glm::dvec3(dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c);
}

void GasGiantField::storms(const glm::dvec3& dir, std::vector<Storm>& out) const
{
    const double cell = 0.32;
    const glm::dvec3 base = glm::floor(dir / cell);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const glm::dvec3 c = base + glm::dvec3(dx, dy, dz);
                const std::uint64_t h = latticeHash(m_seedStorms, static_cast<std::int64_t>(c.x),
                                                    static_cast<std::int64_t>(c.y), static_cast<std::int64_t>(c.z));
                if (unitFromHash(h, 0) > 0.22)
                    continue;
                const glm::dvec3 q = (c + glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) *
                                     cell;
                if (std::abs(glm::length(q) - 1.0) > cell * 0.5 || std::abs(q.y) > 0.8)
                    continue;
                Storm s;
                s.id = h;
                s.center = glm::normalize(q);
                s.radiusRad = 0.03 + 0.10 * std::pow(unitFromHash(h, 4), 2.0);
                s.red = unitFromHash(h, 5) < 0.35 && !m_iceGiant;
                out.push_back(s);
            }
}

SurfaceSample GasGiantField::sample(const glm::dvec3& dir, double) const
{
    SurfaceSample s;
    s.biome = Biome::GasDeck;
    s.temperatureK = static_cast<float>(m_body.meanTemperatureK);
    std::vector<Storm> list;
    storms(drifted(dir), list);
    for (const Storm& st : list)
        if (std::acos(std::clamp(glm::dot(st.center, drifted(dir)), -1.0, 1.0)) < st.radiusRad)
        {
            s.featureClass = FeatureClass::Storm;
            s.featureId = st.id;
        }
    return s;
}

SurfaceAppearance GasGiantField::appearance(const SurfaceSample&, const glm::dvec3& dirIn, double) const
{
    SurfaceAppearance a;
    const glm::dvec3 dir = drifted(dirIn);
    // Zonal stretch: noise varies fast across latitude, slowly along it.
    const glm::dvec3 stretched = glm::normalize(glm::dvec3(dir.x, dir.y * 3.5, dir.z));
    OctaveSpec eddy {m_radius * 0.25, 1.92, 0.55, 7, 2.0};
    const double warp = fbm(m_seedEddies, stretched, m_radius, eddy, 0.0, 0.0).macro;
    double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    const double edge = std::abs(std::cos(lat * (2.0 * m_jets + 1.0)));
    lat += warp * 0.12 * (0.35 + 0.65 * edge);

    // Storm vortices: swirl the latitude inside the oval.
    std::vector<Storm> list;
    storms(dir, list);
    double stormMix = 0.0;
    bool red = false;
    for (const Storm& st : list)
    {
        const double d = std::acos(std::clamp(glm::dot(st.center, dir), -1.0, 1.0)) / st.radiusRad;
        if (d < 1.6)
        {
            const double inside = 1.0 - smoothstep(0.6, 1.0, d);
            lat += (std::sin(d * 9.0) * 0.02) * inside;
            if (inside > stormMix)
            {
                stormMix = inside;
                red = st.red;
            }
        }
    }

    const double band = std::sin(lat * (2.0 * m_jets + 1.0));
    const double fine = fbm(m_seedEddies + 3, stretched, m_radius, OctaveSpec {m_radius * 0.05, 1.92, 0.5, 5, 2.0},
                            0.0, 0.0)
                            .macro;
    // Sharpened band edges and fine turbulence (festoons, filaments).
    const double sharp = std::tanh(band * 2.2) / std::tanh(2.2);
    const float t = static_cast<float>(std::clamp(0.5 + 0.5 * sharp * m_contrast + 0.32 * fine, 0.0, 1.0));
    glm::vec3 c = glm::mix(m_belt, m_zone, t);
    // Polar regions are darker and bluer (haze).
    const float polar = static_cast<float>(smoothstep(0.75, 0.98, std::abs(dir.y)));
    c = glm::mix(c, c * glm::vec3(0.75f, 0.8f, 0.95f), polar);
    if (stormMix > 0.0)
        c = glm::mix(c, red ? m_stormRed : m_zone * 1.05f, static_cast<float>(stormMix * 0.8));
    a.albedo = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));
    a.weights.fill(0.0f);
    return a;
}

void GasGiantField::landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const
{
    std::vector<Storm> list;
    storms(drifted(dir), list);
    for (const Storm& st : list)
        if (std::acos(std::clamp(glm::dot(st.center, drifted(dir)), -1.0, 1.0)) < radiusRad + st.radiusRad)
            out.push_back(SurfaceFeature {FeatureClass::Storm, st.id, st.center, st.radiusRad * m_radius});
    (void)Pi;
}
} // namespace world::celestial::surface
