#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

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
    static constexpr int Subdivision = 3;

    static constexpr int MinimumLevel = 0;
    static constexpr int InitialLevel = 1;
    /*
        Последний уровень, отображаемый на карте Galaxy.

        L9:
            27 ly / 3^9
            ≈ 0.001371742 ly
            ≈ 86.75 AU

        Дальнейшее приближение выполняется уже в карте System.
    */
    static constexpr int MaximumLevel = 9;

    /*
        Level 0 cube edge.

        27 ly gives the following hierarchy:
            L0 = 27 ly
            L1 =  9 ly
            L2 =  3 ly
            L3 =  1 ly
            L4 =  0.333... ly

        The galaxy map is not intended to refine all the way to kilometres.
        A selected empty interstellar region will later create its own
        DeepSpaceSearchFrame with metre-based cells.
    */
    static constexpr double BaseCellSizeLy = 27.0;

public:
    GalaxyNavigationGrid();

    void reset();

    bool enabled() const;
    void setEnabled(bool enabled);
    void toggleEnabled();

    const GalaxyNavigationFrame& frame() const;

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
        Refinement keeps the same physical centre.

        Parent index (i,j,k) at level L becomes central child
        index (3i,3j,3k) at level L+1.
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

    std::vector<GalaxyNavigationCell> neighborhood() const;

private:
    static std::int64_t nearestParentIndex(std::int64_t childIndex);

private:
    GalaxyNavigationFrame m_frame;

    bool m_enabled = true;

    int m_level = MinimumLevel;
    int m_displayRadius = 1;

    GalaxyGridIndex m_anchorIndex;

    std::optional<GalaxyNavigationCell> m_hoveredCell;
    std::optional<GalaxyNavigationCell> m_selectedCell;
};

} // namespace game::navigation
