#include "src/world/celestial/surface/ImportedSurfaceField.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
    constexpr double Pi = 3.14159265358979323846;
}

ImportedSurfaceField::ImportedSurfaceField(const BodySurfaceDescriptor& body,
                                           std::shared_ptr<const ImportedSurfaceMaps> maps)
    : m_body(body), m_maps(std::move(maps))
{
    m_radius = std::max(1000.0, body.radiusM);
    m_seed = surfaceSeed(body.seed);
    m_seedDetail = layerSeed(m_seed, "imported_detail");
    m_seedVariation = layerSeed(m_seed, "imported_variation");
    const int w = m_maps ? std::max(1, m_maps->width) : 1;
    // Source texel at the equator: everything coarser is the curated map.
    m_macroCutoffM = 2.0 * Pi * m_radius / static_cast<double>(w);
    m_detailAmplitudeM = std::clamp(body.reliefAmplitudeKm * 1000.0 * 0.12, 80.0, 1800.0);
    m_detailSpec = OctaveSpec {m_macroCutoffM, 1.92, 0.48, 18, 3.0};
    const SurfaceArchetype archetype = surfaceArchetype(body);
    m_cratered = archetype == SurfaceArchetype::AirlessRocky || archetype == SurfaceArchetype::Icy;
    if (m_cratered)
    {
        // impact gardening rounds the ground: low soft hills, craters carry the
        // relief - from half the map texel (the map has the bigger ones) to 1.5 m
        m_detailAmplitudeM = std::clamp(body.reliefAmplitudeKm * 1000.0 * 0.015, 20.0, 200.0);
        const double density = std::clamp(body.craterDensity, 0.0, 1.0) *
                               (1.0 - 0.6 * std::clamp(body.resurfacingRate, 0.0, 1.0));
        // craters from ~3 % of the radius (52 km on the Moon) down to 1.5 m: the
        // map keeps only the basins and maria (low-passed below), the visible
        // crater population is this layer at every distance
        m_maxCraterRadiusM = std::max(m_macroCutoffM * 0.5, 0.03 * m_radius);
        m_craters.configure(layerSeed(m_seed, "imported_craters"), m_radius, m_maxCraterRadiusM, 1.5,
                            std::max(density, 0.3));
        if (m_maps && m_maps->hasAlbedo())
        {
            double sum = 0.0;
            const std::size_t texels = m_maps->texelCount();
            for (std::size_t i = 0; i < texels; ++i)
            {
                const glm::vec3 c = m_maps->albedoAt(i);
                sum += 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
            }
            m_meanAlbedoLum = sum / static_cast<double>(texels);
            m_craters.setDensityModulation([this](const glm::dvec3& d) { return craterDensityAt(d); });
        }
        if (m_maps && !m_maps->heightM.empty())
        {
            // the basins only: a 4x box down-sample, then a separable gaussian of
            // sigma 5 coarse texels (~110 km on the Moon; longitude wraps)
            const int fw = m_maps->width, fh = m_maps->height;
            const int w = std::max(1, fw / 4), h = std::max(1, fh / 4);
            std::vector<float> coarse(static_cast<std::size_t>(w) * h, 0.0f);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    double acc = 0.0;
                    int n = 0;
                    for (int j = 0; j < 4; ++j)
                        for (int i = 0; i < 4; ++i)
                        {
                            const int sx = x * 4 + i, sy = y * 4 + j;
                            if (sx < fw && sy < fh)
                            {
                                acc += m_maps->heightM[static_cast<std::size_t>(sy) * fw + sx];
                                ++n;
                            }
                        }
                    coarse[static_cast<std::size_t>(y) * w + x] = static_cast<float>(acc / std::max(1, n));
                }
            constexpr int K = 15;
            constexpr double Sigma = 5.0;
            double kernel[2 * K + 1];
            double ks = 0.0;
            for (int i = -K; i <= K; ++i)
                ks += kernel[i + K] = std::exp(-0.5 * i * i / (Sigma * Sigma));
            for (double& k : kernel)
                k /= ks;
            std::vector<float> tmp(coarse.size());
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    double acc = 0.0;
                    for (int i = -K; i <= K; ++i)
                        acc += kernel[i + K] * coarse[static_cast<std::size_t>(y) * w + ((x + i) % w + w) % w];
                    tmp[static_cast<std::size_t>(y) * w + x] = static_cast<float>(acc);
                }
            m_smoothHeightM.resize(tmp.size());
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    double acc = 0.0;
                    for (int i = -K; i <= K; ++i)
                        acc += kernel[i + K] * tmp[static_cast<std::size_t>(std::clamp(y + i, 0, h - 1)) * w + x];
                    m_smoothHeightM[static_cast<std::size_t>(y) * w + x] = static_cast<float>(acc);
                }
            m_smoothW = w;
            m_smoothH = h;
        }
    }
    // craters: a bowl is at most ~0.2 x its diameter deep (complex ones far less),
    // plus the rims and ejecta of the overlapping classes
    m_detailBoundM = m_detailAmplitudeM * 3.0 +
                     (m_cratered ? std::min(m_maxCraterRadiusM * 0.45, 1040.0 * std::pow(m_maxCraterRadiusM * 2.0e-3, 0.3) * 2.5)
                                 : 0.0);
}

