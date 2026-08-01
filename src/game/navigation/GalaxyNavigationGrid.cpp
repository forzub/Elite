#include "src/game/navigation/GalaxyNavigationGrid.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
        std::vector<std::array<std::int64_t, 3>> allowedRootCells
    )
        : m_subdivision(subdivision),
          m_allowedRootCells(std::move(allowedRootCells))
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

        return std::find(
            m_allowedRootCells.begin(),
            m_allowedRootCells.end(),
            std::array<std::int64_t, 3>{
                root.x,
                root.y,
                root.z
            }
        ) != m_allowedRootCells.end();
    }

    void setAllowedRootCells(
        std::vector<std::array<std::int64_t, 3>> cells
    )
    {
        m_allowedRootCells = std::move(cells);
    }

    const std::vector<std::array<std::int64_t, 3>>&
    allowedRootCells() const
    {
        return m_allowedRootCells;
    }

private:
    int m_subdivision = 3;
    std::vector<std::array<std::int64_t, 3>> m_allowedRootCells;
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
              m_config.allowedRootCells
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

void GalaxyNavigationGrid::synchronizeCatalogPositions(
    const std::vector<glm::dvec3>& positionsLy
)
{
    std::vector<std::array<std::int64_t, 3>> allowed =
        m_config.allowedRootCells;

    for (const glm::dvec3& positionLy : positionsLy)
    {
        const auto root = rootIndexForPositionLy(positionLy);

        if (std::find(allowed.begin(), allowed.end(), root) == allowed.end())
            allowed.push_back(root);
    }

    std::sort(allowed.begin(), allowed.end());
    m_domainPolicy->setAllowedRootCells(std::move(allowed));

    /* Revalidate transient/explicit state against the updated domain. */
    setPolicy(m_domainPolicy);
}

const std::vector<std::array<std::int64_t, 3>>&
GalaxyNavigationGrid::allowedRootCells() const
{
    return m_domainPolicy->allowedRootCells();
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

std::array<std::int64_t, 3>
GalaxyNavigationGrid::rootIndexForPositionLy(
    const glm::dvec3& positionLy
) const
{
    const double rootSizeLy = m_config.rootEdgeLy();
    const CubicNavigationFrame& navigationFrame = frame();
    const glm::dvec3 relative = positionLy - navigationFrame.origin;

    return {
        static_cast<std::int64_t>(
            std::llround(
                glm::dot(relative, navigationFrame.axisX) / rootSizeLy
            )
        ),
        static_cast<std::int64_t>(
            std::llround(
                glm::dot(relative, navigationFrame.axisY) / rootSizeLy
            )
        ),
        static_cast<std::int64_t>(
            std::llround(
                glm::dot(relative, navigationFrame.axisZ) / rootSizeLy
            )
        )
    };
}

} // namespace game::navigation
