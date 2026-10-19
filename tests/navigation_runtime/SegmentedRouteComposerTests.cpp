#include "src/game/navigation/traffic/SegmentedRouteComposer.h"
#include "src/game/navigation/traffic/TrafficRoutePlanAdapter.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        using namespace game::navigation;
        using namespace game::navigation::planner;
        using namespace game::navigation::traffic;

        CompiledTrafficRoute traffic;
        traffic.valid = true;

        CompiledTrafficStage blue;
        blue.kind = TrafficRouteStageKind::VolumeTransit;
        blue.fromPortalId = "blue.entry";
        blue.toPortalId = "blue.exit";
        blue.laneId = "test.blue_lane";
        blue.fromWorldMeters = {0.0, 0.0, -600.0};
        blue.toWorldMeters = {0.0, 0.0, 600.0};
        blue.volumeConstraint.volumeId = "test.blue_volume";
        blue.volumeConstraint.policy =
            world::navigation::NavigationVolumePolicy::KeepInside;
        blue.volumeConstraint.maxSpeedMps = 80.0;
        blue.volumeRequiredClearanceMeters = 20.0;

        CompiledNavigationVolumeSection a;
        a.centerWorldMeters = blue.fromWorldMeters;
        a.forwardWorld = {0.0, 0.0, 1.0};
        a.upWorld = {0.0, 1.0, 0.0};
        a.crossSection =
            world::navigation::NavigationCrossSectionKind::Circle;
        a.radiusMeters = 250.0;

        CompiledNavigationVolumeSection b = a;
        b.centerWorldMeters = blue.toWorldMeters;

        blue.volumeSections = {a, b};
        blue.requiredCenterlineMeters = {
            blue.fromWorldMeters,
            blue.toWorldMeters
        };
        traffic.stages.push_back(blue);

        CompiledMandatoryTangentStraight inbound;
        inbound.portalId = blue.fromPortalId;
        inbound.inbound = true;
        inbound.startWorldMeters = {0.0, 0.0, -1300.0};
        inbound.endWorldMeters = blue.fromWorldMeters;
        inbound.forwardWorld = {0.0, 0.0, 1.0};
        inbound.lengthMeters = 700.0;

        CompiledMandatoryTangentStraight outbound;
        outbound.portalId = blue.toPortalId;
        outbound.inbound = false;
        outbound.startWorldMeters = blue.toWorldMeters;
        outbound.endWorldMeters = {0.0, 0.0, 1300.0};
        outbound.forwardWorld = {0.0, 0.0, 1.0};
        outbound.lengthMeters = 700.0;

        traffic.mandatoryTangentStraights = {
            inbound,
            outbound
        };

        RoutePlanRequest request;
        request.startMeters = {0.0, 0.0, -5000.0};
        request.goalMeters = {3500.0, 0.0, 3500.0};
        request.terminalOutward = {0.0, 0.0, -1.0};
        request.terminalReferenceDistanceMeters = 200.0;
        request.agentRadiusMeters = 10.0;
        request.vehicleLengthMeters = 30.0;
        request.maxSpeedMps = 120.0;
        request.acceleratingMps2 = 20.0;
        request.brakingMps2 = 20.0;
        request.lateralMps2 = 15.0;
        request.maxAngularVelocityRadPerSecond = 1.0;
        request.maxAngularAccelerationRadPerSecond2 = 1.0;
        request.initialSpeedMps = 0.0;
        request.hasInitialForward = true;
        request.initialForward = {0.0, 0.0, 1.0};
        request.roundTurns = false;
        request.gateSpacingMeters = 250.0;
        request.terminalGateSpacingMeters = 100.0;

        const auto plan =
            SegmentedRouteComposer::plan(
                request,
                traffic
            );

        require(
            plan.valid(),
            "segmented composer did not produce executable route"
        );
        const auto stageContract =
            TrafficRoutePlanAdapter::buildStageContract(
                traffic,
                plan,
                glm::dvec3(0.0, 1.0, 0.0)
            );
        require(
            stageContract.valid,
            "traffic route-plan adapter rejected segmented geometry"
        );
        const auto& stages = stageContract.stages;
        require(
            stages.size() == 5,
            "segmented composer did not publish five semantic stages"
        );

        require(
            stages[0].kind == RouteStageKind::FreeTransit,
            "first semantic stage is not free transit"
        );
        require(
            stages[1].kind ==
                RouteStageKind::VolumeEntryCapture,
            "BLUE entry stage missing"
        );
        require(
            stages[2].kind ==
                RouteStageKind::VolumeTransit,
            "BLUE transit stage missing"
        );
        require(
            stages[3].kind ==
                RouteStageKind::VolumeExit,
            "BLUE exit stage missing"
        );
        // The geometry of the final leg is free/unconstrained, but its
        // SEMANTIC owner is TerminalApproach: docking-frame updates must
        // rotate only this final stage, never an earlier BLUE gate.
        require(
            stages[4].kind == RouteStageKind::TerminalApproach,
            "final free geometry lost terminal docking-stage ownership"
        );
        require(
            stages[4].framePolicy ==
                RouteStageFramePolicy::DynamicTerminalFrame,
            "terminal docking frames are not dynamically owned"
        );
        require(
            stages[4].refreshPolicy ==
                RouteStageRefreshPolicy::LiveDuringStage,
            "rotating dock stage stopped refreshing while active"
        );
        require(
            stages[4].sourceId == "terminal" &&
                !stages[4].hardContainment,
            "unconstrained terminal stage inherited BLUE containment"
        );
        require(
            stages[4].endProgressMeters >
                stages[4].startProgressMeters + 1.0,
            "geometric final free leg disappeared"
        );
        require(
            std::abs(
                stages[4].endProgressMeters -
                plan.routeCurves.back().endProgressMeters
            ) < 1.0e-3,
            "terminal stage does not cover the final geometry"
        );
        require(
            glm::length(
                plan.routeCurves.back().endMeters - request.goalMeters
            ) < 1.0e-3,
            "terminal free leg no longer reaches docking HOLD endpoint"
        );

        const auto& transit = stages[2];
        require(
            transit.hardContainment,
            "BLUE transit did not retain KeepInside hard containment"
        );
        require(
            transit.volumeId == "test.blue_volume",
            "BLUE transit lost semantic volume id"
        );
        require(
            transit.sourceId == "test.blue_lane",
            "BLUE transit lost semantic lane id"
        );

        for (std::size_t i = 1; i < stages.size(); ++i)
        {
            require(
                stages[i].startProgressMeters + 1.0e-6 >=
                    stages[i - 1].endProgressMeters,
                "semantic stage progress overlaps or reverses"
            );
        }

        const auto containment =
            RouteVolumeContainmentValidator::
                validateKeepInsideRouteInterval(
                    blue,
                    plan.routeCurves,
                    blue.fromWorldMeters,
                    blue.toWorldMeters,
                    request.agentRadiusMeters
                );

        require(
            containment.valid,
            "published BLUE route interval leaves KeepInside volume"
        );

        require(
            transit.startProgressMeters <
                transit.endProgressMeters,
            "BLUE transit has empty progress interval"
        );

        std::cout
            << "SEGMENTED ROUTE COMPOSER TESTS: PASS\n"
            << " - free -> BLUE entry -> BLUE transit -> BLUE exit -> terminal\n"
            << " - final unconstrained geometry owns live docking frame\n"
            << " - BLUE span retains lane/volume semantic ownership\n"
            << " - KeepInside is a hard route-stage contract\n"
            << " - canonical BLUE interval validates inside eroded volume\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "SEGMENTED ROUTE COMPOSER TESTS: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
