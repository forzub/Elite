#pragma once

#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GalaxyNavigationGrid.h"

namespace game::system_map
{
    /*
        Galaxy Map spatial frame: the user is always INSIDE one current
        parent cube. The parent is the coordinate frame of the working level,
        its children (level + 1) are the cubes the user hovers, selects and
        enters. Level -1 is the virtual galaxy overview: a cube aligned to the
        root grid that contains every navigable root cell (it is not a
        navigation cell and has no address; its children are the roots).

        The navigation address space (levels, indices, 25 per axis) is not
        changed by this frame; it only decides what is presented and which
        cells are pointer targets.
    */
    struct GalaxyHierarchyParent
    {
        int level = -1;                     // -1: galaxy overview
        navigation::GalaxyGridIndex index;  // level >= 0 only

        bool overview() const { return level < 0; }
        bool operator==(const GalaxyHierarchyParent& other) const
        {
            return level == other.level && (level < 0 || index == other.index);
        }
        bool operator!=(const GalaxyHierarchyParent& other) const { return !(*this == other); }
    };

    // The parent as a box in light-years with its child lattice.
    struct GalaxyHierarchyBox
    {
        GalaxyHierarchyParent parent;
        glm::dvec3 centerLy {0.0};
        double edgeLy = 1.0;
        int childLevel = 0;
        double childEdgeLy = 1.0;
        int divisions = 25;                 // children per axis
        // Index of the first child (lowest corner) per axis.
        navigation::GalaxyGridIndex firstChild;
    };

    /*
        overviewRoots: the roots the galaxy overview frames (those holding star
        systems and the player). The navigation domain is much larger (a
        140-ly envelope of empty navigable space): framing all of it pushed
        the star systems into a dot in the middle. Empty: every navigable
        root.
    */
    GalaxyHierarchyBox galaxyHierarchyBox(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyParent& parent,
        const std::vector<navigation::GalaxyGridIndex>* overviewRoots = nullptr
    );

    // The parent of a parent (overview for a root, nothing above overview).
    std::optional<GalaxyHierarchyParent> galaxyHierarchyAscend(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyParent& parent
    );

    // The parent at `level` (-1 .. cell level) that contains `cell`.
    GalaxyHierarchyParent galaxyHierarchyAncestor(
        const navigation::GalaxyNavigationGrid& grid,
        const navigation::GalaxyNavigationCell& cell,
        int level
    );

    // Chain overview .. parent (inclusive) for breadcrumbs.
    std::vector<GalaxyHierarchyParent> galaxyHierarchyChain(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyParent& parent
    );

    bool galaxyHierarchyContains(
        const GalaxyHierarchyBox& box,
        const glm::dvec3& positionLy
    );

    bool galaxyHierarchyIntersectsSphere(
        const GalaxyHierarchyBox& box,
        const glm::dvec3& centreLy,
        double radiusLy
    );

    bool galaxyHierarchyIsChild(
        const GalaxyHierarchyBox& box,
        const navigation::GalaxyNavigationCell& cell
    );

    // The navigable child of the box containing the position, if any.
    std::optional<navigation::GalaxyNavigationCell> galaxyHierarchyChildAt(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyBox& box,
        const glm::dvec3& positionLy
    );

    // Child offset inside the parent: 0 .. divisions-1 per axis.
    glm::ivec3 galaxyHierarchyChildOffset(
        const GalaxyHierarchyBox& box,
        const navigation::GalaxyGridIndex& childIndex
    );

    /*
        Camera distance that shows the whole parent cube from ANY orbit
        angle: its circumscribed sphere fits the narrower field of view with
        `margin` (> 1) to the viewport edge. Perspective never cuts an edge.
    */
    float galaxyHierarchyFitDistance(
        double edgeRender,
        float verticalFovRad,
        int viewportWidth,
        int viewportHeight,
        float margin = 1.06f
    );

    /*
        Inner coordinate faces of the parent the camera looks at from inside:
        a horizontal face (floor when the camera is above the centre, ceiling
        below) and the far wall across X or Z that faces the camera most.
        Hysteresis keeps the choice stable while the camera orbits; axes never
        flip on their own.
    */
    struct GalaxyHierarchyFaces
    {
        int horizontalSign = -1;   // -1 floor (y min), +1 ceiling (y max)
        int wallAxis = 2;          // 0: wall across X (x = const), 2: across Z
        int wallSign = -1;         // which side of wallAxis
        bool valid = false;
    };

    GalaxyHierarchyFaces galaxyHierarchyChooseFaces(
        const GalaxyHierarchyFaces& previous,
        const glm::dvec3& cameraEyeLy,
        const glm::dvec3& viewDirection, // from the eye into the scene
        const GalaxyHierarchyBox& box
    );

    /*
        Hover layer for empty space: the child layer perpendicular to the
        axis most aligned with the view, through `throughLy` (the selected
        child, else the parent centre). The cursor ray meets this layer in
        one child: deterministic, axis aligned, drawn while hovered.
    */
    struct GalaxyHierarchyLayer
    {
        int axis = 1;
        double coordinateLy = 0.0;  // plane through the layer's child centres
        bool valid = false;
    };

    GalaxyHierarchyLayer galaxyHierarchyLayer(
        const GalaxyHierarchyBox& box,
        const glm::dvec3& viewDirection,
        const glm::dvec3& throughLy,
        int previousAxis
    );
}
