#pragma once

// THE GUARANTEED ROUTE (author 2026-11-28: «нет хороших вариантов - пусть делает плохой, но маршрут обязан быть построен
// всегда. исключение - если корабль находится в полом кубе, откуда нет выхода или выход меньше габаритов корабля»).
//
// When the optimizer-based planner fails, this one is the last strategy: a resolution-complete search over voxels.
//   1. ESCAPE: a fine grid (cell ~ half the hull radius) around the start - from where the ship is (on a pad, in a
//      hangar, touching a wall) to the first cell with open room (free in the coarse grid) or the fine grid's border.
//      A start that touches its boxes searches from the nearest free cell (the ship is lifted / led out to it).
//   2. TRANSIT: a coarse grid over start .. goal (+ margin) - A* from the escape point to the goal (or the free cell
//      nearest it), 26-neighbour, euclidean heuristic.
//   3. The cell path is shortened by line of sight; EVERY polyline segment is then checked against the exact obstacle
//      geometry with the hull radius + clearance (the cells only propose; a segment that fails keeps its cell steps).
// Occupancy is conservative: a cell is blocked when its centre is inside an obstacle inflated by the hull radius +
// clearance + the cell's half diagonal, so a free cell is free everywhere in it.
// ENCLOSED (the only refusal): the escape search exhausts its fine grid without reaching open room - no exit, or every
// exit narrower than the hull (the fine cell is a fraction of the hull radius).
// Pure: no globals, no I/O; deterministic for the same input.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation::planner
{
struct VoxelRouteRequest
{
    glm::dvec3 startMeters {0.0};
    glm::dvec3 goalMeters {0.0};
    const std::vector<world::navigation::NavigationObstacle>* obstacles = nullptr;
    double agentRadiusMeters = 10.0;
    double clearanceMeters = 5.0;
    int maxCellsPerAxis = 160;           // the transit grid
    double escapeHalfSizeMeters = 400.0; // the escape grid's half size
    std::size_t maxExpansions = 4000000;
};

enum class VoxelRouteOutcome
{
    Found,
    Enclosed,   // no way out of the start's surroundings for this hull
    NoPath,     // out of the start's surroundings, but no way to the goal inside the transit grid (should not happen in space)
    Invalid
};

struct VoxelRouteResult
{
    VoxelRouteOutcome outcome = VoxelRouteOutcome::Invalid;
    std::vector<glm::dvec3> points;   // start, ..., goal (or the free point nearest the goal)
    std::size_t escapePoints = 0;     // points[0 .. escapePoints) lead the ship out of where it stands (no turn-first)
    std::string note;
    [[nodiscard]] bool found() const noexcept { return outcome == VoxelRouteOutcome::Found; }
};

namespace voxel_detail
{
struct Grid
{
    glm::dvec3 origin {0.0};   // the centre of cell (0,0,0)
    double cell = 10.0;
    glm::ivec3 n {0};
    std::vector<std::uint8_t> blocked;
    [[nodiscard]] std::size_t index(const glm::ivec3& c) const
    {
        return (static_cast<std::size_t>(c.z) * static_cast<std::size_t>(n.y) + static_cast<std::size_t>(c.y)) *
                   static_cast<std::size_t>(n.x) + static_cast<std::size_t>(c.x);
    }
    [[nodiscard]] bool inside(const glm::ivec3& c) const
    {
        return c.x >= 0 && c.y >= 0 && c.z >= 0 && c.x < n.x && c.y < n.y && c.z < n.z;
    }
    [[nodiscard]] glm::dvec3 centre(const glm::ivec3& c) const { return origin + glm::dvec3(c) * cell; }
    [[nodiscard]] glm::ivec3 cellOf(const glm::dvec3& p) const
    {
        const glm::dvec3 q = (p - origin) / cell;
        return glm::ivec3(static_cast<int>(std::lround(q.x)), static_cast<int>(std::lround(q.y)), static_cast<int>(std::lround(q.z)));
    }
    [[nodiscard]] bool free(const glm::ivec3& c) const { return inside(c) && !blocked[index(c)]; }
};

// a grid over [lo, hi] with the given cell, its blocked cells rasterized from the obstacles' inflated footprints
inline Grid buildGrid(const glm::dvec3& lo, const glm::dvec3& hi, double cell,
                      const std::vector<world::navigation::NavigationObstacle>& obstacles, double inflation)
{
    Grid g;
    g.cell = cell;
    g.origin = lo;
    g.n = glm::max(glm::ivec3(1), glm::ivec3(glm::ceil((hi - lo) / cell)) + glm::ivec3(1));
    g.blocked.assign(static_cast<std::size_t>(g.n.x) * g.n.y * g.n.z, 0);
    const double cellInflation = inflation + 0.8660254037844386 * cell;   // + the half diagonal: free everywhere in a free cell
    for (const auto& o : obstacles)
    {
        const double r = o.conservativeRadiusMeters() + world::navigation::navigationObstacleInflationMeters(o, cellInflation) + 1.0;
        const glm::ivec3 a = glm::max(glm::ivec3(0), g.cellOf(o.centerMeters - glm::dvec3(r)));
        const glm::ivec3 b = glm::min(g.n - glm::ivec3(1), g.cellOf(o.centerMeters + glm::dvec3(r)));
        for (int z = a.z; z <= b.z; ++z)
            for (int y = a.y; y <= b.y; ++y)
                for (int x = a.x; x <= b.x; ++x)
                {
                    const glm::ivec3 c(x, y, z);
                    auto& cellBlocked = g.blocked[g.index(c)];
                    if (!cellBlocked && world::navigation::pointInsideNavigationObstacle(g.centre(c), o, cellInflation))
                        cellBlocked = 1;
                }
    }
    return g;
}

inline const std::array<glm::ivec3, 26>& neighbours()
{
    static const std::array<glm::ivec3, 26> n = [] {
        std::array<glm::ivec3, 26> out {};
        int k = 0;
        for (int z = -1; z <= 1; ++z)
            for (int y = -1; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                    if (x || y || z)
                        out[static_cast<std::size_t>(k++)] = glm::ivec3(x, y, z);
        return out;
    }();
    return n;
}

// the free cell nearest c (breadth-first, at most `radius` cells away); false: none
inline bool nearestFree(const Grid& g, const glm::ivec3& c, int radius, glm::ivec3& out)
{
    if (g.free(c))
    {
        out = c;
        return true;
    }
    double best = std::numeric_limits<double>::infinity();
    for (int r = 1; r <= radius; ++r)
    {
        for (int z = -r; z <= r; ++z)
            for (int y = -r; y <= r; ++y)
                for (int x = -r; x <= r; ++x)
                {
                    if (std::max({std::abs(x), std::abs(y), std::abs(z)}) != r)
                        continue;
                    const glm::ivec3 q = c + glm::ivec3(x, y, z);
                    const double d = glm::length(glm::dvec3(x, y, z));
                    if (g.free(q) && d < best)
                    {
                        best = d;
                        out = q;
                    }
                }
        if (std::isfinite(best))
            return true;
    }
    return false;
}

// A* from s to the first cell `isGoal` accepts (h = euclidean to `target`); the cell path, or empty
template <class IsGoal>
std::vector<glm::ivec3> search(const Grid& g, const glm::ivec3& s, const glm::ivec3& target, IsGoal isGoal, std::size_t maxExpansions)
{
    const std::size_t total = g.blocked.size();
    std::vector<float> cost(total, std::numeric_limits<float>::infinity());
    std::vector<std::int32_t> from(total, -1);
    using Item = std::pair<float, std::int32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    const auto h = [&](const glm::ivec3& c) { return static_cast<float>(glm::length(glm::dvec3(c - target))); };
    const auto cellOfIndex = [&](std::int32_t i) {
        const int x = i % g.n.x, y = (i / g.n.x) % g.n.y, z = i / (g.n.x * g.n.y);
        return glm::ivec3(x, y, z);
    };
    const auto si = static_cast<std::int32_t>(g.index(s));
    cost[static_cast<std::size_t>(si)] = 0.0f;
    open.push({h(s), si});
    std::size_t expansions = 0;
    while (!open.empty() && expansions < maxExpansions)
    {
        const auto [f, ci] = open.top();
        open.pop();
        const glm::ivec3 c = cellOfIndex(ci);
        const float gc = cost[static_cast<std::size_t>(ci)];
        if (f > gc + h(c) + 1.0e-3f)
            continue;   // stale
        ++expansions;
        if (isGoal(c))
        {
            std::vector<glm::ivec3> path;
            for (std::int32_t i = ci; i >= 0; i = from[static_cast<std::size_t>(i)])
                path.push_back(cellOfIndex(i));
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (const auto& d : neighbours())
        {
            const glm::ivec3 q = c + d;
            if (!g.free(q))
                continue;
            const auto qi = static_cast<std::int32_t>(g.index(q));
            const float nc = gc + static_cast<float>(glm::length(glm::dvec3(d)));
            if (nc < cost[static_cast<std::size_t>(qi)])
            {
                cost[static_cast<std::size_t>(qi)] = nc;
                from[static_cast<std::size_t>(qi)] = ci;
                open.push({nc + h(q), qi});
            }
        }
    }
    return {};
}

inline bool segmentClear(const glm::dvec3& a, const glm::dvec3& b, const VoxelRouteRequest& r)
{
    return world::navigation::segmentClearOfNavigationObstacles(a, b, *r.obstacles, r.agentRadiusMeters, r.clearanceMeters);
}

// the cell path's centres shortened by line of sight (exact geometry): from each kept point, the farthest later point it
// sees; a point that sees no later one keeps the next cell (the cells are free, so their steps are)
inline void appendShortened(const std::vector<glm::dvec3>& pts, const VoxelRouteRequest& r, std::vector<glm::dvec3>& out)
{
    if (pts.empty())
        return;
    std::size_t i = 0;
    out.push_back(pts[0]);
    while (i + 1 < pts.size())
    {
        std::size_t best = i + 1;
        // binary-ish: try far first, step back
        for (std::size_t j = pts.size() - 1; j > i + 1; j = (j > i + 8) ? j - std::max<std::size_t>(1, (j - i) / 8) : j - 1)
            if (segmentClear(pts[i], pts[j], r))
            {
                best = j;
                break;
            }
        out.push_back(pts[best]);
        i = best;
    }
}
} // namespace voxel_detail

inline VoxelRouteResult planVoxelRoute(const VoxelRouteRequest& r)
{
    using namespace voxel_detail;
    VoxelRouteResult out;
    if (!r.obstacles)
        return out;
    const auto& obstacles = *r.obstacles;
    const double inflation = r.agentRadiusMeters + r.clearanceMeters;
    const double fineCell = std::clamp(r.agentRadiusMeters * 0.5, 1.5, 8.0);

    for (double margin : {1500.0, 4000.0})
    {
        // THE TRANSIT GRID first: "open room" is exactly a free cell of it (one cell size for the escape's goal test and
        // the transit search - a start judged open in one grid and blocked in another jumped through a wall)
        const glm::dvec3 lo = glm::min(r.startMeters, r.goalMeters) - glm::dvec3(margin);
        const glm::dvec3 hi = glm::max(r.startMeters, r.goalMeters) + glm::dvec3(margin);
        const double cell = std::max({fineCell * 2.0, 10.0, std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) / std::max(16, r.maxCellsPerAxis)});
        const Grid g = buildGrid(lo, hi, cell, obstacles, inflation);
        // a point is in open room when its transit cell is free and the way to that cell's centre is clear (exact)
        const auto openRoom = [&](const glm::dvec3& p) {
            const glm::ivec3 c = g.cellOf(p);
            return g.free(c) && segmentClear(p, g.centre(c), r);
        };

        std::vector<glm::dvec3> path;
        path.push_back(r.startMeters);
        glm::dvec3 escapePoint = r.startMeters;
        if (!openRoom(r.startMeters))
        {
            // 1. ESCAPE - fine cells (a fraction of the hull: a gap narrower than the hull stays closed) to the first
            // point in open room; the start that touches its boxes searches from its nearest free fine cell
            const double half = std::min(r.escapeHalfSizeMeters, 0.5 * std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}));
            std::vector<world::navigation::NavigationObstacle> nearObstacles;
            for (const auto& o : obstacles)
                if (glm::length(o.centerMeters - r.startMeters) < o.conservativeRadiusMeters() + 2.0 * half + inflation + cell)
                    nearObstacles.push_back(o);
            const Grid fine = buildGrid(r.startMeters - glm::dvec3(half), r.startMeters + glm::dvec3(half), fineCell, nearObstacles,
                                        r.agentRadiusMeters);
            glm::ivec3 s;
            // EMBEDDED (the ship is inside a solid volume - not a hollow; live: a start inside the hub's core): the
            // shortest way to the nearest free cell anywhere in the escape grid, straight through what holds it - the
            // least bad route; only a grid with no free cell at all (the ship's hull fits nowhere) refuses
            const int embeddedRadius = std::max({fine.n.x, fine.n.y, fine.n.z});
            bool deepEmbedded = false;
            if (!nearestFree(fine, fine.cellOf(r.startMeters), embeddedRadius, s))
            {
                // DEEP inside (a solid core larger than the escape grid: live, 660 m inside the hub's core box): the
                // nearest free TRANSIT cell, straight out through the volume
                glm::ivec3 cs;
                if (!nearestFree(g, g.cellOf(r.startMeters), std::max({g.n.x, g.n.y, g.n.z}), cs))
                {
                    out.outcome = VoxelRouteOutcome::Enclosed;
                    out.note = "no free room for the hull anywhere around the ship";
                    return out;
                }
                deepEmbedded = true;
                path.push_back(g.centre(cs));
                escapePoint = path.back();
                out.note = "deep embedded start: straight out through the volume " +
                           std::to_string(static_cast<int>(glm::length(escapePoint - r.startMeters))) + " m; ";
            }
            if (!deepEmbedded)
            {
            std::vector<char> roomCache(fine.blocked.size(), -1);
            const auto isOut = [&](const glm::ivec3& c) {
                auto& v = roomCache[fine.index(c)];
                if (v < 0)
                    v = openRoom(fine.centre(c)) ? 1 : 0;
                return v == 1;
            };
            const auto cells = search(fine, s, fine.cellOf(r.goalMeters), isOut, r.maxExpansions);
            if (cells.empty())
            {
                // the fine grid holds the start's whole surroundings (half <= 400 m): no reachable open room in it =
                // no exit wide enough for the hull, unless the surroundings are larger than the grid (then: a bigger margin)
                out.outcome = VoxelRouteOutcome::Enclosed;
                out.note = "no way out wide enough for the hull (escape cells " + std::to_string(fineCell) + " m)";
                return out;
            }
            std::vector<glm::dvec3> pts;
            for (const auto& c : cells)
                pts.push_back(fine.centre(c));
            std::vector<glm::dvec3> shortened;
            appendShortened(pts, r, shortened);
            for (const auto& p : shortened)
                path.push_back(p);
            escapePoint = path.back();
            if (glm::length(fine.centre(s) - r.startMeters) > 4.0 * r.agentRadiusMeters)
                out.note = "embedded start: out through the volume to the nearest free room " +
                           std::to_string(static_cast<int>(glm::length(fine.centre(s) - r.startMeters))) + " m away; ";
            }
        }
        const std::size_t escapeCount = path.size();

        // 2. TRANSIT - from the escape point's own (free) cell; the goal may snap to its nearest free cell (open space)
        const glm::ivec3 s = g.cellOf(escapePoint);
        glm::ivec3 t;
        if (!g.free(s) || !nearestFree(g, g.cellOf(r.goalMeters), 12, t))
            continue;
        const auto cells = search(g, s, t, [&](const glm::ivec3& c) { return c == t; }, r.maxExpansions);
        if (cells.empty())
            continue;
        std::vector<glm::dvec3> pts;
        pts.push_back(escapePoint);
        for (const auto& c : cells)
            pts.push_back(g.centre(c));
        const bool goalFree = g.cellOf(r.goalMeters) == t && segmentClear(pts.back(), r.goalMeters, r);
        if (goalFree)
            pts.push_back(r.goalMeters);
        std::vector<glm::dvec3> shortened;
        appendShortened(pts, r, shortened);
        for (std::size_t i = 1; i < shortened.size(); ++i)
            path.push_back(shortened[i]);
        out.outcome = VoxelRouteOutcome::Found;
        out.escapePoints = escapeCount;
        out.points = std::move(path);
        out.note += "voxel route: " + std::to_string(out.points.size()) + " points, transit cell " +
                   std::to_string(static_cast<int>(cell)) + " m" + (goalFree ? "" : ", ends at the free point nearest the goal");
        return out;
    }
    out.outcome = VoxelRouteOutcome::NoPath;
    out.note = "no transit path within the grid";
    return out;
}
} // namespace game::navigation::planner
