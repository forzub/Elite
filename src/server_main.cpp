#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <variant>

#include <glm/geometric.hpp>

#include "src/core/ConsoleOutput.h"
#include "src/game/server/HeadlessServerEndpoints.h"
#include "src/game/server/ServerRuntime.h"
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
        << "  EliteServer.exe               Run until Ctrl+C/SIGTERM\n"
        << "  EliteServer.exe --self-test   Boot and advance the real server, then exit\n"
        << "  EliteServer.exe --help        Show this help\n";
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
        transportA,
        debugChannel
    );

    if (!transportA.hasSessionWelcome() ||
        !transportA.hasBootstrapSnapshot() ||
        !debugChannel.hasBootstrapState())
    {
        std::cerr
            << "[FAIL] headless-server bootstrap protocol/debug publication missing\n";
        return 2;
    }

    const auto welcomeA = transportA.sessionWelcome();
    if (welcomeA.sessionId.value == 0 ||
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

    // Pick an already-existing non-primary ship for the second authority
    // session. Admission/spawn persistence is a later layer; this smoke proves
    // that one runtime can route two independent connection/session streams to
    // two different authoritative entities without exposing GameServer memory.
    const auto& bootstrapA = transportA.latestSnapshot();
    const ShipSnapshot* secondShip = nullptr;

    for (const auto& ship : bootstrapA.ships)
    {
        if (ship.id == welcomeA.controlledEntityId)
            continue;

        if (ship.motionLabKind ==
            game::diagnostics::HubMotionLabActorKind::None)
        {
            secondShip = &ship;
            break;
        }
    }

    if (!secondShip)
    {
        for (const auto& ship : bootstrapA.ships)
        {
            if (ship.id != welcomeA.controlledEntityId)
            {
                secondShip = &ship;
                break;
            }
        }
    }

    if (!secondShip)
    {
        std::cerr
            << "[FAIL] headless-server multiplayer smoke has no second ship\n";
        return 5;
    }

    const EntityId shipBId = secondShip->id;
    const auto sessionB = runtime.attachPlayerSessionTransport(
        transportB,
        shipBId
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
    if (welcomeB.sessionId != sessionB ||
        welcomeB.sessionId == welcomeA.sessionId ||
        welcomeB.controlledEntityId != shipBId)
    {
        std::cerr
            << "[FAIL] headless-server second session authority bootstrap is wrong\n";
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
    if (!shipBAfterDetach ||
        shipBAfterDetach->acknowledgedControlTick != 202)
    {
        std::cerr
            << "[FAIL] headless-server detached session retained command authority\n";
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

int runHeadlessServer()
{
    game::server::HeadlessServerTransport transport;
    game::server::HeadlessDebugChannel debugChannel;
    WorldParams worldParams;

    game::server::ServerRuntime runtime(
        worldParams,
        transport,
        debugChannel
    );

    std::signal(SIGINT, handleTerminationSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, handleTerminationSignal);
#endif

    std::cout
        << "[EliteServer] authoritative headless runtime started"
        << " fixed_step_s=" << runtime.fixedStepSeconds()
        << "\n";
    std::cout
        << "[EliteServer] no remote socket transport exists yet; "
        << "running world simulation with empty inbound queues\n";

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

    std::cout << "[EliteServer] stopped\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        const std::string arg = argv[1];
        if (arg == "--self-test")
            return runHeadlessSelfTest();
        if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }

        std::cerr << "[EliteServer] unknown option: " << arg << "\n";
        printUsage();
        return 1;
    }

    return runHeadlessServer();
}
