#include "src/world/celestial/surface/CraterLayer.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
constexpr double MinRadiusOfCell = 0.12;   // crater radius / cell size range
constexpr double MaxRadiusOfCell = 0.38;
}

void CraterLayer::configure(std::uint64_t seed, double bodyRadiusM, double maxCraterRadiusM, double minCraterRadiusM,
                            double density, bool boulders)
{
    m_classes.clear();
    m_withBoulders = boulders;
    m_seed = seed;
    m_bouldersSeed = deriveSeed(seed, "boulders");
    m_radius = std::max(1.0, bodyRadiusM);
    if (density <= 0.0 || maxCraterRadiusM <= minCraterRadiusM)
        return;
    // ~85 % of the cells hold a crater at full density: overlapping bowls of
    // every size, the saturated look of the lunar highlands
    const double probability = std::clamp(0.85 * density, 0.0, 0.9);
    std::uint64_t index = 0;
    for (double cell = maxCraterRadiusM / MaxRadiusOfCell; cell * MinRadiusOfCell >= minCraterRadiusM; cell *= 0.5)
    {
        Class c;
        c.cellM = cell;
        c.probability = probability;
        c.seed = deriveSeed(seed, "crater_class", index++);
        m_classes.push_back(c);
    }
}

CraterSample CraterLayer::sample(const glm::dvec3& dir, double minWavelengthM) const
{
    CraterSample out;
    if (m_classes.empty())
        return out;
    const glm::dvec3 x = dir * m_radius;
    double largestInside = 0.0;
    for (const Class& cls : m_classes)
    {
        const double largest = cls.cellM * MaxRadiusOfCell;
        // resolved at all here? (a crater shows from ~2 samples across its radius)
        if (largest * 2.0 < minWavelengthM)
            break;   // the classes only get smaller
        const glm::dvec3 p = x / cls.cellM;
        const glm::dvec3 base = glm::floor(p);
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const auto cx = static_cast<std::int64_t>(base.x) + dx;
                    const auto cy = static_cast<std::int64_t>(base.y) + dy;
                    const auto cz = static_cast<std::int64_t>(base.z) + dz;
                    const std::uint64_t h = latticeHash(cls.seed, cx, cy, cz);
                    if (unitFromHash(h, 0) >= cls.probability)
                        continue;
                    const glm::dvec3 q = (glm::dvec3(static_cast<double>(cx), static_cast<double>(cy),
                                                     static_cast<double>(cz)) +
                                          glm::dvec3(unitFromHash(h, 1), unitFromHash(h, 2), unitFromHash(h, 3))) *
                                         cls.cellM;
                    // only candidates in the shell around the surface: uniform per area
                    if (std::abs(glm::length(q) - m_radius) > cls.cellM * 0.5)
                        continue;
                    const double radius =
                        cls.cellM * (MinRadiusOfCell + (MaxRadiusOfCell - MinRadiusOfCell) * std::pow(unitFromHash(h, 4), 2.0));
                    const glm::dvec3 centre = glm::normalize(q) * m_radius;
                    const double r = glm::length(x - centre) / radius;
                    if (r >= 3.0)
                        continue;
                    // age: most craters are old and soft, a few fresh and sharp
                    const double fresh = std::pow(unitFromHash(h, 5), 3.0);
                    const double depth = radius * (0.06 + 0.34 * fresh);   // d/D 0.03 (old) .. 0.2 (fresh)
                    const double rim = depth * (0.12 + 0.18 * fresh);
                    double hgt;
                    if (r < 1.0)
                    {
                        // bowl; an old one has a flatter, infilled floor
                        const double bowl = fresh > 0.3 ? 1.0 - r * r : 1.0 - std::pow(r, 4.0);
                        hgt = -depth * bowl + rim * std::pow(r, 4.0);
                    }
                    else
                        hgt = rim / (r * r * r);
                    hgt -= rim / 27.0;   // zero at 3 radii
                    // band limit: fade the class in as it becomes resolved
                    const double fade = std::clamp(radius * 2.0 / std::max(minWavelengthM, 1.0e-3) - 1.0, 0.0, 1.0);
                    out.heightM += hgt * fade;
                    if (r < 2.5)
                        out.freshness = std::max(out.freshness, static_cast<float>(fresh * (1.0 - r / 2.5) * fade));
                    // blocks on the fresh rim and upper wall
                    if (r > 0.7 && r < 1.7)
                        out.rockiness = std::max(out.rockiness,
                                                 static_cast<float>(fresh * (1.0 - std::abs(r - 1.05) / 0.65) * fade));
                    if (r < 1.0 && radius > largestInside && fade > 0.5)
                    {
                        largestInside = radius;
                        out.insideCrater = true;
                        out.craterRadiusM = radius;
                        out.craterId = h;
                    }
                }
    }
    if (m_withBoulders)
        out.heightM += boulders(dir, minWavelengthM, 0.03 + 0.6 * out.rockiness);
    return out;
}

double CraterLayer::boulders(const glm::dvec3& dir, double minWavelengthM, double rockiness) const
{
    // blocks of 0.5 - 6 m: only at ground level of detail, mostly around fresh rims
    constexpr double CellM = 8.0;
    if (minWavelengthM > CellM || rockiness <= 0.0)
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
                const std::uint64_t k = latticeHash(m_bouldersSeed, static_cast<std::int64_t>(cell.x),
                                                    static_cast<std::int64_t>(cell.y), static_cast<std::int64_t>(cell.z));
                if (unitFromHash(k, 0) > rockiness)
                    continue;
                const glm::dvec3 centre = cell + glm::dvec3(unitFromHash(k, 1), unitFromHash(k, 2), unitFromHash(k, 3));
                const double radius = 0.06 + 0.32 * std::pow(unitFromHash(k, 4), 2.0);
                const double d = glm::length(p - centre) / radius;
                if (d < 1.0)
                    h = std::max(h, radius * CellM * 0.8 * std::sqrt(1.0 - d * d));
            }
    return h * fade;
}
} // namespace world::celestial::surface
