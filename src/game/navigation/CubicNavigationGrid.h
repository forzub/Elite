#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <glm/glm.hpp>

namespace game::navigation
{

struct CubicGridIndex
{
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const CubicGridIndex& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const CubicGridIndex& other) const
    {
        return !(*this == other);
    }
};

struct CubicNavigationFrame
{
    std::string id;
    std::string unit;

    glm::dvec3 origin {0.0, 0.0, 0.0};
    glm::dvec3 axisX {1.0, 0.0, 0.0};
    glm::dvec3 axisY {0.0, 1.0, 0.0};
    glm::dvec3 axisZ {0.0, 0.0, 1.0};
};

struct CubicNavigationCell
{
    CubicGridIndex index;
    int level = 0;
    double size = 0.0;
    glm::dvec3 center {0.0, 0.0, 0.0};
};

struct CubicNavigationGridDefinition
{
    CubicNavigationFrame frame;

    int subdivision = 3;
    int minimumLevel = 0;
    int initialLevel = 0;
    int maximumLevel = 0;

    double baseCellSize = 1.0;
};

class CubicNavigationGrid
{
public:
    explicit CubicNavigationGrid(
        const CubicNavigationGridDefinition& definition
    );

    void configure(
        const CubicNavigationGridDefinition& definition
    );

    void reset();

    const CubicNavigationGridDefinition& definition() const;
    const CubicNavigationFrame& frame() const;

    bool enabled() const;
    void setEnabled(bool enabled);

    int level() const;
    int subdivision() const;
    double cellSize() const;
    double cellSize(int level) const;

    const CubicGridIndex& anchorIndex() const;
    CubicNavigationCell anchorCell() const;

    void setAnchorIndex(const CubicGridIndex& index);
    void setAnchorFromPosition(const glm::dvec3& position);

    bool hasHoveredCell() const;
    const CubicNavigationCell& hoveredCell() const;
    void setHoveredCell(const CubicNavigationCell& cell);
    void clearHoveredCell();

    bool hasSelectedCell() const;
    const CubicNavigationCell& selectedCell() const;
    void selectCell(const CubicNavigationCell& cell);
    void clearSelectedCell();

    bool canRefine() const;
    bool canCoarsen() const;
    bool refineAroundAnchor();
    bool coarsenAroundAnchor();

    CubicNavigationCell cell(
        const CubicGridIndex& index,
        int level
    ) const;

    glm::dvec3 cellCenter(
        const CubicGridIndex& index,
        int level
    ) const;

    CubicGridIndex nearestIndexForPosition(
        const glm::dvec3& position,
        int level
    ) const;

private:
    std::int64_t nearestParentIndex(std::int64_t child) const;
    void validateDefinition() const;

private:
    CubicNavigationGridDefinition m_definition;
    bool m_enabled = true;
    int m_level = 0;
    CubicGridIndex m_anchorIndex;
    std::optional<CubicNavigationCell> m_hoveredCell;
    std::optional<CubicNavigationCell> m_selectedCell;
};

} // namespace game::navigation
