#include "src/world/celestial/surface/PlanetSurfaceField.h"

#include <algorithm>
#include <mutex>
#include <cmath>

#include "src/world/celestial/surface/GasGiantField.h"
#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
    constexpr double Pi = 3.14159265358979323846;
    constexpr std::size_t MaterialCount = static_cast<std::size_t>(SurfaceMaterial::Count);

    std::size_t m(SurfaceMaterial material)
    {
        return static_cast<std::size_t>(material);
    }

    // Octaves of a series from its base wavelength down to `finestM`.
    int octavesDownTo(double baseM, double finestM, double lacunarity)
    {
        return std::max(1, static_cast<int>(std::ceil(std::log(baseM / finestM) / std::log(lacunarity))) + 1);
    }

    // Amplitude share of the octaves finer than `cutoffM` (normalised like
    // the sums in SurfaceNoise).
    double tailShare(const OctaveSpec& spec, double cutoffM)
    {
        double full = 0.0;
        double tail = 0.0;
        double amp = 1.0;
        double wavelength = spec.baseWavelengthM;
        for (int i = 0; i < spec.octaves; ++i)
        {
            full += amp;
            if (wavelength < cutoffM)
                tail += amp;
            amp *= spec.gain;
            wavelength /= spec.lacunarity;
        }
        return full > 0.0 ? tail / full : 0.0;
    }

    // Fibonacci sphere point i of n (deterministic quantile sampling).
    glm::dvec3 fibonacciPoint(int i, int n)
    {
        const double y = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(n);
        const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        const double phi = static_cast<double>(i) * 2.399963229728653;
        return glm::dvec3(r * std::cos(phi), y, r * std::sin(phi));
    }

    double volcanismLevel(const std::string& v)
    {
        if (v == "low")
            return 0.08;
        if (v == "moderate")
            return 0.2;
        if (v == "high")
            return 0.35;
        if (v == "extreme" || v == "cryovolcanic")
            return 0.5;
        return 0.0;
    }

    const glm::vec3 MaterialAlbedo[] = {
        {0.36f, 0.33f, 0.30f}, // Rock
        {0.13f, 0.13f, 0.14f}, // Basalt
        {0.46f, 0.44f, 0.41f}, // Regolith
        {0.80f, 0.68f, 0.48f}, // Sand
        {0.38f, 0.30f, 0.21f}, // Soil
        {0.11f, 0.24f, 0.08f}, // Vegetation (lush; dry vegetation is mixed in)
        {0.70f, 0.80f, 0.88f}, // Ice
        {0.94f, 0.95f, 0.97f}, // Snow
        {0.86f, 0.83f, 0.76f}, // Salt
        {0.20f, 0.06f, 0.03f}, // Lava (crust; glow is emission)
    };
}

