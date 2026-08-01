#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

/*
    Optional domain policy for a cubic grid.

    System navigation uses the unrestricted default policy. Galaxy navigation
    supplies a root-cell whitelist policy. The core owns hierarchy and state;
    the policy only decides whether a cell address belongs to the domain.
*/
class CubicNavigationPolicy
{
public:
    virtual ~CubicNavigationPolicy() = default;

    virtual bool isCellNavigable(
        const CubicGridIndex& index,
        int level
    ) const = 0;
};

/*
    Persistent navigation states are deliberately independent:

    - anchor controls the rendered neighborhood and level transitions;
    - hover is transient cursor feedback;
    - selection is an explicit route/address target.
*/
struct CubicNavigationAnchorState
{
    int level = 0;
    CubicGridIndex index;
};

struct CubicNavigationHoverState
{
    std::optional<CubicNavigationCell> cell;
};

struct CubicNavigationSelectionState
{
    std::optional<CubicNavigationCell> cell;
};

class CubicNavigationGrid
{
public:
    explicit CubicNavigationGrid(
        const CubicNavigationGridDefinition& definition,
        std::shared_ptr<const CubicNavigationPolicy> policy = {}
    );

    virtual ~CubicNavigationGrid() = default;

    void configure(
        const CubicNavigationGridDefinition& definition
    );

    void setPolicy(
        std::shared_ptr<const CubicNavigationPolicy> policy
    );

    virtual void reset();

    const CubicNavigationGridDefinition& definition() const;
    const CubicNavigationFrame& frame() const;

    bool enabled() const;
    void setEnabled(bool enabled);
    void toggleEnabled();

    int level() const;
    int minimumLevel() const;
    int initialLevel() const;
    int maximumLevel() const;
    int subdivision() const;
    double cellSize() const;
    double cellSize(int level) const;

    const CubicNavigationAnchorState& anchorState() const;
    const CubicNavigationHoverState& hoverState() const;
    const CubicNavigationSelectionState& selectionState() const;

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

    CubicGridIndex parentIndex(
        const CubicGridIndex& child
    ) const;

    CubicGridIndex ancestorIndex(
        const CubicGridIndex& index,
        int level,
        int ancestorLevel
    ) const;

    bool isCellNavigable(
        const CubicGridIndex& index,
        int level
    ) const;

    bool isCellNavigable(
        const CubicNavigationCell& cell
    ) const;

    std::vector<CubicNavigationCell> neighborhood(
        int radius
    ) const;

protected:
    void validateDefinition() const;

private:
    std::int64_t nearestParentIndex(std::int64_t child) const;
    void normalizeStateForPolicy();

private:
    CubicNavigationGridDefinition m_definition;
    std::shared_ptr<const CubicNavigationPolicy> m_policy;

    bool m_enabled = true;
    CubicNavigationAnchorState m_anchor;
    CubicNavigationHoverState m_hover;
    CubicNavigationSelectionState m_selection;
};

} // namespace game::navigation
