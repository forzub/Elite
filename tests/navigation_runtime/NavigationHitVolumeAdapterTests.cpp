#include "src/game/navigation/NavigationHitVolumeAdapter.h"
#include "src/game/navigation/HubNavigationClearancePolicy.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    using game::navigation::NavigationHitVolumeAdapter;
    using game::simulation::DebugHitVolumeSnapshot;

    // Two solid station pieces with a real service gap between them. A single
    // descriptor-wide aggregate box would erase the gap; exact HitVolumes must
    // preserve it for navigation.
    std::vector<DebugHitVolumeSnapshot> volumes;

    DebugHitVolumeSnapshot left;
    left.moduleId = "left";
    left.center = {-15.0f, 0.0f, 0.0f};
    left.halfSize = {5.0f, 20.0f, 20.0f};
    volumes.push_back(left);

    DebugHitVolumeSnapshot right;
    right.moduleId = "right";
    right.center = {15.0f, 0.0f, 0.0f};
    right.halfSize = {5.0f, 20.0f, 20.0f};
    volumes.push_back(right);

    const auto obstacles = NavigationHitVolumeAdapter::buildObstacles(
        volumes,
        77u,
        glm::dvec3(100.0, 50.0, -25.0),
        glm::dmat3(1.0),
        "object:77",
        {false, 0.0}
    );

    if (obstacles.size() != 2)
    {
        std::cerr << "expected two exact module obstacles, got "
                  << obstacles.size() << "\n";
        return 1;
    }

    if (obstacles[0].id != "object:77::hit::0" ||
        obstacles[1].id != "object:77::hit::1")
    {
        std::cerr << "exact obstacle identity lost\n";
        return 2;
    }

    const glm::dvec3 gapStart(100.0, 50.0, -100.0);
    const glm::dvec3 gapEnd(100.0, 50.0, 50.0);
    if (!world::navigation::segmentClearOfNavigationObstacles(
            gapStart,
            gapEnd,
            obstacles,
            4.0))
    {
        std::cerr
            << "real gap between exact HitVolumes was erased\n";
        return 3;
    }

    const glm::dvec3 blockedStart(85.0, 50.0, -100.0);
    const glm::dvec3 blockedEnd(85.0, 50.0, 50.0);
    if (world::navigation::segmentClearOfNavigationObstacles(
            blockedStart,
            blockedEnd,
            obstacles,
            0.0))
    {
        std::cerr
            << "solid module volume stopped blocking navigation\n";
        return 4;
    }

    // Destroyed volumes must disappear from navigation, matching the runtime
    // HitComponent semantics.
    volumes[0].destroyed = true;
    const auto afterDestroy = NavigationHitVolumeAdapter::buildObstacles(
        volumes,
        77u,
        glm::dvec3(0.0),
        glm::dmat3(1.0),
        "object:77"
    );
    if (afterDestroy.size() != 1 ||
        afterDestroy.front().id != "object:77::hit::1")
    {
        std::cerr
            << "destroyed hit volume remained in navigation geometry\n";
        return 5;
    }

    // The diagnostic docking target is a real four-wall tunnel. Generic Hub
    // infrastructure clearance is intentionally large for open-space routing,
    // but applying it to the target walls seals the authored aperture. The
    // target must instead use the docking port's semantic clearance.
    std::vector<DebugHitVolumeSnapshot> dockWalls;
    const auto addDockWall =
        [&](const char* id,
            const glm::vec3& center,
            const glm::vec3& halfSize)
        {
            DebugHitVolumeSnapshot wall;
            wall.moduleId = id;
            wall.center = center;
            wall.halfSize = halfSize;
            dockWalls.push_back(wall);
        };
    addDockWall("left", {-137.5f,0.0f,0.0f}, {42.5f,180.0f,450.0f});
    addDockWall("right", {137.5f,0.0f,0.0f}, {42.5f,180.0f,450.0f});
    addDockWall("top", {0.0f,117.5f,0.0f}, {95.0f,62.5f,450.0f});
    addDockWall("bottom", {0.0f,-117.5f,0.0f}, {95.0f,62.5f,450.0f});

    constexpr double DockSemanticClearanceMeters = 18.0;
    constexpr double CobraConservativeRadiusMeters = 18.0;
    const glm::dvec3 dockAxisStart(0.0,0.0,-700.0);
    const glm::dvec3 dockAxisEnd(0.0,0.0,450.0);

    const auto semanticDockObstacles =
        NavigationHitVolumeAdapter::buildObstacles(
            dockWalls,
            88u,
            glm::dvec3(0.0),
            glm::dmat3(1.0),
            "object:88",
            {false,DockSemanticClearanceMeters}
        );
    if(!world::navigation::segmentClearOfNavigationObstacles(
            dockAxisStart,
            dockAxisEnd,
            semanticDockObstacles,
            CobraConservativeRadiusMeters))
    {
        std::cerr
            << "semantic docking clearance sealed the authored aperture\n";
        return 6;
    }

    const auto genericHubObstacles =
        NavigationHitVolumeAdapter::buildObstacles(
            dockWalls,
            88u,
            glm::dvec3(0.0),
            glm::dmat3(1.0),
            "object:88",
            {
                false,
                game::navigation::
                    DiagnosticHubInfrastructureClearanceMeters
            }
        );
    if(world::navigation::segmentClearOfNavigationObstacles(
            dockAxisStart,
            dockAxisEnd,
            genericHubObstacles,
            CobraConservativeRadiusMeters))
    {
        std::cerr
            << "docking aperture regression fixture no longer detects generic Hub over-inflation\n";
        return 7;
    }

    std::cout << "NAVIGATION HIT VOLUME ADAPTER TESTS: PASS\n";
    return 0;
}
