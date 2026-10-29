#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include <glm/glm.hpp>

namespace world::coordinates
{

/*
    Galactic grids. One canonical frame, three grids with different jobs.

    Frame: heliocentric ICRS / J2000 equatorial Cartesian light-years,
    x = d cos(dec) cos(ra), y = d cos(dec) sin(ra), z = d sin(dec). Catalog
    position_ly, the Galaxy Map, WorldPosition galactic offsets
    (systemPositionLy * MetersPerLightYear) and interstellar generation all
    use it. (A system's local frame - the engine frame of its barycentre - is
    a separate, per-system convention.)

    1. WorldPosition GalacticCell (WorldPosition.h): 1 ly, centred
       (floor(x + 0.5)) - floating-origin precision of world positions only.
    2. Galaxy navigation root cell: edge = terminalCellEdgeKm * subdivision^depth
       (~40.32 ly), centred (llround(x / edge)) - the root of the Galaxy Map
       navigation hierarchy and of the navigation domain below.
    3. Interstellar generation cell: 25 ly, floored (floor(x / 25)) - the seed
       unit of interstellar content (GalacticCellSeed); a cell always
       generates the same content and a new cell never changes an old one.

    The grids are deliberately not aligned; the conversions below are the
    only way between them.
*/

using GridCell3 = std::array<std::int64_t, 3>;

struct CentredCubicGrid
{
    double edgeLy = 1.0;

    GridCell3 cellOf(const glm::dvec3& p) const
    {
        return {std::llround(p.x / edgeLy), std::llround(p.y / edgeLy), std::llround(p.z / edgeLy)};
    }
    glm::dvec3 lower(const GridCell3& c) const
    {
        return (glm::dvec3(c[0], c[1], c[2]) - 0.5) * edgeLy;
    }
    glm::dvec3 upper(const GridCell3& c) const { return lower(c) + edgeLy; }
    glm::dvec3 centre(const GridCell3& c) const { return glm::dvec3(c[0], c[1], c[2]) * edgeLy; }
};

struct FlooredCubicGrid
{
    double edgeLy = 1.0;

    GridCell3 cellOf(const glm::dvec3& p) const
    {
        return {static_cast<std::int64_t>(std::floor(p.x / edgeLy)),
                static_cast<std::int64_t>(std::floor(p.y / edgeLy)),
                static_cast<std::int64_t>(std::floor(p.z / edgeLy))};
    }
    glm::dvec3 lower(const GridCell3& c) const { return glm::dvec3(c[0], c[1], c[2]) * edgeLy; }
    glm::dvec3 upper(const GridCell3& c) const { return lower(c) + edgeLy; }
    glm::dvec3 centre(const GridCell3& c) const { return lower(c) + 0.5 * edgeLy; }
};

inline constexpr double InterstellarGenerationCellLy = 25.0;

inline FlooredCubicGrid interstellarGenerationGrid()
{
    return {InterstellarGenerationCellLy};
}

/*
    Navigation domain policy.

    Empty interstellar space is navigable; the domain is NOT "root cells that
    contain a catalog star". It is a connected set of navigation root cells:

    - the local envelope: every root cell intersecting the ball of
      localEnvelopeRadiusLy around Sol (fixed by configuration, so adding a
      catalog system never changes it);
    - for every position outside the envelope (a distant quest system, a
      player far away): the root cells intersecting a ball of
      remoteEnvelopeRadiusLy around it plus a one-cell-wide face-connected
      corridor of root cells along the straight line from Sol. The set stays
      sparse: Kepler-62 at ~980 ly adds a few dozen cells, not a block.

    The domain only grows (extension on demand); it never removes cells.
    Materialised content (catalog systems, generated interstellar objects) is
    a separate notion: content is generated per interstellar generation cell
    for the cells covering the domain (generationCellsCovering).
*/
struct GalacticDomainSettings
{
    double localEnvelopeRadiusLy = 140.0;
    double remoteEnvelopeRadiusLy = 20.0;
};

namespace detail
{

inline double distanceToBox(const glm::dvec3& p, const glm::dvec3& lo, const glm::dvec3& hi)
{
    const glm::dvec3 q = glm::clamp(p, lo, hi);
    return glm::length(p - q);
}

template <class Grid>
void addBall(const Grid& grid, const glm::dvec3& c, double r, std::set<GridCell3>& out)
{
    const GridCell3 a = grid.cellOf(c - r);
    const GridCell3 b = grid.cellOf(c + r);
    for (std::int64_t x = a[0]; x <= b[0]; ++x)
        for (std::int64_t y = a[1]; y <= b[1]; ++y)
            for (std::int64_t z = a[2]; z <= b[2]; ++z)
            {
                const GridCell3 cell {x, y, z};
                if (distanceToBox(c, grid.lower(cell), grid.upper(cell)) <= r)
                    out.insert(cell);
            }
}

// Face-connected supercover of the segment a-b (Amanatides & Woo traversal).
template <class Grid>
void addSegment(const Grid& grid, const glm::dvec3& a, const glm::dvec3& b, std::set<GridCell3>& out)
{
    GridCell3 cell = grid.cellOf(a);
    const GridCell3 last = grid.cellOf(b);
    const glm::dvec3 d = b - a;
    std::int64_t step[3];
    double tMax[3], tDelta[3];
    for (int i = 0; i < 3; ++i)
    {
        step[i] = d[i] > 0.0 ? 1 : (d[i] < 0.0 ? -1 : 0);
        const glm::dvec3 lo = grid.lower(cell), hi = grid.upper(cell);
        const double boundary = step[i] > 0 ? hi[i] : lo[i];
        tMax[i] = step[i] != 0 ? (boundary - a[i]) / d[i] : 2.0;
        tDelta[i] = step[i] != 0 ? grid.edgeLy / std::abs(d[i]) : 2.0;
    }
    out.insert(cell);
    for (int guard = 0; cell != last && guard < 1000000; ++guard)
    {
        int axis = 0;
        if (tMax[1] < tMax[axis]) axis = 1;
        if (tMax[2] < tMax[axis]) axis = 2;
        if (tMax[axis] > 1.0)
            break;
        cell[axis] += step[axis];
        tMax[axis] += tDelta[axis];
        out.insert(cell);
    }
    out.insert(last);
}

} // namespace detail

class GalacticNavigationDomain
{
public:
    GalacticNavigationDomain() = default;

