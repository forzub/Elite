#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/NavigationVolumeCatalog.h"
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
        require(static_cast<bool>(graph), "protected traffic graph is null");
        require(
            graph->graphId() == "earth_orbital_hub_test_traffic_v1",
            "unexpected protected traffic graph id"
        );

        NavigationVolumeCatalog volumes;
        require(
            volumes.load(
                "src/assets/data/navigation/hub_navigation_volumes.json"
            ),
            "navigation volume catalog failed to load"
        );
        const auto* cylinderVolume =
            volumes.find("cylinder_b.transit_volume");
        require(
            cylinderVolume,
            "cylinder B navigation volume is missing"
        );
        require(
            cylinderVolume->kind ==
                world::navigation::NavigationVolumeKind::SweptCorridor,
            "cylinder B stopped being generic swept-corridor geometry"
        );
        require(
            cylinderVolume->sections.size() == 2,
            "straight cylinder B should currently have two corridor sections"
        );

        const auto* lane =
            graph->lane("cylinder_b.blue_lane");
        require(lane, "blue cylinder traffic lane is missing");
        require(
            lane->directionPolicy == TransitDirectionPolicy::OneWay,
            "blue cylinder unexpectedly stopped being one-way"
        );
        require(
            lane->entryPortalId == "cylinder_b.entry_front",
            "blue cylinder entry portal changed"
        );
        require(
            lane->exitPortalId == "cylinder_b.exit_rear",
            "blue cylinder exit portal changed"
        );
        require(
            lane->navigationVolumeId == "cylinder_b.transit_volume",
            "traffic lane lost its physical navigation volume"
        );
        require(
            lane->policy ==
                world::navigation::NavigationVolumePolicy::KeepInside,
            "blue traffic lane stopped being hard containment"
        );

        const auto* entryPortal =
            graph->portal("cylinder_b.entry_front");
        const auto* exitPortal =
            graph->portal("cylinder_b.exit_rear");
        require(
            entryPortal && exitPortal,
            "blue cylinder traffic portals are missing"
        );
        require(
            entryPortal->inboundConnection.kind ==
                PortalConnectionKind::MandatoryTangentStraight &&
            entryPortal->inboundConnection.mandatoryStraightMeters == 700.0,
            "blue entry lost mandatory tangent-straight approach"
        );
        require(
            exitPortal->outboundConnection.kind ==
                PortalConnectionKind::MandatoryTangentStraight &&
            exitPortal->outboundConnection.mandatoryStraightMeters == 700.0,
            "blue exit lost mandatory tangent-straight departure"
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
            route.stages[1].kind == TrafficRouteStageKind::VolumeTransit,
            "blue-zone stage is no longer volume transit"
        );
        require(
            route.stages[1].laneId ==
                "cylinder_b.blue_lane",
            "volume-transit stage lost blue-lane ownership"
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

        TrafficLaneDefinition invalidLane;
        invalidLane.id = "lane";
        invalidLane.navigationVolumeId = "volume";
        invalidLane.entryPortalId = "entry";
        invalidLane.exitPortalId = "missing_exit";
        invalidLane.policy =
            world::navigation::NavigationVolumePolicy::KeepInside;
        invalid.lanes.push_back(invalidLane);

        std::string failure;
        require(
            !TrafficRouteGraph::build(std::move(invalid), &failure),
            "invalid traffic topology was accepted"
        );
        require(
            !failure.empty(),
            "invalid mandatory topology failed without diagnosis"
        );

        // BLUE and GREEN are policies over the SAME physical volume.
        // Geometry must not be duplicated merely because traffic policy differs.
        {
            TrafficRouteGraphDefinition preferredGraph;
            preferredGraph.graphId = "preferred-volume-policy";

            NavigationPortalDefinition a;
            a.id = "a";
            a.hubModuleId = "module";
            a.semanticAnchorId = "a_anchor";
            a.crossingForwardLocal = {0.0, 0.0, 1.0};
            a.upLocal = {0.0, 1.0, 0.0};

            NavigationPortalDefinition b = a;
            b.id = "b";
            b.semanticAnchorId = "b_anchor";

            preferredGraph.portals = {a, b};

            TrafficLaneDefinition lane;
            lane.id = "green_lane";
            lane.navigationVolumeId = "shared_volume";
            lane.entryPortalId = "a";
            lane.exitPortalId = "b";
            lane.policy =
                world::navigation::NavigationVolumePolicy::PreferInside;
            preferredGraph.lanes.push_back(lane);

            TrafficRouteEdgeDefinition edge;
            edge.id = "a_to_b";
            edge.fromPortalId = "a";
            edge.toPortalId = "b";
            edge.kind = TrafficRouteStageKind::VolumeTransit;
            edge.laneId = "green_lane";
            preferredGraph.edges.push_back(edge);

            std::string preferredFailure;
            const auto preferred =
                TrafficRouteGraph::build(
                    std::move(preferredGraph),
                    &preferredFailure
                );

            require(
                static_cast<bool>(preferred),
                "preferred/green lane over shared volume was rejected"
            );

            const auto preferredRoute =
                preferred->resolve("a", "b");
            require(
                preferredRoute.valid &&
                preferredRoute.stages.size() == 2,
                "preferred/green lane route did not resolve"
            );
            require(
                preferredRoute.stages[1].volumeConstraint.volumeId ==
                    "shared_volume",
                "preferred/green lane duplicated or lost physical volume identity"
            );
            require(
                preferredRoute.stages[1].volumeConstraint.policy ==
                    world::navigation::NavigationVolumePolicy::PreferInside,
                "preferred/green lane did not preserve soft-inside policy"
            );
        }

        // A bent tunnel/canyon is the same physical concept: one swept
        // navigation volume with more cross-sections, not a new zone type.
        {
            world::navigation::NavigationVolumeDefinition bent;
            bent.id = "bent_test_volume";
            bent.referenceFrameId = "test_frame";
            bent.kind =
                world::navigation::NavigationVolumeKind::SweptCorridor;

            world::navigation::NavigationVolumeSection s0;
            s0.centerLocalMeters = {0.0, 0.0, 0.0};
            s0.forwardLocal = {1.0, 0.0, 0.0};
            s0.upLocal = {0.0, 1.0, 0.0};
            s0.radiusMeters = 50.0;

            auto s1 = s0;
            s1.centerLocalMeters = {100.0, 0.0, 0.0};
            s1.forwardLocal =
                glm::normalize(glm::dvec3(1.0, 0.0, 1.0));

            auto s2 = s0;
            s2.centerLocalMeters = {170.0, 0.0, 70.0};
            s2.forwardLocal = {0.0, 0.0, 1.0};

            bent.sections = {s0, s1, s2};

            require(
                bent.finite(),
                "bent swept navigation volume was rejected"
            );
        }

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

        // LIVE INTEGRATION GUARD: Cube A docking must consume the semantic
        // traffic compiler and hard BLUE validator. Do not regress to the old
        // hand-authored entryPose/exitPose via-point bridge in SpaceState.
        {
            std::ifstream liveSource("src/game/SpaceState.cpp");
            require(
                liveSource.is_open(),
                "cannot inspect live docking traffic integration"
            );

            std::ostringstream source;
            source << liveSource.rdbuf();
            const std::string text = source.str();

            require(
                text.find("RouteStageCompiler::compile") !=
                    std::string::npos,
                "live docking stopped using RouteStageCompiler"
            );
            require(
                text.find("validateKeepInsideRouteInterval") !=
                    std::string::npos,
                "live docking stopped validating BLUE containment"
            );
            require(
                text.find("request.requiredViaPointsMeters = {") ==
                    std::string::npos,
                "live docking regressed to hand-authored traffic via points"
            );
        }

        std::cout << "TRAFFIC ROUTE GRAPH TESTS: PASS\n";
        std::cout << " - topology is immutable after validated build\n";
        std::cout << " - cylinder B entry -> exit remains mandatory and one-way\n";
        std::cout << " - blue portals preserve tangent-straight boundary contracts\n";
        std::cout << " - cube A terminal approach stays downstream of blue transit\n";
        std::cout << " - malformed topology fails closed\n";
        std::cout << " - blue/green share one physical volume model\n";
        std::cout << " - bent tunnels use the same swept-volume primitive\n";
        std::cout << " - RoutePlanner remains outside protected topology ownership\n";
        std::cout << " - live docking consumes compiler + BLUE containment validator\n";
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