PlanetSurfaceField::PlanetSurfaceField(const BodySurfaceDescriptor& body)
    : m_body(body), m_archetype(surfaceArchetype(body))
{
    m_radius = std::max(1000.0, body.radiusM);
    m_reliefM = std::max(200.0, body.reliefAmplitudeKm * 1000.0);
    m_surfaceSeed = surfaceSeed(body.seed);
    m_seedContinents = layerSeed(m_surfaceSeed, "continents");
    m_seedWarp = layerSeed(m_surfaceSeed, "continent_warp");
    m_seedBelts = layerSeed(m_surfaceSeed, "range_belts");
    m_seedRanges = layerSeed(m_surfaceSeed, "ranges");
    m_seedHills = layerSeed(m_surfaceSeed, "hills");
    m_seedCraters = layerSeed(m_surfaceSeed, "impact_craters");
    m_seedVolcanoes = layerSeed(m_surfaceSeed, "volcanoes");
    m_seedLakes = layerSeed(m_surfaceSeed, "lakes");
    m_seedClimate = layerSeed(m_surfaceSeed, "climate");
    m_seedVariation = layerSeed(m_surfaceSeed, "albedo_variation");
    m_seedBoulders = layerSeed(m_surfaceSeed, "boulders");

    const double finest = 0.25;
    // Coastlines / landmarks: everything down to ~R/5000 (1.3 km on Earth).
    m_macroCutoffM = std::clamp(m_radius * 2.0e-4, 100.0, 2000.0);
    // Fractal roughness: amplitude ~ wavelength^H per octave.
    const double h = 0.95 - 0.40 * std::clamp(body.roughness, 0.0, 1.0);

    m_continentSpec.baseWavelengthM = m_radius * 1.3;
    m_continentSpec.gain = 0.52;
    m_continentSpec.octaves = 6;
    m_continentSpec.expStrength = 2.5;

    m_warpSpec.baseWavelengthM = m_radius * 1.8;
    m_warpSpec.gain = 0.5;
    m_warpSpec.octaves = 3;
    m_warpSpec.expStrength = 0.0;

    m_beltSpec.baseWavelengthM = m_radius * 1.1;
    m_beltSpec.gain = 0.45;
    m_beltSpec.octaves = 3;
    m_beltSpec.expStrength = 1.0;

    m_rangeSpec.baseWavelengthM = std::min(m_radius * 0.08, 600000.0);
    m_rangeSpec.gain = std::pow(m_rangeSpec.lacunarity, -h);
    m_rangeSpec.octaves = octavesDownTo(m_rangeSpec.baseWavelengthM, finest, m_rangeSpec.lacunarity);
    m_rangeSpec.expStrength = 5.0; // Tobler mu ~ 1.02

    m_hillSpec.baseWavelengthM = std::min(m_radius * 0.025, 150000.0);
    m_hillSpec.gain = std::pow(m_hillSpec.lacunarity, -h);
    m_hillSpec.octaves = octavesDownTo(m_hillSpec.baseWavelengthM, finest, m_hillSpec.lacunarity);
    m_hillSpec.expStrength = 4.0;

    m_damping = 0.5 + 6.0 * std::clamp(body.erosionPotential, 0.0, 1.0);

    // Tectonic style -> mountain ranges.
    const std::string& tect = body.tectonicRegime;
    m_rangeStrength = tect == "mobile_lid" ? 0.55 : tect == "episodic" ? 0.45 : tect == "stagnant_lid" ? 0.22 : 0.15;
    if (m_archetype == SurfaceArchetype::Icy)
        m_rangeStrength = 0.12;
    m_hillStrength = 0.10 + 0.14 * std::clamp(body.roughness, 0.0, 1.0);

    m_ocean = m_archetype == SurfaceArchetype::Terrestrial && body.oceanCoverage > 0.0;
    m_volcanoProbability = volcanismLevel(body.volcanism);
    if (body.terrainProfile == "volcanic_plains")
        m_volcanoProbability = std::max(m_volcanoProbability, 0.3);
    m_lakeProbability = m_archetype == SurfaceArchetype::Terrestrial ? 0.45
                        : body.terrainProfile == "hydrocarbon_lakes" ? 0.5
                                                                      : 0.0;

    // Crater classes from basin size down to ~100 m; density from the
    // descriptor, fewer where the surface is renewed / eroded.
    const double density = std::clamp(body.craterDensity, 0.0, 1.0) *
                           (1.0 - 0.6 * std::clamp(body.resurfacingRate, 0.0, 1.0)) *
                           (1.0 - 0.5 * std::clamp(body.erosionPotential, 0.0, 1.0));
    double cell = m_radius * 0.30;
    while (cell > 160.0)
    {
        CraterClass c;
        c.cellM = cell;
        c.minRadius = 0.10;
        c.maxRadius = 0.34;
        c.probability = std::clamp(density * 0.85, 0.0, 0.95);
        m_craterClasses.push_back(c);
        cell /= 3.2;
    }

    // Land threshold: the quantile of the continent signal that gives the
    // descriptor's land fraction (dry bodies: highland fraction 0.6).
    {
        const int n = 4096;
        std::vector<double> values;
        values.reserve(n);
        for (int i = 0; i < n; ++i)
            values.push_back(continentSignal(fibonacciPoint(i, n)));
        std::sort(values.begin(), values.end());
        const double ocean = m_ocean ? std::clamp(body.oceanCoverage, 0.02, 0.98) : 0.4;
        m_landThreshold = values[static_cast<std::size_t>(ocean * (n - 1))];
    }

    // Climate.
    const double spread = std::max(0.0, body.maxTemperatureK - body.minTemperatureK);
    const double pressureBar = body.surfacePressurePa / 1.0e5;
    m_equatorPoleK = std::clamp(spread * 1.4, 15.0, 160.0) / (1.0 + 0.5 * std::max(0.0, pressureBar - 1.0));
    m_equatorPoleK *= std::max(0.3, std::cos(body.obliquityDeg * Pi / 180.0));
    m_lapseKPerM = body.surfacePressurePa > 1000.0 ? 0.0065 : 0.0;
    double capFraction = std::clamp(body.iceCoverage, 0.0, 0.9);
    if (m_archetype == SurfaceArchetype::DryAtmospheric && body.meanTemperatureK < 260.0)
        capFraction = std::max(capFraction, 0.06 * body.glaciationPotential);
    m_capSinLatitude = capFraction > 0.0 ? 1.0 - capFraction : 2.0;

    // Bedrock / cover mixture of the composition.
    for (const auto& [name, fraction] : body.surfaceMaterials)
    {
        const float f = static_cast<float>(fraction);
        SurfaceMaterial mat = SurfaceMaterial::Rock;
        if (name.find("basalt") != std::string::npos || name.find("olivine") != std::string::npos ||
            name.find("peridot") != std::string::npos)
            mat = SurfaceMaterial::Basalt;
        else if (name.find("regolith") != std::string::npos || name.find("anorthosite") != std::string::npos)
            mat = SurfaceMaterial::Regolith;
        else if (name.find("sediment") != std::string::npos || name.find("tholin") != std::string::npos ||
                 name.find("organic") != std::string::npos)
            mat = SurfaceMaterial::Soil;
        else if (name.find("carbonate") != std::string::npos || name.find("sulfate") != std::string::npos ||
                 name.find("salt") != std::string::npos || name.find("chloride") != std::string::npos)
            mat = SurfaceMaterial::Salt;
        else if (name.find("ice") != std::string::npos)
            mat = SurfaceMaterial::Ice;
        else if (name.find("iron_oxide") != std::string::npos)
        {
            mat = SurfaceMaterial::Regolith;
            m_ironOxide += f;
        }
        else if (name.find("sulfur") != std::string::npos)
        {
            mat = SurfaceMaterial::Salt;
            m_sulfur += f;
        }
        m_bedrock[m(mat)] += f;
    }
    float sum = 0.0f;
    for (float w : m_bedrock)
        sum += w;
    if (sum <= 0.0f)
    {
        m_bedrock[m(m_archetype == SurfaceArchetype::Icy ? SurfaceMaterial::Ice : SurfaceMaterial::Rock)] = 1.0f;
        sum = 1.0f;
    }
    for (float& w : m_bedrock)
        w /= sum;
    if (m_archetype == SurfaceArchetype::DryAtmospheric && m_ironOxide == 0.0f && body.ironCore > 0.2)
        m_ironOxide = 0.1f;
    m_regolithTint = glm::mix(glm::vec3(1.0f), glm::vec3(1.35f, 0.78f, 0.55f), std::min(1.0f, m_ironOxide * 3.0f));
    m_regolithTint = glm::mix(m_regolithTint, glm::vec3(1.25f, 1.15f, 0.45f), std::min(1.0f, m_sulfur * 3.0f));
    m_vegetation = m_archetype == SurfaceArchetype::Terrestrial && body.meanTemperatureK > 255.0 &&
                   body.meanTemperatureK < 330.0;

    // What LOD detail can add to the macro height (checks, skirts).
    const double rangeH = m_reliefM * 1.4 * m_rangeStrength;
    const double hillH = m_reliefM * m_hillStrength;
    m_detailBoundM = rangeH * tailShare(m_rangeSpec, m_macroCutoffM) +
                     hillH * tailShare(m_hillSpec, m_macroCutoffM) + 0.25 * m_macroCutoffM + 3.0;
}