    GalacticNavigationDomain(const CentredCubicGrid& grid, const GalacticDomainSettings& settings)
        : m_grid(grid), m_settings(settings)
    {
        detail::addBall(m_grid, glm::dvec3(0.0), m_settings.localEnvelopeRadiusLy, m_cells);
    }

    // Make a position navigable: no-op inside the domain's envelope, else a
    // remote envelope plus a corridor from Sol. Returns true when cells were added.
    bool extendTo(const glm::dvec3& positionLy)
    {
        const std::size_t before = m_cells.size();
        if (glm::length(positionLy) > m_settings.localEnvelopeRadiusLy)
        {
            detail::addBall(m_grid, positionLy, m_settings.remoteEnvelopeRadiusLy, m_cells);
            detail::addSegment(m_grid, glm::dvec3(0.0), positionLy, m_cells);
        }
        else
        {
            m_cells.insert(m_grid.cellOf(positionLy));
        }
        return m_cells.size() != before;
    }

    bool contains(const GridCell3& cell) const { return m_cells.count(cell) != 0; }

    std::vector<GridCell3> cells() const { return {m_cells.begin(), m_cells.end()}; }
    std::size_t size() const { return m_cells.size(); }
    const CentredCubicGrid& grid() const { return m_grid; }
    const GalacticDomainSettings& settings() const { return m_settings; }

private:
    CentredCubicGrid m_grid;
    GalacticDomainSettings m_settings;
    std::set<GridCell3> m_cells;
};

// Face-connected components of a cell set, largest first.
inline std::vector<std::vector<GridCell3>> faceConnectedComponents(const std::vector<GridCell3>& cells)
{
    std::set<GridCell3> rest(cells.begin(), cells.end());
    std::vector<std::vector<GridCell3>> out;
    static constexpr int Faces[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    while (!rest.empty())
    {
        std::vector<GridCell3> comp {*rest.begin()};
        rest.erase(rest.begin());
        for (std::size_t i = 0; i < comp.size(); ++i)
            for (const auto& f : Faces)
            {
                const GridCell3 n {comp[i][0] + f[0], comp[i][1] + f[1], comp[i][2] + f[2]};
                if (rest.erase(n))
                    comp.push_back(n);
            }
        std::sort(comp.begin(), comp.end());
        out.push_back(std::move(comp));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
    return out;
}

// Interstellar generation cells whose box overlaps a navigation root cell of the domain.
inline std::vector<GridCell3> generationCellsCovering(const GalacticNavigationDomain& domain,
                                                      const FlooredCubicGrid& generation)
{
    std::set<GridCell3> out;
    const double eps = 1.0e-9;
    for (const auto& root : domain.cells())
    {
        const GridCell3 a = generation.cellOf(domain.grid().lower(root) + eps);
        const GridCell3 b = generation.cellOf(domain.grid().upper(root) - eps);
        for (std::int64_t x = a[0]; x <= b[0]; ++x)
            for (std::int64_t y = a[1]; y <= b[1]; ++y)
                for (std::int64_t z = a[2]; z <= b[2]; ++z)
                    out.insert({x, y, z});
    }
    return {out.begin(), out.end()};
}

} // namespace world::coordinates
