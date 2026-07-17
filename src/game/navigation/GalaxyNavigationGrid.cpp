#include "src/game/navigation/GalaxyNavigationGrid.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{

GalaxyNavigationGrid::GalaxyNavigationGrid()
{
    reset();
}

void GalaxyNavigationGrid::reset()
{
    m_enabled = false;
    m_level = MinimumLevel;
    m_displayRadius = 1;
    m_anchorIndex = {};
    m_hoveredCell.reset();
    m_selectedCell.reset();
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
        MinimumLevel,
        MaximumLevel
    );

    double divisor = 1.0;

    for (int i = 0; i < level; ++i)
        divisor *= static_cast<double>(Subdivision);

    return BaseCellSizeLy / divisor;
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
    m_anchorIndex = index;
}

void GalaxyNavigationGrid::setAnchorFromPositionLy(
    const glm::dvec3& positionLy
)
{
    m_anchorIndex =
        nearestIndexForPositionLy(
            positionLy,
            m_level
        );
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
    m_hoveredCell = cellValue;
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
    m_level = std::clamp(
        cellValue.level,
        MinimumLevel,
        MaximumLevel
    );

    m_anchorIndex = cellValue.index;
    m_selectedCell = cell(m_anchorIndex, m_level);
    m_hoveredCell = m_selectedCell;
}

void GalaxyNavigationGrid::clearSelectedCell()
{
    m_selectedCell.reset();
}

bool GalaxyNavigationGrid::canRefine() const
{
    return m_level < MaximumLevel;
}

bool GalaxyNavigationGrid::canCoarsen() const
{
    return m_level > MinimumLevel;
}

bool GalaxyNavigationGrid::refineAroundAnchor()
{
    if (!canRefine())
        return false;

    m_anchorIndex.x *= Subdivision;
    m_anchorIndex.y *= Subdivision;
    m_anchorIndex.z *= Subdivision;

    ++m_level;

    const GalaxyNavigationCell centralChild =
        cell(m_anchorIndex, m_level);

    m_selectedCell = centralChild;
    m_hoveredCell = centralChild;

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

    const GalaxyNavigationCell parent =
        cell(m_anchorIndex, m_level);

    m_selectedCell = parent;
    m_hoveredCell = parent;

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
        MinimumLevel,
        MaximumLevel
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

                result.push_back(
                    cell(index, m_level)
                );
            }
        }
    }

    return result;
}

std::int64_t GalaxyNavigationGrid::nearestParentIndex(
    std::int64_t childIndex
)
{
    return static_cast<std::int64_t>(
        std::llround(
            static_cast<double>(childIndex) /
            static_cast<double>(Subdivision)
        )
    );
}

} // namespace game::navigation