double PlanetSurfaceField::continentSignal(const glm::dvec3& dir) const
{
    const glm::dvec3 warp(fbm(m_seedWarp, dir, m_radius, m_warpSpec, 0.0, 0.0).macro,
                          fbm(m_seedWarp + 1, dir, m_radius, m_warpSpec, 0.0, 0.0).macro,
                          fbm(m_seedWarp + 2, dir, m_radius, m_warpSpec, 0.0, 0.0).macro);
    const glm::dvec3 warped = glm::normalize(dir + warp * 0.45);
    return fbm(m_seedContinents, warped, m_radius, m_continentSpec, 0.0, 0.0).macro;
}

PlanetSurfaceField::Terrain PlanetSurfaceField::terrain(const glm::dvec3& dir, double minWavelengthM) const
{
    Terrain t;
    t.continent = continentSignal(dir) - m_landThreshold;
    const double c = t.continent;

    double base = 0.0;
    if (m_ocean)
    {
        // Continuous at the coast: a gentle land profile, a shelf, then the
        // continental slope into the abyss.
        base = c > 0.0 ? m_reliefM * 0.12 * (1.0 - std::exp(-c / 0.12))
                       : -m_reliefM * 0.5 * (1.0 - std::exp(c / 0.05));
    }
    else
    {
        // Highlands / basins dichotomy (maria, lowland plains).
        base = m_reliefM * 0.22 * std::tanh(c * 3.5);
    }

    // Mountain belts: narrow bands along zero crossings of a separate field.
    const double beltField = fbm(m_seedBelts, dir, m_radius, m_beltSpec, 0.0, 0.0).macro;
    double belt = 1.0 - smoothstep(0.0, 0.07, std::abs(beltField));
    belt *= m_ocean ? smoothstep(-0.10, 0.06, c) : 1.0;
    t.ranges = belt;

    const BandSum ridged = ridgedMultifractal(m_seedRanges, dir, m_radius, m_rangeSpec, m_macroCutoffM, minWavelengthM);
    const BandSum hills = dampedFbm(m_seedHills, dir, m_radius, m_hillSpec, m_damping, m_macroCutoffM, minWavelengthM);
    const double rangeH = m_reliefM * 1.4 * m_rangeStrength;
    const double hillH = m_reliefM * m_hillStrength * (m_ocean ? 0.5 + 0.5 * smoothstep(-0.05, 0.1, c) : 1.0);

    t.macro = base + belt * ridged.macro * rangeH + hills.macro * hillH;
    t.total = base + belt * ridged.total * rangeH + hills.total * hillH;
    if (belt * ridged.macro > 0.18)
    {
        const glm::dvec3 cell = glm::floor(dir * 4.0);
        t.rangeId = latticeHash(m_seedBelts, static_cast<std::int64_t>(cell.x), static_cast<std::int64_t>(cell.y),
                                static_cast<std::int64_t>(cell.z));
    }
    return t;
}

void PlanetSurfaceField::craterCandidates(int classIndex, const glm::dvec3& dir, std::vector<Landmark>& out) const
{
    const CraterClass& cls = m_craterClasses[static_cast<std::size_t>(classIndex)];
    if (cls.probability <= 0.0)
        return;
    const std::uint64_t seed = deriveSeed(m_seedCraters, "class", static_cast<std::uint64_t>(classIndex));
    const glm::dvec3 p = dir * m_radius / cls.cellM;
    const glm::dvec3 base = glm::floor(p);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const auto cx = static_cast<std::int64_t>(base.x) + dx;
                const auto cy = static_cast<std::int64_t>(base.y) + dy;
                const auto cz = static_cast<std::int64_t>(base.z) + dz;
                const std::uint64_t h = latticeHash(seed, cx, cy, cz);
                if (unitFromHash(h, 0) >= cls.probability)
                    continue;
                const glm::dvec3 q = (glm::dvec3(static_cast<double>(cx), static_cast<double>(cy),
                                                 static_cast<double>(cz)) +
                                      glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) *
                                     cls.cellM;
                // Only candidates in the shell around the surface: uniform
                // density per area, each crater claimed by exactly one cell.
                if (std::abs(glm::length(q) - m_radius) > cls.cellM * 0.5)
                    continue;
                Landmark l;
                l.featureClass = FeatureClass::ImpactCrater;
                l.id = h;
                l.center = glm::normalize(q);
                l.radiusM = cls.cellM * (cls.minRadius + (cls.maxRadius - cls.minRadius) *
                                                            std::pow(unitFromHash(h, 4), 2.0));
                l.freshness = std::pow(unitFromHash(h, 5), 3.0);
                // Simple bowls are deep; complex craters flatter.
                const double depthRatio = l.radiusM < 8000.0 ? 0.2 : 0.2 * std::pow(8000.0 / l.radiusM, 0.6);
                const double degradation = 1.0 - 0.75 * std::clamp(m_body.erosionPotential, 0.0, 1.0);
                l.depthM = l.radiusM * depthRatio * (0.45 + 0.55 * (0.3 + 0.7 * l.freshness)) * degradation;
                out.push_back(l);
            }
}

