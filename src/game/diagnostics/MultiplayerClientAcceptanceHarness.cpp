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
game::network::SessionHello makeAcceptanceIdentity(
    std::uint64_t tokenSeed,
    std::uint64_t salt
)
{
    game::network::SessionHello hello;
    hello.accountHandle = "accept-" + std::to_string(tokenSeed);
    for (std::size_t i = 0; i < hello.authToken.bytes.size(); ++i)
    {
        const unsigned shift = static_cast<unsigned>((i % 8u) * 8u);
        hello.authToken.bytes[i] = static_cast<std::uint8_t>(
            ((tokenSeed >> shift) ^ (salt + i * 29u)) & 0xffu
        );
    }
    if (!hello.authToken.valid())
        hello.authToken.bytes[0] = 1;
    hello.intent = game::network::AuthenticationIntent::Register;
    return hello;
}

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

void synchronizeClient(
    game::server::ServerRuntime& runtime,
    GameClient& client,
    const char* label
)
{
    for (int frame = 0; frame < SynchronizationFrameLimit; ++frame)
    {
        runtime.advance(FrameSeconds);
        client.updateSynchronization(FrameSeconds);

        if (client.readyForGameplay())
            return;

        if (client.connectionState() == ClientConnectionState::Failed)
        {
            throw MultiplayerAcceptanceFailure(
                std::string(label) + " synchronization failed: " +
                client.connectionError()
            );
        }
    }

    throw MultiplayerAcceptanceFailure(
        std::string(label) + " did not reach Ready"
    );
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

void requireAccountReconnectReturnsSamePersistentPlayer()
{
    game::debug::LocalDebugSessionControl debugChannel;
    WorldParams worldParams;
    game::server::ServerRuntime runtime(worldParams, debugChannel);
    const auto registrationIdentity = makeAcceptanceIdentity(3001u, 31u);
    auto signInIdentity = registrationIdentity;
    signInIdentity.intent = game::network::AuthenticationIntent::SignIn;

    auto invalidHandleIdentity = signInIdentity;
    invalidHandleIdentity.accountHandle = "-bad";
    LocalLoopbackTransport invalidHandleTransport;
    const auto invalidHandleSession = runtime.attachPlayerSessionTransport(
        invalidHandleTransport,
        invalidHandleIdentity
    );
    require(!invalidHandleSession, "invalid account handle was admitted");
    game::network::SessionReject invalidHandleReject;
    require(
        invalidHandleTransport.receiveSessionReject(invalidHandleReject) &&
        invalidHandleReject.reason ==
            game::network::SessionRejectReason::InvalidAccountHandle,
        "invalid account handle did not receive typed InvalidAccountHandle rejection"
    );

    LocalLoopbackTransport unknownSignInTransport;
    const auto unknownSession = runtime.attachPlayerSessionTransport(
        unknownSignInTransport,
        signInIdentity
    );
    require(!unknownSession,
            "unknown credential was implicitly enrolled during sign-in");
    game::network::SessionReject unknownReject;
    require(
        unknownSignInTransport.receiveSessionReject(unknownReject) &&
        unknownReject.reason == game::network::SessionRejectReason::UnknownAccount,
        "unknown sign-in did not receive typed UnknownAccount rejection"
    );

    LocalLoopbackTransport transportFirst;
    const auto firstSession = runtime.attachPlayerSessionTransport(
        transportFirst,
        registrationIdentity
    );
    require(static_cast<bool>(firstSession), "first account enrollment failed");

    auto conflictingRegistration = makeAcceptanceIdentity(3999u, 91u);
    conflictingRegistration.accountHandle = registrationIdentity.accountHandle;
    LocalLoopbackTransport conflictingTransport;
    const auto conflictingSession = runtime.attachPlayerSessionTransport(
        conflictingTransport,
        conflictingRegistration
    );
    require(!conflictingSession,
            "duplicate account handle was rebound to another credential");
    game::network::SessionReject conflictingReject;
    require(
        conflictingTransport.receiveSessionReject(conflictingReject) &&
        conflictingReject.reason ==
            game::network::SessionRejectReason::AccountHandleTaken,
        "duplicate account handle did not receive AccountHandleTaken rejection"
    );

    GameClient firstClient(transportFirst);
    firstClient.beginSynchronization();
    synchronizeClient(runtime, firstClient, "first account session");

    const PlayerId playerId = firstClient.playerIdentityId();
    const ShipInstanceId shipId = firstClient.controlledShipInstanceId();
    const EntityId entityId = firstClient.localControlledEntityId();
    require(playerId && shipId != 0 && entityId.value != 0,
            "first account session has incomplete persistent authority");

    // Build a non-zero control epoch before disconnect. A fresh GameClient
    // created below starts its own controlTick sequence at 1; reconnect must
    // not inherit this old acknowledgement through the persistent EntityId.
    ShipControlState firstControl;
    firstControl.yawInput = 0.4f;
    const float fixedDt = static_cast<float>(runtime.fixedStepSeconds());
    for (int frame = 0; frame < 36; ++frame)
    {
        firstClient.prepareGameplayFrame(FrameSeconds);
        firstClient.submitInput(firstControl);
        runtime.advance(FrameSeconds);
        firstClient.update(
            static_cast<float>(FrameSeconds),
            fixedDt,
            FrameSeconds
        );
    }

    const std::uint64_t oldAcknowledgedControlTick =
        firstClient.lastAcknowledgedControlTick();
    require(
        oldAcknowledgedControlTick > 1,
        "first account session did not establish a control epoch before reconnect"
    );

    LocalLoopbackTransport duplicateTransport;
    const auto duplicateSession = runtime.attachPlayerSessionTransport(
        duplicateTransport,
        signInIdentity
    );
    require(!duplicateSession,
            "same account obtained two concurrent gameplay sessions");
    game::network::SessionReject duplicateReject;
    require(
        duplicateTransport.receiveSessionReject(duplicateReject) &&
        duplicateReject.reason == game::network::SessionRejectReason::AlreadyActive,
        "duplicate live sign-in did not receive typed AlreadyActive rejection"
    );

    require(
        runtime.detachPlayerSessionTransport(firstSession),
        "first account session could not detach for reconnect"
    );

    LocalLoopbackTransport reconnectTransport;
    const auto reconnectSession = runtime.attachPlayerSessionTransport(
        reconnectTransport,
        signInIdentity
    );
    require(static_cast<bool>(reconnectSession),
            "known account could not reconnect after disconnect");

    GameClient reconnectClient(reconnectTransport);
    reconnectClient.beginSynchronization();
    synchronizeClient(runtime, reconnectClient, "reconnected account session");

    require(
        reconnectClient.playerIdentityId() == playerId,
        "reconnected account was rebound to a different PlayerId"
    );
    require(
        reconnectClient.controlledShipInstanceId() == shipId,
        "reconnected account was rebound to a different ShipInstanceId"
    );
    require(
        reconnectClient.localControlledEntityId() == entityId,
        "reconnected account lost the existing materialized ship EntityId"
    );
    require(
        reconnectClient.lastAcknowledgedControlTick() == 0,
        "reconnected GameClient inherited the previous session control epoch"
    );

    ShipControlState reconnectControl;
    reconnectControl.yawInput = -0.35f;
    for (int frame = 0; frame < 24; ++frame)
    {
        reconnectClient.prepareGameplayFrame(FrameSeconds);
        reconnectClient.submitInput(reconnectControl);
        runtime.advance(FrameSeconds);
        reconnectClient.update(
            static_cast<float>(FrameSeconds),
            fixedDt,
            FrameSeconds
        );
    }

    const std::uint64_t reconnectAcknowledgedControlTick =
        reconnectClient.lastAcknowledgedControlTick();
    require(
        reconnectAcknowledgedControlTick > 0 &&
        reconnectAcknowledgedControlTick < oldAcknowledgedControlTick,
        "new session controlTick sequence was not accepted as a fresh epoch"
    );

    std::cerr
        << "[PASS] account reconnect returns same persistent player/ship with fresh control epoch"
        << " player=" << playerId.value
        << " ship=" << shipId
        << " entity=" << entityId.value
        << " old_ack=" << oldAcknowledgedControlTick
        << " reconnect_ack=" << reconnectAcknowledgedControlTick
        << "\n";
}

void requireDedicatedBootstrapAdmissionPath()
{
    LocalLoopbackTransport transportA;
    LocalLoopbackTransport transportB;
    game::debug::LocalDebugSessionControl debugChannel;
    WorldParams worldParams;

    // This constructor is the dedicated-server path used by NetworkServerHost.
    // It materializes two explicit ShipRole::Player bootstrap admission slots.
    game::server::ServerRuntime runtime(worldParams, debugChannel);

    const auto sessionA = runtime.attachPlayerSessionTransport(
        transportA,
        makeAcceptanceIdentity(2001u, 1u)
    );
    const auto sessionB = runtime.attachPlayerSessionTransport(
        transportB,
        makeAcceptanceIdentity(2002u, 2u)
    );
    require(static_cast<bool>(sessionA), "dedicated bootstrap rejected client A");
    require(static_cast<bool>(sessionB), "dedicated bootstrap rejected client B");
    require(
        runtime.connectedPlayerSessionCount() == 2,
        "dedicated bootstrap runtime does not hold two sessions"
    );

    GameClient clientA(transportA);
    GameClient clientB(transportB);
    clientA.beginSynchronization();
    clientB.beginSynchronization();
    synchronizeClients(runtime, clientA, clientB);

    const EntityId shipAId = clientA.playerId();
    const EntityId shipBId = clientB.playerId();
    require(
        shipAId.value != 0 && shipBId.value != 0 && shipAId != shipBId,
        "dedicated bootstrap did not assign two distinct player entities"
    );

    const auto& aOnA = requireShip(clientA, shipAId, "bootstrap A on client A");
    const auto& bOnA = requireShip(clientA, shipBId, "bootstrap B on client A");
    const auto& aOnB = requireShip(clientB, shipAId, "bootstrap A on client B");
    const auto& bOnB = requireShip(clientB, shipBId, "bootstrap B on client B");

    require(
        aOnA.role == ShipRole::Player && bOnA.role == ShipRole::Player &&
        aOnB.role == ShipRole::Player && bOnB.role == ShipRole::Player,
        "dedicated bootstrap admitted a non-player-role ship"
    );
    require(
        aOnA.typeId == bOnA.typeId && aOnB.typeId == bOnB.typeId,
        "dedicated bootstrap player slots do not share the authored ship type"
    );
    require(
        aOnA.transform.motion.systemId == bOnA.transform.motion.systemId &&
        aOnA.transform.motion.hubId == bOnA.transform.motion.hubId,
        "dedicated bootstrap player slots are not in one hub reference context"
    );

    const glm::dvec3 worldA =
        world::coordinates::fullMeters(aOnA.transform.worldPosition);
    const glm::dvec3 worldB =
        world::coordinates::fullMeters(bOnA.transform.worldPosition);
    const double worldDistance = glm::length(worldB - worldA);
    const double localDistance = glm::length(
        bOnA.transform.motion.localPositionMeters -
        aOnA.transform.motion.localPositionMeters
    );

    require(
        std::isfinite(worldDistance) && std::abs(worldDistance - 50.0) <= 0.5,
        "dedicated bootstrap world-space player spacing is not 50 m"
    );
    require(
        std::isfinite(localDistance) && std::abs(localDistance - 50.0) <= 0.5,
        "dedicated bootstrap hub-local player spacing is not 50 m"
    );

    // Exercise the same client prediction/presentation update used after NEW GAME.
    for (int frame = 0; frame < 12; ++frame)
        runGameplayFrame(runtime, clientA, clientB, nullptr, nullptr);

    const auto& aRenderOnA = requireShip(clientA, shipAId, "bootstrap A render on client A");
    const auto& bRenderOnA = requireShip(clientA, shipBId, "bootstrap B render on client A");
    const auto& aRenderOnB = requireShip(clientB, shipAId, "bootstrap A render on client B");
    const auto& bRenderOnB = requireShip(clientB, shipBId, "bootstrap B render on client B");

    const auto renderDistance = [](const ClientShipState& lhs, const ClientShipState& rhs)
    {
        return glm::length(
            world::coordinates::fullMeters(rhs.renderTransform.worldPosition) -
            world::coordinates::fullMeters(lhs.renderTransform.worldPosition)
        );
    };

    const double renderDistanceA = renderDistance(aRenderOnA, bRenderOnA);
    const double renderDistanceB = renderDistance(aRenderOnB, bRenderOnB);
    require(
        std::isfinite(renderDistanceA) && std::abs(renderDistanceA - 50.0) <= 1.0,
        "client A presentation does not preserve dedicated bootstrap spacing"
    );
    require(
        std::isfinite(renderDistanceB) && std::abs(renderDistanceB - 50.0) <= 1.0,
        "client B presentation does not preserve dedicated bootstrap spacing"
    );

    std::cerr
        << "[PASS] dedicated two-slot bootstrap admission"
        << " localA=" << shipAId.value
        << " localB=" << shipBId.value
        << " worldDistanceM=" << worldDistance
        << " renderDistanceA=" << renderDistanceA
        << " renderDistanceB=" << renderDistanceB
        << "\n";
}

}

