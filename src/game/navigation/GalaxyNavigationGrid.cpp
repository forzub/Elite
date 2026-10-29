#include "src/game/navigation/GalaxyNavigationGrid.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "src/world/coordinates/GalacticGrids.h"

namespace game::navigation
{
namespace
{

CubicGridIndex parentIndex(
    const CubicGridIndex& child,
    int subdivision
)
{
    const double divisor = static_cast<double>(subdivision);

    return {
        static_cast<std::int64_t>(
            std::llround(static_cast<double>(child.x) / divisor)
        ),
        static_cast<std::int64_t>(
            std::llround(static_cast<double>(child.y) / divisor)
        ),
        static_cast<std::int64_t>(
            std::llround(static_cast<double>(child.z) / divisor)
        )
    };
}

} // namespace

class GalaxyNavigationDomainPolicy final
    : public CubicNavigationPolicy
{
public:
    GalaxyNavigationDomainPolicy(
        int subdivision,
        world::coordinates::GalacticNavigationDomain domain
    )
        : m_subdivision(subdivision),
          m_domain(std::move(domain)),
          m_rootCells(m_domain.cells())
    {
    }

    bool isCellNavigable(
        const CubicGridIndex& index,
        int level
    ) const override
    {
        CubicGridIndex root = index;

        for (int levelCursor = level;
             levelCursor > 0;
             --levelCursor)
        {
            root = parentIndex(root, m_subdivision);
        }

        return m_domain.contains({root.x, root.y, root.z});
    }

    bool extendTo(const glm::dvec3& positionLy)
    {
        if (!m_domain.extendTo(positionLy))
            return false;
        m_rootCells = m_domain.cells();
        return true;
    }

    const std::vector<std::array<std::int64_t, 3>>&
    rootCells() const
    {
        return m_rootCells;
    }

private:
    int m_subdivision = 3;
    world::coordinates::GalacticNavigationDomain m_domain;
    std::vector<std::array<std::int64_t, 3>> m_rootCells;
};

GalaxyNavigationGrid::GalaxyNavigationGrid()
    : GalaxyNavigationGrid(loadConfig())
{
}

GalaxyNavigationGrid::GalaxyNavigationGrid(
    GalaxyNavigationConfig config
)
    : CubicNavigationGrid(makeDefinition(config)),
      m_config(std::move(config)),
      m_domainPolicy(
          std::make_shared<GalaxyNavigationDomainPolicy>(
              m_config.subdivisionPerAxis,
              world::coordinates::GalacticNavigationDomain(
                  {m_config.rootEdgeLy()},
                  {m_config.localEnvelopeRadiusLy,
                   m_config.remoteEnvelopeRadiusLy}
              )
          )
      )
{
    setPolicy(m_domainPolicy);
    reset();
}

GalaxyNavigationConfig GalaxyNavigationGrid::loadConfig()
{
    return GalaxyNavigationConfig::loadFromRuntimeOrSource(
        "assets/data/navigation/navigation_grid.json",
        "src/assets/data/navigation/navigation_grid.json"
    );
}

CubicNavigationGridDefinition GalaxyNavigationGrid::makeDefinition(
    const GalaxyNavigationConfig& config
)
{
    CubicNavigationGridDefinition result;

    result.frame.id = "sol_equatorial_j2000";
    result.frame.unit = "LY";
    result.frame.origin = glm::dvec3(0.0);
    result.frame.axisX = glm::dvec3(1.0, 0.0, 0.0);
    result.frame.axisY = glm::dvec3(0.0, 1.0, 0.0);
    result.frame.axisZ = glm::dvec3(0.0, 0.0, 1.0);

    result.subdivision = config.subdivisionPerAxis;
    result.minimumLevel = config.minimumLevel;
    result.initialLevel = config.initialLevel;
    result.maximumLevel = config.galaxyMaximumLevel();
    result.baseCellSize = config.rootEdgeLy();

    return result;
}

void GalaxyNavigationGrid::reset()
{
    CubicNavigationGrid::reset();
    m_displayRadius = 1;
}

const GalaxyNavigationConfig& GalaxyNavigationGrid::config() const
{
    return m_config;
}

void GalaxyNavigationGrid::extendDomainTo(
    const std::vector<glm::dvec3>& positionsLy
)
{
    bool changed = false;

    for (const glm::dvec3& positionLy : positionsLy)
        changed = m_domainPolicy->extendTo(positionLy) || changed;

    /* Revalidate transient/explicit state against the updated domain. */
    if (changed)
        setPolicy(m_domainPolicy);
}

const std::vector<std::array<std::int64_t, 3>>&
GalaxyNavigationGrid::navigableRootCells() const
{
    return m_domainPolicy->rootCells();
}

double GalaxyNavigationGrid::cellSizeLy() const
{
    return cellSize();
}

double GalaxyNavigationGrid::cellSizeLy(int levelValue) const
{
    return cellSize(levelValue);
}

int GalaxyNavigationGrid::displayRadius() const
{
    return m_displayRadius;
}

void GalaxyNavigationGrid::setDisplayRadius(int radius)
{
    m_displayRadius = std::clamp(radius, 1, 2);
}

void GalaxyNavigationGrid::setAnchorFromPositionLy(
    const glm::dvec3& positionLy
)
{
    setAnchorFromPosition(positionLy);
}

glm::dvec3 GalaxyNavigationGrid::cellCenterLy(
    const GalaxyGridIndex& index,
    int levelValue
) const
{
    return cellCenter(index, levelValue);
}

GalaxyGridIndex GalaxyNavigationGrid::nearestIndexForPositionLy(
    const glm::dvec3& positionLy,
    int levelValue
) const
{
    return nearestIndexForPosition(positionLy, levelValue);
}

GalaxyGridIndex GalaxyNavigationGrid::rootIndexForCell(
    const GalaxyGridIndex& index,
    int levelValue
) const
{
    return ancestorIndex(index, levelValue, 0);
}

std::vector<GalaxyNavigationCell>
GalaxyNavigationGrid::neighborhood() const
{
    return CubicNavigationGrid::neighborhood(m_displayRadius);
}

} // namespace game::navigation