void PlanetSurfaceField::volcanoCandidates(const glm::dvec3& dir, std::vector<Landmark>& out) const
{
    if (m_volcanoProbability <= 0.0)
        return;
    const double cellM = m_radius * 0.28;
    const glm::dvec3 base = glm::floor(dir * m_radius / cellM);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const auto cx = static_cast<std::int64_t>(base.x) + dx;
                const auto cy = static_cast<std::int64_t>(base.y) + dy;
                const auto cz = static_cast<std::int64_t>(base.z) + dz;
                const std::uint64_t h = latticeHash(m_seedVolcanoes, cx, cy, cz);
                if (unitFromHash(h, 0) >= m_volcanoProbability)
                    continue;
                const glm::dvec3 q = (glm::dvec3(static_cast<double>(cx), static_cast<double>(cy),
                                                 static_cast<double>(cz)) +
                                      glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) *
                                     cellM;
                if (std::abs(glm::length(q) - m_radius) > cellM * 0.5)
                    continue;
                Landmark l;
                l.featureClass = FeatureClass::Volcano;
                l.id = h;
                l.center = glm::normalize(q);
                l.radiusM = cellM * (0.06 + 0.16 * unitFromHash(h, 4));
                // Low gravity builds taller edifices (Olympus Mons).
                const double gravityScale = std::clamp(9.81 / std::max(0.5, m_body.surfaceGravityMs2), 0.6, 2.5);
                l.depthM = m_reliefM * (0.12 + 0.30 * unitFromHash(h, 5)) * gravityScale;
                out.push_back(l);
            }
}

void PlanetSurfaceField::lakeCandidates(const glm::dvec3& dir, std::vector<Landmark>& out) const
{
    if (m_lakeProbability <= 0.0)
        return;
    const double cellM = m_radius * 0.045;
    const glm::dvec3 base = glm::floor(dir * m_radius / cellM);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const auto cx = static_cast<std::int64_t>(base.x) + dx;
                const auto cy = static_cast<std::int64_t>(base.y) + dy;
                const auto cz = static_cast<std::int64_t>(base.z) + dz;
                if (const auto lake = lakeInCell(cx, cy, cz))
                    out.push_back(*lake);
            }
}

std::optional<PlanetSurfaceField::Landmark> PlanetSurfaceField::lakeInCell(std::int64_t cx, std::int64_t cy,
                                                                          std::int64_t cz) const
{
    const std::uint64_t key = latticeHash(0x1A4Eull, cx, cy, cz);
    {
        std::shared_lock lock(m_lakeMutex);
        const auto it = m_lakeCache.find(key);
        if (it != m_lakeCache.end())
            return it->second;
    }
    const double cellM = m_radius * 0.045;
    std::optional<Landmark> result;
    do
    {
        const std::uint64_t h = latticeHash(m_seedLakes, cx, cy, cz);
        if (unitFromHash(h, 0) >= m_lakeProbability)
            continue;
        const glm::dvec3 q = (glm::dvec3(static_cast<double>(cx), static_cast<double>(cy),
                                         static_cast<double>(cz)) +
                              glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) *
                             cellM;
        if (std::abs(glm::length(q) - m_radius) > cellM * 0.5)
            continue;
        const glm::dvec3 center = glm::normalize(q);
        // A lake needs dry land and enough water from the climate
        // (or a cold methane cycle near the poles).
        const Terrain at = terrain(center, m_macroCutoffM);
        float temperature = 0.0f;
        float moisture = 0.0f;
        climate(center, at.macro, at.continent, at.ranges, temperature, moisture);
        if (m_archetype == SurfaceArchetype::Terrestrial)
        {
            if (at.macro < 30.0 || at.macro > 3000.0 || at.ranges > 0.5)
                continue;
            const double glacial = std::abs(center.y) > 0.7 ? m_body.glaciationPotential : 0.0;
            if (moisture + 0.4 * glacial < 0.42 || temperature < 250.0f)
                continue;
        }
        else if (std::abs(center.y) < 0.75)
        {
            continue; // hydrocarbon lakes: polar regions
        }
        Landmark l;
        l.featureClass = FeatureClass::Lake;
        l.id = h;
        l.center = center;
        l.radiusM = cellM * (0.10 + 0.22 * unitFromHash(h, 4));
        l.depthM = std::min(400.0, l.radiusM * 0.004 + 30.0);
        l.levelM = at.macro;
        result = l;
    } while (false);
    std::unique_lock lock(m_lakeMutex);
    m_lakeCache.emplace(key, result);
    return result;
}

