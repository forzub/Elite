#include "src/game/navigation/CubicNavigationGrid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game::navigation
{

CubicNavigationGrid::CubicNavigationGrid(
    const CubicNavigationGridDefinition& definition,
    std::shared_ptr<const CubicNavigationPolicy> policy
)
    : m_policy(std::move(policy))
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

void CubicNavigationGrid::setPolicy(
    std::shared_ptr<const CubicNavigationPolicy> policy
)
{
    m_policy = std::move(policy);
    normalizeStateForPolicy();
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
    m_anchor.level = m_definition.initialLevel;
    m_anchor.index = {};
    m_hover.cell.reset();

    if (isCellNavigable(m_anchor.index, m_anchor.level))
        m_selection.cell = anchorCell();
    else
        m_selection.cell.reset();
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

void CubicNavigationGrid::setEnabled(bool enabledValue)
{
    m_enabled = enabledValue;
    if (!m_enabled)
        m_hover.cell.reset();
}

void CubicNavigationGrid::toggleEnabled()
{
    setEnabled(!m_enabled);
}

int CubicNavigationGrid::level() const
{
    return m_anchor.level;
}

int CubicNavigationGrid::minimumLevel() const
{
    return m_definition.minimumLevel;
}

int CubicNavigationGrid::initialLevel() const
{
    return m_definition.initialLevel;
}

int CubicNavigationGrid::maximumLevel() const
{
    return m_definition.maximumLevel;
}

int CubicNavigationGrid::subdivision() const
{
    return m_definition.subdivision;
}

double CubicNavigationGrid::cellSize() const
{
    return cellSize(m_anchor.level);
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

const CubicNavigationAnchorState&
CubicNavigationGrid::anchorState() const
{
    return m_anchor;
}

const CubicNavigationHoverState&
CubicNavigationGrid::hoverState() const
{
    return m_hover;
}

const CubicNavigationSelectionState&
CubicNavigationGrid::selectionState() const
{
    return m_selection;
}

const CubicGridIndex& CubicNavigationGrid::anchorIndex() const
{
    return m_anchor.index;
}

CubicNavigationCell CubicNavigationGrid::anchorCell() const
{
    return cell(m_anchor.index, m_anchor.level);
}

void CubicNavigationGrid::setAnchorIndex(
    const CubicGridIndex& index
)
{
    if (isCellNavigable(index, m_anchor.level))
        m_anchor.index = index;
}

void CubicNavigationGrid::setAnchorFromPosition(
    const glm::dvec3& position
)
{
    setAnchorIndex(
        nearestIndexForPosition(position, m_anchor.level)
    );
}

bool CubicNavigationGrid::hasHoveredCell() const
{
    return m_hover.cell.has_value();
}

const CubicNavigationCell& CubicNavigationGrid::hoveredCell() const
{
    return m_hover.cell.value();
}

void CubicNavigationGrid::setHoveredCell(
    const CubicNavigationCell& cellValue
)
{
    const CubicNavigationCell normalized =
        cell(cellValue.index, cellValue.level);

    if (isCellNavigable(normalized))
        m_hover.cell = normalized;
    else
        m_hover.cell.reset();
}

void CubicNavigationGrid::clearHoveredCell()
{
    m_hover.cell.reset();
}

bool CubicNavigationGrid::hasSelectedCell() const
{
    return m_selection.cell.has_value();
}

const CubicNavigationCell& CubicNavigationGrid::selectedCell() const
{
    return m_selection.cell.value();
}

void CubicNavigationGrid::selectCell(
    const CubicNavigationCell& cellValue
)
{
    const CubicNavigationCell normalized =
        cell(cellValue.index, cellValue.level);

    if (!isCellNavigable(normalized))
        return;

    /*
        Selection is an address/route target. It does not change the view
        anchor or the current hierarchy level.
    */
    m_selection.cell = normalized;
}

void CubicNavigationGrid::clearSelectedCell()
{
    m_selection.cell.reset();
}

bool CubicNavigationGrid::canRefine() const
{
    return m_anchor.level < m_definition.maximumLevel;
}

bool CubicNavigationGrid::canCoarsen() const
{
    return m_anchor.level > m_definition.minimumLevel;
}

bool CubicNavigationGrid::refineAroundAnchor()
{
    if (!canRefine())
        return false;

    CubicGridIndex refined = m_anchor.index;
    refined.x *= m_definition.subdivision;
    refined.y *= m_definition.subdivision;
    refined.z *= m_definition.subdivision;

    const int refinedLevel = m_anchor.level + 1;
    if (!isCellNavigable(refined, refinedLevel))
        return false;

    m_anchor.index = refined;
    m_anchor.level = refinedLevel;
    m_hover.cell.reset();
    return true;
}

bool CubicNavigationGrid::coarsenAroundAnchor()
{
    if (!canCoarsen())
        return false;

    const CubicGridIndex coarsened = parentIndex(m_anchor.index);
    const int coarsenedLevel = m_anchor.level - 1;

    if (!isCellNavigable(coarsened, coarsenedLevel))
        return false;

    m_anchor.index = coarsened;
    m_anchor.level = coarsenedLevel;
    m_hover.cell.reset();
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

CubicGridIndex CubicNavigationGrid::parentIndex(
    const CubicGridIndex& child
) const
{
    return {
        nearestParentIndex(child.x),
        nearestParentIndex(child.y),
        nearestParentIndex(child.z)
    };
}

CubicGridIndex CubicNavigationGrid::ancestorIndex(
    const CubicGridIndex& index,
    int levelValue,
    int ancestorLevel
) const
{
    CubicGridIndex result = index;

    for (int levelCursor = levelValue;
         levelCursor > ancestorLevel;
         --levelCursor)
    {
        result = parentIndex(result);
    }

    return result;
}

bool CubicNavigationGrid::isCellNavigable(
    const CubicGridIndex& index,
    int levelValue
) const
{
    return !m_policy || m_policy->isCellNavigable(index, levelValue);
}

bool CubicNavigationGrid::isCellNavigable(
    const CubicNavigationCell& cellValue
) const
{
    return isCellNavigable(cellValue.index, cellValue.level);
}

std::vector<CubicNavigationCell> CubicNavigationGrid::neighborhood(
    int radius
) const
{
    radius = std::max(0, radius);
    const int side = radius * 2 + 1;

    std::vector<CubicNavigationCell> result;
    result.reserve(static_cast<std::size_t>(side * side * side));

    for (int dz = -radius; dz <= radius; ++dz)
    {
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                CubicGridIndex index;
                index.x = m_anchor.index.x + dx;
                index.y = m_anchor.index.y + dy;
                index.z = m_anchor.index.z + dz;

                if (isCellNavigable(index, m_anchor.level))
                    result.push_back(cell(index, m_anchor.level));
            }
        }
    }

    return result;
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

void CubicNavigationGrid::normalizeStateForPolicy()
{
    if (!isCellNavigable(m_anchor.index, m_anchor.level))
    {
        const CubicGridIndex origin {};
        if (isCellNavigable(origin, m_anchor.level))
            m_anchor.index = origin;
    }

    if (m_hover.cell && !isCellNavigable(*m_hover.cell))
        m_hover.cell.reset();

    if (m_selection.cell && !isCellNavigable(*m_selection.cell))
        m_selection.cell.reset();
}

} // namespace game::navigation
