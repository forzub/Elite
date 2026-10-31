#include "src/world/celestial/surface/SmallBodyField.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
    constexpr double TwoPi = 6.283185307179586;

    glm::dvec3 randomDirection(std::uint64_t h)
    {
        const double z = 2.0 * unitFromHash(h, 10) - 1.0;
        const double phi = TwoPi * unitFromHash(h, 11);
        const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
        return glm::dvec3(r * std::cos(phi), r * std::sin(phi), z);
    }

    // Polynomial smooth minimum (k: blend width).
    double smoothMin(double a, double b, double k)
    {
        const double h = std::clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
        return b + (a - b) * h - k * h * (1.0 - h);
    }

    double craterShape(double r, double depth)
    {
        // Bowl with a raised rim, in fractions of the body radius.
        if (r >= 1.6)
            return 0.0;
        if (r < 1.0)
            return -depth * (1.0 - r * r) + depth * 0.25 * r * r * r * r;
        return depth * 0.25 * (1.0 - smoothstep(1.0, 1.6, r));
    }
}

SmallBodyField::SmallBodyField(const SmallBodyDescriptor& body) : m_body(body)
{
    m_radius = std::max(5.0, body.meanRadiusM);
    m_seed = surfaceSeed(body.seed);
    m_seedShape = layerSeed(m_seed, "shape");
    m_seedPlanes = layerSeed(m_seed, "fracture_planes");
    m_seedCraters = layerSeed(m_seed, "craters");
    m_seedDetail = layerSeed(m_seed, "regolith_detail");
    m_seedGrooves = layerSeed(m_seed, "grooves");

    // Triaxial ellipsoid (a >= b >= c), seeded orientation.
    const double b = 0.62 + 0.33 * unitFromHash(m_seedShape, 0);
    const double c = 0.42 + (b - 0.42) * unitFromHash(m_seedShape, 1);
    m_axes = glm::dvec3(1.0, b, c) / std::cbrt(b * c);
    const glm::dvec3 ax = randomDirection(m_seedShape);
    glm::dvec3 ay = glm::normalize(glm::cross(ax, std::abs(ax.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0)));
    const double roll = TwoPi * unitFromHash(m_seedShape, 2);
    const glm::dvec3 az0 = glm::cross(ax, ay);
    ay = ay * std::cos(roll) + az0 * std::sin(roll);
    m_orientation = glm::dmat3(ax, ay, glm::cross(ax, ay));

    // Fracture facets: rubble pile vs monolith.
    const int planes = 3 + static_cast<int>(unitFromHash(m_seedPlanes) * 6.0);
    for (int i = 0; i < planes; ++i)
    {
        const std::uint64_t h = deriveSeed(m_seedPlanes, "plane", static_cast<std::uint64_t>(i));
        m_planes.push_back(glm::dvec4(randomDirection(h), 0.70 + 0.22 * unitFromHash(h, 0)));
    }

    // Large craters: an explicit list (the body's landmarks).
    const int craters = 5 + static_cast<int>(unitFromHash(m_seedCraters) * 10.0);
    for (int i = 0; i < craters; ++i)
    {
        const std::uint64_t h = deriveSeed(m_seedCraters, "large", static_cast<std::uint64_t>(i));
        Crater cr;
        cr.id = h;
        cr.center = randomDirection(h);
        cr.radiusRad = 0.14 + 0.38 * std::pow(unitFromHash(h, 0), 1.5);
        cr.depth = cr.radiusRad * (0.12 + 0.14 * unitFromHash(h, 1));
        m_craters.push_back(cr);
    }

    m_macroCutoffM = std::max(2.0, m_radius * 0.03);
    m_deformSpec = OctaveSpec {m_radius * 2.2, 1.92, 0.5, 3, 1.0};
    m_detailSpec = OctaveSpec {m_radius * 0.35, 1.92, std::pow(1.92, -0.8), 1, 4.0};
    m_detailSpec.octaves =
        std::max(1, static_cast<int>(std::ceil(std::log(m_detailSpec.baseWavelengthM / 0.25) / std::log(1.92))) + 1);
    m_grooveSpec = OctaveSpec {m_radius * 0.6, 1.92, 0.55, 6, 3.0};
    m_detailBoundM = m_radius * 0.035 + 2.0;
}

double SmallBodyField::macroShape(const glm::dvec3& dir) const
{
    const glm::dvec3 local = glm::transpose(m_orientation) * dir;
    const glm::dvec3 q = local / m_axes;
    double r = 1.0 / glm::length(q);
    r *= std::exp(0.16 * fbm(m_seedShape, dir, m_radius, m_deformSpec, 0.0, 0.0).macro);
    for (const glm::dvec4& p : m_planes)
    {
        const double d = glm::dot(dir, glm::dvec3(p));
        if (d > 0.05)
            r = smoothMin(r, p.w / d, 0.05);
    }
    for (const Crater& c : m_craters)
    {
        const double a = std::acos(std::clamp(glm::dot(dir, c.center), -1.0, 1.0)) / c.radiusRad;
        r += craterShape(a, c.depth);
    }
    return r;
}