double PlanetSurfaceField::craterProfile(const Landmark& c, double r) const
{
    const double rim = c.depthM * 0.22;
    if (r >= 1.0)
        return rim / (r * r * r) * (r < 3.0 ? 1.0 : 0.0) - (r >= 3.0 ? 0.0 : rim / 27.0);
    if (c.radiusM < 8000.0)
        return -c.depthM * (1.0 - r * r) + rim * r * r * r * r - rim / 27.0;
    // Complex crater: flat floor, terraced wall, central peak.
    const double floorDepth = -c.depthM;
    const double wall = smoothstep(0.55, 1.0, r);
    const double peak = c.depthM * 0.5 * std::exp(-(r / 0.13) * (r / 0.13));
    return floorDepth * (1.0 - wall) + rim * wall + peak - rim / 27.0;
}

double PlanetSurfaceField::boulders(const glm::dvec3& dir, double minWavelengthM, double rockiness) const
{
    // Rocks of 0.5 - 6 m: only at ground LOD, only on rocky ground.
    constexpr double CellM = 8.0;
    if (minWavelengthM > CellM || rockiness <= 0.02)
        return 0.0;
    const double fade = std::clamp(CellM / minWavelengthM - 1.0, 0.0, 1.0);
    const glm::dvec3 p = dir * m_radius / CellM;
    const glm::dvec3 base = glm::floor(p);
    double h = 0.0;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const glm::dvec3 cell = base + glm::dvec3(dx, dy, dz);
                const std::uint64_t k = latticeHash(m_seedBoulders, static_cast<std::int64_t>(cell.x),
                                                    static_cast<std::int64_t>(cell.y),
                                                    static_cast<std::int64_t>(cell.z));
                if (unitFromHash(k, 0) > rockiness)
                    continue;
                const glm::dvec3 centre = cell + glm::dvec3(unitFromHash(k, 1), unitFromHash(k, 2), unitFromHash(k, 3));
                const double radius = (0.06 + 0.32 * std::pow(unitFromHash(k, 4), 2.0));
                const double d = glm::length(p - centre) / radius;
                if (d < 1.0)
                    h = std::max(h, radius * CellM * 0.8 * std::sqrt(1.0 - d * d));
            }
    return h * fade;
}

void PlanetSurfaceField::climate(const glm::dvec3& dir, double macroHeightM, double continent, double ranges,
                                 float& temperatureK, float& moisture) const
{
    static const OctaveSpec warpSpec {0.0, 1.92, 0.5, 3, 0.0};
    OctaveSpec spec = warpSpec;
    spec.baseWavelengthM = m_radius * 0.9;
    // Climate borders follow latitude but are deformed by circulation
    // (warp) - no straight colour stripes.
    const double warp = fbm(m_seedClimate, dir, m_radius, spec, 0.0, 0.0).macro * 0.22;
    const double lat = std::clamp(std::asin(std::clamp(dir.y, -1.0, 1.0)) + warp, -Pi / 2.0, Pi / 2.0);
    const double s2 = std::sin(lat) * std::sin(lat);
    const bool water = m_ocean && macroHeightM < 0.0;
    double t = m_body.meanTemperatureK + m_equatorPoleK * (1.0 / 3.0 - s2) * (water ? 0.8 : 1.0);
    t -= m_lapseKPerM * std::max(0.0, macroHeightM);
    temperatureK = static_cast<float>(t);

    if (m_archetype != SurfaceArchetype::Terrestrial)
    {
        moisture = static_cast<float>(std::clamp(m_body.weatherPotential * 0.2, 0.0, 1.0));
        return;
    }
    // Hadley / Ferrel / polar cells: wet ITCZ, dry subtropics, wet storm
    // tracks, dry poles; inland drier, rain shadow behind ranges.
    const double latDeg = std::abs(lat) * 180.0 / Pi;
    double band = 0.85 * std::exp(-(latDeg / 14.0) * (latDeg / 14.0)) +
                  0.60 * std::exp(-((latDeg - 55.0) / 16.0) * ((latDeg - 55.0) / 16.0)) + 0.36 -
                  0.18 * std::exp(-((latDeg - 25.0) / 8.0) * ((latDeg - 25.0) / 8.0));
    spec.baseWavelengthM = m_radius * 0.3;
    const double noise = fbm(m_seedClimate + 7, dir, m_radius, spec, 0.0, 0.0).macro;
    const double inland = smoothstep(0.0, 0.30, continent);
    double mo = band * (1.0 - 0.45 * inland) - 0.25 * ranges + 0.18 * noise;
    mo *= 0.35 + 0.65 * std::clamp(m_body.weatherPotential + 0.3, 0.0, 1.0);
    moisture = static_cast<float>(std::clamp(water ? 1.0 : mo, 0.0, 1.0));
}

Biome PlanetSurfaceField::biome(const SurfaceSample& s) const
{
    switch (m_archetype)
    {
        case SurfaceArchetype::AirlessRocky:
        case SurfaceArchetype::DryAtmospheric:
            return s.iceMask > 0.5f ? Biome::IceSheet : s.water ? Biome::Lake : Biome::Barren;
        case SurfaceArchetype::Volcanic:
            return Biome::Volcanic;
        case SurfaceArchetype::Icy:
            return s.lake ? Biome::Lake : Biome::IcePlain;
        default:
            break;
    }
    if (s.water)
        return s.iceMask > 0.5f ? Biome::IceSheet : s.lake ? Biome::Lake : Biome::Ocean;
    if (s.iceMask > 0.5f)
        return Biome::IceSheet;
    const double tc = s.temperatureK - 273.15;
    const double mo = s.moisture;
    if (tc < -8.0)
        return mo > 0.08 ? Biome::Snow : Biome::Tundra;
    if (tc < 0.0)
        return Biome::Tundra;
    if (mo < 0.20)
        return Biome::Desert;
    if (tc < 7.0)
        return mo > 0.38 ? Biome::BorealForest : Biome::Tundra;
    if (tc < 20.0)
        return mo > 0.48 ? Biome::TemperateForest : Biome::Grassland;
    if (mo > 0.62)
        return Biome::Rainforest;
    return mo > 0.32 ? Biome::Savanna : Biome::Desert;
}

