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

void CraterLayer::setDensityModulation(const std::function<double(const glm::dvec3&)>& f)
{
    auto grid = std::make_shared<std::vector<float>>(static_cast<std::size_t>(DensityGrid) * DensityGrid * DensityGrid, 1.0f);
    for (int z = 0; z < DensityGrid; ++z)
        for (int y = 0; y < DensityGrid; ++y)
            for (int x = 0; x < DensityGrid; ++x)
            {
                const glm::dvec3 c((x + 0.5) / DensityGrid * 2.0 - 1.0, (y + 0.5) / DensityGrid * 2.0 - 1.0,
                                   (z + 0.5) / DensityGrid * 2.0 - 1.0);
                if (glm::length(c) > 1.0e-6)
                    (*grid)[(static_cast<std::size_t>(z) * DensityGrid + y) * DensityGrid + x] =
                        static_cast<float>(std::clamp(f(glm::normalize(c)), 0.0, 1.0));
            }
    m_density = std::move(grid);
}

double CraterLayer::densityAt(const glm::dvec3& unitDir) const
{
    // the cell holding the direction's point on the unit sphere (cells ~2 / 48 of the
    // radius: ~70 km on the Moon - the maria are hundreds of km)
    const auto cell = [](double v) {
        return std::clamp(static_cast<int>((v * 0.5 + 0.5) * DensityGrid), 0, DensityGrid - 1);
    };
    return (*m_density)[(static_cast<std::size_t>(cell(unitDir.z)) * DensityGrid + cell(unitDir.y)) * DensityGrid +
                        cell(unitDir.x)];
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
                    if (r >= 5.0)
                        continue;
                    if (m_density && unitFromHash(h, 0) >= cls.probability * densityAt(centre / m_radius))
                        continue;
                    // age: small craters are young more often (they are erased fast and
                    // made all the time), big ones mostly old and soft
                    const double youth = 1.5 + 1.5 * std::clamp(std::log10(std::max(radius, 1.0) / 30.0), 0.0, 1.0);
                    const double fresh = std::pow(unitFromHash(h, 5), youth);
                    // fade: the class comes in as it becomes resolved (band limit)
                    const double fade = std::clamp(radius * 2.0 / std::max(minWavelengthM, 1.0e-3) - 1.0, 0.0, 1.0);
                    // bright rays of a fresh crater out to 5 radii: spokes of immature ejecta
                    if (fresh > 0.55 && r > 0.9)
                    {
                        const glm::dvec3 n = glm::normalize(q);
                        const glm::dvec3 t1 = glm::normalize(glm::cross(n, std::abs(n.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0)));
                        const glm::dvec3 t2 = glm::cross(n, t1);
                        const glm::dvec3 off = x - centre;
                        const double theta = std::atan2(glm::dot(off, t2), glm::dot(off, t1));
                        const double spokes = 5.0 + std::floor(unitFromHash(h, 6) * 6.0);
                        const double ray = std::pow(std::max(0.0, std::cos(spokes * theta + unitFromHash(h, 7) * 6.2832)), 10.0);
                        out.freshness = std::max(out.freshness, static_cast<float>(ray * std::clamp((5.0 - r) / 4.0, 0.0, 1.0) *
                                                                                   (fresh - 0.55) / 0.45 * 0.8 * fade));
                    }
                    if (r >= 3.0)
                        continue;
                    // d/D 0.05 (old) .. 0.2 (fresh); a complex crater (D > ~15 km) is
                    // shallower: d ~ 1.04 km x D_km^0.3 (lunar morphometry)
                    const double complexDepth = 1040.0 * std::pow(radius * 2.0e-3, 0.3) * (0.45 + 0.55 * fresh);
                    const double depth = std::min(radius * (0.10 + 0.30 * fresh), complexDepth);
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

bool CraterLayer::landmark(const glm::dvec3& dir, double minRadiusM, std::uint64_t& craterId, double& craterRadiusM) const
{
    bool found = false;
    craterRadiusM = 0.0;
    const glm::dvec3 x = dir * m_radius;
    for (const Class& cls : m_classes)
    {
        if (cls.cellM * MaxRadiusOfCell < minRadiusM)
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
                    if (std::abs(glm::length(q) - m_radius) > cls.cellM * 0.5)
                        continue;
                    const double radius =
                        cls.cellM * (MinRadiusOfCell + (MaxRadiusOfCell - MinRadiusOfCell) * std::pow(unitFromHash(h, 4), 2.0));
                    if (radius < minRadiusM || radius <= craterRadiusM)
                        continue;
                    const glm::dvec3 centre = glm::normalize(q) * m_radius;
                    if (glm::length(x - centre) >= radius)
                        continue;
                    if (m_density && unitFromHash(h, 0) >= cls.probability * densityAt(centre / m_radius))
                        continue;
                    craterRadiusM = radius;
                    craterId = h;
                    found = true;
                }
    }
    return found;
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