void SmallBodyField::mediumCraters(const glm::dvec3& dir, double cell, std::uint64_t seed,
                                   std::vector<Crater>& out) const
{
    const glm::dvec3 base = glm::floor(dir / cell);
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const glm::dvec3 c = base + glm::dvec3(dx, dy, dz);
                const std::uint64_t h = latticeHash(seed, static_cast<std::int64_t>(c.x), static_cast<std::int64_t>(c.y),
                                                    static_cast<std::int64_t>(c.z));
                if (unitFromHash(h, 0) > 0.7)
                    continue;
                const glm::dvec3 q = (c + glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) * cell;
                if (std::abs(glm::length(q) - 1.0) > cell * 0.5)
                    continue;
                Crater cr;
                cr.id = h;
                cr.center = glm::normalize(q);
                cr.radiusRad = cell * (0.12 + 0.25 * unitFromHash(h, 4));
                cr.depth = cr.radiusRad * 0.2;
                out.push_back(cr);
            }
}

SurfaceSample SmallBodyField::sample(const glm::dvec3& dir, double minWavelengthM) const
{
    SurfaceSample s;
    s.biome = Biome::Barren;
    s.temperatureK = 180.0f;
    double macro = macroShape(dir);
    double total = macro;

    // Medium / small craters: classes from R/8 down to ~1 m.
    std::vector<Crater> list;
    std::uint64_t classIndex = 0;
    for (double cell = 0.5; cell * m_radius * 0.12 > 0.5; cell /= 2.6, ++classIndex)
    {
        const double radiusM = cell * 0.37 * m_radius;
        const bool isMacro = cell * 0.12 * m_radius >= m_macroCutoffM;
        if (!isMacro && radiusM * 3.0 < minWavelengthM)
            break;
        list.clear();
        mediumCraters(dir, cell, deriveSeed(m_seedCraters, "class", classIndex), list);
        for (const Crater& c : list)
        {
            const double a = std::acos(std::clamp(glm::dot(dir, c.center), -1.0, 1.0)) / c.radiusRad;
            const double h = craterShape(a, c.depth);
            const double fade = std::clamp(c.radiusRad * m_radius * 2.0 / std::max(minWavelengthM, 1.0e-3) - 1.0, 0.0,
                                           1.0);
            if (isMacro)
                macro += h;
            total += h * (isMacro ? 1.0 : fade);
            if (a < 1.0 && isMacro)
            {
                s.featureClass = FeatureClass::ImpactCrater;
                s.featureId = c.id;
            }
            if (a < 1.6)
                s.freshness = std::max(s.freshness, static_cast<float>(unitFromHash(c.id, 7) * (1.0 - a / 1.6)));
        }
    }
    for (const Crater& c : m_craters)
        if (std::acos(std::clamp(glm::dot(dir, c.center), -1.0, 1.0)) < c.radiusRad)
        {
            s.featureClass = FeatureClass::ImpactCrater;
            s.featureId = c.id;
        }

    // Grooves and regolith relief (detail band below the macro cutoff).
    const double cutoffFraction = m_macroCutoffM;
    const BandSum grooves = ridgedMultifractal(m_seedGrooves, dir, m_radius, m_grooveSpec, cutoffFraction, minWavelengthM);
    const BandSum detail =
        dampedFbm(m_seedDetail, dir, m_radius, m_detailSpec, 2.0, cutoffFraction, minWavelengthM);
    macro += 0.012 * grooves.macro + 0.02 * detail.macro;
    total += 0.012 * grooves.total + 0.02 * detail.total;

    s.macroHeightM = (macro - 1.0) * m_radius;
    s.heightM = (total - 1.0) * m_radius;
    return s;
}

SurfaceAppearance SmallBodyField::appearance(const SurfaceSample& s, const glm::dvec3& dir, double slope) const
{
    SurfaceAppearance a;
    a.weights.fill(0.0f);
    glm::vec3 base;
    std::size_t loose = static_cast<std::size_t>(SurfaceMaterial::Regolith);
    switch (m_body.composition)
    {
        case SmallBodyComposition::Carbonaceous: base = {0.07f, 0.065f, 0.06f}; break;
        case SmallBodyComposition::Metallic: base = {0.36f, 0.35f, 0.34f}; break;
        case SmallBodyComposition::Icy:
            base = {0.62f, 0.64f, 0.68f};
            loose = static_cast<std::size_t>(SurfaceMaterial::Ice);
            break;
        default: base = {0.30f, 0.26f, 0.21f}; break;
    }
    const float steep = static_cast<float>(smoothstep(0.1, 0.4, slope));
    a.weights[loose] = 1.0f - steep;
    a.weights[static_cast<std::size_t>(SurfaceMaterial::Rock)] = steep;
    const double variation =
        fbm(m_seedDetail + 5, dir, m_radius, OctaveSpec {m_radius * 0.4, 1.92, 0.5, 4, 2.0}, 0.0, 0.0).macro;
    glm::vec3 c = base * static_cast<float>(1.0 + 0.18 * variation);
    c *= 1.0f + 0.25f * steep + 0.4f * s.freshness; // exposed / fresh rock is brighter
    a.albedo = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));
    return a;
}

void SmallBodyField::landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const
{
    for (const Crater& c : m_craters)
        if (std::acos(std::clamp(glm::dot(dir, c.center), -1.0, 1.0)) < radiusRad + c.radiusRad)
            out.push_back(SurfaceFeature {FeatureClass::ImpactCrater, c.id, c.center, c.radiusRad * m_radius});
}
} // namespace world::celestial::surface
