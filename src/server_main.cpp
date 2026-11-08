#include <algorithm>
#include "src/game/server/GameServer.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <variant>
#include <deque>
#include <map>
#include <memory>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#include <glm/geometric.hpp>

#include "src/core/ConsoleOutput.h"
#include "src/platform/RuntimeRoot.h"
#include "src/platform/ProcessSingleInstanceGuard.h"
#include "src/game/server/HeadlessServerEndpoints.h"
#include "src/game/server/ServerRuntime.h"
#include "src/game/server/ServerTickProfile.h"
#include "src/game/server/NetworkServerHost.h"
#include "src/game/diagnostics/NavigationRuntimeLab.h"
#include "src/game/network/NetworkEndpoint.h"
#include "src/world/WorldParams.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/game/network/WireMessageCodec.h"
#include "src/game/network/WireCompression.h"

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
        << "  EliteServer.exe --load-test [K|2-3] [SECONDS] [move]  K player bots per star system (0 = one bot, the baseline);\n"
        << "                                           move: travel / jump cycles (ELITE_DEV_JUMP_CHARGE_S, ELITE_DEV_ECONOMY_TICK_S)\n"
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

    bool visibleHorizonEvidenceComplete = false;
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

        if (observation.placementProbeCaptured)
        {
            const glm::dvec3 expectedPlacement =
                game::diagnostics::NavigationRuntimeLabStartVisualLocalMeters;
            const double placementErrorMeters =
                glm::length(
                    observation.placementPositionMap -
                    expectedPlacement
                );

            if (!std::isfinite(placementErrorMeters) ||
                placementErrorMeters >
                    game::diagnostics::
                        NavigationRuntimeLabPlacementToleranceMeters)
            {
                std::cerr
                    << "[NAV-SELFTEST]"
                    << " placement_error_m="
                    << placementErrorMeters
                    << " placement_map=("
                    << observation.placementPositionMap.x << ","
                    << observation.placementPositionMap.y << ","
                    << observation.placementPositionMap.z << ")"
                    << " expected_placement_map=("
                    << expectedPlacement.x << ","
                    << expectedPlacement.y << ","
                    << expectedPlacement.z << ")"
                    << " placement_motion_local_tactical=("
                    << observation.placementMotionLocalTactical.x << ","
                    << observation.placementMotionLocalTactical.y << ","
                    << observation.placementMotionLocalTactical.z << ")"
                    << " placement_time_s="
                    << observation.placementServerTimeSeconds
                    << "\n";
                std::cerr
                    << "[FAIL] reference-frame placement does not round-trip "
                    << "to the configured NavigationRuntimeLab start\n";
                return 53;
            }
        }

        if (observation.rotatingActorCandidateSeen &&
            !observation.rotatingActorAngularVelocityVerified)
        {
            std::cerr
                << "[NAV-SELFTEST]"
                << " rotating_actor_seen="
                << observation.rotatingActorCandidateSeen
                << " rotating_actor_entity="
                << observation.rotatingActorEntityId
                << " rotating_actor_omega_expected=("
                << observation.expectedRotatingActorAngularVelocityMapRadPerSecond.x << ","
                << observation.expectedRotatingActorAngularVelocityMapRadPerSecond.y << ","
                << observation.expectedRotatingActorAngularVelocityMapRadPerSecond.z << ")"
                << " rotating_actor_omega_observed=("
                << observation.observedRotatingActorAngularVelocityMapRadPerSecond.x << ","
                << observation.observedRotatingActorAngularVelocityMapRadPerSecond.y << ","
                << observation.observedRotatingActorAngularVelocityMapRadPerSecond.z << ")"
                << " rotating_actor_omega_error="
                << observation.rotatingActorAngularVelocityErrorRadPerSecond
                << "\n";
            std::cerr
                << "[FAIL] NavigationMap did not preserve rotating infrastructure "
                << "angular velocity through the live working-frame boundary\n";
            return 54;
        }

        if (observation.exactStaticGeometryPublished &&
            !observation.configuredRouteExactObstacleBlockPublished)
        {
            std::cerr
                << "[NAV-SELFTEST]"
                << " exact_static="
                << observation.exactStaticGeometryPublished
                << " exact_static_obstacles="
                << observation.exactStaticObstacleCount
                << " configured_route_exact_block="
                << observation.configuredRouteExactObstacleBlockPublished
                << " obstacle_entity="
                << observation.obstacleEntityId
                << "\n";
            std::cerr
                << "[FAIL] configured NavigationRuntimeLab route does not "
                << "intersect exact CUBE 08 HitVolume\n";
            return 38;
        }

        if (observation.exactStaticGeometryPublished &&
            !observation.slitPortalExactOpenPublished)
        {
            std::cerr
                << "[NAV-SELFTEST]"
                << " slit_exact_open="
                << observation.slitPortalExactOpenPublished
                << " slit_exact_obstacles_examined="
                << observation.slitPortalExactObstaclesExamined
                << "\n";
            std::cerr
                << "[FAIL] authored exact-static slit tunnel does not admit "
                << "the current ship envelope\n";
            return 57;
        }

        // firstLiveNominal* remains diagnostic-only. With a finite-depth
        // portal/tunnel topology the first bounded target may legitimately end
        // on a different region/portal boundary; exact physical safety is
        // proven separately by the exact-obstacle sweep below.

        if (observation.firstExactStaticViolationCaptured)
        {
            std::cerr
                << "[NAV-SELFTEST]"
                << " exact_static_violation=1"
                << " violation_entity="
                << observation.firstExactStaticViolationEntityId
                << " proving_obstacle_entity="
                << observation.obstacleEntityId
                << " violation_start_map=("
                << observation.firstExactStaticViolationStartMap.x << ","
                << observation.firstExactStaticViolationStartMap.y << ","
                << observation.firstExactStaticViolationStartMap.z << ")"
                << " violation_end_map=("
                << observation.firstExactStaticViolationEndMap.x << ","
                << observation.firstExactStaticViolationEndMap.y << ","
                << observation.firstExactStaticViolationEndMap.z << ")"
                << " selected_target_map=("
                << observation.firstExactStaticViolationSelectedTargetMap.x << ","
                << observation.firstExactStaticViolationSelectedTargetMap.y << ","
                << observation.firstExactStaticViolationSelectedTargetMap.z << ")"
                << " planner_status="
                << static_cast<int>(
                    observation.firstExactStaticViolationPlannerStatus
                )
                << "\n";
            std::cerr
                << "[FAIL] authoritative ship swept through exact static "
                << "HitVolume while executing accepted navigation\n";
            return 52;
        }

        visibleHorizonEvidenceComplete =
            observation.valid &&
            observation.exactStaticGeometryPublished &&
            observation.exactStaticObstacleCount > 0 &&
            observation.configuredRouteExactObstacleBlockPublished &&
            observation.exactStaticMotionSamples > 0 &&
            !observation.exactStaticViolationSeen &&
            observation.planCount > 0 &&
            observation.schedulerEnqueueAcceptedCount > 0 &&
            observation.schedulerDispatchCount > 0 &&
            observation.schedulerCompletedCurrentCount ==
                observation.schedulerDispatchCount &&
            observation.schedulerCompletedStaleCount == 0 &&
            observation.schedulerDispatchCount == observation.planCount &&
            observation.schedulerMaximumPendingJobs == 1 &&
            observation.schedulerMaximumInFlightJobs == 1 &&
            observation.schedulerPlannerMaximumMicroseconds > 0.0 &&
            observation.executionCount > 0 &&
            observation.dynamicQueryCount > 0 &&
            observation.maximumDynamicCandidateCount >= 2 &&
            observation.movingGapUpperEntityId != 0 &&
            observation.movingGapLowerEntityId != 0 &&
            observation.movingGapPairCandidateSeen &&
            observation.movingGapKinematicsVerified &&
            observation.slitPortalExactOpenPublished &&
            observation.visibleHorizonBypassSeen &&
            observation.visibleHorizonBypassActive &&
            observation.executionSeen &&
            observation.nonZeroExecutedDemandSeen &&
            observation.lateralExecutedDemandSeen;

        if (visibleHorizonEvidenceComplete)
            break;
    }

    const double progressMeters =
        observation.initialGoalDistanceMeters > 0.0
            ? observation.initialGoalDistanceMeters -
                observation.minimumGoalDistanceMeters
            : 0.0;

    if (!visibleHorizonEvidenceComplete)
    {
        std::cerr
            << "[NAV-SELFTEST]"
            << " simulated_s=" << simulatedSeconds
            << " plans=" << observation.planCount
            << " scheduler_enqueued="
            << observation.schedulerEnqueueAcceptedCount
            << " scheduler_replaced="
            << observation.schedulerEnqueueReplacedCount
            << " scheduler_duplicates="
            << observation.schedulerEnqueueDuplicateCount
            << " scheduler_stale_enqueue="
            << observation.schedulerEnqueueStaleCount
            << " scheduler_dispatched="
            << observation.schedulerDispatchCount
            << " scheduler_completed_current="
            << observation.schedulerCompletedCurrentCount
            << " scheduler_completed_stale="
            << observation.schedulerCompletedStaleCount
            << " scheduler_max_pending="
            << observation.schedulerMaximumPendingJobs
            << " scheduler_max_inflight="
            << observation.schedulerMaximumInFlightJobs
            << " scheduler_dispatch_total_us="
            << observation.schedulerDispatchTotalMicroseconds
            << " scheduler_dispatch_max_us="
            << observation.schedulerDispatchMaximumMicroseconds
            << " scheduler_planner_total_us="
            << observation.schedulerPlannerTotalMicroseconds
            << " scheduler_planner_max_us="
            << observation.schedulerPlannerMaximumMicroseconds
            << " executions=" << observation.executionCount
            << " obstacle_candidate="
            << observation.obstacleCandidateSeen
            << " obstacle_conflict="
            << observation.obstaclePrimaryConflictSeen
            << " exact_obstacle_block="
            << observation.obstacleExactStaticBlockSeen
            << " dynamic_queries="
            << observation.dynamicQueryCount
            << " max_dynamic_candidates="
            << observation.maximumDynamicCandidateCount
            << " rotating_actor_seen="
            << observation.rotatingActorCandidateSeen
            << " rotating_actor_omega_verified="
            << observation.rotatingActorAngularVelocityVerified
            << " rotating_actor_omega_error="
            << observation.rotatingActorAngularVelocityErrorRadPerSecond
            << " moving_gap_pair="
            << observation.movingGapPairCandidateSeen
            << " moving_gap_kinematics="
            << observation.movingGapKinematicsVerified
            << " moving_gap_velocity_error="
            << observation.movingGapMaximumVelocityErrorMps
            << " moving_precision="
            << observation.movingPrecisionAttemptedSeen
            << " moving_passage_feasible="
            << observation.movingPassageFeasibleSeen
            << " moving_eval_status="
            << static_cast<int>(
                observation.movingPassageLastEvaluatorStatus
            )
            << " moving_req_fwd_mps2="
            << observation.movingPassageRequiredPeakForwardAccelerationMps2
            << " moving_req_rev_mps2="
            << observation.movingPassageRequiredPeakReverseAccelerationMps2
            << " moving_req_lat_mps2="
            << observation.movingPassageRequiredPeakLateralAccelerationMps2
            << " moving_req_vert_mps2="
            << observation.movingPassageRequiredPeakVerticalAccelerationMps2
            << " moving_sample_clearance_m="
            << observation.movingPassageMinimumSampleClearanceMeters
            << " moving_continuous_clearance_m="
            << observation.movingPassageMinimumContinuousClearanceMeters
            << " moving_passage_static_safe="
            << observation.movingPassageStaticSafeSeen
            << " moving_passage_authority="
            << observation.movingPassageAuthoritySeen
            << " moving_passage_authority_active="
            << observation.movingPassageAuthorityActive
            << " moving_passage_executed="
            << observation.movingPassageExecutedSeen
            << " moving_passage_execution_active="
            << observation.movingPassageExecutionActive
            << " moving_passage_applied="
            << observation.movingPassageAppliedAccelerationSeen
            << " visible_horizon_bypass="
            << observation.visibleHorizonBypassSeen
            << " visible_horizon_bypass_active="
            << observation.visibleHorizonBypassActive
            << " visible_horizon_direct_recovered="
            << observation.visibleHorizonDirectRecoveredSeen
            << " visible_horizon_max_offset_m="
            << observation.maximumVisibleHorizonOffsetMeters
            << " moving_gap_passed="
            << observation.movingGapPlanePassed
            << " slit_exact_open="
            << observation.slitPortalExactOpenPublished
            << " slit_portal="
            << observation.slitPortalWaypointSeen
            << " slit_entry_capture="
            << observation.slitEntryCaptureSeen
            << " slit_entry_vel_aligned="
            << observation.slitEntryVelocityAlignedSeen
            << " slit_entry_fwd_aligned="
            << observation.slitEntryForwardAlignedSeen
            << " slit_entry_crossed_aligned="
            << observation.slitEntryPlaneCrossedAligned
            << " slit_entry_vel_angle_rad="
            << observation.slitEntryVelocityAngleRad
            << " slit_entry_fwd_angle_rad="
            << observation.slitEntryForwardAngleRad
            << " slit_entry_lateral_mps="
            << observation.slitEntryLateralSpeedMps
            << " slit_entry_cross_track_m="
            << observation.slitEntryCrossTrackMeters
            << " slit_passed="
            << observation.slitTunnelPassed
            << " slit_margin_m="
            << observation.slitTunnelCrossingMarginMeters
            << " max_moving_exec_mps2="
            << observation.maximumMovingPassageExecutedDemandMps2
            << " max_moving_applied_mps2="
            << observation.maximumMovingPassageAppliedAccelerationMps2
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
            << " configured_route_exact_block="
            << observation.configuredRouteExactObstacleBlockPublished
            << " first_live_probe_blocked="
            << observation.firstLiveNominalExactBlocked
            << " first_live_blocker_entity="
            << observation.firstLiveNominalBlockingEntityId
            << " first_live_horizon_m="
            << observation.firstLiveHorizonMeters
            << " exact_static_query="
            << observation.exactStaticQuerySeen
            << " exact_static_block="
            << observation.nominalStaticBlockSeen
            << " max_exact_static_examined="
            << observation.maximumExactStaticObstaclesExamined
            << " exact_static_motion_samples="
            << observation.exactStaticMotionSamples
            << " exact_static_violation="
            << observation.exactStaticViolationSeen
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
            << "[FAIL] navigation-runtime projected visible-horizon bypass evidence incomplete\n";
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

        // Capture replication from an epoch in which the moving obstacle pair
        // is actively causing a visible-horizon bypass. This proves the
        // selected local steering intent reaches the same authoritative
        // PilotSkillExecutor / ShipControlState publication seam.
        if (!observation.visibleHorizonBypassActive ||
            !observation.executionSeen)
        {
            continue;
        }

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
                executedLinearAccelerationDemandSystemMps2
        );
        const glm::dvec3 authoritativeDemand(
            authoritativeExecution->
                executedLinearAccelerationDemandSystemMps2
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
                executedLinearAccelerationDemandSystemMps2
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
            replicatedDemandMagnitude > 1.0e-6 &&
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

    // Replication was captured while projected visible-horizon bypass owned the
    // moving-pair bypass. Continue the SAME authoritative run. The ordered
    // acceptance is:
    // visible-horizon bypass -> replicated active epoch -> obstacle-plane crossing
    // -> direct-line recovery -> portal capture -> tunnel transit/exit.
    bool behaviorEvidenceComplete = false;

    while (simulatedSeconds + step <= MaxSimulatedSeconds + 1.0e-9)
    {
        runtime.advance(step);
        simulatedSeconds += step;
        observation = runtime.navigationRuntimeLabObservation();

        if (observation.firstExactStaticViolationCaptured)
        {
            std::cerr
                << "[FAIL] moving-passage continuation crossed exact static geometry"
                << " violation_entity="
                << observation.firstExactStaticViolationEntityId
                << " segment_revision="
                << observation.firstExactStaticViolationAcceptedSegmentRevision
                << " planner_status="
                << static_cast<int>(
                    observation.firstExactStaticViolationPlannerStatus
                )
                << " last_replan_reason="
                << static_cast<int>(
                    observation.firstExactStaticViolationLastReplanReason
                )
                << " static_invalidations="
                << observation.acceptedSegmentStaticSafetyInvalidationCount
                << " emergency_recoveries="
                << observation.acceptedSegmentEmergencyRecoveryCount
                << " emergency_recovery_active="
                << observation.acceptedSegmentEmergencyRecoveryActive
                << " stopping_reserve_blocked="
                << observation.acceptedSegmentLastStoppingReserveBlocked
                << " stopping_reserve_s="
                << observation.acceptedSegmentLastStoppingReserveSeconds
                << " stopping_reserve_m="
                << observation.acceptedSegmentLastStoppingReserveDistanceMeters
                << " previous_monitor_blocker="
                << observation.firstExactStaticViolationPreviousBlockingEntityId
                << " previous_target_blocked="
                << observation.firstExactStaticViolationPreviousTargetBlocked
                << " previous_forecast_blocked="
                << observation.firstExactStaticViolationPreviousForecastBlocked
                << " previous_executed_forecast_blocked="
                << observation.firstExactStaticViolationPreviousExecutedForecastBlocked
                << " previous_probe_time_s="
                << observation.firstExactStaticViolationPreviousProbeTimeSeconds
                << " previous_forecast_s="
                << observation.firstExactStaticViolationPreviousForecastSeconds
                << " violation_start=("
                << observation.firstExactStaticViolationStartMap.x << ","
                << observation.firstExactStaticViolationStartMap.y << ","
                << observation.firstExactStaticViolationStartMap.z << ")"
                << " violation_end=("
                << observation.firstExactStaticViolationEndMap.x << ","
                << observation.firstExactStaticViolationEndMap.y << ","
                << observation.firstExactStaticViolationEndMap.z << ")"
                << " current_velocity=("
                << observation.firstExactStaticViolationCurrentVelocityMapMps.x << ","
                << observation.firstExactStaticViolationCurrentVelocityMapMps.y << ","
                << observation.firstExactStaticViolationCurrentVelocityMapMps.z << ")"
                << " accepted_target=("
                << observation.firstExactStaticViolationAcceptedTargetMap.x << ","
                << observation.firstExactStaticViolationAcceptedTargetMap.y << ","
                << observation.firstExactStaticViolationAcceptedTargetMap.z << ")"
                << " accepted_target_velocity=("
                << observation.firstExactStaticViolationAcceptedTargetVelocityMapMps.x << ","
                << observation.firstExactStaticViolationAcceptedTargetVelocityMapMps.y << ","
                << observation.firstExactStaticViolationAcceptedTargetVelocityMapMps.z << ")"
                << " last_executed_demand=("
                << observation.firstExactStaticViolationLastExecutedDemandMapMps2.x << ","
                << observation.firstExactStaticViolationLastExecutedDemandMapMps2.y << ","
                << observation.firstExactStaticViolationLastExecutedDemandMapMps2.z << ")"
                << " previous_probe_start=("
                << observation.firstExactStaticViolationPreviousProbeStartMap.x << ","
                << observation.firstExactStaticViolationPreviousProbeStartMap.y << ","
                << observation.firstExactStaticViolationPreviousProbeStartMap.z << ")"
                << " previous_target=("
                << observation.firstExactStaticViolationPreviousTargetMap.x << ","
                << observation.firstExactStaticViolationPreviousTargetMap.y << ","
                << observation.firstExactStaticViolationPreviousTargetMap.z << ")"
                << " previous_forecast_end=("
                << observation.firstExactStaticViolationPreviousForecastEndMap.x << ","
                << observation.firstExactStaticViolationPreviousForecastEndMap.y << ","
                << observation.firstExactStaticViolationPreviousForecastEndMap.z << ")"
                << " previous_executed_forecast_end=("
                << observation.firstExactStaticViolationPreviousExecutedForecastEndMap.x << ","
                << observation.firstExactStaticViolationPreviousExecutedForecastEndMap.y << ","
                << observation.firstExactStaticViolationPreviousExecutedForecastEndMap.z << ")"
                << " previous_velocity=("
                << observation.firstExactStaticViolationPreviousVelocityMapMps.x << ","
                << observation.firstExactStaticViolationPreviousVelocityMapMps.y << ","
                << observation.firstExactStaticViolationPreviousVelocityMapMps.z << ")"
                << " previous_ideal_accel=("
                << observation.firstExactStaticViolationPreviousIdealAccelerationMapMps2.x << ","
                << observation.firstExactStaticViolationPreviousIdealAccelerationMapMps2.y << ","
                << observation.firstExactStaticViolationPreviousIdealAccelerationMapMps2.z << ")"
                << " previous_executed_accel=("
                << observation.firstExactStaticViolationPreviousExecutedAccelerationMapMps2.x << ","
                << observation.firstExactStaticViolationPreviousExecutedAccelerationMapMps2.y << ","
                << observation.firstExactStaticViolationPreviousExecutedAccelerationMapMps2.z << ")"
                << "\n";
            return 55;
        }

        const double continuedProgressMeters =
            observation.initialGoalDistanceMeters > 0.0
                ? observation.initialGoalDistanceMeters -
                    observation.minimumGoalDistanceMeters
                : 0.0;

        behaviorEvidenceComplete =
            observation.movingGapPlanePassed &&
            observation.visibleHorizonBypassSeen &&
            observation.visibleHorizonDirectRecoveredSeen &&
            observation.slitPortalExactOpenPublished &&
            observation.slitPortalWaypointSeen &&
            observation.slitEntryCaptureSeen &&
            observation.slitEntryVelocityAlignedSeen &&
            observation.slitEntryForwardAlignedSeen &&
            observation.slitEntryPlaneCrossedAligned &&
            observation.slitTunnelPassed &&
            observation.exactStaticGeometryPublished &&
            observation.exactStaticObstacleCount > 0 &&
            observation.configuredRouteExactObstacleBlockPublished &&
            observation.exactStaticQuerySeen &&
            observation.maximumExactStaticObstaclesExamined > 0 &&
            observation.exactStaticMotionSamples > 0 &&
            !observation.exactStaticViolationSeen &&
            observation.rotatingActorEntityId != 0 &&
            observation.rotatingActorCandidateSeen &&
            observation.rotatingActorAngularVelocityVerified &&
            !observation.obstacleCandidateSeen &&
            !observation.obstaclePrimaryConflictSeen &&
            observation.executionSeen &&
            observation.nonZeroExecutedDemandSeen &&
            observation.lateralExecutedDemandSeen &&
            observation.passedObstaclePlane &&
            observation.maximumStraightLineDeviationMeters > 1.0 &&
            continuedProgressMeters > 3500.0;

        if (behaviorEvidenceComplete)
            break;
    }

    if (!behaviorEvidenceComplete)
    {
        std::cerr
            << "[FAIL] visibility-bypass replication succeeded but "
            << "the ordered live flight did not complete moving-pair bypass, "
            << "direct recovery and exact-static tunnel passage inside the 120 s bound"
            << " moving_gap_passed=" << observation.movingGapPlanePassed
            << " slit_portal=" << observation.slitPortalWaypointSeen
            << " slit_entry_capture=" << observation.slitEntryCaptureSeen
            << " slit_entry_vel_aligned="
            << observation.slitEntryVelocityAlignedSeen
            << " slit_entry_fwd_aligned="
            << observation.slitEntryForwardAlignedSeen
            << " slit_entry_crossed_aligned="
            << observation.slitEntryPlaneCrossedAligned
            << " slit_entry_vel_angle_rad="
            << observation.slitEntryVelocityAngleRad
            << " slit_entry_fwd_angle_rad="
            << observation.slitEntryForwardAngleRad
            << " slit_entry_lateral_mps="
            << observation.slitEntryLateralSpeedMps
            << " slit_entry_cross_track_m="
            << observation.slitEntryCrossTrackMeters
            << " slit_passed=" << observation.slitTunnelPassed
            << " slit_margin_m=" << observation.slitTunnelCrossingMarginMeters
            << " passed_obstacle_plane=" << observation.passedObstaclePlane
            << " exact_static_violation=" << observation.exactStaticViolationSeen
            << " simulated_s=" << simulatedSeconds
            << " first_bypass_captured="
            << observation.firstVisibleHorizonBypassCaptured
            << " first_bypass_t_s="
            << observation.firstVisibleHorizonBypassTimeSeconds
            << " first_bypass_segment="
            << observation.firstVisibleHorizonBypassSegmentRevision
            << " first_bypass_replan_reason="
            << static_cast<int>(
                observation.firstVisibleHorizonBypassReplanReason
            )
            << " first_bypass_offset_m="
            << observation.firstVisibleHorizonBypassOffsetMeters
            << " first_bypass_agent_pos=("
            << observation.firstVisibleHorizonBypassAgentPositionMap.x << ","
            << observation.firstVisibleHorizonBypassAgentPositionMap.y << ","
            << observation.firstVisibleHorizonBypassAgentPositionMap.z << ")"
            << " first_bypass_agent_vel=("
            << observation.firstVisibleHorizonBypassAgentVelocityMapMps.x << ","
            << observation.firstVisibleHorizonBypassAgentVelocityMapMps.y << ","
            << observation.firstVisibleHorizonBypassAgentVelocityMapMps.z << ")"
            << " first_bypass_target=("
            << observation.firstVisibleHorizonBypassSelectedTargetMap.x << ","
            << observation.firstVisibleHorizonBypassSelectedTargetMap.y << ","
            << observation.firstVisibleHorizonBypassSelectedTargetMap.z << ")"
            << " first_bypass_desired_vel=("
            << observation.firstVisibleHorizonBypassDesiredVelocityMapMps.x << ","
            << observation.firstVisibleHorizonBypassDesiredVelocityMapMps.y << ","
            << observation.firstVisibleHorizonBypassDesiredVelocityMapMps.z << ")"
            << " first_bypass_align_forward="
            << observation.firstVisibleHorizonBypassAcceptedAlignForward
            << " first_bypass_forward=("
            << observation.firstVisibleHorizonBypassAcceptedForwardMap.x << ","
            << observation.firstVisibleHorizonBypassAcceptedForwardMap.y << ","
            << observation.firstVisibleHorizonBypassAcceptedForwardMap.z << ")"
            << " first_bypass_ideal_accel=("
            << observation.firstVisibleHorizonBypassIdealAccelerationMapMps2.x << ","
            << observation.firstVisibleHorizonBypassIdealAccelerationMapMps2.y << ","
            << observation.firstVisibleHorizonBypassIdealAccelerationMapMps2.z << ")"
            << " first_bypass_exec_captured="
            << observation.firstVisibleHorizonBypassExecutedCaptured
            << " first_bypass_exec_accel=("
            << observation.firstVisibleHorizonBypassExecutedAccelerationMapMps2.x << ","
            << observation.firstVisibleHorizonBypassExecutedAccelerationMapMps2.y << ","
            << observation.firstVisibleHorizonBypassExecutedAccelerationMapMps2.z << ")"
            << " first_bypass_applied_captured="
            << observation.firstVisibleHorizonBypassAppliedCaptured
            << " first_bypass_main_accel=("
            << observation.firstVisibleHorizonBypassAppliedMainAccelerationMapMps2.x << ","
            << observation.firstVisibleHorizonBypassAppliedMainAccelerationMapMps2.y << ","
            << observation.firstVisibleHorizonBypassAppliedMainAccelerationMapMps2.z << ")"
            << " first_bypass_rcs_accel=("
            << observation.firstVisibleHorizonBypassAppliedRcsAccelerationMapMps2.x << ","
            << observation.firstVisibleHorizonBypassAppliedRcsAccelerationMapMps2.y << ","
            << observation.firstVisibleHorizonBypassAppliedRcsAccelerationMapMps2.z << ")"
            << " first_bypass_total_accel=("
            << observation.firstVisibleHorizonBypassAppliedTotalAccelerationMapMps2.x << ","
            << observation.firstVisibleHorizonBypassAppliedTotalAccelerationMapMps2.y << ","
            << observation.firstVisibleHorizonBypassAppliedTotalAccelerationMapMps2.z << ")"
            << "\n";
        return 56;
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
        << " scheduler_enqueued="
        << observation.schedulerEnqueueAcceptedCount
        << " scheduler_replaced="
        << observation.schedulerEnqueueReplacedCount
        << " scheduler_duplicates="
        << observation.schedulerEnqueueDuplicateCount
        << " scheduler_stale_enqueue="
        << observation.schedulerEnqueueStaleCount
        << " scheduler_dispatched="
        << observation.schedulerDispatchCount
        << " scheduler_completed_current="
        << observation.schedulerCompletedCurrentCount
        << " scheduler_completed_stale="
        << observation.schedulerCompletedStaleCount
        << " scheduler_max_pending="
        << observation.schedulerMaximumPendingJobs
        << " scheduler_max_inflight="
        << observation.schedulerMaximumInFlightJobs
        << " scheduler_dispatch_total_us="
        << observation.schedulerDispatchTotalMicroseconds
        << " scheduler_dispatch_max_us="
        << observation.schedulerDispatchMaximumMicroseconds
        << " scheduler_planner_total_us="
        << observation.schedulerPlannerTotalMicroseconds
        << " scheduler_planner_max_us="
        << observation.schedulerPlannerMaximumMicroseconds
        << " executions=" << observation.executionCount
        << " obstacle_candidate="
        << observation.obstacleCandidateSeen
        << " obstacle_conflict="
        << observation.obstaclePrimaryConflictSeen
        << " exact_obstacle_block="
        << observation.obstacleExactStaticBlockSeen
        << " dynamic_queries="
        << observation.dynamicQueryCount
        << " max_dynamic_candidates="
        << observation.maximumDynamicCandidateCount
        << " rotating_actor_seen="
        << observation.rotatingActorCandidateSeen
        << " rotating_actor_omega_verified="
        << observation.rotatingActorAngularVelocityVerified
        << " rotating_actor_omega_error="
        << observation.rotatingActorAngularVelocityErrorRadPerSecond
        << " moving_gap_pair="
        << observation.movingGapPairCandidateSeen
        << " moving_gap_kinematics="
        << observation.movingGapKinematicsVerified
        << " moving_gap_velocity_error="
        << observation.movingGapMaximumVelocityErrorMps
        << " moving_precision="
        << observation.movingPrecisionAttemptedSeen
        << " moving_passage_feasible="
        << observation.movingPassageFeasibleSeen
        << " moving_eval_status="
        << static_cast<int>(
            observation.movingPassageLastEvaluatorStatus
        )
        << " moving_req_fwd_mps2="
        << observation.movingPassageRequiredPeakForwardAccelerationMps2
        << " moving_req_rev_mps2="
        << observation.movingPassageRequiredPeakReverseAccelerationMps2
        << " moving_req_lat_mps2="
        << observation.movingPassageRequiredPeakLateralAccelerationMps2
        << " moving_req_vert_mps2="
        << observation.movingPassageRequiredPeakVerticalAccelerationMps2
        << " moving_sample_clearance_m="
        << observation.movingPassageMinimumSampleClearanceMeters
        << " moving_continuous_clearance_m="
        << observation.movingPassageMinimumContinuousClearanceMeters
        << " moving_passage_static_safe="
        << observation.movingPassageStaticSafeSeen
        << " moving_passage_authority="
        << observation.movingPassageAuthoritySeen
        << " moving_passage_executed="
        << observation.movingPassageExecutedSeen
        << " moving_passage_applied="
        << observation.movingPassageAppliedAccelerationSeen
        << " visible_horizon_bypass="
        << observation.visibleHorizonBypassSeen
        << " visible_horizon_bypass_active="
        << observation.visibleHorizonBypassActive
        << " visible_horizon_direct_recovered="
        << observation.visibleHorizonDirectRecoveredSeen
        << " visible_horizon_max_offset_m="
        << observation.maximumVisibleHorizonOffsetMeters
        << " moving_gap_passed="
        << observation.movingGapPlanePassed
        << " slit_exact_open="
        << observation.slitPortalExactOpenPublished
        << " slit_portal="
        << observation.slitPortalWaypointSeen
        << " slit_entry_capture="
        << observation.slitEntryCaptureSeen
        << " slit_entry_vel_aligned="
        << observation.slitEntryVelocityAlignedSeen
        << " slit_entry_fwd_aligned="
        << observation.slitEntryForwardAlignedSeen
        << " slit_entry_crossed_aligned="
        << observation.slitEntryPlaneCrossedAligned
        << " slit_entry_vel_angle_rad="
        << observation.slitEntryVelocityAngleRad
        << " slit_entry_fwd_angle_rad="
        << observation.slitEntryForwardAngleRad
        << " slit_entry_lateral_mps="
        << observation.slitEntryLateralSpeedMps
        << " slit_entry_cross_track_m="
        << observation.slitEntryCrossTrackMeters
        << " slit_passed="
        << observation.slitTunnelPassed
        << " slit_margin_m="
        << observation.slitTunnelCrossingMarginMeters
        << " slit_crossing_map=("
        << observation.slitTunnelCrossingMap.x << ","
        << observation.slitTunnelCrossingMap.y << ","
        << observation.slitTunnelCrossingMap.z << ")"
        << " max_moving_exec_mps2="
        << observation.maximumMovingPassageExecutedDemandMps2
        << " max_moving_applied_mps2="
        << observation.maximumMovingPassageAppliedAccelerationMps2
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
        << " configured_route_exact_block="
        << observation.configuredRouteExactObstacleBlockPublished
        << " first_live_probe_blocked="
        << observation.firstLiveNominalExactBlocked
        << " first_live_blocker_entity="
        << observation.firstLiveNominalBlockingEntityId
        << " first_live_horizon_m="
        << observation.firstLiveHorizonMeters
        << " exact_static_query="
        << observation.exactStaticQuerySeen
        << " exact_static_block="
        << observation.nominalStaticBlockSeen
        << " max_exact_static_examined="
        << observation.maximumExactStaticObstaclesExamined
        << " exact_static_motion_samples="
        << observation.exactStaticMotionSamples
        << " exact_static_violation="
        << observation.exactStaticViolationSeen
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
        << "[PASS] navigation-runtime projected visible-horizon bypass drove real physics,"
        << " recovered the direct route after the moving obstacle pair,"
        << " portal capture aligned flight path + hull axis before entry,"
        << " the ship crossed the exact-static tunnel collision-free,"
        << " execution matched same-tick sparse/canonical replication,"
        << " and every live planner call crossed B14 dispatch/commit authority\n";
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

// Live hub-traffic probe: the real authoritative runtime with one attached
// player session. Every interval it reads the CANONICAL client snapshot (what
// a client world actually receives) and logs every dock traffic ship there:
// tier, motion fidelity, dock FIXED-frame position, distance to the player.
// Server-side traffic/dispatcher log: ELITE_DOCK_TRAFFIC_LOG.
int runDockTrafficProbe(double seconds, const std::string& logPath)
{
    core::disableRuntimeStdoutNoise();

    game::server::HeadlessServerTransport transport;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;
    game::server::ServerRuntime runtime(worldParams, debugChannel);
    const auto session = runtime.attachPlayerSessionTransport(
        transport, makeSelfTestIdentity(1301u, 13u));
    if (!session || !transport.hasSessionWelcome())
    {
        std::cerr << "[FAIL] dock-traffic-probe bootstrap failed\n";
        return 60;
    }
    const EntityId playerId = transport.sessionWelcome().controlledEntityId;
    std::FILE* out = std::fopen(logPath.c_str(), "w");
    if (!out)
        return 61;

    const double step = runtime.fixedStepSeconds();
    double t = 0.0;
    double nextSample = 0.0;
    while (t < seconds)
    {
        runtime.advance(step);
        t += step;
        if (t < nextSample)
            continue;
        nextSample += 10.0;

        const auto& snap = transport.latestCanonicalSnapshot();
        const ObjectSnapshot* dock = nullptr;
        const ObjectSnapshot* base = nullptr;
        for (const auto& o : snap.objects)
        {
            if (o.hubAttachment.moduleId == "hub_hex_dock_a")
                dock = &o;
            else if (o.hubAttachment.moduleId == "hub_hex_dock_base")
                base = &o;
        }
        const ShipSnapshot* player = findShipSnapshot(snap, playerId);
        if (!dock || !base || !player)
        {
            std::fprintf(out, "t=%.1f dock=%d base=%d player=%d\n", t, dock != nullptr, base != nullptr,
                         player != nullptr);
            continue;
        }
        const glm::dvec3 origin = world::coordinates::fullMeters(dock->worldPosition);
        const glm::dmat3 toFixed = glm::transpose(glm::dmat3(glm::mat3(base->orientation)));
        const glm::dvec3 pp = world::coordinates::fullMeters(player->transform.worldPosition);
        const glm::dvec3 pf = toFixed * (pp - origin);
        int traffic = 0;
        for (const auto& ship : snap.ships)
            traffic += ship.instanceId >= 9000001 && ship.instanceId < 9100000 ? 1 : 0;
        std::fprintf(out, "t=%.1f ships=%zu traffic=%d player_fixed=(%.0f,%.0f,%.0f) player_dock_m=%.0f\n", t,
                     snap.ships.size(), traffic, pf.x, pf.y, pf.z, glm::length(pp - origin));
        for (const auto& ship : snap.ships)
        {
            if (ship.instanceId < 9000001 || ship.instanceId >= 9100000)
                continue;
            const glm::dvec3 sp = world::coordinates::fullMeters(ship.transform.worldPosition);
            const glm::dvec3 f = toFixed * (sp - origin);
            std::fprintf(out,
                         "  ship inst=%llu entity=%llu type=%d tier=%d fidelity=%d fixed=(%.0f,%.0f,%.0f) "
                         "dock_m=%.0f player_m=%.0f speed=%.1f hub=%s %s phase=%s dest=%s route_pts=%zu\n",
                         static_cast<unsigned long long>(ship.instanceId),
                         static_cast<unsigned long long>(ship.id.value), static_cast<int>(ship.typeId),
                         static_cast<int>(ship.simulationTier), static_cast<int>(ship.motionFidelity), f.x, f.y,
                         f.z, glm::length(sp - origin), glm::length(sp - pp),
                         glm::length(ship.transform.motion.worldVelocityMps -
                                     glm::dvec3(dock->linearVelocityMps)),
                         ship.transform.motion.hubId.empty() ? "-" : ship.transform.motion.hubId.c_str(),
                         ship.traffic.callsign.c_str(), ship.traffic.phase.c_str(),
                         ship.traffic.destination.c_str(), ship.traffic.routeOffsetsMeters.size());
        }
        std::fflush(out);
    }
    std::fclose(out);
    std::cerr << "[PASS] dock-traffic-probe simulated_s=" << t << "\n";
    return 0;
}

// ---- Load test (author 2026-11-08: "по 2-3 нпс в каждой из 61 систем ... нпс в данной ситуации
// были имитацией игроков, т.е. сервер нагружаем той информацией, которой он оперирует по отношению
// к игроку"). Every bot is a real player session: an authenticated account, a player ship from the
// bootstrap pool, its own transport that receives the per-session snapshots and encodes them to
// the wire exactly like the TCP transport (then drops the bytes), a control state every tick,
// time syncs and galaxy map requests like a client. The ships are sent to their systems, so the
// server simulates every system with a ship in it (the multi-system tick).
class LoadTestTransport final : public IServerTransport
{
public:
    void update(float) override {}
    bool receiveSessionHello(game::network::SessionHello&) override { return false; }
    bool receiveClientMessage(game::network::ClientMessage& out) override
    {
        if (m_messages.empty())
            return false;
        out = std::move(m_messages.front());
        m_messages.pop_front();
        return true;
    }
    bool receiveMapRequest(game::network::MapRequest& out) override
    {
        if (m_maps.empty())
            return false;
        out = std::move(m_maps.front());
        m_maps.pop_front();
        return true;
    }
    bool receiveTimeSyncRequest(game::network::TimeSyncRequest& out) override
    {
        if (m_times.empty())
            return false;
        out = m_times.front();
        m_times.pop_front();
        return true;
    }
    void publishSessionRejectImmediately(const game::network::SessionReject&) override { m_rejected = true; }
    void publishSessionWelcomeImmediately(const game::network::SessionWelcome& w) override
    {
        m_welcome = w;
        m_hasWelcome = true;
    }
    void publishSnapshot(const SimulationSnapshot& s) override { encode(s); }
    void publishSnapshotImmediately(const SimulationSnapshot& s) override { encode(s); }
    void sendMapResponse(game::network::MapResponse r) override { encodeAny(r); }
    void sendTimeSyncResponse(game::network::TimeSyncResponse r) override { encodeAny(r); }

    void push(game::network::ClientMessage m) { m_messages.push_back(std::move(m)); }
    void pushMap(game::network::MapRequest m) { m_maps.push_back(std::move(m)); }
    void pushTime(game::network::TimeSyncRequest t) { m_times.push_back(t); }

    game::network::SessionWelcome m_welcome {};
    bool m_hasWelcome = false;
    bool m_rejected = false;
    std::uint64_t m_bytes = 0;
    std::uint64_t m_snapshots = 0;
    std::uint64_t m_snapshotShips = 0;

private:
    void encode(const SimulationSnapshot& s)
    {
        ++m_snapshots;
        m_snapshotShips += s.ships.size();
        encodeAny(s);
        if (m_breakdownWanted)
        {
            // what the bytes are: the snapshot encoded with one part removed at a time
            m_breakdownWanted = false;
            const auto size = [this](const SimulationSnapshot& v) {
                game::network::wire::WireMessageKind kind {};
                std::vector<std::uint8_t> payload;
                game::network::wire::encodeMessagePayload(v, kind, payload, m_compressor);
                return payload.size();
            };
            const std::size_t all = size(s);
            SimulationSnapshot c = s;
            c.ships.clear();
            const std::size_t noShips = size(c);
            c = s;
            c.signals.clear();
            const std::size_t noSignals = size(c);
            c = s;
            c.objects.clear();
            const std::size_t noObjects = size(c);
            c = s;
            c.hubs.clear();
            const std::size_t noHubs = size(c);
            c = s;
            c.session = {};
            const std::size_t noSession = size(c);
            std::cerr << "[LoadTest] snapshot " << all << " B: ships " << s.ships.size() << " = " << all - noShips << " B, signals "
                      << s.signals.size() << " = " << all - noSignals << " B, objects " << s.objects.size() << " = " << all - noObjects
                      << " B, hubs " << s.hubs.size() << " = " << all - noHubs << " B, session = " << all - noSession << " B\n";
        }
    }

public:
    bool m_breakdownWanted = false;

private:
    template <typename T>
    void encodeAny(const T& value)
    {
        game::network::wire::WireMessageKind kind {};
        std::vector<std::uint8_t> payload;
        if (game::network::wire::encodeMessagePayload(value, kind, payload, m_compressor))
            m_bytes += payload.size();
    }
    std::deque<game::network::ClientMessage> m_messages;
    std::deque<game::network::MapRequest> m_maps;
    std::deque<game::network::TimeSyncRequest> m_times;
    game::network::wire::NoWireCompression m_compressor;
};

std::size_t processWorkingSetMb()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc {};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc))
        return static_cast<std::size_t>(pmc.WorkingSetSize / (1024 * 1024));
