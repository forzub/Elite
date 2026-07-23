#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GalaxyNavigationConfig.h"

namespace game::navigation
{

/*
    Heliocentric equatorial Cartesian frame, epoch/equinox J2000.

    Origin:
        Sol.

    Axes:
        +X -> vernal equinox, RA 0h, Dec 0 deg;
        +Y -> RA 6h, Dec 0 deg;
        +Z -> north celestial pole, Dec +90 deg.

    The frame is right-handed:
        X cross Y = Z.

    Units used by the galaxy navigation grid:
        light years.

    These are the same coordinates already stored in:
        assets/data/galaxy/star_systems.json
*/
struct GalaxyNavigationFrame
{
    std::string id = "sol_equatorial_j2000";

    glm::dvec3 originLy {0.0, 0.0, 0.0};

    glm::dvec3 axisX {1.0, 0.0, 0.0};
    glm::dvec3 axisY {0.0, 1.0, 0.0};
    glm::dvec3 axisZ {0.0, 0.0, 1.0};
};

struct GalaxyGridIndex
{
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const GalaxyGridIndex& other) const
    {
        return
            x == other.x &&
            y == other.y &&
            z == other.z;
    }

    bool operator!=(const GalaxyGridIndex& other) const
    {
        return !(*this == other);
    }
};

struct GalaxyNavigationCell
{
    GalaxyGridIndex index;

    int level = 0;

    double sizeLy = 0.0;
    glm::dvec3 centerLy {0.0, 0.0, 0.0};
};

/*
    Mathematical/state layer of the galaxy cubic navigation grid.

    This class knows nothing about OpenGL, the camera, pixels or mouse input.
    It only owns the fixed grid address, hierarchy and coordinate conversion.
*/
class GalaxyNavigationGrid
{
public:
    GalaxyNavigationGrid();

    void reset();

    bool enabled() const;
    void setEnabled(bool enabled);
    void toggleEnabled();

    const GalaxyNavigationFrame& frame() const;
    const GalaxyNavigationConfig& config() const;

    int subdivision() const;
    int minimumLevel() const;
    int initialLevel() const;
    int maximumLevel() const;

    int level() const;
    double cellSizeLy() const;
    double cellSizeLy(int level) const;

    int displayRadius() const;
    void setDisplayRadius(int radius);

    const GalaxyGridIndex& anchorIndex() const;
    GalaxyNavigationCell anchorCell() const;

    void setAnchorIndex(const GalaxyGridIndex& index);
    void setAnchorFromPositionLy(const glm::dvec3& positionLy);

    bool hasHoveredCell() const;
    const GalaxyNavigationCell& hoveredCell() const;
    void setHoveredCell(const GalaxyNavigationCell& cell);
    void clearHoveredCell();

    bool hasSelectedCell() const;
    const GalaxyNavigationCell& selectedCell() const;
    void selectCell(const GalaxyNavigationCell& cell);
    void clearSelectedCell();

    bool canRefine() const;
    bool canCoarsen() const;

    /*
        Refinement changes only view resolution and view anchor.
        Explicit selection is independent and keeps its address.

        The caller may re-anchor to an arbitrary navigable position
        after the level changes.
    */
    bool refineAroundAnchor();
    bool coarsenAroundAnchor();

    GalaxyNavigationCell cell(
        const GalaxyGridIndex& index,
        int level
    ) const;

    glm::dvec3 cellCenterLy(
        const GalaxyGridIndex& index,
        int level
    ) const;

    GalaxyGridIndex nearestIndexForPositionLy(
        const glm::dvec3& positionLy,
        int level
    ) const;

    GalaxyGridIndex rootIndexForCell(
        const GalaxyGridIndex& index,
        int level
    ) const;

    bool isCellNavigable(
        const GalaxyGridIndex& index,
        int level
    ) const;

    bool isCellNavigable(
        const GalaxyNavigationCell& cell
    ) const;

    std::vector<GalaxyNavigationCell> neighborhood() const;

private:
    std::int64_t nearestParentIndex(std::int64_t childIndex) const;

private:
    GalaxyNavigationFrame m_frame;
    GalaxyNavigationConfig m_config;

    bool m_enabled = true;

    int m_level = 1;
    int m_displayRadius = 1;

    GalaxyGridIndex m_anchorIndex;

    std::optional<GalaxyNavigationCell> m_hoveredCell;
    std::optional<GalaxyNavigationCell> m_selectedCell;
};

} // namespace game::navigation
