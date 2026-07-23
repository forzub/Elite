#include "src/game/navigation/GalaxyNavigationGrid.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{

GalaxyNavigationGrid::GalaxyNavigationGrid()
{
    m_config =
        GalaxyNavigationConfig::loadFromRuntimeOrSource(
            "assets/data/navigation/navigation_grid.json",
            "src/assets/data/navigation/navigation_grid.json"
        );

    reset();
}






void GalaxyNavigationGrid::reset()
{
    m_enabled = true;
    m_level = m_config.initialLevel;
    m_displayRadius = 1;

    /*
        Начальный куб расположен вокруг начала координат:
        Sol, индекс 0/0/0.
    */
    m_anchorIndex = {};

    /*
        При первом открытии карты центральный куб уже является
        зафиксированным выбранным кубом.
    */
    m_selectedCell =
        cell(
            m_anchorIndex,
            m_level
        );

    /*
        Hover появляется только после движения курсора.
    */
    m_hoveredCell.reset();
}





bool GalaxyNavigationGrid::enabled() const
{
    return m_enabled;
}

void GalaxyNavigationGrid::setEnabled(bool enabled)
{
    m_enabled = enabled;

    if (!m_enabled)
        m_hoveredCell.reset();
}

void GalaxyNavigationGrid::toggleEnabled()
{
    setEnabled(!m_enabled);
}

const GalaxyNavigationFrame& GalaxyNavigationGrid::frame() const
{
    return m_frame;
}

const GalaxyNavigationConfig& GalaxyNavigationGrid::config() const
{
    return m_config;
}

int GalaxyNavigationGrid::subdivision() const
{
    return m_config.subdivisionPerAxis;
}

int GalaxyNavigationGrid::minimumLevel() const
{
    return m_config.minimumLevel;
}

int GalaxyNavigationGrid::initialLevel() const
{
    return m_config.initialLevel;
}

int GalaxyNavigationGrid::maximumLevel() const
{
    return m_config.galaxyMaximumLevel();
}

int GalaxyNavigationGrid::level() const
{
    return m_level;
}

double GalaxyNavigationGrid::cellSizeLy() const
{
    return cellSizeLy(m_level);
}

double GalaxyNavigationGrid::cellSizeLy(int level) const
{
    level = std::clamp(
        level,
        minimumLevel(),
        maximumLevel()
    );

    return m_config.cellEdgeLy(level);
}

int GalaxyNavigationGrid::displayRadius() const
{
    return m_displayRadius;
}

void GalaxyNavigationGrid::setDisplayRadius(int radius)
{
    /*
        radius=1 -> 3x3x3
        radius=2 -> 5x5x5
    */
    m_displayRadius = std::clamp(radius, 1, 2);
}

const GalaxyGridIndex& GalaxyNavigationGrid::anchorIndex() const
{
    return m_anchorIndex;
}

GalaxyNavigationCell GalaxyNavigationGrid::anchorCell() const
{
    return cell(m_anchorIndex, m_level);
}

void GalaxyNavigationGrid::setAnchorIndex(
    const GalaxyGridIndex& index
)
{
    if (isCellNavigable(index, m_level))
        m_anchorIndex = index;
}

void GalaxyNavigationGrid::setAnchorFromPositionLy(
    const glm::dvec3& positionLy
)
{
    const GalaxyGridIndex candidate =
        nearestIndexForPositionLy(
            positionLy,
            m_level
        );

    if (isCellNavigable(candidate, m_level))
        m_anchorIndex = candidate;
}

bool GalaxyNavigationGrid::hasHoveredCell() const
{
    return m_hoveredCell.has_value();
}

const GalaxyNavigationCell& GalaxyNavigationGrid::hoveredCell() const
{
    return m_hoveredCell.value();
}

void GalaxyNavigationGrid::setHoveredCell(
    const GalaxyNavigationCell& cellValue
)
{
    if (isCellNavigable(cellValue))
        m_hoveredCell = cellValue;
    else
        m_hoveredCell.reset();
}

void GalaxyNavigationGrid::clearHoveredCell()
{
    m_hoveredCell.reset();
}

bool GalaxyNavigationGrid::hasSelectedCell() const
{
    return m_selectedCell.has_value();
}

const GalaxyNavigationCell& GalaxyNavigationGrid::selectedCell() const
{
    return m_selectedCell.value();
}

void GalaxyNavigationGrid::selectCell(
    const GalaxyNavigationCell& cellValue
)
{
    if (!isCellNavigable(cellValue))
        return;

    /*
        Selection is a route/address target, not the camera anchor.

        Changing the selected cube must not change:
        - the current view level;
        - the local render neighborhood;
        - the point used by wheel navigation.
    */
    m_selectedCell =
        cell(
            cellValue.index,
            cellValue.level
        );
}

