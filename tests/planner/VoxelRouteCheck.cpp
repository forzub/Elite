// The guaranteed route (VoxelRoutePlanner.h) on the author's cases (2026-11-28):
//  - open space, a wall between the ship and the goal       -> a route round it, every segment clear
//  - a ship on a pad (touching the deck)                     -> led off it, then to the goal
//  - a ship in a hangar with a door wider than the hull      -> led out through the door
//  - a hangar whose door is narrower than the hull           -> ENCLOSED (the only refusal)
//  - a closed hollow cube                                    -> ENCLOSED
#include <cstdio>
#include <vector>

#include "src/game/navigation/planner/VoxelRoutePlanner.h"

namespace
{
int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}
world::navigation::NavigationObstacle box(const char* id, glm::dvec3 c, glm::dvec3 half)
{
    world::navigation::NavigationObstacle o;
    o.id = id;
    o.shape = world::navigation::NavigationObstacleShape::Box;
    o.centerMeters = c;
    o.halfExtentsMeters = half;
    return o;
}
// a hollow cube (inner half size `in`, wall `w`) with a door of half width `door` (0 = closed) on +x
std::vector<world::navigation::NavigationObstacle> hangar(double in, double w, double door)
{
    std::vector<world::navigation::NavigationObstacle> v;
    const double o = in + w;
    v.push_back(box("floor", {0, -in - w * 0.5, 0}, {o, w * 0.5, o}));
    v.push_back(box("roof", {0, in + w * 0.5, 0}, {o, w * 0.5, o}));
    v.push_back(box("back", {-in - w * 0.5, 0, 0}, {w * 0.5, in, o}));
    v.push_back(box("left", {0, 0, -in - w * 0.5}, {in, in, w * 0.5}));
    v.push_back(box("right", {0, 0, in + w * 0.5}, {in, in, w * 0.5}));
    if (door <= 0.0)
        v.push_back(box("front", {in + w * 0.5, 0, 0}, {w * 0.5, in, o}));
    else
    {   // the front wall with a square door of half size `door` in its middle
        v.push_back(box("front_l", {in + w * 0.5, 0, -(door + (in - door) * 0.5)}, {w * 0.5, in, (in - door) * 0.5}));
        v.push_back(box("front_r", {in + w * 0.5, 0, (door + (in - door) * 0.5)}, {w * 0.5, in, (in - door) * 0.5}));
        v.push_back(box("front_t", {in + w * 0.5, (door + (in - door) * 0.5), 0}, {w * 0.5, (in - door) * 0.5, door}));
        v.push_back(box("front_b", {in + w * 0.5, -(door + (in - door) * 0.5), 0}, {w * 0.5, (in - door) * 0.5, door}));
    }
    return v;
}
bool allClear(const game::navigation::planner::VoxelRouteResult& r, const std::vector<world::navigation::NavigationObstacle>& obs,
              double hull)
{
    for (std::size_t i = 0; i + 1 < r.points.size(); ++i)
        if (!world::navigation::segmentClearOfNavigationObstacles(r.points[i], r.points[i + 1], obs, hull * 0.99))
            return false;
    return true;
}
} // namespace

int main()
{
    using namespace game::navigation::planner;
    const double hull = 10.0;
    {   // a wall between the ship and the goal
        std::vector<world::navigation::NavigationObstacle> obs {box("wall", {0, 0, 0}, {20, 800, 800})};
        VoxelRouteRequest r;
        r.startMeters = {-1500, 0, 0};
        r.goalMeters = {1500, 0, 0};
        r.obstacles = &obs;
        r.agentRadiusMeters = hull;
        const auto res = planVoxelRoute(r);
        check(res.found() && res.points.size() >= 3, "a wall: a route round it");
        check(allClear(res, obs, hull), "a wall: every segment clear of the wall");
        std::printf("wall: %s\n", res.note.c_str());
    }
    {   // on a pad: the ship's hull touches the deck
        std::vector<world::navigation::NavigationObstacle> obs {box("deck", {0, -5, 0}, {200, 5, 200})};
        VoxelRouteRequest r;
        r.startMeters = {0, 4, 0};   // the hull radius 10: inside the deck's inflation
        r.goalMeters = {2000, 600, 0};
        r.obstacles = &obs;
        r.agentRadiusMeters = hull;
        const auto res = planVoxelRoute(r);
        check(res.found() && res.escapePoints >= 2, "on a pad: led off the deck, then to the goal");
        std::printf("pad: %s, escape points %zu\n", res.note.c_str(), res.escapePoints);
    }
    {   // in a hangar, the door wider than the hull (half 30 m vs hull 10 m)
        const auto obs = hangar(60.0, 6.0, 30.0);
        VoxelRouteRequest r;
        r.startMeters = {-20, 0, 0};
        r.goalMeters = {1500, 300, 0};
        r.obstacles = &obs;
        r.agentRadiusMeters = hull;
        const auto res = planVoxelRoute(r);
        check(res.found(), "a hangar with a wide door: led out");
        bool throughDoor = false;
        for (std::size_t i = 0; i + 1 < res.points.size(); ++i)
            if (res.points[i].x < 60.0 && res.points[i + 1].x > 66.0)
                throughDoor = std::abs(res.points[i + 1].y) < 30.0 && std::abs(res.points[i + 1].z) < 30.0;
        check(allClear(res, obs, hull), "a hangar: every segment clear of the walls");
        std::printf("hangar wide door: %s (through the door %s)\n", res.note.c_str(), throughDoor ? "yes" : "?");
    }
    {   // the door narrower than the hull (half 7 m vs hull radius 10 m)
        const auto obs = hangar(60.0, 6.0, 7.0);
        VoxelRouteRequest r;
        r.startMeters = {-20, 0, 0};
        r.goalMeters = {1500, 300, 0};
        r.obstacles = &obs;
        r.agentRadiusMeters = hull;
        const auto res = planVoxelRoute(r);
        check(res.outcome == VoxelRouteOutcome::Enclosed, "a hangar with a door narrower than the hull: ENCLOSED");
        std::printf("hangar narrow door: outcome %d %s\n", static_cast<int>(res.outcome), res.note.c_str());
    }
    {   // EMBEDDED: the ship inside a solid block (live: a start inside the hub's core) - out by the shortest way, not refused
        std::vector<world::navigation::NavigationObstacle> obs {box("core", {0, 0, 0}, {80, 80, 80})};
        VoxelRouteRequest r;
        r.startMeters = {10, 0, 0};
        r.goalMeters = {1500, 300, 0};
        r.obstacles = &obs;
        r.agentRadiusMeters = hull;
        const auto res = planVoxelRoute(r);
        check(res.found() && res.escapePoints >= 2, "embedded in a solid block: led out, not refused");
        std::printf("embedded: %s\n", res.note.c_str());
    }
    {   // a closed hollow cube
        const auto obs = hangar(60.0, 6.0, 0.0);
        VoxelRouteRequest r;
        r.startMeters = {0, 0, 0};
        r.goalMeters = {1500, 300, 0};
        r.obstacles = &obs;
        r.agentRadiusMeters = hull;
        const auto res = planVoxelRoute(r);
        check(res.outcome == VoxelRouteOutcome::Enclosed, "a closed cube: ENCLOSED");
    }
    if (g_failures)
    {
        std::printf("%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("voxel route contract OK: round a wall, off a pad, out of a hangar; enclosed only without a wide-enough exit\n");
    return 0;
}