SurfaceSample PlanetSurfaceField::sample(const glm::dvec3& dir, double minWavelengthM) const
{
    SurfaceSample s;
    const Terrain t = terrain(dir, minWavelengthM);
    double macro = t.macro;
    double total = t.total;
    if (t.rangeId != 0)
    {
        s.featureClass = FeatureClass::MountainRange;
        s.featureId = t.rangeId;
    }

    // Landmarks. A class whose features are all >= the macro cutoff is
    // macro (every LOD); smaller ones add detail only when resolved.
    std::vector<Landmark> marks;
    const glm::dvec3 x = dir * m_radius;
    double strongest = 0.0;
    for (std::size_t k = 0; k < m_craterClasses.size(); ++k)
    {
        const CraterClass& cls = m_craterClasses[k];
        const double largest = cls.cellM * cls.maxRadius;
        // A class that can hold a macro crater is always evaluated.
        if (largest < m_macroCutoffM && largest * 3.0 < minWavelengthM)
            continue;
        marks.clear();
        craterCandidates(static_cast<int>(k), dir, marks);
        for (const Landmark& c : marks)
        {
            const double r = glm::length(x - c.center * m_radius) / c.radiusM;
            if (r >= 3.0)
                continue;
            const double h = craterProfile(c, r);
            const bool isMacro = c.radiusM >= m_macroCutoffM;
            const double fade = std::clamp(c.radiusM * 2.0 / std::max(minWavelengthM, 1.0e-3) - 1.0, 0.0, 1.0);
            if (isMacro)
                macro += h;
            total += h * (isMacro ? 1.0 : fade);
            if (r < 1.3 && c.radiusM > strongest && isMacro)
            {
                strongest = c.radiusM;
                s.featureClass = FeatureClass::ImpactCrater;
                s.featureId = c.id;
            }
            if (r < 2.2)
                s.freshness = std::max(s.freshness, static_cast<float>(c.freshness * (1.0 - r / 2.2)));
        }
    }

    marks.clear();
    volcanoCandidates(dir, marks);
    for (const Landmark& v : marks)
    {
        const double r = glm::length(x - v.center * m_radius) / v.radiusM;
        if (r >= 1.0)
            continue;
        // Shield / cone with a summit caldera.
        double h = v.depthM * std::pow(1.0 - r, 1.6);
        if (r < 0.10)
            h -= v.depthM * 0.12 * (1.0 - r / 0.10);
        macro += h;
        total += h;
        if (r < 0.7)
        {
            s.featureClass = FeatureClass::Volcano;
            s.featureId = v.id;
        }
    }

    // Lakes: the bed is lowered, the water surface is the lake level; the
    // shore is kept above it.
    marks.clear();
    lakeCandidates(dir, marks);
    for (const Landmark& l : marks)
    {
        const double wobble = fbm(l.id, dir, m_radius, OctaveSpec {l.radiusM * 1.2, 1.92, 0.5, 3, 0.0}, 0.0, 0.0).macro;
        const double r = glm::length(x - l.center * m_radius) / l.radiusM * (1.0 + 0.35 * wobble);
        if (r >= 1.6)
            continue;
        if (r < 1.0)
        {
            const double bed = l.levelM - l.depthM * (1.0 - r * r);
            macro = std::min(macro, bed);
            total = std::min(total, bed);
            s.water = true;
            s.lake = true;
            s.waterLevelM = l.levelM;
            s.featureClass = FeatureClass::Lake;
            s.featureId = l.id;
        }
        else
        {
            const double shore = l.levelM + 2.0 + (r - 1.0) * 40.0;
            const double blend = smoothstep(1.0, 1.6, r);
            macro = std::max(macro, glm::mix(shore, macro, blend));
            total = std::max(total, glm::mix(shore, total, blend));
        }
    }

    s.macroHeightM = macro;
    if (m_ocean && !s.lake && macro < 0.0)
    {
        s.water = true;
        s.waterLevelM = 0.0;
    }
    if (m_body.terrainProfile == "lava_ocean" && macro < 0.0)
    {
        s.water = true; // a molten sea at the datum
        s.waterLevelM = 0.0;
    }

    climate(dir, macro, t.continent, t.ranges, s.temperatureK, s.moisture);

    // Ice caps (descriptor ice coverage), glaciers / sea ice from climate.
    {
        OctaveSpec capSpec {m_radius * 0.5, 1.92, 0.5, 3, 0.0};
        const double edge = std::abs(dir.y) + 0.06 * fbm(m_seedClimate + 13, dir, m_radius, capSpec, 0.0, 0.0).macro;
        double ice = smoothstep(m_capSinLatitude - 0.01, m_capSinLatitude + 0.01, edge);
        if (m_archetype == SurfaceArchetype::Terrestrial)
        {
            if (s.water && !s.lake)
                ice = std::max(ice, smoothstep(268.0, 262.0, s.temperatureK)); // sea ice
            else if (!s.water)
                ice = std::max(ice, smoothstep(258.0, 250.0, s.temperatureK) *
                                        smoothstep(0.08, 0.2, static_cast<double>(s.moisture)));
        }
        if (m_archetype == SurfaceArchetype::Icy)
            ice = 1.0;
        s.iceMask = static_cast<float>(ice);
        if (ice > 0.5 && m_capSinLatitude < 1.0 && edge > m_capSinLatitude && s.featureClass == FeatureClass::None)
        {
            s.featureClass = FeatureClass::IceCap;
            s.featureId = dir.y > 0.0 ? 1 : 2;
        }
    }

    // Displayed height: water at its level; land never below the water it
    // borders (the coastline is the sign of the macro field at every LOD).
    if (s.water)
    {
        s.heightM = s.waterLevelM;
    }
    else
    {
        const double floorM = m_ocean ? 0.5 : -1.0e9;
        double h = std::max(total, floorM);
        if (minWavelengthM < 8.0)
        {
            // Rocks on steep / high / bare ground only.
            const double rockiness = std::clamp(0.15 + 0.6 * t.ranges + (m_vegetation ? -0.1 : 0.2), 0.0, 0.8);
            h += boulders(dir, minWavelengthM, rockiness);
        }
        s.heightM = h;
    }
    if (s.featureClass == FeatureClass::None)
    {
        s.featureClass = FeatureClass::Continent;
        const glm::dvec3 cell = glm::floor(dir * 2.0);
        s.featureId = latticeHash(m_seedContinents, static_cast<std::int64_t>(cell.x),
                                  static_cast<std::int64_t>(cell.y), static_cast<std::int64_t>(cell.z)) |
                      (t.continent > 0.0 ? 1ull : 0ull);
    }
    s.biome = biome(s);
    return s;
}

