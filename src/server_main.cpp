#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <variant>

#include <glm/geometric.hpp>

#include "src/core/ConsoleOutput.h"
#include "src/platform/RuntimeRoot.h"
#include "src/platform/ProcessSingleInstanceGuard.h"
#include "src/game/server/HeadlessServerEndpoints.h"
#include "src/game/server/ServerRuntime.h"
#include "src/game/server/NetworkServerHost.h"
#include "src/game/diagnostics/NavigationRuntimeLab.h"
#include "src/game/network/NetworkEndpoint.h"
#include "src/world/WorldParams.h"
#include "src/world/coordinates/WorldPosition.h"

namespace
{
std::atomic<bool> g_running {true};

void handleTerminationSignal(int)
{
    g_running.store(false);
}

void printUsage()
{
    std::cout
        << "EliteServer headless authoritative runtime\n"
        << "Usage:\n"
        << "  EliteServer.exe                         Run headless without remote clients\n"
        << "  EliteServer.exe --listen HOST:PORT      Accept remote TCP game sessions\n"
        << "  EliteServer.exe --reset-auth-state      Clear server account registrations before start (development/test)\n"
        << "  EliteServer.exe --self-test             Boot and advance the real server, then exit\n"
        << "  EliteServer.exe --self-test-navigation  Run Stage-12 live navigation proving actor\n"
        << "  EliteServer.exe --help                  Show this help\n";
}

const ShipSnapshot* findShipSnapshot(
    const SimulationSnapshot& snapshot,
    EntityId id
)
{
    const auto it = std::find_if(
        snapshot.ships.begin(),
        snapshot.ships.end(),
        [id](const ShipSnapshot& ship)
        {
            return ship.id == id;
        }
    );

    return it == snapshot.ships.end() ? nullptr : &*it;
}

const ShipSnapshot* findShipSnapshotByInstanceId(
    const SimulationSnapshot& snapshot,
    ShipInstanceId instanceId
)
{
    const auto it = std::find_if(
        snapshot.ships.begin(),
        snapshot.ships.end(),
        [instanceId](const ShipSnapshot& ship)
        {
            return ship.instanceId == instanceId;
        }
    );

    return it == snapshot.ships.end() ? nullptr : &*it;
}

std::uint64_t mapResponseRequestId(
    const game::network::MapResponse& response
)
{
    return std::visit(
        [](const auto& typedResponse)
        {
            return typedResponse.requestId;
        },
        response
    );
}

game::network::SessionHello makeSelfTestIdentity(
    std::uint64_t tokenSeed,
    std::uint64_t tokenSalt
)
{
    game::network::SessionHello hello;
    hello.accountHandle = "selftest-" + std::to_string(tokenSeed);
    for (std::size_t i = 0; i < hello.authToken.bytes.size(); ++i)
    {
        const unsigned shift = static_cast<unsigned>((i % 8u) * 8u);
        hello.authToken.bytes[i] = static_cast<std::uint8_t>(
            ((tokenSeed >> shift) ^ (tokenSalt + i * 37u)) & 0xffu
        );
    }
    if (!hello.authToken.valid())
        hello.authToken.bytes[0] = 1;
    hello.intent = game::network::AuthenticationIntent::Register;
    return hello;
}

int runHeadlessSelfTest()
{
    core::disableRuntimeStdoutNoise();

    game::server::HeadlessServerTransport transportA;
    game::server::HeadlessServerTransport transportB;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;

    std::cerr << "[SELFTEST] headless-server stage=construct-runtime\n";
    game::server::ServerRuntime runtime(
        worldParams,
        debugChannel
    );

    const auto sessionA = runtime.attachPlayerSessionTransport(
        transportA,
        makeSelfTestIdentity(1001u, 1u)
    );

    if (!sessionA ||
        !transportA.hasSessionWelcome() ||
        !transportA.hasBootstrapSnapshot() ||
        !debugChannel.hasBootstrapState())
    {
        std::cerr
            << "[FAIL] headless-server bootstrap protocol/debug publication missing\n";
        return 2;
    }

    const auto welcomeA = transportA.sessionWelcome();
    if (welcomeA.sessionId != sessionA ||
        !welcomeA.playerId ||
        welcomeA.controlledShipInstanceId == 0 ||
        welcomeA.controlledEntityId.value == 0 ||
        welcomeA.starAtlasCatalog.schemaVersion == 0 ||
        welcomeA.starAtlasCatalog.contentFingerprint == 0)
    {
        std::cerr
            << "[FAIL] headless-server invalid authoritative session bootstrap\n";
        return 3;
    }

    const auto initialTick =
        transportA.latestSnapshot().metadata.serverTick;
    const auto initialPublicationCount =
        transportA.snapshotPublicationCount();

    const double step = runtime.fixedStepSeconds();
    if (!std::isfinite(step) || step <= 0.0)
    {
        std::cerr << "[FAIL] headless-server invalid fixed step\n";
        return 4;
    }

    // Dedicated runtime owns two explicit persistent bootstrap players/ships.
    // The second transport is admitted by PlayerId availability, never by
    // hijacking an arbitrary NPC EntityId.
    const auto sessionB = runtime.attachPlayerSessionTransport(
        transportB,
        makeSelfTestIdentity(1002u, 2u)
    );

    if (!sessionB ||
        runtime.connectedPlayerSessionCount() != 2 ||
        !transportB.hasSessionWelcome() ||
        !transportB.hasBootstrapSnapshot())
    {
        std::cerr
            << "[FAIL] headless-server second transport/session admission failed\n";
        return 6;
    }

    const auto welcomeB = transportB.sessionWelcome();
    const EntityId shipBId = welcomeB.controlledEntityId;
    if (welcomeB.sessionId != sessionB ||
        welcomeB.sessionId == welcomeA.sessionId ||
        !welcomeB.playerId ||
        welcomeB.playerId == welcomeA.playerId ||
        welcomeB.controlledShipInstanceId == 0 ||
        welcomeB.controlledShipInstanceId == welcomeA.controlledShipInstanceId ||
        shipBId.value == 0 ||
        shipBId == welcomeA.controlledEntityId)
    {
        std::cerr
            << "[FAIL] headless-server second persistent player/ship authority bootstrap is wrong\n";
        return 7;
    }

    // Bootstrap navigation must already be composed for the second controlled
    // entity rather than copying the legacy primary-player navigation payload.
    const auto& bootstrapB = transportB.latestSnapshot();
    const auto* bootstrapBShip = findShipSnapshot(bootstrapB, shipBId);
    if (!bootstrapBShip ||
        bootstrapB.session.playerNavigation.currentSystemId !=
            bootstrapBShip->transform.motion.systemId)
    {
        std::cerr
            << "[FAIL] headless-server second session navigation view is not entity-owned\n";
        return 8;
    }

    if (bootstrapBShip->transform.motion.systemId >= 0)
    {
        const glm::dvec3 authoritativePosition =
            world::coordinates::fullMeters(
                bootstrapBShip->transform.worldPosition
            );
        const double navigationError = glm::length(
            bootstrapB.session.playerNavigation.systemLocalMeters -
            authoritativePosition
        );

        if (!std::isfinite(navigationError) || navigationError > 0.01)
        {
            std::cerr
                << "[FAIL] headless-server second session navigation position mismatch"
                << " error_m=" << navigationError << "\n";
            return 9;
        }
    }

    ShipControlState controlA;
    controlA.controlTick = 101;
    controlA.forwardInput = 0.35f;

    game::network::ClientMessage messageA;
    messageA.clientTick = 101;
    messageA.payload = controlA;
    transportA.enqueueClientMessage(std::move(messageA));

    ShipControlState controlB;
    controlB.controlTick = 202;
    controlB.yawInput = 0.45f;

    game::network::ClientMessage messageB;
    messageB.clientTick = 202;
    messageB.payload = controlB;
    transportB.enqueueClientMessage(std::move(messageB));

    game::network::GalaxyMapRequest mapA;
    mapA.requestId = 1001;
    transportA.enqueueMapRequest(mapA);

    game::network::GalaxyMapRequest mapB;
    mapB.requestId = 2002;
    transportB.enqueueMapRequest(mapB);

    game::network::TimeSyncRequest timeA;
    timeA.sequence = 3001;
    timeA.clientSendTimeSeconds = 1.25;
    transportA.enqueueTimeSyncRequest(timeA);

    game::network::TimeSyncRequest timeB;
    timeB.sequence = 4002;
    timeB.clientSendTimeSeconds = 2.50;
    transportB.enqueueTimeSyncRequest(timeB);

    // Cross at least two normal replication publications (current cadence is
    // every three authoritative ticks) while both sessions are attached.
    for (int i = 0; i < 8; ++i)
        runtime.advance(step);

    // Normal M7 packets are sparse. The headless endpoint retains a canonical
    // view exactly like a real client world so omission is never mistaken for
    // loss while validating cross-session authoritative state.
    const auto& snapshotA = transportA.latestCanonicalSnapshot();
    const auto& snapshotB = transportB.latestCanonicalSnapshot();
    const auto* shipAOnA =
        findShipSnapshot(snapshotA, welcomeA.controlledEntityId);
    const auto* shipBOnA = findShipSnapshot(snapshotA, shipBId);
    const auto* shipAOnB =
        findShipSnapshot(snapshotB, welcomeA.controlledEntityId);
    const auto* shipBOnB = findShipSnapshot(snapshotB, shipBId);

    if (!shipAOnA || !shipBOnA || !shipAOnB || !shipBOnB)
    {
        std::cerr
            << "[FAIL] headless-server multiplayer snapshots lost controlled ships\n";
        return 10;
    }

    if (shipAOnA->acknowledgedControlTick != 101 ||
        shipAOnB->acknowledgedControlTick != 101 ||
        shipBOnA->acknowledgedControlTick != 202 ||
        shipBOnB->acknowledgedControlTick != 202)
    {
        std::cerr
            << "[FAIL] headless-server independent session command routing failed"
            << " ackA=" << shipAOnA->acknowledgedControlTick
            << " ackB=" << shipBOnB->acknowledgedControlTick
            << "\n";
        return 11;
    }

    if (!transportA.hasMapResponse() ||
        !transportB.hasMapResponse() ||
        transportA.mapResponseCount() != 1 ||
        transportB.mapResponseCount() != 1 ||
        mapResponseRequestId(transportA.latestMapResponse()) != 1001 ||
        mapResponseRequestId(transportB.latestMapResponse()) != 2002)
    {
        std::cerr
            << "[FAIL] headless-server map responses crossed session transports\n";
        return 12;
    }

    if (!transportA.hasTimeSyncResponse() ||
        !transportB.hasTimeSyncResponse() ||
        transportA.latestTimeSyncResponse().sequence != 3001 ||
        transportB.latestTimeSyncResponse().sequence != 4002)
    {
        std::cerr
            << "[FAIL] headless-server time-sync responses crossed connections\n";
        return 13;
    }

    // Each connection receives the same authoritative world facts for now, but
    // its session-navigation view is derived from its own controlled entity.
    const auto checkSessionNavigation = [](const SimulationSnapshot& snapshot,
                                           EntityId controlledId)
    {
        const auto* ship = findShipSnapshot(snapshot, controlledId);
        if (!ship)
            return false;

        if (snapshot.session.playerNavigation.currentSystemId !=
            ship->transform.motion.systemId)
        {
            return false;
        }

        if (ship->transform.motion.systemId < 0)
            return true;

        const glm::dvec3 p = world::coordinates::fullMeters(
            ship->transform.worldPosition
        );
        const double error = glm::length(
            snapshot.session.playerNavigation.systemLocalMeters - p
        );
        return std::isfinite(error) && error <= 0.01;
    };

    if (!checkSessionNavigation(snapshotA, welcomeA.controlledEntityId) ||
        !checkSessionNavigation(snapshotB, shipBId))
    {
        std::cerr
            << "[FAIL] headless-server per-session snapshot navigation failed\n";
        return 14;
    }

    const auto bPublicationsBeforeDetach =
        transportB.snapshotPublicationCount();
    const auto aPublicationsBeforeDetach =
        transportA.snapshotPublicationCount();

    if (!runtime.detachPlayerSessionTransport(sessionB) ||
        runtime.connectedPlayerSessionCount() != 1)
    {
        std::cerr
            << "[FAIL] headless-server secondary session disconnect failed\n";
        return 15;
    }

    ShipControlState disconnectedControlB;
    disconnectedControlB.controlTick = 203;
    disconnectedControlB.forwardInput = 1.0f;
    game::network::ClientMessage disconnectedMessageB;
    disconnectedMessageB.clientTick = 203;
    disconnectedMessageB.payload = disconnectedControlB;
    transportB.enqueueClientMessage(std::move(disconnectedMessageB));

    for (int i = 0; i < 4; ++i)
        runtime.advance(step);

    if (transportB.snapshotPublicationCount() != bPublicationsBeforeDetach ||
        transportA.snapshotPublicationCount() <= aPublicationsBeforeDetach)
    {
        std::cerr
            << "[FAIL] headless-server detached transport still participates or primary stalled\n";
        return 16;
    }

    const auto* shipBAfterDetach =
        findShipSnapshot(transportA.latestCanonicalSnapshot(), shipBId);
    // Disconnect destroys the session-owned numbered-control epoch. The
    // replicated ACK therefore returns to zero until a fresh session sends
    // new controls. If the detached transport were still authoritative, the
    // injected tick 203 above would recreate the stream and ACK 203 instead.
    if (!shipBAfterDetach ||
        shipBAfterDetach->acknowledgedControlTick != 0)
    {
        std::cerr
            << "[FAIL] headless-server detached session control epoch was not reset"
            << " ack="
            << (shipBAfterDetach
                    ? shipBAfterDetach->acknowledgedControlTick
                    : 0)
            << "\n";
        return 17;
    }

    const auto finalTick =
        transportA.latestSnapshot().metadata.serverTick;
    const auto finalPublicationCount =
        transportA.snapshotPublicationCount();

    if (finalTick <= initialTick ||
        finalPublicationCount <= initialPublicationCount)
    {
        std::cerr
            << "[FAIL] headless-server real authoritative runtime did not advance"
            << " initial_tick=" << initialTick
            << " final_tick=" << finalTick
            << " initial_publications=" << initialPublicationCount
            << " final_publications=" << finalPublicationCount
            << "\n";
        return 18;
    }

    std::cerr
        << "[PASS] headless-server boot + two-session authoritative routing smoke"
        << " final_tick=" << finalTick
        << "\n";
    return 0;
}


int runNavigationRuntimeSelfTest()
{
    using game::diagnostics::NavigationRuntimeLabArrivalRadiusMeters;
    using game::diagnostics::NavigationRuntimeLabInstanceId;

    core::disableRuntimeStdoutNoise();

    game::server::HeadlessServerTransport transport;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;

    std::cerr
        << "[NAV-SELFTEST] stage=construct-authoritative-runtime\n";

    game::server::ServerRuntime runtime(
        worldParams,
        debugChannel
    );

    const auto session = runtime.attachPlayerSessionTransport(
        transport,
        makeSelfTestIdentity(1202u, 12u)
    );

    if (!session ||
        !transport.hasBootstrapSnapshot())
    {
        std::cerr
            << "[FAIL] navigation-runtime self-test bootstrap failed\n";
        return 30;
    }

    const double step = runtime.fixedStepSeconds();
    if (!std::isfinite(step) || step <= 0.0)
    {
        std::cerr
            << "[FAIL] navigation-runtime self-test invalid fixed step\n";
        return 31;
    }

    auto observation = runtime.navigationRuntimeLabObservation();
    if (!observation.valid ||
        observation.shipEntityId == 0)
    {
        std::cerr
            << "[FAIL] navigation-runtime lab did not initialize\n";
        return 32;
    }

    const auto& bootstrap =
        transport.latestCanonicalSnapshot();
    const auto* bootstrapLab =
        findShipSnapshotByInstanceId(
            bootstrap,
            NavigationRuntimeLabInstanceId
        );

    if (!bootstrapLab ||
        bootstrapLab->id.value != observation.shipEntityId)
    {
        std::cerr
            << "[FAIL] navigation-runtime lab ship is missing from authoritative bootstrap"
            << " observation_entity=" << observation.shipEntityId
            << "\n";
        return 33;
    }

    constexpr double MaxSimulatedSeconds = 120.0;
    const std::uint64_t maxSteps =
        static_cast<std::uint64_t>(
            std::ceil(MaxSimulatedSeconds / step)
        );

    bool behaviorEvidenceComplete = false;
    double simulatedSeconds = 0.0;

    for (std::uint64_t i = 0; i < maxSteps; ++i)
    {
        runtime.advance(step);
        simulatedSeconds += step;

        observation =
            runtime.navigationRuntimeLabObservation();

        const double progressMeters =
            observation.initialGoalDistanceMeters > 0.0
                ? observation.initialGoalDistanceMeters -
                    observation.minimumGoalDistanceMeters
                : 0.0;

        behaviorEvidenceComplete =
            observation.valid &&
            observation.exactStaticGeometryPublished &&
            observation.exactStaticObstacleCount > 0 &&
            observation.planCount > 0 &&
            observation.executionCount > 0 &&
            observation.obstacleCandidateSeen &&
            observation.obstaclePrimaryConflictSeen &&
            observation.adjustedTargetSeen &&
            observation.executionSeen &&
            observation.nonZeroExecutedDemandSeen &&
            observation.lateralExecutedDemandSeen &&
            observation.passedObstaclePlane &&
            observation.minimumConservativeClearanceMeters > 0.0 &&
            observation.maximumStraightLineDeviationMeters > 1.0 &&
            progressMeters > 3500.0;

        if (behaviorEvidenceComplete)
            break;
    }

    const double progressMeters =
        observation.initialGoalDistanceMeters > 0.0
            ? observation.initialGoalDistanceMeters -
                observation.minimumGoalDistanceMeters
            : 0.0;

    if (!behaviorEvidenceComplete)
    {
        std::cerr
            << "[NAV-SELFTEST]"
            << " simulated_s=" << simulatedSeconds
            << " plans=" << observation.planCount
            << " executions=" << observation.executionCount
            << " obstacle_candidate="
            << observation.obstacleCandidateSeen
            << " obstacle_conflict="
            << observation.obstaclePrimaryConflictSeen
            << " adjusted="
            << observation.adjustedTargetSeen
            << " conflict_hold="
            << observation.conflictHoldSeen
            << " lateral_exec="
            << observation.lateralExecutedDemandSeen
            << " exact_static="
            << observation.exactStaticGeometryPublished
            << " exact_static_obstacles="
            << observation.exactStaticObstacleCount
            << " max_lateral_demand_mps2="
            << observation.maximumExecutedLateralDemandMps2
            << " max_applied_accel_mps2="
            << observation.maximumAppliedEngineAccelerationMps2
            << " max_applied_lateral_accel_mps2="
            << observation.maximumAppliedLateralAccelerationMps2
            << " max_relative_speed_mps="
            << observation.maximumRelativeSpeedMps
            << " max_route_deviation_m="
            << observation.maximumStraightLineDeviationMeters
            << " min_center_distance_m="
            << observation.minimumObstacleCenterDistanceMeters
            << " min_conservative_clearance_m="
            << observation.minimumConservativeClearanceMeters
            << " progress_m=" << progressMeters
            << " remaining_goal_m="
            << observation.minimumGoalDistanceMeters
            << " passed_obstacle_plane="
            << observation.passedObstaclePlane
            << " reached_goal="
            << observation.reachedGoal
            << "\n";
        std::cerr
            << "[FAIL] navigation-runtime live behavior evidence incomplete\n";
        return 37;
    }

    // Sparse replication is intentionally cadence-limited. Do not compare the
    // client's retained execution with the newest per-fixed-step diagnostic
    // observation: they can legitimately represent different epochs. Wait for
    // a sparse packet that actually publishes the lab row, then compare that
    // packet with GameServer's authoritative published snapshot at the exact
    // same server tick.
    constexpr int MaxReplicationProbeSteps = 60;
    std::size_t publicationCount =
        transport.snapshotPublicationCount();

    bool replicationEvidenceComplete = false;
    double replicationErrorMps2 =
        std::numeric_limits<double>::infinity();
    double canonicalReplicationErrorMps2 =
        std::numeric_limits<double>::infinity();
    double replicatedDemandMagnitude = 0.0;
    std::uint64_t replicationServerTick = 0;

    for (int i = 0; i < MaxReplicationProbeSteps; ++i)
    {
        runtime.advance(step);
        simulatedSeconds += step;
        observation =
            runtime.navigationRuntimeLabObservation();

        const std::size_t nextPublicationCount =
            transport.snapshotPublicationCount();
        if (nextPublicationCount == publicationCount)
            continue;

        publicationCount = nextPublicationCount;

        const auto& sparsePacket =
            transport.latestSnapshot();
        const auto* sparseLab =
            findShipSnapshotByInstanceId(
                sparsePacket,
                NavigationRuntimeLabInstanceId
            );
        if (!sparseLab)
            continue;

        const auto* replicatedExecution =
            std::get_if<game::simulation::NavigationExecutionSnapshot>(
                &sparseLab->navigationExecution
            );
        if (!replicatedExecution ||
            !replicatedExecution->valid ||
            replicatedExecution->intentRevision != 1202001)
        {
            continue;
        }

        SimulationSnapshot authoritativePublished;
        if (!runtime.copyAuthoritativePublishedSnapshot(
                authoritativePublished))
        {
            std::cerr
                << "[FAIL] navigation-runtime could not copy authoritative publication\n";
            return 38;
        }

        if (authoritativePublished.metadata.serverTick !=
            sparsePacket.metadata.serverTick)
        {
            std::cerr
                << "[FAIL] navigation-runtime sparse packet/source tick mismatch"
                << " sparse_tick="
                << sparsePacket.metadata.serverTick
                << " source_tick="
                << authoritativePublished.metadata.serverTick
                << "\n";
            return 39;
        }

        const auto* authoritativeLab =
            findShipSnapshotByInstanceId(
                authoritativePublished,
                NavigationRuntimeLabInstanceId
            );
        if (!authoritativeLab)
        {
            std::cerr
                << "[FAIL] navigation-runtime lab missing from authoritative publication"
                << " tick=" << authoritativePublished.metadata.serverTick
                << "\n";
            return 40;
        }

        const auto* authoritativeExecution =
            std::get_if<game::simulation::NavigationExecutionSnapshot>(
                &authoritativeLab->navigationExecution
            );
        if (!authoritativeExecution ||
            !authoritativeExecution->valid)
        {
            std::cerr
                << "[FAIL] navigation-runtime authoritative publication lacks execution"
                << " tick=" << authoritativePublished.metadata.serverTick
                << "\n";
            return 41;
        }

        const glm::dvec3 replicatedDemand(
            replicatedExecution->
                executedLinearAccelerationDemandMapMps2
        );
        const glm::dvec3 authoritativeDemand(
            authoritativeExecution->
                executedLinearAccelerationDemandMapMps2
        );

        replicatedDemandMagnitude =
            glm::length(replicatedDemand);
        replicationErrorMps2 =
            glm::length(
                replicatedDemand -
                authoritativeDemand
            );

        const auto& canonicalSnapshot =
            transport.latestCanonicalSnapshot();
        const auto* canonicalLab =
            findShipSnapshotByInstanceId(
                canonicalSnapshot,
                NavigationRuntimeLabInstanceId
            );
        const auto* canonicalExecution =
            canonicalLab
                ? std::get_if<
                      game::simulation::NavigationExecutionSnapshot
                  >(&canonicalLab->navigationExecution)
                : nullptr;

        if (!canonicalExecution ||
            !canonicalExecution->valid)
        {
            std::cerr
                << "[FAIL] navigation-runtime canonical hydration lost published execution"
                << " tick=" << sparsePacket.metadata.serverTick
                << "\n";
            return 42;
        }

        const glm::dvec3 canonicalDemand(
            canonicalExecution->
                executedLinearAccelerationDemandMapMps2
        );
        canonicalReplicationErrorMps2 =
            glm::length(
                canonicalDemand -
                authoritativeDemand
            );

        const bool metadataMatches =
            replicatedExecution->intentRevision ==
                authoritativeExecution->intentRevision &&
            replicatedExecution->activeTargetRevision ==
                authoritativeExecution->activeTargetRevision &&
            replicatedExecution->emergency ==
                authoritativeExecution->emergency &&
            replicatedExecution->reactionBlocked ==
                authoritativeExecution->reactionBlocked &&
            replicatedExecution->decisionSampled ==
                authoritativeExecution->decisionSampled &&
            replicatedExecution->queuedCommandApplied ==
                authoritativeExecution->queuedCommandApplied &&
            replicatedExecution->pendingCommandCount ==
                authoritativeExecution->pendingCommandCount;

        replicationEvidenceComplete =
            std::isfinite(replicationErrorMps2) &&
            std::isfinite(canonicalReplicationErrorMps2) &&
            replicationErrorMps2 <= 1.0e-12 &&
            canonicalReplicationErrorMps2 <= 1.0e-12 &&
            metadataMatches;

        if (replicationEvidenceComplete)
        {
            replicationServerTick =
                sparsePacket.metadata.serverTick;
            break;
        }
    }

    if (!replicationEvidenceComplete)
    {
        std::cerr
            << "[FAIL] navigation-runtime same-tick sparse replication proof incomplete"
            << " error_mps2=" << replicationErrorMps2
            << " canonical_error_mps2="
            << canonicalReplicationErrorMps2
            << "\n";
        return 43;
    }

    const double finalProgressMeters =
        observation.initialGoalDistanceMeters > 0.0
            ? observation.initialGoalDistanceMeters -
                observation.minimumGoalDistanceMeters
            : 0.0;

    std::cerr
        << "[NAV-SELFTEST]"
        << " simulated_s=" << simulatedSeconds
        << " plans=" << observation.planCount
        << " executions=" << observation.executionCount
        << " obstacle_candidate="
        << observation.obstacleCandidateSeen
        << " obstacle_conflict="
        << observation.obstaclePrimaryConflictSeen
        << " adjusted="
        << observation.adjustedTargetSeen
        << " conflict_hold="
        << observation.conflictHoldSeen
        << " lateral_exec="
        << observation.lateralExecutedDemandSeen
        << " exact_static="
        << observation.exactStaticGeometryPublished
        << " exact_static_obstacles="
        << observation.exactStaticObstacleCount
        << " max_lateral_demand_mps2="
        << observation.maximumExecutedLateralDemandMps2
        << " max_applied_accel_mps2="
        << observation.maximumAppliedEngineAccelerationMps2
        << " max_applied_lateral_accel_mps2="
        << observation.maximumAppliedLateralAccelerationMps2
        << " max_relative_speed_mps="
        << observation.maximumRelativeSpeedMps
        << " max_route_deviation_m="
        << observation.maximumStraightLineDeviationMeters
        << " min_center_distance_m="
        << observation.minimumObstacleCenterDistanceMeters
        << " min_conservative_clearance_m="
        << observation.minimumConservativeClearanceMeters
        << " progress_m=" << finalProgressMeters
        << " remaining_goal_m="
        << observation.minimumGoalDistanceMeters
        << " passed_obstacle_plane="
        << observation.passedObstaclePlane
        << " reached_goal="
        << observation.reachedGoal
        << " replicated_exec_mps2="
        << replicatedDemandMagnitude
        << " replication_tick="
        << replicationServerTick
        << " replication_error_mps2="
        << replicationErrorMps2
        << " canonical_replication_error_mps2="
        << canonicalReplicationErrorMps2
        << "\n";

    // Reaching the final goal is not required for this first live-obstacle
    // gate. The proof closes once the real actor has passed the deliberately
    // blocked obstacle and continues making progress toward that goal.
    if (observation.minimumGoalDistanceMeters <=
        NavigationRuntimeLabArrivalRadiusMeters)
    {
        std::cerr
            << "[NAV-SELFTEST] final goal also reached inside proving window\n";
    }

    std::cerr
        << "[PASS] navigation-runtime CUBE 08 caused authoritative avoidance"
        << " with exact static HitVolume geometry, positive clearance"
        << " and replicated execution\n";
    return 0;
}


int runNetworkServer(
    const game::network::NetworkEndpoint& endpoint,
    bool oneClientSelfTest,
    bool resetAuthState)
{
    // The authoritative server has a large amount of legacy subsystem/debug
    // stdout noise (mesh normalization, hit/seam construction, power-bus
    // registration, etc.). Keep normal headless operation concise: errors,
    // lifecycle and temporary connection diagnostics use stderr.
    core::disableRuntimeStdoutNoise();

    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;
    game::server::NetworkServerHost host(worldParams, debugChannel);

    if (resetAuthState)
    {
        if (!host.resetAuthenticationStateForDevelopment())
        {
            std::cerr << "[EliteServer] cannot reset auth state while sessions are active\n";
            return 7;
        }
        std::cerr << "[EliteServer] development auth registrations reset\n";
    }

    if (!host.listen(endpoint.host, endpoint.port))
    {
        std::cerr << "[EliteServer] listen failed: "
                  << host.lastError() << "\n";
        return 2;
    }

    std::signal(SIGINT, handleTerminationSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, handleTerminationSignal);
#endif

    std::cerr
        << "[EliteServer] listening endpoint="
        << endpoint.host << ':' << host.localPort()
        << " fixed_step_s=" << host.fixedStepSeconds()
        << std::endl;

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    const auto deadline = previous + std::chrono::seconds(15);
    bool sawClient = false;

    while (g_running.load())
    {
        const auto now = Clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - previous).count();
        previous = now;

        host.advance(elapsed);
        sawClient = sawClient || host.acceptedConnectionCount() > 0;

        if (oneClientSelfTest)
        {
            if (sawClient && host.connectedSessionCount() == 0)
            {
                std::cerr
                    << "[PASS] network-server process admitted and detached remote session\n";
                return 0;
            }

            if (now >= deadline)
            {
                std::cerr
                    << "[FAIL] network-server one-client self-test timed out"
                    << " accepted=" << host.acceptedConnectionCount()
                    << " connected=" << host.connectedSessionCount()
                    << "\n";
                return 3;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    host.close();
    std::cerr << "[EliteServer] stopped\n";
    return 0;
}

int runHeadlessServer(bool resetAuthState)
{
    core::disableRuntimeStdoutNoise();

    game::server::HeadlessServerTransport transport;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;

    game::server::ServerRuntime runtime(
        worldParams,
        transport,
        debugChannel
    );

    if (resetAuthState && !runtime.resetAuthenticationStateForDevelopment())
    {
        std::cerr << "[EliteServer] cannot reset auth state while sessions are active\n";
        return 7;
    }

    std::signal(SIGINT, handleTerminationSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, handleTerminationSignal);
#endif

    std::cerr
        << "[EliteServer] authoritative headless runtime started"
        << " fixed_step_s=" << runtime.fixedStepSeconds()
        << "\n";
    std::cerr
        << "[EliteServer] remote listener disabled in this mode; "
        << "use --listen HOST:PORT to accept clients\n";

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();

    while (g_running.load())
    {
        const auto now = Clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - previous).count();
        previous = now;

        runtime.advance(elapsed);

        // ServerRunner owns fixed-step debt/catch-up.  This sleep only avoids
        // busy-spinning the process while there is no socket event loop yet.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cerr << "[EliteServer] stopped\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    std::string runtimeRootError;
    if (!platform::initializeExecutableRuntimeRoot(&runtimeRootError))
    {
        std::cerr << "[EliteServer] runtime root initialization failed: "
                  << runtimeRootError << "\n";
        return 6;
    }

    bool headlessSelfTest = false;
    bool navigationSelfTest = false;
    bool oneClientSelfTest = false;
    bool resetAuthState = false;
    bool haveListenEndpoint = false;
    game::network::NetworkEndpoint listenEndpoint;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--self-test")
        {
            headlessSelfTest = true;
            continue;
        }

        if (arg == "--self-test-navigation")
        {
            navigationSelfTest = true;
            continue;
        }

        if (arg == "--self-test-one-client")
        {
            oneClientSelfTest = true;
            continue;
        }

        if (arg == "--reset-auth-state")
        {
            resetAuthState = true;
            continue;
        }

        if (arg == "--listen")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "[EliteServer] --listen requires HOST:PORT\n";
                return 1;
            }

            std::string error;
            if (!game::network::parseNetworkEndpoint(
                    argv[++i],
                    listenEndpoint,
                    &error))
            {
                std::cerr << "[EliteServer] invalid listen endpoint: "
                          << error << "\n";
                return 1;
            }
            haveListenEndpoint = true;
            continue;
        }

        if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }

