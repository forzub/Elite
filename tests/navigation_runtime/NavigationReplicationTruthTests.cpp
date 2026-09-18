#include "src/game/network/WireDataCodec.h"
#include "src/game/navigation/ClientNavigationWorkspace.h"
#include "src/game/presentation/GuidanceHudPresentation.h"
#include "src/game/simulation/SimulationSnapshot.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

game::simulation::NavigationExecutionSnapshot executionFixture()
{
    game::simulation::NavigationExecutionSnapshot execution;
    execution.valid = true;
    execution.intentRevision = 101u;
    execution.activeTargetRevision = 99u;
    execution.idealLinearAccelerationDemandMapMps2 =
        glm::dvec3(1.0, 2.0, 3.0);
    execution.idealAngularAccelerationDemandMapRadPerSec2 =
        glm::dvec3(0.1, 0.2, 0.3);
    execution.executedLinearAccelerationDemandMapMps2 =
        glm::dvec3(0.8, 1.6, 2.4);
    execution.executedAngularAccelerationDemandMapRadPerSec2 =
        glm::dvec3(0.08, 0.16, 0.24);
    execution.emergency = true;
    execution.hazardUrgency01 = 0.85;
    execution.reactionBlocked = true;
    execution.decisionSampled = false;
    execution.queuedCommandApplied = false;
    execution.pendingCommandCount = 2u;
    return execution;
}

void testNavigationExecutionWireRoundTrip()
{
    SimulationSnapshot source;
    ShipSnapshot ship;
    ship.id = EntityId{77u};
    ship.instanceId = 4242u;
    ship.role = ShipRole::NPC;
    ship.typeId = ObjectType::CobraMk1;
    ship.navigationExecution = executionFixture();
    source.ships.push_back(ship);

    std::vector<std::uint8_t> payload;
    require(
        game::network::wire::encodeSimulationSnapshot(source, payload),
        "navigation execution snapshot encode must succeed"
    );

    SimulationSnapshot decoded;
    require(
        game::network::wire::decodeSimulationSnapshot(payload, decoded),
        "navigation execution snapshot decode must succeed"
    );
    require(decoded.ships.size() == 1u,
            "wire round-trip must preserve ship row");

    const auto* execution =
        std::get_if<game::simulation::NavigationExecutionSnapshot>(
            &decoded.ships.front().navigationExecution
        );
    require(execution != nullptr,
            "wire round-trip must preserve navigation execution payload");
    require(execution->valid,
            "wire round-trip must preserve execution validity");
    require(execution->intentRevision == 101u,
            "wire round-trip must preserve intent revision");
    require(execution->activeTargetRevision == 99u,
            "wire round-trip must preserve active target revision");
    requireNear(
        execution->executedLinearAccelerationDemandMapMps2.y,
        1.6,
        1.0e-12,
        "wire round-trip must preserve executed linear demand"
    );
    requireNear(
        execution->executedAngularAccelerationDemandMapRadPerSec2.z,
        0.24,
        1.0e-12,
        "wire round-trip must preserve executed angular demand"
    );
    require(execution->emergency && execution->reactionBlocked,
            "wire round-trip must preserve execution diagnostics");
    require(execution->pendingCommandCount == 2u,
            "wire round-trip must preserve pending command count");
}

void testStableShipIdentityLookup()
{
    game::navigation::ReplicatedNavigationExecutionState state;

    game::navigation::ReplicatedNavigationExecution entry;
    entry.entityId = EntityId{77u};
    entry.shipInstanceId = 4242u;
    entry.execution = executionFixture();

    state.replace({entry});

    const auto* byEntity = state.find(EntityId{77u});
    require(byEntity != nullptr,
            "replicated execution must be addressable by runtime entity");
    require(byEntity->execution.intentRevision == 101u,
            "entity lookup must retain exact execution revision");

    const auto* byAsset = state.find(
        game::navigation::NavigationAssetRef::ship(4242u)
    );
    require(byAsset != nullptr,
            "replicated execution must be addressable by stable ship identity");
    require(byAsset->entityId == EntityId{77u},
            "stable ship identity must resolve to current runtime entity");

    require(
        state.find(game::navigation::NavigationAssetRef::ship(9999u)) ==
            nullptr,
        "unknown stable ship identity must not fabricate execution state"
    );
}

void testGuidancePresentationReadsServerExecutionTruth()
{
    game::navigation::ClientNavigationWorkspace navigation;
    navigation.routePlan().setStartExecutor(
        game::navigation::NavigationAssetRef::ship(4242u)
    );

    game::navigation::ReplicatedNavigationExecution entry;
    entry.entityId = EntityId{77u};
    entry.shipInstanceId = 4242u;
    entry.execution = executionFixture();
    navigation.syncReplicatedNavigationExecution({entry});

    ClientShipState player;
    const auto presentation =
        game::presentation::buildGuidanceCorridorHudPresentation(
            navigation,
            player,
            100.0
        );

    // A corridor is intentionally not published. Execution truth must still
    // reach HUD/debug presentation without invoking any planner.
    require(!presentation.visible,
            "execution diagnostic fixture must not invent a guidance corridor");
    require(presentation.hasAuthoritativeExecution,
            "guidance presentation must expose replicated server execution");
    require(
        presentation.authoritativeExecutionEntityId == EntityId{77u},
        "guidance presentation must preserve authoritative entity identity"
    );
    require(presentation.authoritativeIntentRevision == 101u,
            "guidance presentation must preserve authoritative intent revision");
    require(presentation.authoritativeActiveTargetRevision == 99u,
            "guidance presentation must preserve active pilot target revision");
    requireNear(
        presentation.authoritativeExecutedLinearAccelerationMapMps2.x,
        0.8,
        1.0e-12,
        "guidance presentation must expose exact executed linear demand"
    );
    requireNear(
        presentation.authoritativeExecutedAngularAccelerationMapRadPerSec2.z,
        0.24,
        1.0e-12,
        "guidance presentation must expose exact executed angular demand"
    );
    require(presentation.authoritativeEmergency,
            "guidance presentation must expose emergency execution state");
    require(presentation.authoritativeReactionBlocked,
            "guidance presentation must expose pilot reaction state");
}

} // namespace

int main()
{
    try
    {
        testNavigationExecutionWireRoundTrip();
        testStableShipIdentityLookup();
        testGuidancePresentationReadsServerExecutionTruth();

        std::cout << "NAVIGATION REPLICATION TRUTH TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION REPLICATION TRUTH TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