SurfaceAppearance PlanetSurfaceField::appearance(const SurfaceSample& s, const glm::dvec3& dir, double slope) const
{
    SurfaceAppearance a;
    auto& w = a.weights;
    w.fill(0.0f);

    OctaveSpec varSpec {m_radius * 0.02, 1.92, 0.55, 5, 3.0};
    const double variation = fbm(m_seedVariation, dir, m_radius, varSpec, 0.0, 0.0).macro;
    const float steep = static_cast<float>(smoothstep(0.08, 0.30, slope));

    if (s.water)
    {
        if (s.biome == Biome::Volcanic || m_body.terrainProfile == "lava_ocean")
        {
            w[m(SurfaceMaterial::Lava)] = 1.0f;
            a.albedo = MaterialAlbedo[m(SurfaceMaterial::Lava)];
            a.emission = 0.9f;
            return a;
        }
        const double depth = std::max(0.0, s.waterLevelM - s.macroHeightM);
        const float deep = static_cast<float>(smoothstep(0.0, 3000.0, depth));
        glm::vec3 water = glm::mix(glm::vec3(0.05f, 0.22f, 0.26f), glm::vec3(0.012f, 0.045f, 0.11f), deep);
        if (s.lake && m_archetype == SurfaceArchetype::Icy)
            water = glm::vec3(0.04f, 0.035f, 0.03f); // hydrocarbon
        a.albedo = glm::mix(water, MaterialAlbedo[m(SurfaceMaterial::Ice)], s.iceMask);
        w[m(SurfaceMaterial::Ice)] = s.iceMask;
        return a;
    }

    // Bedrock of the composition is exposed on steep ground.
    const auto addBedrock = [&](float amount) {
        for (std::size_t i = 0; i < MaterialCount; ++i)
            w[i] += amount * m_bedrock[i];
    };

    switch (m_archetype)
    {
        case SurfaceArchetype::Terrestrial:
        {
            const double tc = s.temperatureK - 273.15;
            const float rock = std::clamp(steep + static_cast<float>(smoothstep(-2.0, -12.0, tc)) * 0.5f, 0.0f, 1.0f);
            const float snow = static_cast<float>(smoothstep(1.0, -6.0, tc)) *
                               static_cast<float>(smoothstep(0.05, 0.15, static_cast<double>(s.moisture)));
            float veg = 0.0f;
            if (m_vegetation)
                veg = static_cast<float>(smoothstep(-4.0, 6.0, tc) * smoothstep(0.18, 0.45, s.moisture));
            const float sand = static_cast<float>(smoothstep(0.26, 0.12, s.moisture) * smoothstep(2.0, 10.0, tc)) +
                               static_cast<float>(smoothstep(12.0, 2.0, s.heightM)) * 0.7f;
            const float open = 1.0f - rock;
            w[m(SurfaceMaterial::Snow)] = snow * (1.0f - 0.6f * steep);
            const float rest = 1.0f - w[m(SurfaceMaterial::Snow)];
            addBedrock(rock * rest);
            w[m(SurfaceMaterial::Vegetation)] = open * rest * veg * (1.0f - std::min(1.0f, sand));
            w[m(SurfaceMaterial::Sand)] = open * rest * std::min(1.0f, sand) * (1.0f - veg * 0.5f);
            w[m(SurfaceMaterial::Soil)] =
                std::max(0.0f, open * rest - w[m(SurfaceMaterial::Vegetation)] - w[m(SurfaceMaterial::Sand)]);
            w[m(SurfaceMaterial::Ice)] = s.iceMask;
            break;
        }
        case SurfaceArchetype::AirlessRocky:
        case SurfaceArchetype::DryAtmospheric:
        {
            const bool dunes = m_body.terrainProfile == "dune_desert";
            addBedrock(steep);
            const float loose = 1.0f - steep;
            w[m(SurfaceMaterial::Regolith)] += loose * (dunes ? 0.55f : 0.85f);
            w[m(SurfaceMaterial::Sand)] += loose * (dunes ? 0.45f : 0.0f);
            // Basalt plains in the lowlands (maria) of a basaltic body.
            const float maria =
                static_cast<float>(smoothstep(0.0, -m_reliefM * 0.15, s.macroHeightM)) * m_bedrock[m(SurfaceMaterial::Basalt)];
            w[m(SurfaceMaterial::Basalt)] += loose * maria * 1.5f;
            w[m(SurfaceMaterial::Ice)] += s.iceMask * 2.0f;
            break;
        }
        case SurfaceArchetype::Volcanic:
            w[m(SurfaceMaterial::Basalt)] = 1.0f - steep * 0.5f;
            addBedrock(steep * 0.5f);
            if (m_sulfur > 0.0f)
                w[m(SurfaceMaterial::Salt)] += static_cast<float>(smoothstep(-0.2, 0.4, variation));
            break;
        case SurfaceArchetype::Icy:
        default:
            w[m(SurfaceMaterial::Ice)] = 0.7f;
            w[m(SurfaceMaterial::Snow)] = static_cast<float>(0.3 * (1.0 - steep));
            // Dark contaminant (tholins, salts) in patches and fractures.
            w[m(SurfaceMaterial::Soil)] = static_cast<float>(
                smoothstep(0.1, 0.5, variation) * (1.0 - m_bedrock[m(SurfaceMaterial::Ice)]) * 1.2);
            addBedrock(steep * 0.3f);
            break;
    }

    float sum = 0.0f;
    for (float v : w)
        sum += v;
    if (sum <= 0.0f)
    {
        w[m(SurfaceMaterial::Rock)] = 1.0f;
        sum = 1.0f;
    }
    glm::vec3 albedo(0.0f);
    for (std::size_t i = 0; i < MaterialCount; ++i)
    {
        w[i] /= sum;
        glm::vec3 c = MaterialAlbedo[i];
        if (i == m(SurfaceMaterial::Regolith) || i == m(SurfaceMaterial::Sand) || i == m(SurfaceMaterial::Soil))
            c *= m_regolithTint;
        if (i == m(SurfaceMaterial::Vegetation))
            c = glm::mix(glm::vec3(0.36f, 0.34f, 0.18f), c, static_cast<float>(smoothstep(0.25, 0.6, s.moisture)));
        albedo += w[i] * c;
    }
    // Fresh ejecta is bright; deterministic regional variation.
    albedo *= 1.0f + 0.35f * s.freshness * (1.0f - w[m(SurfaceMaterial::Vegetation)]);
    albedo *= static_cast<float>(1.0 + 0.14 * variation);
    a.albedo = glm::clamp(albedo, glm::vec3(0.0f), glm::vec3(1.0f));
    if (m_archetype == SurfaceArchetype::Volcanic && m_body.meanTemperatureK > 700.0)
        a.emission = static_cast<float>(smoothstep(0.0, -m_reliefM * 0.1, s.macroHeightM)) * 0.6f;
    return a;
}

