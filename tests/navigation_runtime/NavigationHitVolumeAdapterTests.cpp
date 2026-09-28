#include "src/game/navigation/NavigationHitVolumeAdapter.h"
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

    std::cout << "NAVIGATION HIT VOLUME ADAPTER TESTS: PASS\n";
    return 0;
}
