#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/NavigationVolumeCatalog.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"
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

game::navigation::planner::RouteCurveSegment line(
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    game::navigation::planner::RouteCurveSegment curve;
    curve.kind = game::navigation::planner::RouteCurveKind::Line;
    curve.startMeters = a;
    curve.endMeters = b;
    curve.startProgressMeters = 0.0;
    curve.endProgressMeters = glm::length(b - a);
    return curve;
}

} // namespace

int main()
{
    try
    {
        using namespace game::navigation;
        using namespace game::navigation::traffic;
        using namespace game::navigation::planner;

        HubSemanticAnchorCatalog anchors;
        NavigationVolumeCatalog volumes;
        TrafficRouteGraphCatalog graphCatalog;

        require(
            anchors.load("src/assets/data/navigation/hub_semantic_anchors.json"),
            "semantic anchor catalog failed"
        );
        require(
            volumes.load("src/assets/data/navigation/hub_navigation_volumes.json"),
            "navigation volume catalog failed"
        );
        require(
            graphCatalog.load(
                "src/assets/data/navigation/hub_traffic_route_graph.json"
            ),
            "traffic graph catalog failed"
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
        frames["guidance_dock_cube_a"] = {};

        const auto compiled =
            RouteStageCompiler::compile(
                route,
                *graph,
                anchors,
                volumes,
                frames
            );
        require(compiled.valid, "stage compilation failed");

        const auto& blue = compiled.stages[1];
        require(
            blue.volumeSections.size() == 2,
            "compiled blue stage lost volume geometry"
        );

        // Radius 100, authored clearance 22, hull radius 10 => route center
        // may use radius < 68 m. Axis transit is comfortably valid.
        const auto validAxis =
            RouteVolumeContainmentValidator::validateKeepInside(
                blue,
                {line({0.0, 0.0, -600.0}, {0.0, 0.0, 600.0})},
                10.0
            );
        require(validAxis.valid, "axis route rejected inside blue cylinder");

        // Production boundary case: one canonical Line may extend outside
        // BLUE on both sides because tangent-straight working leads are part
        // of the same primitive. Validate only the semantic portal interval.
        const auto extendedAxis =
            RouteVolumeContainmentValidator::validateKeepInsideRouteInterval(
                blue,
                {line({0.0, 0.0, -1300.0}, {0.0, 0.0, 1300.0})},
                {0.0, 0.0, -600.0},
                {0.0, 0.0, 600.0},
                10.0
            );
        require(
            extendedAxis.valid,
            "BLUE interval rejected valid route with external tangent leads"
        );

        // Endpoints are legal and exactly the same traffic portals, but the
        // Bezier bows 120 m sideways. A via-point-only system would accept
        // this topology; hard BLUE containment must reject it.
        RouteCurveSegment escaping;
        escaping.kind = RouteCurveKind::CubicBezier;
        escaping.startMeters = {0.0, 0.0, -600.0};
        escaping.endMeters = {0.0, 0.0, 600.0};
        escaping.bezierControl1Meters = {120.0, 0.0, -200.0};
        escaping.bezierControl2Meters = {120.0, 0.0, 200.0};
        escaping.startProgressMeters = 0.0;
        escaping.endProgressMeters = 1300.0;

        const auto invalidBezier =
            RouteVolumeContainmentValidator::validateKeepInside(
                blue,
                {escaping},
                10.0
            );
        require(
            !invalidBezier.valid,
            "Bezier escaped blue cylinder without containment rejection"
        );

        // Even a straight route can be invalid once hull+clearance erodes the
        // usable aperture. 70 m from axis is inside raw radius 100, but not
        // inside the usable radius 68.
        const auto hullTooClose =
            RouteVolumeContainmentValidator::validateKeepInside(
                blue,
                {line({70.0, 0.0, -600.0}, {70.0, 0.0, 600.0})},
                10.0
            );
        require(
            !hullTooClose.valid,
            "validator ignored hull + authored clearance erosion"
        );

        std::cout << "ROUTE VOLUME CONTAINMENT TESTS: PASS\n";
        std::cout << " - centerline transit remains valid\n";
        std::cout << " - portal interval ignores legal external tangent leads\n";
        std::cout << " - Bezier cannot bow outside mandatory BLUE volume\n";
        std::cout << " - hull plus infrastructure clearance erodes aperture\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ROUTE VOLUME CONTAINMENT TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
