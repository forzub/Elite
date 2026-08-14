#include "src/game/diagnostics/MultiplayerClientAcceptanceHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <glm/geometric.hpp>
#include <stdexcept>
#include <string>

#include "src/game/client/GameClient.h"
#include "src/game/debug/LocalDebugSessionControl.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/server/ServerRuntime.h"
#include "src/world/WorldParams.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::diagnostics
{
namespace
{
constexpr double FrameSeconds = 1.0 / 60.0;
constexpr int SynchronizationFrameLimit = 600;
constexpr int AckDrainFrameLimit = 120;

class MultiplayerAcceptanceFailure final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw MultiplayerAcceptanceFailure(message);
}

const ClientShipState& requireShip(
    const GameClient& client,
    EntityId id,
    const char* label
)
{
    const auto& ships = client.world().ships();
    const auto it = ships.find(id.value);
    if (it == ships.end())
    {
        throw MultiplayerAcceptanceFailure(
            std::string(label) + " disappeared from client world"
        );
    }

    return it->second;
}

EntityId chooseSecondaryControlledShip(const GameClient& primaryClient)
{
    const EntityId primaryId = primaryClient.playerId();
    EntityId fallback {0};

    for (const auto& [_, ship] : primaryClient.world().ships())
    {
        if (ship.id == primaryId)
            continue;

        if (ship.motionLabKind !=
            game::diagnostics::HubMotionLabActorKind::None)
        {
            continue;
        }

        if (fallback.value == 0)
            fallback = ship.id;

        if (ship.transform.motion.mode ==
            game::navigation::MotionMode::HubTactical)
        {
            return ship.id;
        }
    }

    return fallback;
}

void synchronizeClients(
    game::server::ServerRuntime& runtime,
    GameClient& clientA,
    GameClient& clientB
)
{
    for (int frame = 0; frame < SynchronizationFrameLimit; ++frame)
    {
        runtime.advance(FrameSeconds);
        clientA.updateSynchronization(FrameSeconds);
        clientB.updateSynchronization(FrameSeconds);

        if (clientA.readyForGameplay() && clientB.readyForGameplay())
            return;

        if (clientA.connectionState() == ClientConnectionState::Failed)
        {
            throw MultiplayerAcceptanceFailure(
                std::string("client A synchronization failed: ") +
                clientA.connectionError()
            );
        }

        if (clientB.connectionState() == ClientConnectionState::Failed)
        {
            throw MultiplayerAcceptanceFailure(
                std::string("client B synchronization failed: ") +
                clientB.connectionError()
            );
        }
    }

    throw MultiplayerAcceptanceFailure(
        "two GameClient instances did not reach Ready on one ServerRuntime"
    );
}

void runGameplayFrame(
    game::server::ServerRuntime& runtime,
    GameClient& clientA,
    GameClient& clientB,
    const ShipControlState* controlA,
    const ShipControlState* controlB
)
{
    const float frameDt = static_cast<float>(FrameSeconds);
    const float fixedDt = static_cast<float>(runtime.fixedStepSeconds());

    // Same ordering used by production gameplay: each client consumes the
    // previous authoritative publication, samples local input, the shared
    // authoritative runtime advances once, then both clients predict/present.
    clientA.prepareGameplayFrame(FrameSeconds);
    clientB.prepareGameplayFrame(FrameSeconds);

    if (controlA)
        clientA.submitInput(*controlA);
    if (controlB)
        clientB.submitInput(*controlB);

    runtime.advance(FrameSeconds);

    clientA.update(frameDt, fixedDt, FrameSeconds);
    clientB.update(frameDt, fixedDt, FrameSeconds);
}

void requireNavigationMatchesAuthoritativeLocalEntity(
    const GameClient& client,
    EntityId localEntityId,
    const char* label
)
{
    const auto& navigation = client.playerNavigation();

    if (navigation.currentSystemId < 0)
        return;

    /*
        playerNavigation belongs to the newest authoritative session snapshot.
        The local ClientShipState::transform does not: after gameplay starts it
        is immediately replayed/predicted ahead of that snapshot. Comparing the
        two therefore turns healthy client prediction into a false multiplayer
        navigation failure.

        Sample the retained authoritative ship history at the exact epoch that
        supplied playerNavigation instead. This keeps the M4/M5 assertion on
        the intended boundary: session A navigation follows authoritative ship
        A, and session B navigation follows authoritative ship B.
    */
    const auto sample = client.world().sampleSystemMapShipsAtServerTime(
        navigation.currentSystemId,
        client.lastSimulationMetadata().serverTimeSeconds
    );

    require(
        sample.status == game::client::SystemMapShipSampleStatus::Ready,
        std::string(label) + " cannot sample authoritative local entity at navigation epoch"
    );

    const auto it = std::find_if(
        sample.ships.begin(),
        sample.ships.end(),
        [&](const game::client::SystemMapShipSample& ship)
        {
            return ship.id == localEntityId;
        }
    );

    require(
        it != sample.ships.end(),
        std::string(label) + " authoritative local entity is absent at navigation epoch"
    );

    require(
        navigation.currentSystemId == it->systemId,
        std::string(label) + " navigation system does not follow its authoritative local entity"
    );

    const glm::dvec3 shipPosition =
        world::coordinates::fullMeters(it->worldPosition);
    const double errorMeters = glm::length(
        navigation.systemLocalMeters - shipPosition
    );

    require(
        std::isfinite(errorMeters) && errorMeters <= 0.05,
        std::string(label) + " navigation position does not match its authoritative local entity"
    );
}
}

