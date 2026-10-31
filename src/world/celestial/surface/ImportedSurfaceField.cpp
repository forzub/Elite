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
    m_detailBoundM = m_detailAmplitudeM * 3.0;
}

ImportedSurfaceField::Bilinear ImportedSurfaceField::locate(const glm::dvec3& dir) const
{
    const int w = m_maps->width;
    const int h = m_maps->height;
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
    if (!m_maps->heightM.empty())
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
    if (minWavelengthM < m_macroCutoffM)
    {
        // Land gets terrain below the source resolution, the sea floor none.
        // Rolling hills plus ridges, rougher on high ground (the curated
        // map's own elevation says where the mountains are).
        const double landWeight = m_maps->water.empty() ? 1.0 : smoothstep(0.0, 400.0, coast);
        const double rugged = 0.35 + 0.65 * smoothstep(200.0, 2500.0, std::abs(macro));
        const double hills = fbm(m_seedDetail, dir, m_radius, m_detailSpec, 1.0e30, minWavelengthM).total;
        const double ridges = ridgedMultifractal(m_seedDetail + 1, dir, m_radius, m_detailSpec, 1.0e30,
                                                 minWavelengthM).total;
        detail = (0.6 * hills + 0.9 * (ridges - 0.45) * rugged) * m_detailAmplitudeM * rugged * landWeight;
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
    if (!m_maps || m_maps->albedo.empty())
        return a;
    const Bilinear b = locate(dir);
    const auto& al = m_maps->albedo;
    glm::vec3 c = (al[static_cast<std::size_t>(b.i00)] * static_cast<float>(1.0 - b.fx) +
                   al[static_cast<std::size_t>(b.i10)] * static_cast<float>(b.fx)) *
                      static_cast<float>(1.0 - b.fy) +
                  (al[static_cast<std::size_t>(b.i01)] * static_cast<float>(1.0 - b.fx) +
                   al[static_cast<std::size_t>(b.i11)] * static_cast<float>(b.fx)) *
                      static_cast<float>(b.fy);
    if (!s.water && s.bandLimitM < m_macroCutoffM)
    {
        // Sub-texel variation and rock on steep ground: the map colour stays
        // the mean, the detail only modulates it.
        const double v = fbm(m_seedVariation, dir, m_radius, OctaveSpec {m_macroCutoffM * 0.5, 1.92, 0.5, 8, 0.0},
                             1.0e30, std::max(s.bandLimitM, 1.0))
                             .total;
        c *= static_cast<float>(1.0 + 0.30 * v);
        const float rock = static_cast<float>(smoothstep(0.08, 0.3, slope));
        c = glm::mix(c, glm::vec3(0.32f, 0.30f, 0.28f), rock * 0.5f);
    }
    a.albedo = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));
    a.weights[static_cast<std::size_t>(s.water ? SurfaceMaterial::Ice : SurfaceMaterial::Rock)] = 1.0f;
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
