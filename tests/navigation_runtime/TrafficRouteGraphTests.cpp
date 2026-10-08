#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/traffic/TrafficRouteGraph.h"
#include "src/game/navigation/traffic/TrafficRouteGraphCatalog.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

} // namespace

int main()
{
    try
    {
        using namespace game::navigation;
        using namespace game::navigation::traffic;

        // PROTECTED CONTRACT: the catalog exposes immutable topology only.
        static_assert(
            std::is_same_v<
                decltype(std::declval<const TrafficRouteGraphCatalog&>().graph()),
                const std::shared_ptr<const TrafficRouteGraph>&
            >,
            "traffic graph catalog no longer exposes immutable graph ownership"
        );

        static_assert(
            !std::is_copy_constructible_v<TrafficRouteGraph> &&
            !std::is_copy_assignable_v<TrafficRouteGraph> &&
            !std::is_move_constructible_v<TrafficRouteGraph> &&
            !std::is_move_assignable_v<TrafficRouteGraph>,
            "built traffic topology became copyable/mutable by accident"
        );

        HubSemanticAnchorCatalog anchors;
        require(
            anchors.load("src/assets/data/navigation/hub_semantic_anchors.json"),
            "semantic anchor catalog failed to load"
        );

        const auto* transitEntry =
            anchors.find(
                "guidance_dock_cylinder_b",
                "traffic_entry_front"
            );
        const auto* transitExit =
            anchors.find(
                "guidance_dock_cylinder_b",
                "traffic_exit_rear"
            );

        require(transitEntry, "cylinder transit entry anchor is missing");
        require(transitExit, "cylinder transit exit anchor is missing");
        require(
            transitEntry->kind == HubSemanticAnchorKind::TransitGate,
            "traffic entry was silently changed away from transit_gate"
        );
        require(
            transitExit->kind == HubSemanticAnchorKind::TransitGate,
            "traffic exit was silently changed away from transit_gate"
        );

        TrafficRouteGraphCatalog catalog;
        require(
            catalog.load(
                "src/assets/data/navigation/hub_traffic_route_graph.json"
            ),
            "protected traffic graph failed to load"
        );

        const auto graph = catalog.graph();
        require(graph, "protected traffic graph is null");
        require(
            graph->graphId() == "earth_orbital_hub_test_traffic_v1",
            "unexpected protected traffic graph id"
        );

        const auto* zone =
            graph->mandatoryZone("cylinder_b.blue_transit");
        require(zone, "blue mandatory transit zone is missing");
        require(
            zone->directionPolicy == TransitDirectionPolicy::OneWay,
            "blue cylinder unexpectedly stopped being one-way"
        );
        require(
            zone->entryPortalId == "cylinder_b.entry_front",
            "blue cylinder entry portal changed"
        );
        require(
            zone->exitPortalId == "cylinder_b.exit_rear",
            "blue cylinder exit portal changed"
        );

        const auto route =
            graph->resolve(
                "cylinder_b.entry_front",
                "cube_a.dock_front"
            );

        require(route.valid, "protected traffic route did not resolve");
        require(
            route.portalSequence.size() == 3,
            "protected traffic route portal count changed"
        );
        require(
            route.portalSequence[0] == "cylinder_b.entry_front" &&
            route.portalSequence[1] == "cylinder_b.exit_rear" &&
            route.portalSequence[2] == "cube_a.dock_front",
            "protected traffic route portal order changed"
        );

        require(
            route.stages.size() == 3,
            "protected traffic route stage count changed"
        );
        require(
            route.stages[0].kind == TrafficRouteStageKind::FreeApproach,
            "ship-to-blue-zone stage is no longer free approach"
        );
        require(
            route.stages[1].kind == TrafficRouteStageKind::MandatoryTransit,
            "blue-zone stage is no longer mandatory transit"
        );
        require(
            route.stages[1].mandatoryZoneId ==
                "cylinder_b.blue_transit",
            "mandatory transit stage lost blue-zone ownership"
        );
        require(
            route.stages[2].kind == TrafficRouteStageKind::TerminalApproach,
            "blue-zone exit no longer leads to terminal dock approach"
        );

        // One-way means exactly one-way. Do not let generic graph search invent
        // a reverse lane through the same blue volume.
        const auto reverse =
            graph->resolve(
                "cylinder_b.exit_rear",
                "cylinder_b.entry_front"
            );
        require(
            !reverse.valid,
            "one-way blue transit unexpectedly resolved in reverse"
        );

        // Malformed topology must fail closed instead of being partially used.
        TrafficRouteGraphDefinition invalid;
        invalid.graphId = "invalid";
        NavigationPortalDefinition onlyPortal;
        onlyPortal.id = "entry";
        onlyPortal.hubModuleId = "module";
        onlyPortal.semanticAnchorId = "entry_anchor";
        invalid.portals.push_back(onlyPortal);

        MandatoryTransitZoneDefinition invalidZone;
        invalidZone.id = "zone";
        invalidZone.hubModuleId = "module";
        invalidZone.entryPortalId = "entry";
        invalidZone.exitPortalId = "missing_exit";
        invalidZone.radiusMeters = 100.0;
        invalidZone.halfLengthMeters = 500.0;
        invalid.mandatoryZones.push_back(invalidZone);

        std::string failure;
        require(
            !TrafficRouteGraph::build(std::move(invalid), &failure),
            "invalid mandatory topology was accepted"
        );
        require(
            !failure.empty(),
            "invalid mandatory topology failed without diagnosis"
        );

        // ARCHITECTURE GUARD: geometric RoutePlanner must not own or mutate
        // semantic traffic topology. A dedicated resolver/stage compiler is
        // the only legal bridge between these layers.
        {
            std::ifstream plannerSource(
                "src/game/navigation/planner/RoutePlanner.cpp"
            );
            require(
                plannerSource.is_open(),
                "cannot inspect RoutePlanner architecture boundary"
            );

            std::ostringstream source;
            source << plannerSource.rdbuf();
            require(
                source.str().find("TrafficRouteGraph") ==
                    std::string::npos,
                "RoutePlanner directly depends on protected traffic topology"
            );
        }

        std::cout << "TRAFFIC ROUTE GRAPH TESTS: PASS\n";
        std::cout << " - topology is immutable after validated build\n";
        std::cout << " - cylinder B entry -> exit remains mandatory and one-way\n";
        std::cout << " - cube A terminal approach stays downstream of blue transit\n";
        std::cout << " - malformed topology fails closed\n";
        std::cout << " - RoutePlanner remains outside protected topology ownership\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "TRAFFIC ROUTE GRAPH TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