int runMultiplayerClientAcceptanceSelfTest()
{
    try
    {
        LocalLoopbackTransport transportA;
        LocalLoopbackTransport transportB;
        game::debug::LocalDebugSessionControl debugChannel;
        WorldParams worldParams;

        game::server::ServerRuntime runtime(
            worldParams,
            transportA,
            debugChannel
        );

        GameClient clientA(transportA);
        clientA.beginSynchronization();

        // Consume A's immediate welcome/bootstrap through the real GameClient
        // before choosing a second controlled entity. The harness never reads
        // GameServer/GameSimulation memory and never forges a session welcome.
        clientA.updateSynchronization(0.0);
        require(
            clientA.playerId().value != 0,
            "client A did not accept its server-assigned controlled entity"
        );

        const EntityId shipAId = clientA.playerId();
        const EntityId shipBId = chooseSecondaryControlledShip(clientA);
        require(
            shipBId.value != 0 && shipBId != shipAId,
            "initial world has no second ship suitable for a second client"
        );

        const auto sessionB = runtime.attachPlayerSessionTransport(
            transportB,
            shipBId
        );
        require(
            static_cast<bool>(sessionB),
            "ServerRuntime rejected the second player session transport"
        );
        require(
            runtime.connectedPlayerSessionCount() == 2,
            "ServerRuntime does not report two connected player sessions"
        );

        GameClient clientB(transportB);
        clientB.beginSynchronization();
        synchronizeClients(runtime, clientA, clientB);

        require(
            clientA.playerId() == shipAId,
            "client A controlled entity changed during synchronization"
        );
        require(
            clientB.playerId() == shipBId,
            "client B did not receive its own server-assigned controlled entity"
        );
        require(
            clientA.playerId() != clientB.playerId(),
            "two GameClient instances were assigned the same controlled entity"
        );

        require(
            clientA.world().localControlledEntityId() == shipAId &&
            clientB.world().localControlledEntityId() == shipBId,
            "GameClient did not propagate SessionWelcome identity into ClientWorldState"
        );
        require(
            clientA.world().isLocalControlledEntity(shipAId) &&
            !clientA.world().isLocalControlledEntity(shipBId),
            "client A does not distinguish local A from remote B"
        );
        require(
            clientB.world().isLocalControlledEntity(shipBId) &&
            !clientB.world().isLocalControlledEntity(shipAId),
            "client B does not distinguish local B from remote A"
        );

        // Both real clients must retain both replicated entities while applying
        // prediction only to their own server-assigned identity.
        (void)requireShip(clientA, shipAId, "ship A on client A");
        (void)requireShip(clientA, shipBId, "ship B on client A");
        (void)requireShip(clientB, shipAId, "ship A on client B");
        (void)requireShip(clientB, shipBId, "ship B on client B");

        requireNavigationMatchesAuthoritativeLocalEntity(clientA, shipAId, "client A");
        requireNavigationMatchesAuthoritativeLocalEntity(clientB, shipBId, "client B");

        // Start A first so the two client-side numbered input streams cannot
        // accidentally look identical. B remains a fully synchronized remote
        // human entity on A while it has emitted no local command of its own.
        ShipControlState controlA;
        controlA.forwardInput = 0.75f;
        for (int frame = 0; frame < 24; ++frame)
            runGameplayFrame(runtime, clientA, clientB, &controlA, nullptr);

        const std::uint64_t aAckBeforeB =
            clientA.lastAcknowledgedControlTick();
        require(
            aAckBeforeB > 0,
            "client A did not receive authoritative acknowledgement before B started input"
        );
        require(
            clientB.lastAcknowledgedControlTick() == 0,
            "client B acknowledged input before B had emitted any numbered input"
        );

        ShipControlState controlB;
        controlB.yawInput = 0.65f;
        for (int frame = 0; frame < 24; ++frame)
            runGameplayFrame(runtime, clientA, clientB, &controlA, &controlB);

        ShipControlState neutral;
        for (int frame = 0; frame < AckDrainFrameLimit; ++frame)
            runGameplayFrame(runtime, clientA, clientB, &neutral, &neutral);

        const std::uint64_t ackA = clientA.lastAcknowledgedControlTick();
        const std::uint64_t ackB = clientB.lastAcknowledgedControlTick();
        require(ackB > 0, "client B never received an authoritative input acknowledgement");
        require(
            ackA > ackB,
            "independent client input sequences collapsed into one shared acknowledgement stream"
        );

        const auto& aOnA = requireShip(clientA, shipAId, "ship A on client A");
        const auto& bOnA = requireShip(clientA, shipBId, "ship B on client A");
        const auto& aOnB = requireShip(clientB, shipAId, "ship A on client B");
        const auto& bOnB = requireShip(clientB, shipBId, "ship B on client B");

        require(
            aOnA.acknowledgedControlTick == ackA &&
            aOnB.acknowledgedControlTick == ackA,
            "clients disagree on authoritative acknowledgement for ship A"
        );
        require(
            bOnA.acknowledgedControlTick == ackB &&
            bOnB.acknowledgedControlTick == ackB,
            "clients disagree on authoritative acknowledgement for ship B"
        );

        requireNavigationMatchesAuthoritativeLocalEntity(clientA, shipAId, "client A after input");
        requireNavigationMatchesAuthoritativeLocalEntity(clientB, shipBId, "client B after input");

        std::cerr
            << "[PASS] two GameClient instances share one authoritative runtime"
            << " localA=" << shipAId.value
            << " localB=" << shipBId.value
            << " ackA=" << ackA
            << " ackB=" << ackB
            << "\n";
        return 0;
    }
    catch (const MultiplayerAcceptanceFailure& ex)
    {
        std::cerr << "[FAIL] multiplayer client acceptance: " << ex.what() << "\n";
        return 2;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] multiplayer client acceptance exception: " << ex.what() << "\n";
        return 3;
    }
}
}
