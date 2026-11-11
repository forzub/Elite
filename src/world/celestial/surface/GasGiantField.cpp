#include "src/world/celestial/surface/GasGiantField.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
// fbm (SurfaceNoise) has an RMS of ~0.125 for these specs (measured, 20 000 samples); the ridged sum
// ~0.68 mean / 0.15 RMS. Amplitudes below are written per unit RMS: (noise / Rms) * amplitude.
constexpr double FbmRms = 0.125;
constexpr double RidgedMean = 0.68;

double fraction(const BodySurfaceDescriptor& body, const char* gas)
{
    for (const auto& [name, value] : body.atmosphereComposition)
        if (name == gas)
            return value;
    return 0.0;
}

double wrapPi(double a)
{
    a = std::fmod(a + Pi, 2.0 * Pi);
    if (a < 0.0)
        a += 2.0 * Pi;
    return a - Pi;
}

glm::dvec3 fromLatLon(double lat, double lon)
{
    return {std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon)};
}

glm::vec3 scaleHue(const glm::vec3& c, double warm)
{
    // warm > 0: redder / browner, < 0: yellower / greyer
    return glm::clamp(c * glm::vec3(static_cast<float>(1.0 + 0.10 * warm), 1.0f, static_cast<float>(1.0 - 0.14 * warm)),
                      glm::vec3(0.0f), glm::vec3(1.0f));
}
} // namespace