void GalaxyNavigationGrid::clearSelectedCell()
{
    m_selectedCell.reset();
}

bool GalaxyNavigationGrid::canRefine() const
{
    return m_level < maximumLevel();
}

bool GalaxyNavigationGrid::canCoarsen() const
{
    return m_level > minimumLevel();
}

bool GalaxyNavigationGrid::refineAroundAnchor()
{
    if (!canRefine())
        return false;

    m_anchorIndex.x *= subdivision();
    m_anchorIndex.y *= subdivision();
    m_anchorIndex.z *= subdivision();

    ++m_level;

    /*
        The view anchor changes resolution, while the explicit
        user selection keeps its original address.
    */
    m_hoveredCell.reset();

    return true;
}

bool GalaxyNavigationGrid::coarsenAroundAnchor()
{
    if (!canCoarsen())
        return false;

    m_anchorIndex.x = nearestParentIndex(m_anchorIndex.x);
    m_anchorIndex.y = nearestParentIndex(m_anchorIndex.y);
    m_anchorIndex.z = nearestParentIndex(m_anchorIndex.z);

    --m_level;

    m_hoveredCell.reset();

    return true;
}

GalaxyNavigationCell GalaxyNavigationGrid::cell(
    const GalaxyGridIndex& index,
    int levelValue
) const
{
    GalaxyNavigationCell result;

    result.index = index;
    result.level = std::clamp(
        levelValue,
        minimumLevel(),
        maximumLevel()
    );

    result.sizeLy = cellSizeLy(result.level);
    result.centerLy = cellCenterLy(index, result.level);

    return result;
}

glm::dvec3 GalaxyNavigationGrid::cellCenterLy(
    const GalaxyGridIndex& index,
    int levelValue
) const
{
    const double size = cellSizeLy(levelValue);

    return
        m_frame.originLy +
        m_frame.axisX *
            (static_cast<double>(index.x) * size) +
        m_frame.axisY *
            (static_cast<double>(index.y) * size) +
        m_frame.axisZ *
            (static_cast<double>(index.z) * size);
}

GalaxyGridIndex GalaxyNavigationGrid::nearestIndexForPositionLy(
    const glm::dvec3& positionLy,
    int levelValue
) const
{
    const double size = cellSizeLy(levelValue);

    const glm::dvec3 relative =
        positionLy - m_frame.originLy;

    GalaxyGridIndex result;

    result.x = static_cast<std::int64_t>(
        std::llround(
            glm::dot(relative, m_frame.axisX) / size
        )
    );

    result.y = static_cast<std::int64_t>(
        std::llround(
            glm::dot(relative, m_frame.axisY) / size
        )
    );

    result.z = static_cast<std::int64_t>(
        std::llround(
            glm::dot(relative, m_frame.axisZ) / size
        )
    );

    return result;
}

GalaxyGridIndex GalaxyNavigationGrid::rootIndexForCell(
    const GalaxyGridIndex& index,
    int levelValue
) const
{
    GalaxyGridIndex root = index;

    for (int level = levelValue; level > 0; --level)
    {
        root.x = nearestParentIndex(root.x);
        root.y = nearestParentIndex(root.y);
        root.z = nearestParentIndex(root.z);
    }

    return root;
}

bool GalaxyNavigationGrid::isCellNavigable(
    const GalaxyGridIndex& index,
    int levelValue
) const
{
    const GalaxyGridIndex root =
        rootIndexForCell(index, levelValue);

    return m_config.isRootAllowed(
        root.x,
        root.y,
        root.z
    );
}

bool GalaxyNavigationGrid::isCellNavigable(
    const GalaxyNavigationCell& cellValue
) const
{
    return isCellNavigable(
        cellValue.index,
        cellValue.level
    );
}

std::vector<GalaxyNavigationCell>
GalaxyNavigationGrid::neighborhood() const
{
    const int radius = m_displayRadius;
    const int side = radius * 2 + 1;

    std::vector<GalaxyNavigationCell> result;

    result.reserve(
        static_cast<std::size_t>(
            side * side * side
        )
    );

    for (int dz = -radius; dz <= radius; ++dz)
    {
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                GalaxyGridIndex index;

                index.x = m_anchorIndex.x + dx;
                index.y = m_anchorIndex.y + dy;
                index.z = m_anchorIndex.z + dz;

                if (isCellNavigable(index, m_level))
                {
                    result.push_back(
                        cell(index, m_level)
                    );
                }
            }
        }
    }

    return result;
}

std::int64_t GalaxyNavigationGrid::nearestParentIndex(
    std::int64_t childIndex
) const
{
    return static_cast<std::int64_t>(
        std::llround(
            static_cast<double>(childIndex) /
            static_cast<double>(subdivision())
        )
    );
}

} // namespace game::navigation