#endif
    return 0;
}

// perSystem: ships per system (0 = one bot in the start system only: the baseline; -1 = 2 or 3)
int runLoadTest(int perSystem, double measureSeconds, bool move)
{
    core::disableRuntimeStdoutNoise();
    world::celestial::StarAtlasDatabase atlas;
    if (!atlas.loadFromRuntimeOrSource())
    {
        std::cerr << "[LoadTest] star atlas not loaded\n";
        return 2;
    }
    struct Bot
    {
        int systemId = -1;
        int ordinal = 0;
        std::unique_ptr<LoadTestTransport> transport;
        EntityId ship {};
        std::uint64_t tick = 0;
        double phase = 0.0;
        int currentSystemId = -1;     // from the authoritative snapshot, once a second
        int jumpFromSystemId = -1;    // the system a jump was requested from (-1: none pending)
        int jumpsRequested = 0;
        int jumpsDone = 0;
    };
    std::vector<Bot> bots;
    if (perSystem == 0)
        bots.push_back(Bot {-1, 0});
    else
        for (const auto& s : atlas.systems())
        {
            const int n = perSystem > 0 ? perSystem : 2 + (s.id % 2);
            for (int k = 0; k < n; ++k)
                bots.push_back(Bot {s.id, k});
        }
    std::cerr << "[LoadTest] " << bots.size() << " player bots in " << (perSystem == 0 ? 1 : atlas.systems().size()) << " systems\n";

    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;
    const auto bootStart = std::chrono::steady_clock::now();
    game::server::ServerRuntime runtime(worldParams, debugChannel, bots.size());
    std::cerr << "[LoadTest] runtime built in " << std::chrono::duration<double>(std::chrono::steady_clock::now() - bootStart).count()
              << " s, " << runtime.orbitalHubCount() << " orbital hubs, working set " << processWorkingSetMb() << " MB\n";

    for (std::size_t i = 0; i < bots.size(); ++i)
    {
        auto& b = bots[i];
        b.transport = std::make_unique<LoadTestTransport>();
        b.phase = static_cast<double>((i * 7919) % 60);
        if (!runtime.attachPlayerSessionTransport(*b.transport, makeSelfTestIdentity(50000u + i, 7u)) || !b.transport->m_hasWelcome)
        {
            std::cerr << "[LoadTest] session " << i << " not admitted\n";
            return 3;
        }
        b.ship = b.transport->m_welcome.controlledEntityId;
    }
    const double step = runtime.fixedStepSeconds();
    // the ships leave the start hub: a few seconds first (sessions settle), then each to its system
    for (int i = 0; i < static_cast<int>(1.0 / step); ++i)
        runtime.advance(step);
    int placed = 0;
    for (auto& b : bots)
        if (b.systemId >= 0 && runtime.devPlaceShipInSystem(b.ship, b.systemId, b.ordinal))
            ++placed;
    std::cerr << "[LoadTest] " << placed << " ships sent to their systems; step " << step << " s\n";

    // the move scenario: the jump neighbours (< 30 ly) of every system
    std::map<int, std::vector<int>> neighbours;
    for (const auto& a : atlas.systems())
        for (const auto& o : atlas.systems())
            if (a.id != o.id && glm::length(a.positionLy - o.positionLy) < 30.0)
                neighbours[a.id].push_back(o.id);
    const long cycleTicks = static_cast<long>(240.0 / step);
    const auto at = [step](double seconds) { return static_cast<long>(seconds / step); };
    std::size_t botIndex = 0;
    const auto command = [](Bot& b, ClientShipCommand::Type type, int index) {
        ClientShipCommand c;
        c.type = type;
        c.index = index;
        game::network::ClientMessage m;
        m.clientTick = b.tick;
        m.payload = c;
        b.transport->push(std::move(m));
    };
    const auto driveBots = [&](double now) {
        botIndex = 0;
        for (auto& b : bots)
        {
            ++b.tick;
            ++botIndex;
            ShipControlState c;
            c.controlTick = b.tick;
            // a pilot's minute: coast, pitch, thrust, yaw, brake (move: only while stabilized, not
            // in the J travel phase - a turn at hundreds of km/s steers into a planet)
            const double t = std::fmod(now + b.phase, 60.0);
            const bool travelling = move && (static_cast<long>(now / step) + static_cast<long>(b.phase * 4.0 / step)) %
                    static_cast<long>(240.0 / step) < static_cast<long>(110.0 / step);
            if (travelling)
                ;
            else if (t >= 20.0 && t < 30.0)
                c.pitchInput = 0.3f;
            else if (t >= 30.0 && t < 45.0)
                c.forwardInput = 0.4f;
            else if (t >= 45.0 && t < 55.0)
                c.yawInput = -0.3f;
            else if (t >= 55.0)
                c.forwardInput = -0.4f;
            game::network::ClientMessage m;
            m.clientTick = b.tick;
            m.payload = c;
            b.transport->push(std::move(m));
            if (move)
            {
                // a 4-minute travel cycle like a player's: out of stabilization, J engine on, full J
                // acceleration 37 s, brake 40 s; then every second cycle of every other pilot a
                // jump to a neighbour star (< 30 ly); the others stop the J engine and stabilize
                const long k = (static_cast<long>(now / step) + at(b.phase * 4.0)) % cycleTicks;
                const bool jumper = ((botIndex + static_cast<std::size_t>((now + b.phase * 4.0) / 240.0)) % 2) == 0;
                if (k == at(0.0))
                    command(b, ClientShipCommand::ToggleStabilization, 0);
                else if (k == at(2.0))
                    command(b, ClientShipCommand::ToggleJEngine, 0);
                else if (k == at(3.0))
                    command(b, ClientShipCommand::SetFreeFlightThrust, 1);
                else if (k == at(40.0))
                    command(b, ClientShipCommand::SetFreeFlightThrust, 2);
                else if (k == at(80.0))
                    command(b, ClientShipCommand::SetFreeFlightThrust, 0);
                else if (k == at(85.0) && jumper)
                {
                    const auto n = neighbours.find(b.currentSystemId);
                    if (n != neighbours.end() && !n->second.empty())
                    {
                        command(b, ClientShipCommand::RequestJump, n->second[(b.tick / 7) % n->second.size()]);
                        b.jumpFromSystemId = b.currentSystemId;
                        ++b.jumpsRequested;
                    }
                }
                else if (k == at(100.0) && !jumper)
                    command(b, ClientShipCommand::ToggleJEngine, 0);
                else if (k == at(105.0) && !jumper)
                    command(b, ClientShipCommand::ToggleStabilization, 0);
                else if (k == at(230.0) && jumper && b.jumpFromSystemId >= 0 && b.currentSystemId == b.jumpFromSystemId)
                {
                    // the jump did not happen: cancel, stop, stabilize
                    command(b, ClientShipCommand::RequestJump, -1);
                    command(b, ClientShipCommand::ToggleJEngine, 0);
                    command(b, ClientShipCommand::ToggleStabilization, 0);
                    b.jumpFromSystemId = -1;
                }
            }
            if (b.tick % static_cast<std::uint64_t>(2.0 / step) == 0)
            {
                game::network::TimeSyncRequest ts;
                ts.sequence = b.tick;
                ts.clientSendTimeSeconds = now;
                b.transport->pushTime(ts);
            }
            if ((b.tick + static_cast<std::uint64_t>(b.phase / step)) % static_cast<std::uint64_t>(60.0 / step) == 0)
            {
                game::network::GalaxyMapRequest map;
                map.requestId = b.tick;
                b.transport->pushMap(map);
            }
        }
    };

    // warm-up: the exits are placed by their systems' passes
    double now = 0.0;
    for (int i = 0; i < static_cast<int>(5.0 / step); ++i, now += step)
    {
        driveBots(now);
        runtime.advance(step);
    }
    {
        SimulationSnapshot snap;
        runtime.copyAuthoritativePublishedSnapshot(snap);
        std::map<int, int> perSys;
        int inJump = 0;
        for (const auto& s : snap.ships)
        {
            ++perSys[s.transform.motion.systemId];
            if (s.transform.motion.jumpState != 0)
                ++inJump;
        }
        std::cerr << "[LoadTest] after warm-up: " << snap.ships.size() << " ships in " << perSys.size() << " systems, " << inJump
                  << " still in a jump exit, " << runtime.lastTickSystemPasses() << " system passes per tick\n";
    }

    for (auto& b : bots)
        b.transport->m_bytes = b.transport->m_snapshots = b.transport->m_snapshotShips = 0;
    bots.front().transport->m_breakdownWanted = true;
    game::server::serverTickProfile() = {};
    bots.back().transport->m_breakdownWanted = true;
    std::vector<double> tickMs;
    std::vector<double> simMs;
    const int ticks = static_cast<int>(measureSeconds / step);
    tickMs.reserve(ticks);
    simMs.reserve(ticks);
    std::map<std::uint32_t, Bot*> botByShip;
    for (auto& b : bots)
        botByShip[b.ship.value] = &b;
    const auto observe = [&](bool report, double at) {
        SimulationSnapshot snap;
        runtime.copyAuthoritativePublishedSnapshot(snap);
        std::map<int, int> perSys;
        int jumpStates[4] {};
        int freeFlight = 0;
        int present = 0;
        for (const auto& s : snap.ships)
        {
            const auto it = botByShip.find(s.id.value);
            if (it == botByShip.end())
                continue;
            ++present;
            Bot& b = *it->second;
            const int sys = s.transform.motion.systemId;
            if (b.currentSystemId >= 0 && sys != b.currentSystemId && b.jumpFromSystemId >= 0)
            {
                ++b.jumpsDone;
                b.jumpFromSystemId = -1;
            }
            b.currentSystemId = sys;
            ++perSys[sys];
            ++jumpStates[std::min<int>(3, s.transform.motion.jumpState)];
            if (s.transform.motion.freeFlight)
                ++freeFlight;
        }
        if (!report)
            return;
        int requested = 0, done = 0;
        for (const auto& b : bots)
        {
            requested += b.jumpsRequested;
            done += b.jumpsDone;
        }
        std::cerr << "[LoadTest] t=" << at << " s: " << present << " of " << bots.size() << " player ships in " << perSys.size()
                  << " systems; normal flight " << freeFlight << "; charging " << jumpStates[1] << ", charged-waiting " << jumpStates[2]
                  << ", exiting " << jumpStates[3] << "; jumps requested " << requested << ", done " << done << "\n";
    };
    observe(false, 0.0);
    const auto wallStart = std::chrono::steady_clock::now();
    const int secondTicks = static_cast<int>(1.0 / step);
    for (int i = 0; i < ticks; ++i, now += step)
    {
        driveBots(now);
        const auto t0 = std::chrono::steady_clock::now();
        runtime.advance(step);
        tickMs.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        simMs.push_back(runtime.lastTickSimulationSeconds() * 1000.0);
        if (i % secondTicks == 0)
            observe(i % (30 * secondTicks) == 0, i * step);
    }
    observe(true, ticks * step);
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
    const auto stats = [](std::vector<double> v) {
        std::sort(v.begin(), v.end());
        double sum = 0.0;
        for (double x : v)
            sum += x;
        char b[160];
        std::snprintf(b, sizeof b, "avg %.3f  p50 %.3f  p95 %.3f  p99 %.3f  max %.3f ms", sum / v.size(), v[v.size() / 2],
                      v[v.size() * 95 / 100], v[v.size() * 99 / 100], v.back());
        return std::string(b);
    };
    std::uint64_t bytes = 0, snaps = 0, snapShips = 0;
    for (const auto& b : bots)
    {
        bytes += b.transport->m_bytes;
        snaps += b.transport->m_snapshots;
        snapShips += b.transport->m_snapshotShips;
    }
    std::cerr << "[LoadTest] RESULT bots " << bots.size() << ", systems simulated " << runtime.lastTickSystemPasses() << ", ticks " << ticks
              << " (" << measureSeconds << " s of game time at " << 1.0 / step << " Hz) in " << wall << " s wall\n";
    std::cerr << "[LoadTest] RESULT server tick (whole: sessions, simulation, replication, wire encoding): " << stats(tickMs) << "\n";
    std::cerr << "[LoadTest] RESULT simulation part (all system passes): " << stats(simMs) << "\n";
    {
        double busy = 0.0;
        for (double x : tickMs)
            busy += x;
        std::cerr << "[LoadTest] RESULT tick budget " << step * 1000.0 << " ms: the server ticks used " << 100.0 * busy / 1000.0 / measureSeconds
                  << " % of one core (" << wall << " s wall with the test driver)\n";
    }
    std::cerr << "[LoadTest] RESULT wire: " << bytes / measureSeconds / 1024.0 << " KiB/s total, "
              << bytes / measureSeconds / 1024.0 / bots.size() << " KiB/s per player, " << (snaps ? snapShips / snaps : 0)
              << " ships per snapshot, " << snaps / measureSeconds / bots.size() << " snapshots/s per player\n";
    std::cerr << "[LoadTest] RESULT working set " << processWorkingSetMb() << " MB\n";
    {
        const auto& p = game::server::serverTickProfile();
        const double n = std::max<long long>(1, p.ticks);
        char b[512];
        std::snprintf(b, sizeof b,
                      "[LoadTest] RESULT per tick (ms): simulation %.3f, sensors %.3f, snapshot build %.3f, session plans %.3f, "
                      "selection %.3f, copies %.3f, publish+encode %.3f, inbound %.3f, map requests %.3f\n",
                      1000 * p.simulation / n, 1000 * p.sensors / n, 1000 * p.snapshotBuild / n, 1000 * p.sessionPlan / n,
                      1000 * p.sessionSelect / n, 1000 * p.sessionCopy / n, 1000 * p.sessionPublish / n, 1000 * p.inbound / n,
                      1000 * p.mapRequests / n);
        std::cerr << b;
        std::snprintf(b, sizeof b,
                      "[LoadTest] RESULT simulation stages per tick (ms): ship maintenance %.3f, hubs %.3f, hub frames + free flight %.3f, "
                      "static objects %.3f, AI / controls %.3f, physics %.3f, signals / radar %.3f\n",
                      1000 * p.simStage[0] / n, 1000 * p.simStage[1] / n, 1000 * p.simStage[2] / n, 1000 * p.simStage[3] / n,
                      1000 * p.simStage[4] / n, 1000 * p.simStage[5] / n, 1000 * p.simStage[6] / n);
        std::cerr << b;
        std::snprintf(b, sizeof b, "[LoadTest] RESULT snapshot build per tick (ms): ship/object DTOs %.3f, session data %.3f, canonical merge %.3f\n",
                      1000 * p.snapshotParts[0] / n, 1000 * p.snapshotParts[1] / n, 1000 * p.snapshotParts[2] / n);
        std::cerr << b;
        if (p.economyTicks > 0)
        {
            std::snprintf(b, sizeof b, "[LoadTest] RESULT economy: %lld full rounds (a new day for every hub), %.3f ms per round, %.3f ms per server tick (%lld rows)\n",
                          p.economyTicks, 1000 * p.economy / p.economyTicks, 1000 * p.economy / n, p.economyQuotes);
            std::cerr << b;
        }
    }
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
    {
        // DEV commands only when the operator asks for them
        bool dev = false;
        for (int k = 1; k < argc; ++k)
            dev = dev || std::string(argv[k]) == "--dev";
        if (const char* v = std::getenv("ELITE_DEV_COMMANDS"); v && *v == '1')
            dev = true;
        GameServer::setDevCommandsAllowed(dev);
        std::cerr << "[Server] DEV commands " << (dev ? "ALLOWED" : "rejected") << std::endl;
    }
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
    bool loadTest = false;
    int loadTestPerSystem = -1;
    double loadTestSeconds = 30.0;
    bool loadTestMove = false;
    double dockTrafficProbeSeconds = 0.0;
    std::string dockTrafficProbeLog;
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

        if (arg == "--dock-traffic-probe")
        {
            if (i + 2 >= argc)
            {
                std::cerr << "[EliteServer] --dock-traffic-probe requires SECONDS LOG\n";
                return 1;
            }
            dockTrafficProbeSeconds = std::atof(argv[++i]);
            dockTrafficProbeLog = argv[++i];
            continue;
        }

        if (arg == "--load-test")
        {
            loadTest = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                const std::string k = argv[++i];
                loadTestPerSystem = k == "2-3" ? -1 : std::atoi(k.c_str());
            }
            if (i + 1 < argc && argv[i + 1][0] != '-')
                loadTestSeconds = std::atof(argv[++i]);
            if (i + 1 < argc && std::string(argv[i + 1]) == "move")
            {
                loadTestMove = true;
                ++i;
            }
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

    if (loadTest)
        return runLoadTest(loadTestPerSystem, loadTestSeconds, loadTestMove);

    if (navigationSelfTest)
        return runNavigationRuntimeSelfTest();

    if (dockTrafficProbeSeconds > 0.0)
        return runDockTrafficProbe(dockTrafficProbeSeconds, dockTrafficProbeLog);

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
