#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/NavigationVolumeCatalog.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"
#include "src/game/navigation/traffic/TrafficRouteGraphCatalog.h"

#include <iostream>
#include <stdexcept>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double epsilon = 1.0e-9
)
{
    return glm::length(a - b) <= epsilon;
}

} // namespace

int main()
{
    try
    {
        using namespace game::navigation;
        using namespace game::navigation::traffic;

        HubSemanticAnchorCatalog anchors;
        require(
            anchors.load("src/assets/data/navigation/hub_semantic_anchors.json"),
            "semantic anchor catalog failed to load"
        );

        NavigationVolumeCatalog volumes;
        require(
            volumes.load("src/assets/data/navigation/hub_navigation_volumes.json"),
            "navigation volume catalog failed to load"
        );

        TrafficRouteGraphCatalog graphCatalog;
        require(
            graphCatalog.load(
                "src/assets/data/navigation/hub_traffic_route_graph.json"
            ),
            "traffic graph catalog failed to load"
        );

        const auto graph = graphCatalog.graph();
        require(static_cast<bool>(graph), "traffic graph is null");

        const auto route =
            graph->resolve(
                "cylinder_b.entry_front",
                "cube_a.dock_front"
            );
        require(route.valid, "traffic route failed to resolve");

        RouteStageCompiler::ReferenceFrameMap frames;
        frames["guidance_dock_cylinder_b"] = {};
        NavigationReferenceFramePose cubeFrame;
        cubeFrame.originWorldMeters = {2500.0, 0.0, 600.0};
        frames["guidance_dock_cube_a"] = cubeFrame;

        const auto compiled =
            RouteStageCompiler::compile(
                route,
                *graph,
                anchors,
                volumes,
                frames
            );

        require(compiled.valid, "traffic route failed stage compilation");
        require(
            compiled.stages.size() == 3,
            "compiled stage count changed"
        );

        const auto& transit = compiled.stages[1];
        require(
            transit.kind == TrafficRouteStageKind::VolumeTransit,
            "blue lane stopped compiling as volume transit"
        );
        require(
            transit.volumeConstraint.policy ==
                world::navigation::NavigationVolumePolicy::KeepInside,
            "blue lane lost KeepInside during stage compilation"
        );
        require(
            transit.requiredCenterlineMeters.size() == 2,
            "straight cylinder should compile two centerline stations"
        );
        require(
            near(
                transit.requiredCenterlineMeters.front(),
                glm::dvec3(0.0, 0.0, -600.0)
            ) &&
            near(
                transit.requiredCenterlineMeters.back(),
                glm::dvec3(0.0, 0.0, 600.0)
            ),
            "cylinder centerline no longer follows authored volume sections"
        );

        require(
            compiled.mandatoryTangentStraights.size() == 2,
            "blue cylinder should compile inbound and outbound tangent straights"
        );
        require(
            compiled.mandatoryTangentStraights[0].inbound &&
            near(
                compiled.mandatoryTangentStraights[0].startWorldMeters,
                glm::dvec3(0.0, 0.0, -1300.0)
            ) &&
            near(
                compiled.mandatoryTangentStraights[0].endWorldMeters,
                glm::dvec3(0.0, 0.0, -600.0)
            ),
            "blue entry mandatory tangent straight is wrong"
        );
        require(
            !compiled.mandatoryTangentStraights[1].inbound &&
            near(
                compiled.mandatoryTangentStraights[1].startWorldMeters,
                glm::dvec3(0.0, 0.0, 600.0)
            ) &&
            near(
                compiled.mandatoryTangentStraights[1].endWorldMeters,
                glm::dvec3(0.0, 0.0, 1300.0)
            ),
            "blue exit mandatory tangent straight is wrong"
        );

        require(
            compiled.requiredViaPointsMeters.size() == 5,
            "compiled route lost tangent-straight boundary stations"
        );
        require(
            near(
                compiled.requiredViaPointsMeters[0],
                glm::dvec3(0.0, 0.0, -1300.0)
            ),
            "compiled route does not begin blue entry alignment straight"
        );
        require(
            near(
                compiled.requiredViaPointsMeters[1],
                glm::dvec3(0.0, 0.0, -600.0)
            ),
            "compiled route does not enter cylinder B at front portal"
        );
        require(
            near(
                compiled.requiredViaPointsMeters[2],
                glm::dvec3(0.0, 0.0, 600.0)
            ),
            "compiled route does not leave cylinder B at rear portal"
        );
        require(
            near(
                compiled.requiredViaPointsMeters[3],
                glm::dvec3(0.0, 0.0, 1300.0)
            ),
            "compiled route does not preserve blue exit departure straight"
        );
        require(
            near(
                compiled.requiredViaPointsMeters[4],
                glm::dvec3(2500.0, 0.0, 150.0)
            ),
            "compiled route did not continue to cube A dock portal"
        );

        // Missing frame data is authoritative failure, not permission to
        // silently bypass a mandatory blue lane.
        frames.erase("guidance_dock_cylinder_b");
        const auto missingFrame =
            RouteStageCompiler::compile(
                route,
                *graph,
                anchors,
                volumes,
                frames
            );
        require(
            !missingFrame.valid,
            "mandatory blue lane compiled without its runtime frame"
        );

        std::cout << "ROUTE STAGE COMPILER TESTS: PASS\n";
        std::cout << " - blue cylinder becomes ordered planner via-points\n";
        std::cout << " - blue entry/exit compile protected tangent straights\n";
        std::cout << " - KeepInside survives topology -> geometry compilation\n";
        std::cout << " - missing mandatory frame fails closed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ROUTE STAGE COMPILER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