void PlanetSurfaceField::landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const
{
    // Macro landmarks around the cap: craters (macro classes), volcanoes,
    // lakes - sampled on a grid of directions inside the cap.
    std::vector<Landmark> marks;
    const glm::dvec3 axisA = glm::normalize(glm::cross(dir, std::abs(dir.y) < 0.9 ? glm::dvec3(0, 1, 0)
                                                                                   : glm::dvec3(1, 0, 0)));
    const glm::dvec3 axisB = glm::cross(dir, axisA);
    const int steps = 6;
    const auto addUnique = [&](const Landmark& l) {
        if (std::acos(std::clamp(glm::dot(l.center, dir), -1.0, 1.0)) > radiusRad + l.radiusM / m_radius)
            return;
        for (const auto& f : out)
            if (f.id == l.id && f.featureClass == l.featureClass)
                return;
        out.push_back(SurfaceFeature {l.featureClass, l.id, l.center, l.radiusM});
    };
    for (int i = -steps; i <= steps; ++i)
        for (int j = -steps; j <= steps; ++j)
        {
            const double a = radiusRad * static_cast<double>(i) / steps;
            const double b = radiusRad * static_cast<double>(j) / steps;
            const glm::dvec3 d = glm::normalize(dir + axisA * a + axisB * b);
            for (std::size_t k = 0; k < m_craterClasses.size(); ++k)
            {
                if (m_craterClasses[k].cellM * m_craterClasses[k].minRadius < m_macroCutoffM)
                    continue;
                marks.clear();
                craterCandidates(static_cast<int>(k), d, marks);
                for (const auto& l : marks)
                    addUnique(l);
            }
            marks.clear();
            volcanoCandidates(d, marks);
            lakeCandidates(d, marks);
            for (const auto& l : marks)
                addUnique(l);
        }
    std::sort(out.begin(), out.end(), [](const SurfaceFeature& a, const SurfaceFeature& b) {
        return a.featureClass != b.featureClass ? a.featureClass < b.featureClass : a.id < b.id;
    });
}

std::unique_ptr<SurfaceField> makeSurfaceField(const BodySurfaceDescriptor& body)
{
    if (!hasSolidSurface(body))
        return std::make_unique<GasGiantField>(body);
    return std::make_unique<PlanetSurfaceField>(body);
}
} // namespace world::celestial::surface