        std::cerr << "[EliteServer] unknown option: " << arg << "\n";
        printUsage();
        return 1;
    }

    if (oneClientSelfTest && !haveListenEndpoint)
    {
        std::cerr
            << "[EliteServer] --self-test-one-client requires --listen HOST:PORT\n";
        return 1;
    }

    // Help/argument validation remains available while another server is alive.
    // Every mode that actually constructs authoritative runtime state must own
    // the same process-wide OS lock.
    platform::ProcessSingleInstanceGuard instanceGuard("EliteServer");
    if (!instanceGuard.ownsInstance())
    {
        if (instanceGuard.anotherInstanceRunning())
        {
            std::cerr
                << "[EliteServer] another EliteServer instance is already running; "
                << "only one server instance is allowed on this machine\n";
            return 4;
        }

        std::cerr
            << "[EliteServer] failed to establish single-instance guard: "
            << instanceGuard.error() << "\n";
        return 5;
    }

    if (headlessSelfTest)
        return runHeadlessSelfTest();

    if (navigationSelfTest)
        return runNavigationRuntimeSelfTest();

    if (haveListenEndpoint)
        return runNetworkServer(listenEndpoint, oneClientSelfTest, resetAuthState);

    if (resetAuthState)
    {
        std::cerr << "[EliteServer] --reset-auth-state currently requires --listen; "
                  << "M8E.3 will apply it to durable storage before any runtime session exists\n";
        return 1;
    }

    return runHeadlessServer(false);
}