double ImportedSurfaceField::craterDensityAt(const glm::dvec3& dir) const
{
    // the dark maria are young lava plains: a fraction of the highland craters
    const Bilinear b = locate(dir);
    const glm::vec3 c = m_maps->albedoAt(static_cast<std::size_t>(b.i00));
    const double lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
    const double t = std::clamp((lum / std::max(m_meanAlbedoLum, 1.0e-4) - 0.7) / 0.4, 0.0, 1.0);
    return 0.2 + 0.8 * t * t * (3.0 - 2.0 * t);
}

ImportedSurfaceField::Bilinear ImportedSurfaceField::locate(const glm::dvec3& dir, int w, int h)
{
    const double lon = std::atan2(dir.z, dir.x);
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    const double x = (0.5 + lon / (2.0 * Pi)) * w - 0.5;
    const double y = std::clamp((0.5 - lat / Pi) * h - 0.5, 0.0, static_cast<double>(h - 1));
    const double fx0 = std::floor(x);
    const double fy0 = std::floor(y);
    Bilinear b;
    b.fx = x - fx0;
    b.fy = y - fy0;
    const int x0 = ((static_cast<int>(fx0) % w) + w) % w;
    const int x1 = (x0 + 1) % w;
    const int y0 = static_cast<int>(fy0);
    const int y1 = std::min(h - 1, y0 + 1);
    b.i00 = y0 * w + x0;
    b.i10 = y0 * w + x1;
    b.i01 = y1 * w + x0;
    b.i11 = y1 * w + x1;
    return b;
}

SurfaceSample ImportedSurfaceField::sample(const glm::dvec3& dir, double minWavelengthM) const
{
    SurfaceSample s;
    s.bandLimitM = minWavelengthM;
    s.biome = Biome::Barren;
    s.temperatureK = static_cast<float>(m_body.meanTemperatureK);
    if (!m_maps || m_maps->width <= 0)
        return s;
    const Bilinear b = locate(dir);
    const auto lerp4 = [&](float a, float c, float d, float e) {
        return (a * (1.0 - b.fx) + c * b.fx) * (1.0 - b.fy) + (d * (1.0 - b.fx) + e * b.fx) * b.fy;
    };
    double macro = 0.0;
    if (!m_smoothHeightM.empty())
    {
        const Bilinear c = locate(dir, m_smoothW, m_smoothH);
        const auto& g = m_smoothHeightM;
        macro = (g[c.i00] * (1.0 - c.fx) + g[c.i10] * c.fx) * (1.0 - c.fy) + (g[c.i01] * (1.0 - c.fx) + g[c.i11] * c.fx) * c.fy;
    }
    else if (!m_maps->heightM.empty())
        macro = lerp4(m_maps->heightM[b.i00], m_maps->heightM[b.i10], m_maps->heightM[b.i01], m_maps->heightM[b.i11]);
    s.macroHeightM = macro;
    double coast = 1.0e4;
    if (!m_maps->water.empty())
    {
        // Signed coast from the water mask: interpolated, zero set = coast.
        const auto land = [&](int i) { return m_maps->water[static_cast<std::size_t>(i)] ? -1.0f : 1.0f; };
        const double c = lerp4(land(b.i00), land(b.i10), land(b.i01), land(b.i11));
        coast = c * 400.0 + std::max(0.0, macro) * (c > 0.0 ? 1.0 : 0.0);
        s.water = c < 0.0;
    }
    s.coastM = coast;
    s.waterLevelM = 0.0;
    double detail = 0.0;
    if (m_cratered && minWavelengthM >= m_macroCutoffM)
    {
        // above the map texel: the big craters only (the hills are sub-texel)
        const CraterSample c = m_craters.sample(dir, minWavelengthM);
        detail = c.heightM;
        s.freshness = c.freshness;
        // the landmark: a macro-sized crater, the same at every LOD (not the resolved bowl)
        std::uint64_t craterId = 0;
        double craterRadius = 0.0;
        if (m_craters.landmark(dir, m_macroCutoffM, craterId, craterRadius))
        {
            s.featureClass = FeatureClass::ImpactCrater;
            s.featureId = craterId;
        }
    }
    else if (minWavelengthM < m_macroCutoffM)
    {
        // Land gets terrain below the source resolution, the sea floor none.
        // Rolling hills plus ridges, rougher on high ground (the curated
        // map's own elevation says where the mountains are).
        const double landWeight = m_maps->water.empty() ? 1.0 : smoothstep(0.0, 400.0, coast);
        const double hills = fbm(m_seedDetail, dir, m_radius, m_detailSpec, 1.0e30, minWavelengthM).total;
        if (m_cratered)
        {
            const CraterSample c = m_craters.sample(dir, minWavelengthM);
            detail = 0.6 * hills * m_detailAmplitudeM + c.heightM;
            s.freshness = c.freshness;
            // the landmark: a macro-sized crater, the same at every LOD (not the resolved bowl)
            std::uint64_t craterId = 0;
            double craterRadius = 0.0;
            if (m_craters.landmark(dir, m_macroCutoffM, craterId, craterRadius))
            {
                s.featureClass = FeatureClass::ImpactCrater;
                s.featureId = craterId;
            }
        }
        else
        {
            const double rugged = 0.35 + 0.65 * smoothstep(200.0, 2500.0, std::abs(macro));
            const double ridges = ridgedMultifractal(m_seedDetail + 1, dir, m_radius, m_detailSpec, 1.0e30,
                                                     minWavelengthM).total;
            detail = (0.6 * hills + 0.9 * (ridges - 0.45) * rugged) * m_detailAmplitudeM * rugged * landWeight;
        }
    }
    s.heightM = s.water ? std::min(macro, 0.0) : std::max(macro + detail, 0.5);
    if (m_maps->water.empty())
        s.heightM = macro + detail;
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    s.temperatureK = static_cast<float>(m_body.meanTemperatureK + 25.0 - 55.0 * std::abs(std::sin(lat)));
    s.biome = s.water ? Biome::Ocean : Biome::Barren;
    return s;
}