int runMultiplayerClientAcceptanceSelfTest()
{
    try
    {
        requireAccountReconnectReturnsSamePersistentPlayer();
        requireDedicatedBootstrapAdmissionPath();

        LocalLoopbackTransport transportA;
        LocalLoopbackTransport transportB;
        game::debug::LocalDebugSessionControl debugChannel;
        WorldParams worldParams;

        // Exercise the actual dedicated admission path: two persistent player
        // identities, each assigned to its own persistent bootstrap ship, on
        // one authoritative ServerRuntime. No NPC EntityId is hijacked.
        game::server::ServerRuntime runtime(
            worldParams,
            debugChannel
        );

        const auto sessionA = runtime.attachPlayerSessionTransport(
            transportA,
            makeAcceptanceIdentity(2001u, 1u)
        );
        const auto sessionB = runtime.attachPlayerSessionTransport(
            transportB,
            makeAcceptanceIdentity(2002u, 2u)
        );
        require(
            static_cast<bool>(sessionA) && static_cast<bool>(sessionB),
            "ServerRuntime did not admit two persistent player identities"
        );

        GameClient clientA(transportA);
        GameClient clientB(transportB);
        clientA.beginSynchronization();
        clientB.beginSynchronization();
        synchronizeClients(runtime, clientA, clientB);

        require(
            clientA.playerIdentityId() && clientB.playerIdentityId() &&
            clientA.playerIdentityId() != clientB.playerIdentityId(),
            "two clients were not assigned distinct persistent PlayerIds"
        );
        require(
            clientA.controlledShipInstanceId() != 0 &&
            clientB.controlledShipInstanceId() != 0 &&
            clientA.controlledShipInstanceId() !=
                clientB.controlledShipInstanceId(),
            "two players were not assigned distinct persistent ShipInstanceIds"
        );

        const EntityId shipAId = clientA.localControlledEntityId();
        const EntityId shipBId = clientB.localControlledEntityId();
        require(
            shipAId.value != 0 && shipBId.value != 0 && shipAId != shipBId,
            "two players were not materialized as distinct runtime entities"
        );
        require(
            static_cast<bool>(sessionB),
            "ServerRuntime rejected the second player session transport"
        );
        require(
            runtime.connectedPlayerSessionCount() == 2,
            "ServerRuntime does not report two connected player sessions"
        );

        require(
            clientA.localControlledEntityId() == shipAId &&
            clientB.localControlledEntityId() == shipBId,
            "client runtime control identity changed during synchronization"
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
        const auto& shipAOnA = requireShip(clientA, shipAId, "ship A on client A");
        const auto& shipBOnA = requireShip(clientA, shipBId, "ship B on client A");
        const auto& shipAOnB = requireShip(clientB, shipAId, "ship A on client B");
        const auto& shipBOnB = requireShip(clientB, shipBId, "ship B on client B");

        require(
            shipAOnA.instanceId == clientA.controlledShipInstanceId() &&
            shipBOnB.instanceId == clientB.controlledShipInstanceId() &&
            shipAOnA.instanceId == shipAOnB.instanceId &&
            shipBOnA.instanceId == shipBOnB.instanceId,
            "persistent ship identity diverged between two client world views"
        );

        const glm::dvec3 shipAPosition =
            world::coordinates::fullMeters(shipAOnA.transform.worldPosition);
        const glm::dvec3 shipBPosition =
            world::coordinates::fullMeters(shipBOnA.transform.worldPosition);
        const double bootstrapDistanceMeters =
            glm::length(shipBPosition - shipAPosition);
        require(
            std::isfinite(bootstrapDistanceMeters) &&
            std::abs(bootstrapDistanceMeters - 50.0) <= 0.05,
            "two bootstrap player ships are not 50 m apart in one replicated world"
        );

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
