#include "src/game/system_map/GalaxyHierarchy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::system_map
{
namespace
{
    double axisValue(const glm::dvec3& v, int axis)
    {
        return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
    }

    std::int64_t indexValue(const navigation::GalaxyGridIndex& i, int axis)
    {
        return axis == 0 ? i.x : (axis == 1 ? i.y : i.z);
    }
}

    GalaxyHierarchyBox galaxyHierarchyBox(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyParent& parent,
        const std::vector<navigation::GalaxyGridIndex>* overviewRoots
    )
    {
        GalaxyHierarchyBox box;
        box.parent = parent;

        if (!parent.overview())
        {
            const int division = grid.subdivision();
            const std::int64_t half = division / 2;
            box.centerLy = grid.cellCenterLy(parent.index, parent.level);
            box.edgeLy = grid.cellSizeLy(parent.level);
            box.childLevel = parent.level + 1;
            box.childEdgeLy = grid.cellSizeLy(box.childLevel);
            box.divisions = division;
            box.firstChild = {
                parent.index.x * division - half,
                parent.index.y * division - half,
                parent.index.z * division - half
            };
            return box;
        }

        // Overview: the cube on the root lattice around the framed roots.
        std::vector<navigation::GalaxyGridIndex> roots;
        if (overviewRoots && !overviewRoots->empty())
            roots = *overviewRoots;
        else
            for (const auto& root : grid.navigableRootCells())
                roots.push_back({root[0], root[1], root[2]});
        std::int64_t lo[3] = {0, 0, 0};
        std::int64_t hi[3] = {0, 0, 0};
        if (!roots.empty())
        {
            for (int a = 0; a < 3; ++a)
            {
                lo[a] = std::numeric_limits<std::int64_t>::max();
                hi[a] = std::numeric_limits<std::int64_t>::min();
            }
            for (const auto& root : roots)
                for (int a = 0; a < 3; ++a)
                {
                    lo[a] = std::min(lo[a], indexValue(root, a));
                    hi[a] = std::max(hi[a], indexValue(root, a));
                }
        }
        std::int64_t divisions = 1;
        for (int a = 0; a < 3; ++a)
            divisions = std::max(divisions, hi[a] - lo[a] + 1);
        std::int64_t first[3];
        for (int a = 0; a < 3; ++a)
            first[a] = lo[a] - (divisions - (hi[a] - lo[a] + 1)) / 2;

        const double rootEdge = grid.cellSizeLy(0);
        box.edgeLy = rootEdge * static_cast<double>(divisions);
        box.childLevel = 0;
        box.childEdgeLy = rootEdge;
        box.divisions = static_cast<int>(divisions);
        box.firstChild = {first[0], first[1], first[2]};
        const double centreOffset = 0.5 * static_cast<double>(divisions - 1);
        box.centerLy = grid.cellCenterLy({0, 0, 0}, 0) + glm::dvec3(
            (static_cast<double>(first[0]) + centreOffset) * rootEdge,
            (static_cast<double>(first[1]) + centreOffset) * rootEdge,
            (static_cast<double>(first[2]) + centreOffset) * rootEdge);
        return box;
    }

    std::optional<GalaxyHierarchyParent> galaxyHierarchyAscend(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyParent& parent
    )
    {
        if (parent.overview())
            return std::nullopt;
        GalaxyHierarchyParent up;
        if (parent.level == 0)
            return up; // overview
        up.level = parent.level - 1;
        up.index = grid.parentIndex(parent.index);
        return up;
    }

    GalaxyHierarchyParent galaxyHierarchyAncestor(
        const navigation::GalaxyNavigationGrid& grid,
        const navigation::GalaxyNavigationCell& cell,
        int level
    )
    {
        GalaxyHierarchyParent result;
        if (level < 0)
            return result;
        result.level = std::min(level, cell.level);
        result.index = grid.ancestorIndex(cell.index, cell.level, result.level);
        return result;
    }

    std::vector<GalaxyHierarchyParent> galaxyHierarchyChain(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyParent& parent
    )
    {
        std::vector<GalaxyHierarchyParent> chain;
        std::optional<GalaxyHierarchyParent> cursor = parent;
        while (cursor)
        {
            chain.push_back(*cursor);
            cursor = galaxyHierarchyAscend(grid, *cursor);
        }
        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    bool galaxyHierarchyContains(
        const GalaxyHierarchyBox& box,
        const glm::dvec3& positionLy
    )
    {
        const glm::dvec3 d = glm::abs(positionLy - box.centerLy);
        const double half = box.edgeLy * 0.5 * (1.0 + 1.0e-9);
        return d.x <= half && d.y <= half && d.z <= half;
    }

    bool galaxyHierarchyIntersectsSphere(
        const GalaxyHierarchyBox& box,
        const glm::dvec3& centreLy,
        double radiusLy
    )
    {
        const double half = box.edgeLy * 0.5;
        const glm::dvec3 d = glm::max(glm::abs(centreLy - box.centerLy) - glm::dvec3(half), glm::dvec3(0.0));
        return glm::dot(d, d) <= radiusLy * radiusLy;
    }

    bool galaxyHierarchyIsChild(
        const GalaxyHierarchyBox& box,
        const navigation::GalaxyNavigationCell& cell
    )
    {
        if (cell.level != box.childLevel)
            return false;
        for (int a = 0; a < 3; ++a)
        {
            const std::int64_t offset = indexValue(cell.index, a) - indexValue(box.firstChild, a);
            if (offset < 0 || offset >= box.divisions)
                return false;
        }
        return true;
    }

    std::optional<navigation::GalaxyNavigationCell> galaxyHierarchyChildAt(
        const navigation::GalaxyNavigationGrid& grid,
        const GalaxyHierarchyBox& box,
        const glm::dvec3& positionLy
    )
    {
        const auto index = grid.nearestIndexForPositionLy(positionLy, box.childLevel);
        const auto cell = grid.cell(index, box.childLevel);
        if (!galaxyHierarchyIsChild(box, cell) || !grid.isCellNavigable(cell))
            return std::nullopt;
        return cell;
    }

    glm::ivec3 galaxyHierarchyChildOffset(
        const GalaxyHierarchyBox& box,
        const navigation::GalaxyGridIndex& childIndex
    )
    {
        return glm::ivec3(
            static_cast<int>(childIndex.x - box.firstChild.x),
            static_cast<int>(childIndex.y - box.firstChild.y),
            static_cast<int>(childIndex.z - box.firstChild.z));
    }

    float galaxyHierarchyFitDistance(
        double edgeRender,
        float verticalFovRad,
        int viewportWidth,
        int viewportHeight,
        float margin
    )
    {
        const double aspect =
            viewportHeight > 0
                ? static_cast<double>(std::max(viewportWidth, 1)) / static_cast<double>(viewportHeight)
                : 1.0;
        const double halfY = 0.5 * static_cast<double>(verticalFovRad);
        const double halfX = std::atan(std::tan(halfY) * aspect);
        const double half = std::max(1.0e-3, std::min(halfX, halfY));
        const double sphereRadius = std::max(edgeRender, 1.0e-9) * 0.5 * std::sqrt(3.0);
        return static_cast<float>(sphereRadius * static_cast<double>(margin) / std::sin(half));
    }

    GalaxyHierarchyFaces galaxyHierarchyChooseFaces(
        const GalaxyHierarchyFaces& previous,
        const glm::dvec3& cameraEyeLy,
        const glm::dvec3& viewDirection,
        const GalaxyHierarchyBox& box
    )
    {
        GalaxyHierarchyFaces faces = previous;
        // Dead band ~ +-3 deg of pitch at the fitted distance: an orbit near
        // the horizon never makes floor and ceiling alternate.
        const glm::dvec3 fromCentre = cameraEyeLy - box.centerLy;
        const double deadBand = 0.10 * box.edgeLy;

        // Horizontal face: the one on the far side of the camera.
        if (!previous.valid || std::abs(fromCentre.y) > deadBand)
            faces.horizontalSign = fromCentre.y >= 0.0 ? -1 : 1;

        // Far wall across X or Z: the one the view looks into most.
        const double scoreX = std::abs(viewDirection.x);
        const double scoreZ = std::abs(viewDirection.z);
        if (!previous.valid)
            faces.wallAxis = scoreX > scoreZ ? 0 : 2;
        else if (previous.wallAxis == 0 && scoreZ > scoreX + 0.15)
            faces.wallAxis = 2;
        else if (previous.wallAxis == 2 && scoreX > scoreZ + 0.15)
            faces.wallAxis = 0;
        const double along = axisValue(fromCentre, faces.wallAxis);
        if (!previous.valid || faces.wallAxis != previous.wallAxis || std::abs(along) > deadBand)
            faces.wallSign = along >= 0.0 ? -1 : 1;

        faces.valid = true;
        return faces;
    }

    GalaxyHierarchyLayer galaxyHierarchyLayer(
        const GalaxyHierarchyBox& box,
        const glm::dvec3& viewDirection,
        const glm::dvec3& throughLy,
        int previousAxis
    )
    {
        GalaxyHierarchyLayer layer;
        const glm::dvec3 score = glm::abs(viewDirection);
        int axis = 0;
        for (int a = 1; a < 3; ++a)
            if (axisValue(score, a) > axisValue(score, axis))
                axis = a;
        if (previousAxis >= 0 && previousAxis <= 2 &&
            axisValue(score, axis) < axisValue(score, previousAxis) + 0.10)
        {
            axis = previousAxis;
        }
        layer.axis = axis;

        // Centre of the child layer containing `throughLy` (clamped inside).
        const double first = static_cast<double>(indexValue(box.firstChild, axis));
        const double last = first + static_cast<double>(box.divisions - 1);
        const double index = std::clamp(
            std::round(axisValue(throughLy, axis) / box.childEdgeLy), first, last);
        layer.coordinateLy = index * box.childEdgeLy;
        layer.valid = true;
        return layer;
    }
}