SurfaceAppearance ImportedSurfaceField::appearance(const SurfaceSample& s, const glm::dvec3& dir, double slope) const
{
    SurfaceAppearance a;
    a.weights.fill(0.0f);
    if (!m_maps || !m_maps->hasAlbedo())
        return a;
    const Bilinear b = locate(dir);
    const auto al = [&](int i) { return m_maps->albedoAt(static_cast<std::size_t>(i)); };
    glm::vec3 c = (al(b.i00) * static_cast<float>(1.0 - b.fx) + al(b.i10) * static_cast<float>(b.fx)) *
                      static_cast<float>(1.0 - b.fy) +
                  (al(b.i01) * static_cast<float>(1.0 - b.fx) + al(b.i11) * static_cast<float>(b.fx)) *
                      static_cast<float>(b.fy);
    if (!s.water && s.bandLimitM < m_macroCutoffM)
    {
        // Sub-texel variation and rock on steep ground: the map colour stays
        // the mean, the detail only modulates it.
        const double v = fbm(m_seedVariation, dir, m_radius, OctaveSpec {m_macroCutoffM * 0.5, 1.92, 0.5, 8, 0.0},
                             1.0e30, std::max(s.bandLimitM, 1.0))
                             .total;
        c *= static_cast<float>(1.0 + (m_cratered ? 0.12 : 0.30) * v);
        const float rock = static_cast<float>(smoothstep(0.08, 0.3, slope));
        c = glm::mix(c, glm::vec3(0.32f, 0.30f, 0.28f), rock * 0.5f);
        // space weathering: mature regolith darker, fresh ejecta / rays lighter
        // toward white (immature); the map's mean colour stays the reference
        if (m_cratered)
        {
            c *= 0.85f;
            c += (glm::vec3(1.0f) - c) * (0.35f * s.freshness);
        }
    }
    a.albedo = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));
    if (s.water)
        a.weights[static_cast<std::size_t>(SurfaceMaterial::Ice)] = 1.0f;
    else if (m_cratered)
    {
        // regolith everywhere; bare rock on steep walls and fresh rims
        const float rock = std::clamp(static_cast<float>(smoothstep(0.35, 0.7, slope)) + 0.6f * s.freshness, 0.0f, 1.0f);
        a.weights[static_cast<std::size_t>(SurfaceMaterial::Regolith)] = 1.0f - rock;
        a.weights[static_cast<std::size_t>(SurfaceMaterial::Rock)] = rock;
    }
    else
        a.weights[static_cast<std::size_t>(SurfaceMaterial::Rock)] = 1.0f;
    return a;
}

void importedHeightsFromGrey(const std::uint8_t* grey, int width, int height, double rangeM, bool signedRange,
                             ImportedSurfaceMaps& out)
{
    out.width = width;
    out.height = height;
    out.heightM.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t i = 0; i < out.heightM.size(); ++i)
    {
        const double g = grey[i] / 255.0;
        double hM = signedRange ? (g * 2.0 - 1.0) * rangeM : g * rangeM;
        if (!out.water.empty() && out.water[i])
            hM = -3500.0;
        out.heightM[i] = static_cast<float>(hM);
    }
}
} // namespace world::celestial::surface