GasGiantField::GasGiantField(const BodySurfaceDescriptor& body, double epochSeconds)
    : m_body(body), m_epoch(epochSeconds)
{
    m_iceGiant = surfaceArchetype(body) == SurfaceArchetype::IceGiant;
    m_radius = std::max(1.0e6, body.radiusM);
    m_surfaceSeed = surfaceSeed(body.seed);
    m_seedBands = layerSeed(m_surfaceSeed, "gg2_bands");
    m_seedShear = layerSeed(m_surfaceSeed, "gg2_shear");
    m_seedTexture = layerSeed(m_surfaceSeed, "gg2_texture");
    m_seedVortices = layerSeed(m_surfaceSeed, "gg2_vortices");
    m_seedPolar = layerSeed(m_surfaceSeed, "gg2_polar");

    // More jets on a large fast rotator (Rhines scale ~ sqrt(U / beta)).
    const double size = std::clamp(m_radius / 7.0e7, 0.3, 2.0);
    const double spin = std::clamp(10.0 / std::max(1.0, body.rotationPeriodHours), 0.2, 2.5);
    m_jets = m_iceGiant ? 2.0 + 1.0 * spin : 3.5 + 2.5 * std::sqrt(size * spin);
    m_jets += unitFromHash(m_seedBands) * 0.8;

    // Palette from the composition and temperature (cloud chemistry).
    const double methane = fraction(body, "CH4");
    m_hot = body.meanTemperatureK > 900.0;
    const bool methaneDeck = m_iceGiant || methane > 0.015;
    if (m_hot)
    {
        m_zone = {0.30f, 0.15f, 0.09f}; // alkali / silicate haze (linear)
        m_belt = {0.10f, 0.045f, 0.03f};
        m_polar = {0.07f, 0.04f, 0.04f};
        m_contrast = 0.6;
    }
    else if (methaneDeck)
    {
        m_zone = {0.40f, 0.66f, 0.76f}; // methane absorbs red (linear; Uranus-like cyan)
        m_belt = {0.17f, 0.36f, 0.66f}; // Neptune-like blue
        m_polar = {0.26f, 0.46f, 0.66f};
        m_contrast = 0.35;
    }
    else
    {
        m_zone = {0.84f, 0.76f, 0.58f}; // ammonia ice zones (linear of sRGB ~ 0.93 0.89 0.80)
        m_belt = {0.42f, 0.22f, 0.10f}; // tholin / sulfur belts (sRGB ~ 0.68 0.50 0.35)
        m_polar = {0.30f, 0.28f, 0.27f};
        // a colder deck lies deeper under thicker ammonia haze: paler, lower contrast, yellower
        // (Jupiter ~165 K full contrast, Saturn ~135 K about half)
        m_contrast = std::clamp((body.meanTemperatureK - 95.0) / 70.0, 0.35, 1.0);
        const float cold = static_cast<float>(1.0 - m_contrast);
        m_zone = glm::mix(m_zone, glm::vec3(0.86f, 0.74f, 0.48f), cold);
        m_belt = glm::mix(m_belt, glm::vec3(0.62f, 0.46f, 0.26f), cold);
        // per-body variety: zones white .. yellow, belts orange .. brown .. grey
        const double zv = unitFromHash(m_seedBands, 6), bv = unitFromHash(m_seedBands, 7);
        m_zone = glm::mix(m_zone, glm::vec3(0.82f, 0.70f, 0.42f), static_cast<float>(0.5 * zv));
        if (bv < 0.33)
            m_belt = glm::mix(m_belt, glm::vec3(0.30f, 0.20f, 0.14f), 0.5f);       // browner
        else if (bv > 0.80)
            m_belt = glm::mix(m_belt, glm::vec3(0.36f, 0.32f, 0.30f), 0.45f);      // greyer
    }
    const double tint = unitFromHash(m_seedBands, 3) - 0.5;
    m_belt = scaleHue(m_belt, tint);

    // ---- band table, per hemisphere (unequal widths) ------------------------
    m_polarLat = (m_iceGiant ? 1.20 : 1.05) + 0.12 * unitFromHash(m_seedBands, 4);
    // belts + zones per hemisphere (Jupiter ~5-6 to +-60 deg; ice giants 2-3)
    const int perHemisphere = std::clamp(static_cast<int>(std::lround(m_jets * 0.8)), m_iceGiant ? 2 : 3, 7);
    const double eqHalf = m_polarLat / (perHemisphere + 0.6) * (0.7 + 0.5 * unitFromHash(m_seedBands, 5));
    const auto makeBand = [&](double lo, double hi, bool zone, std::uint64_t h) {
        Band b;
        b.latLo = lo;
        b.latHi = hi;
        b.zone = zone;
        const double bright = unitFromHash(h, 0), hue = unitFromHash(h, 1) - 0.5, rust = unitFromHash(h, 2);
        if (zone)
        {
            b.colour = m_zone * static_cast<float>(0.90 + 0.14 * bright);
            b.colour = scaleHue(b.colour, hue * 0.8);
            b.turbulence = 0.2 + 0.3 * unitFromHash(h, 3);
        }
        else
        {
            b.colour = m_belt * static_cast<float>(0.65 + 0.55 * bright);
            if (!methaneDeck && !m_hot && rust < 0.35)
                b.colour = glm::mix(b.colour, glm::vec3(0.45f, 0.14f, 0.05f), 0.55f); // a "rusty" belt
            b.colour = scaleHue(b.colour, hue);
            b.turbulence = 0.6 + 0.4 * unitFromHash(h, 3);
        }
        // contrast of the class: low-contrast giants pull every band to the mean
        b.colour = glm::mix(glm::mix(m_zone, m_belt, 0.5f), b.colour, static_cast<float>(m_contrast));
        return b;
    };
    std::vector<Band> north, south;
    for (int hemi = 0; hemi < 2; ++hemi)
    {
        const std::uint64_t hs = layerSeed(m_seedBands, hemi == 0 ? "north" : "south");
        std::vector<double> weights;
        for (int i = 0; i < perHemisphere; ++i)
            weights.push_back(0.45 + 1.1 * unitFromHash(hs, i));
        double sum = 0.0;
        for (double w : weights)
            sum += w;
        double lat = eqHalf;
        auto& list = hemi == 0 ? north : south;
        for (std::size_t i = 0; i < weights.size(); ++i)
        {
            const double width = (m_polarLat - eqHalf) * weights[i] / sum;
            const bool zone = (i % 2) == 1;   // belt next to the equatorial zone
            list.push_back(makeBand(lat, lat + width, zone, latticeHash(hs, static_cast<std::int64_t>(i), hemi, 7)));
            lat += width;
        }
    }
    for (auto it = south.rbegin(); it != south.rend(); ++it)
    {
        Band b = *it;
        b.latLo = -it->latHi;
        b.latHi = -it->latLo;
        m_bands.push_back(b);
    }
    Band equatorial = makeBand(-eqHalf, eqHalf, true, latticeHash(m_seedBands, 0, 0, 0));
    equatorial.turbulence = 0.55;   // the equatorial plumes
    m_bands.push_back(equatorial);
    for (const Band& b : north)
        m_bands.push_back(b);

    // ---- vortices on band edges ---------------------------------------------
    for (std::size_t i = 0; i + 1 < m_bands.size(); ++i)
    {
        const Band& a = m_bands[i];
        const Band& b = m_bands[i + 1];
        const double edge = a.latHi;
        const std::uint64_t he = latticeHash(m_seedVortices, static_cast<std::int64_t>(i), 1, 0);
        const int count = static_cast<int>(unitFromHash(he, 0) * (m_iceGiant ? 1.3 : 3.6));
        const Band& zoneSide = a.zone ? a : b;
        const double side = a.zone ? -1.0 : 1.0;   // the oval sits on the zone side of the edge
        for (int k = 0; k < count; ++k)
        {
            const std::uint64_t hv = latticeHash(m_seedVortices, static_cast<std::int64_t>(i), k, 2);
            Vortex v;
            v.id = hv;
            const double zoneWidth = zoneSide.latHi - zoneSide.latLo;
            v.radiusLat = zoneWidth * (0.18 + 0.22 * unitFromHash(hv, 0));
            v.lat = edge + side * v.radiusLat * (0.6 + 0.5 * unitFromHash(hv, 1));
            v.lon = (unitFromHash(hv, 2) * 2.0 - 1.0) * Pi;
            v.aspect = 1.4 + 1.0 * unitFromHash(hv, 3);
            v.spin = (v.lat < 0.0 ? -1.0 : 1.0);    // anticyclones (zone side), sense by hemisphere
            const bool white = unitFromHash(hv, 4) < 0.75;
            v.colour = white ? glm::min(m_zone * 1.06f, glm::vec3(1.0f)) : b.colour * 0.85f;
            if (m_iceGiant)
            {
                v.dark = unitFromHash(hv, 5) < 0.6;
                v.colour = v.dark ? m_belt * 0.55f : glm::min(m_zone * 1.12f, glm::vec3(1.0f));
            }
            m_vortices.push_back(v);
        }
    }
    // the great oval: a large ammonia-cloud giant, southern tropical edge (~ -22 deg)
    if (!m_iceGiant && !m_hot && !methaneDeck && m_radius > 4.0e7 && unitFromHash(m_seedVortices, 9) < 0.65)
    {
        Vortex v;
        v.id = latticeHash(m_seedVortices, 99, 99, 99);
        v.great = true;
        v.radiusLat = 0.055 + 0.02 * unitFromHash(m_seedVortices, 10);
        v.aspect = 1.7 + 0.3 * unitFromHash(m_seedVortices, 11);
        double best = 1.0e9;
        for (std::size_t i = 0; i + 1 < m_bands.size(); ++i)
            if (std::abs(m_bands[i].latHi + 0.38) < std::abs(best + 0.38))
                best = m_bands[i].latHi;
        v.lat = best - v.radiusLat * 0.3;
        v.lon = (unitFromHash(m_seedVortices, 12) * 2.0 - 1.0) * Pi;
        v.spin = -1.0;
        v.colour = {0.62f, 0.20f, 0.08f};
        m_vortices.push_back(v);
    }
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

const GasGiantField::Band& GasGiantField::bandAt(double lat, double& t, const Band*& neighbour, double& edge) const
{
    std::size_t i = 0;
    while (i + 1 < m_bands.size() && lat > m_bands[i].latHi)
        ++i;
    const Band& b = m_bands[i];
    const double width = std::max(1.0e-6, b.latHi - b.latLo);
    t = std::clamp((lat - b.latLo) / width, 0.0, 1.0);
    neighbour = nullptr;
    if (t < 0.5 && i > 0)
        neighbour = &m_bands[i - 1];
    else if (t >= 0.5 && i + 1 < m_bands.size())
        neighbour = &m_bands[i + 1];
    edge = 1.0 - std::min(t, 1.0 - t) * 2.0;   // 1 at an edge, 0 in the middle
    return b;
}

void GasGiantField::shearWarp(double lat, double lon, const glm::dvec3& dir, double& lat2, double& lon2) const
{
    // noise coordinates stretched across latitude: structures elongated along the jets
    const glm::dvec3 stretched(dir.x, dir.y * 4.0, dir.z);
    double t0 = 0.0, edge0 = 0.0;
    const Band* n0 = nullptr;
    const Band& b0 = bandAt(lat, t0, n0, edge0);
    const double shearAmp = (0.3 + 0.7 * edge0 * edge0) * (0.4 + 0.6 * b0.turbulence);

    // shear flow: large warp + festoons at the edges
    const double wLat = fbm(m_seedShear, stretched, m_radius, OctaveSpec {m_radius * 0.30, 1.92, 0.55, 6, 2.0}, 0.0, 0.0).macro;
    const double wLon = fbm(m_seedShear + 1, stretched, m_radius, OctaveSpec {m_radius * 0.40, 1.92, 0.5, 5, 2.0}, 0.0, 0.0).macro;
    const double fest = ridgedMultifractal(m_seedShear + 2, stretched, m_radius,
                                           OctaveSpec {m_radius * 0.10, 1.92, 0.5, 4, 1.0}, 0.0, 0.0).macro;
    // RMS ~3 deg of latitude at an edge, ~6 deg of longitude; festoons ~2 deg at the edges
    lat2 = lat + (wLat / FbmRms) * 0.050 * shearAmp + (fest - RidgedMean) / 0.15 * 0.035 * edge0 * edge0 * b0.turbulence;
    lon2 = lon + (wLon / FbmRms) * 0.10 * shearAmp;
}

double GasGiantField::bandSignal(double lat) const
{
    // band index + position inside it: integers on the band edges (the Elite-style lines)
    std::size_t i = 0;
    while (i + 1 < m_bands.size() && lat > m_bands[i].latHi)
        ++i;
    const Band& b = m_bands[i];
    return static_cast<double>(i) + std::clamp((lat - b.latLo) / std::max(1.0e-6, b.latHi - b.latLo), 0.0, 1.0);
}

glm::vec3 GasGiantField::deckColour(double lat, double lon, const glm::dvec3& dir, double bandLimitM,
                                    double& turbulenceOut) const
{
    double lat2 = lat, lon2 = lon;
    shearWarp(lat, lon, dir, lat2, lon2);

    double t = 0.0, edge = 0.0;
    const Band* nb = nullptr;
    const Band& b = bandAt(lat2, t, nb, edge);
    glm::vec3 c = b.colour;
    if (nb)
    {
        // a sharp but not hard edge: blend over the outer ~8 % of the band
        const double d = std::min(t, 1.0 - t);
        const float k = static_cast<float>(0.5 * (1.0 - smoothstep(0.0, 0.08, d)));
        c = glm::mix(c, nb->colour, k);
    }
    turbulenceOut = b.turbulence * (0.5 + 0.5 * edge);

    // texture: folded filaments along the jets, clumps in the belts
    const glm::dvec3 q = fromLatLon(lat2, lon2);
    // folded texture: the filament noise is itself domain-warped (swirls, curls in the belts)
    const glm::dvec3 qs(q.x, q.y * 3.0, q.z);
    const double fx = fbm(m_seedTexture + 11, qs, m_radius, OctaveSpec {m_radius * 0.08, 1.92, 0.5, 4, 2.0}, 0.0, 0.0).macro;
    const double fz = fbm(m_seedTexture + 12, qs, m_radius, OctaveSpec {m_radius * 0.08, 1.92, 0.5, 4, 2.0}, 0.0, 0.0).macro;
    const glm::dvec3 folded = qs + glm::dvec3(fx, 0.0, fz) / FbmRms * (0.035 * (0.3 + b.turbulence));
    const glm::dvec3 streak(folded.x, folded.y * 3.0, folded.z);
    const double fil = fbm(m_seedTexture, streak, m_radius, OctaveSpec {m_radius * 0.05, 1.92, 0.58, 6, 1.5}, 0.0, 0.0).macro;
    const double clump = fbm(m_seedTexture + 1, folded, m_radius, OctaveSpec {m_radius * 0.06, 1.92, 0.55, 5, 2.5}, 0.0, 0.0).macro;
    // thin zonal stripes inside every band (latitude-only noise), then the folded texture
    const double stripes = fbm(m_seedTexture + 21, glm::dvec3(0.0, std::sin(lat2), 0.0), m_radius,
                               OctaveSpec {m_radius * 0.02, 1.92, 0.6, 4, 1.0}, 0.0, 0.0).macro;
    // brightness modulation, per unit RMS: stripes +-8 %, filaments +-12 %, belt clumps +-15 %
    double mod = 0.08 * stripes / FbmRms + 0.12 * fil / FbmRms * (0.35 + 0.65 * b.turbulence) +
                 (b.zone ? 0.05 : 0.15) * clump / FbmRms * b.turbulence;
    // finer filaments with a closer look (band-limited: nothing at map scale)
    if (bandLimitM > 0.0 && bandLimitM < m_radius * 0.02)
    {
        const glm::dvec3 fine(q.x, q.y * 12.0, q.z);
        mod += 0.07 / FbmRms * b.turbulence *
               fbm(m_seedTexture + 7, fine, m_radius, OctaveSpec {m_radius * 0.02, 1.92, 0.62, 14, 1.0}, 1.0e30, bandLimitM)
                   .total;
    }
    c *= static_cast<float>(1.0 + mod * m_contrast);

    // ice giants: rare bright methane streaks
    if (m_iceGiant)
    {
        // few, long, thin: a large-scale gate times a streak shape stretched along the jets
        const double gate = fbm(m_seedTexture + 4, q, m_radius, OctaveSpec {m_radius * 0.6, 1.92, 0.5, 3, 2.0}, 0.0, 0.0).macro;
        const double s = fbm(m_seedTexture + 3, glm::dvec3(q.x, q.y * 14.0, q.z), m_radius,
                             OctaveSpec {m_radius * 0.12, 1.92, 0.5, 3, 1.0}, 0.0, 0.0).macro;
        const double streakMask = smoothstep(1.2, 2.0, gate / FbmRms) * smoothstep(1.0, 1.8, s / FbmRms) *
                                  smoothstep(0.15, 0.45, std::abs(lat2)) * (1.0 - smoothstep(0.9, 1.2, std::abs(lat2)));
        c = glm::mix(c, glm::vec3(0.95f, 0.98f, 1.0f), static_cast<float>(streakMask * 0.8));
    }
    return c;
}

SurfaceSample GasGiantField::sample(const glm::dvec3& dirIn, double minWavelengthM) const
{
    SurfaceSample s;
    s.bandLimitM = minWavelengthM;
    s.biome = Biome::GasDeck;
    s.temperatureK = static_cast<float>(m_body.meanTemperatureK);
    const glm::dvec3 dir = drifted(dirIn);
    const double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    const double lon = std::atan2(dir.z, dir.x);
    // The Elite-style lines of a gas giant (the coast channel is unused on a deck: kept > 0, never
    // water): the warped band edges and the oval rims - not the fine texture of the colour.
    double lat2 = lat, lon2 = lon;
    shearWarp(lat, lon, dir, lat2, lon2);
    double signal = bandSignal(std::clamp(lat2, -Pi * 0.5, Pi * 0.5));
    // polar regions carry no band lines
    if (std::abs(lat2) > m_polarLat + 0.05)
        signal = std::floor(signal) + 0.5;
    for (const Vortex& v : m_vortices)
    {
        const double dy = (lat - v.lat) / v.radiusLat;
        if (std::abs(dy) > 1.0)
            continue;
        const double dx = wrapPi(lon - v.lon) * std::cos(v.lat) / (v.radiusLat * v.aspect);
        const double r = std::sqrt(dx * dx + dy * dy);
        if (r < 1.0)
        {
            s.featureClass = FeatureClass::Storm;
            s.featureId = v.id;
            signal = std::floor(signal) + 0.5 + 0.5 * r;   // a line on the rim (r = 1 -> an integer)
        }
    }
    s.coastM = (10.0 + signal) * 1000.0;
    return s;
}

SurfaceAppearance GasGiantField::appearance(const SurfaceSample& sample, const glm::dvec3& dirIn, double) const
{
    SurfaceAppearance a;
    a.weights.fill(0.0f);
    const glm::dvec3 dir = drifted(dirIn);
    double lat = std::asin(std::clamp(dir.y, -1.0, 1.0));
    double lon = std::atan2(dir.z, dir.x);

    // vortices: swirl the sampling point (spiral arms), then the oval itself on top
    float ovalMix = 0.0f;
    glm::vec3 ovalColour(1.0f);
    float rimDark = 0.0f;
    double wakeBoost = 0.0;
    for (const Vortex& v : m_vortices)
    {
        const double dy = (lat - v.lat) / v.radiusLat;
        if (std::abs(dy) > 2.4)
            continue;
        const double sx = v.radiusLat * v.aspect / std::max(0.05, std::cos(v.lat));
        const double dx = wrapPi(lon - v.lon) / sx;
        if (v.great && dx > 0.8 && dx < 7.0 && std::abs(dy) < 1.3)
            wakeBoost = std::max(wakeBoost, (1.0 - dx / 7.0) * (1.0 - std::abs(dy) / 1.3));
        const double r2 = dx * dx + dy * dy;
        if (r2 > 2.4 * 2.4)
            continue;
        const double r = std::sqrt(r2);
        const double angle = v.spin * (v.great ? 4.5 : 3.0) * std::exp(-r2 * 1.1);
        const double ca = std::cos(angle), sa = std::sin(angle);
        const double rx = dx * ca - dy * sa, ry = dx * sa + dy * ca;
        lat = v.lat + ry * v.radiusLat;
        lon = v.lon + rx * sx;
        if (r < 1.0)
        {
            const float inside = static_cast<float>(1.0 - smoothstep(0.72, 1.0, r));
            if (inside > ovalMix)
            {
                ovalMix = inside;
                // a soft core, slightly darker toward the rim (no rings: the swirled deck shows the arms)
                ovalColour = v.colour * static_cast<float>(1.0 - 0.18 * r * r);
            }
        }
        rimDark = std::max(rimDark, static_cast<float>(std::exp(-std::pow((r - 1.0) / 0.10, 2.0)) * (v.dark ? 0.0 : 0.18)));
    }
    lat = std::clamp(lat, -Pi * 0.5, Pi * 0.5);

    double turbulence = 0.0;
    glm::vec3 c = deckColour(lat, lon, fromLatLon(lat, lon), sample.bandLimitM, turbulence);
    if (wakeBoost > 0.0)
    {
        // the great oval's turbulent wake: churned, mixed brighter / darker
        const glm::dvec3 q = fromLatLon(lat, lon);
        const double churn = fbm(m_seedTexture + 5, q, m_radius, OctaveSpec {m_radius * 0.03, 1.92, 0.6, 5, 2.0}, 0.0, 0.0).macro;
        c *= static_cast<float>(1.0 + 0.25 * churn / FbmRms * wakeBoost);
    }
    c *= 1.0f - rimDark;
    c = glm::mix(c, ovalColour, ovalMix * 0.7f);

    // polar regions: mottled haze with small cyclones instead of bands
    const double polarNoise =
        fbm(m_seedPolar, dir, m_radius, OctaveSpec {m_radius * 0.20, 1.92, 0.5, 4, 2.0}, 0.0, 0.0).macro;
    const double pol = smoothstep(m_polarLat - 0.12, m_polarLat + 0.22, std::abs(lat) + 0.10 * polarNoise / FbmRms);
    if (pol > 0.0)
    {
        const double mottle =
            fbm(m_seedPolar + 1, dir, m_radius, OctaveSpec {m_radius * 0.05, 1.92, 0.55, 5, 3.0}, 0.0, 0.0).macro;
        const double cyclones = ridgedMultifractal(m_seedPolar + 2, dir, m_radius,
                                                   OctaveSpec {m_radius * 0.035, 1.92, 0.5, 3, 1.0}, 0.0, 0.0).macro;
        // the band colours fade into the mottle (not a cap): keep the deck under the haze
        glm::vec3 p = glm::mix(c * 0.8f, m_polar, 0.6f) *
                      static_cast<float>(0.85 + 0.15 * mottle / FbmRms + 0.20 * smoothstep(0.85, 0.97, cyclones));
        c = glm::mix(c, p, static_cast<float>(pol));
    }
    // general darkening towards the poles (thicker haze over the deck)
    const float haze = static_cast<float>(smoothstep(0.6, 1.5, std::abs(lat)));
    c = glm::mix(c, c * glm::vec3(0.72f, 0.76f, 0.88f), haze * 0.6f);
    a.albedo = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));
    return a;
}

void GasGiantField::landmarks(const glm::dvec3& dirIn, double radiusRad, std::vector<SurfaceFeature>& out) const
{
    const glm::dvec3 dir = drifted(dirIn);
    for (const Vortex& v : m_vortices)
    {
        const glm::dvec3 centre = fromLatLon(v.lat, v.lon);
        const double extent = v.radiusLat * v.aspect;
        if (std::acos(std::clamp(glm::dot(centre, dir), -1.0, 1.0)) < radiusRad + extent)
            out.push_back(SurfaceFeature {FeatureClass::Storm, v.id, centre, extent * m_radius});
    }
}
} // namespace world::celestial::surface
