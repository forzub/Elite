#include "src/game/navigation/CubicNavigationGrid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game::navigation
{

CubicNavigationGrid::CubicNavigationGrid(
    const CubicNavigationGridDefinition& definition
)
{
    configure(definition);
}

void CubicNavigationGrid::configure(
    const CubicNavigationGridDefinition& definition
)
{
    m_definition = definition;
    validateDefinition();
    reset();
}

void CubicNavigationGrid::validateDefinition() const
{
    if (m_definition.subdivision < 3 ||
        (m_definition.subdivision % 2) == 0)
    {
        throw std::runtime_error(
            "Cubic navigation subdivision must be odd and >= 3"
        );
    }

    if (m_definition.baseCellSize <= 0.0 ||
        m_definition.minimumLevel < 0 ||
        m_definition.maximumLevel < m_definition.minimumLevel ||
        m_definition.initialLevel < m_definition.minimumLevel ||
        m_definition.initialLevel > m_definition.maximumLevel)
    {
        throw std::runtime_error(
            "Invalid cubic navigation grid definition"
        );
    }
}

void CubicNavigationGrid::reset()
{
    m_enabled = true;
    m_level = m_definition.initialLevel;
    m_anchorIndex = {};
    m_selectedCell = cell(m_anchorIndex, m_level);
    m_hoveredCell.reset();
}

const CubicNavigationGridDefinition&
CubicNavigationGrid::definition() const
{
    return m_definition;
}

const CubicNavigationFrame& CubicNavigationGrid::frame() const
{
    return m_definition.frame;
}

bool CubicNavigationGrid::enabled() const
{
    return m_enabled;
}

void CubicNavigationGrid::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
        m_hoveredCell.reset();
}

int CubicNavigationGrid::level() const
{
    return m_level;
}

int CubicNavigationGrid::subdivision() const
{
    return m_definition.subdivision;
}

double CubicNavigationGrid::cellSize() const
{
    return cellSize(m_level);
}

double CubicNavigationGrid::cellSize(int levelValue) const
{
    levelValue = std::clamp(
        levelValue,
        m_definition.minimumLevel,
        m_definition.maximumLevel
    );

    double divisor = 1.0;
    for (int i = 0; i < levelValue; ++i)
        divisor *= static_cast<double>(m_definition.subdivision);

    return m_definition.baseCellSize / divisor;
}

const CubicGridIndex& CubicNavigationGrid::anchorIndex() const
{
    return m_anchorIndex;
}

CubicNavigationCell CubicNavigationGrid::anchorCell() const
{
    return cell(m_anchorIndex, m_level);
}

void CubicNavigationGrid::setAnchorIndex(
    const CubicGridIndex& index
)
{
    m_anchorIndex = index;
}

void CubicNavigationGrid::setAnchorFromPosition(
    const glm::dvec3& position
)
{
    m_anchorIndex = nearestIndexForPosition(position, m_level);
}

bool CubicNavigationGrid::hasHoveredCell() const
{
    return m_hoveredCell.has_value();
}

const CubicNavigationCell& CubicNavigationGrid::hoveredCell() const
{
    return m_hoveredCell.value();
}

void CubicNavigationGrid::setHoveredCell(
    const CubicNavigationCell& cellValue
)
{
    m_hoveredCell = cellValue;
}

void CubicNavigationGrid::clearHoveredCell()
{
    m_hoveredCell.reset();
}

bool CubicNavigationGrid::hasSelectedCell() const
{
    return m_selectedCell.has_value();
}

const CubicNavigationCell& CubicNavigationGrid::selectedCell() const
{
    return m_selectedCell.value();
}

void CubicNavigationGrid::selectCell(
    const CubicNavigationCell& cellValue
)
{
    m_level = std::clamp(
        cellValue.level,
        m_definition.minimumLevel,
        m_definition.maximumLevel
    );

    m_anchorIndex = cellValue.index;
    m_selectedCell = cell(m_anchorIndex, m_level);
    m_hoveredCell = m_selectedCell;
}

void CubicNavigationGrid::clearSelectedCell()
{
    m_selectedCell.reset();
}

bool CubicNavigationGrid::canRefine() const
{
    return m_level < m_definition.maximumLevel;
}

bool CubicNavigationGrid::canCoarsen() const
{
    return m_level > m_definition.minimumLevel;
}

bool CubicNavigationGrid::refineAroundAnchor()
{
    if (!canRefine())
        return false;

    m_anchorIndex.x *= m_definition.subdivision;
    m_anchorIndex.y *= m_definition.subdivision;
    m_anchorIndex.z *= m_definition.subdivision;
    ++m_level;

    const CubicNavigationCell child = cell(m_anchorIndex, m_level);
    m_selectedCell = child;
    m_hoveredCell = child;
    return true;
}

bool CubicNavigationGrid::coarsenAroundAnchor()
{
    if (!canCoarsen())
        return false;

    m_anchorIndex.x = nearestParentIndex(m_anchorIndex.x);
    m_anchorIndex.y = nearestParentIndex(m_anchorIndex.y);
    m_anchorIndex.z = nearestParentIndex(m_anchorIndex.z);
    --m_level;

    const CubicNavigationCell parent = cell(m_anchorIndex, m_level);
    m_selectedCell = parent;
    m_hoveredCell = parent;
    return true;
}

CubicNavigationCell CubicNavigationGrid::cell(
    const CubicGridIndex& index,
    int levelValue
) const
{
    CubicNavigationCell result;
    result.index = index;
    result.level = std::clamp(
        levelValue,
        m_definition.minimumLevel,
        m_definition.maximumLevel
    );
    result.size = cellSize(result.level);
    result.center = cellCenter(index, result.level);
    return result;
}

glm::dvec3 CubicNavigationGrid::cellCenter(
    const CubicGridIndex& index,
    int levelValue
) const
{
    const double size = cellSize(levelValue);
    const CubicNavigationFrame& f = m_definition.frame;

    return
        f.origin +
        f.axisX * (static_cast<double>(index.x) * size) +
        f.axisY * (static_cast<double>(index.y) * size) +
        f.axisZ * (static_cast<double>(index.z) * size);
}

CubicGridIndex CubicNavigationGrid::nearestIndexForPosition(
    const glm::dvec3& position,
    int levelValue
) const
{
    const double size = cellSize(levelValue);
    const CubicNavigationFrame& f = m_definition.frame;
    const glm::dvec3 relative = position - f.origin;

    return {
        static_cast<std::int64_t>(
            std::llround(glm::dot(relative, f.axisX) / size)
        ),
        static_cast<std::int64_t>(
            std::llround(glm::dot(relative, f.axisY) / size)
        ),
        static_cast<std::int64_t>(
            std::llround(glm::dot(relative, f.axisZ) / size)
        )
    };
}

std::int64_t CubicNavigationGrid::nearestParentIndex(
    std::int64_t child
) const
{
    return static_cast<std::int64_t>(
        std::llround(
            static_cast<double>(child) /
            static_cast<double>(m_definition.subdivision)
        )
    );
}

} // namespace game::navigation
