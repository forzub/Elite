#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <variant>

#include "src/game/network/TcpTransport.h"
#include "src/game/network/ReplicationEnvelope.h"

namespace
{
using namespace game::network;

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[FAIL] TCP wire transport contract: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

template<typename Predicate>
bool spinUntil(
    TcpClientTransport& client,
    TcpServerTransport& server,
    Predicate predicate,
    int iterations = 500)
{
    for (int i = 0; i < iterations; ++i)
    {
        client.service();
        server.update(0.0f);

        if (predicate())
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return false;
}

std::unique_ptr<TcpServerTransport> acceptConnected(
    TcpServerListener& listener,
    TcpClientTransport& client)
{
    for (int i = 0; i < 500; ++i)
    {
        if (auto accepted = listener.acceptPending())
            return accepted;

        client.service();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return nullptr;
}

SimulationSnapshot makeSnapshot()
{
    SimulationSnapshot snapshot;
    snapshot.metadata.serverTick = 42u;
    snapshot.metadata.serverTimeSeconds = 12.5;
    snapshot.metadata.universeTimeSeconds = 123456.0;
    snapshot.metadata.universeTimelineRevision = 9u;
    snapshot.replication.entitySetMode =
        ReplicatedEntitySetMode::SparseRetainMissing;
    snapshot.replication.removedShipIds.push_back(EntityId{99u});

    ShipSnapshot ship {};
    ship.id = EntityId{7u};
    ship.instanceId = 7007u;
    ship.role = ShipRole::Player;
    ship.typeId = ObjectType::CobraMk1;
    ship.acknowledgedControlTick = 77u;
    ship.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    ship.transform.worldPosition.localMeters =
        glm::dvec3(100.0, 200.0, 300.0);
    ship.transform.motion.systemId = 3;
    snapshot.ships.push_back(ship);

    snapshot.session.playerNavigation.currentSystemId = 3;
    snapshot.session.playerNavigation.systemLocalMeters =
        glm::dvec3(100.0, 200.0, 300.0);
    snapshot.session.universeTimeSeconds = 123456.0;
    snapshot.session.universeTimelineRevision = 9u;
    snapshot.session.universeDate = "3026-01-01";
    return snapshot;
}

void testFullProtocolAcrossKernelTcp()
{
    TcpServerListener listener;
    require(listener.listen("127.0.0.1", 0u),
        "listener failed: " + listener.lastError());
    require(listener.localPort() != 0u, "ephemeral listener port was not assigned");

    TcpClientTransport client;
    require(client.connect("127.0.0.1", listener.localPort()),
        "client connect failed: " + client.lastError());

    auto server = acceptConnected(listener, client);
    require(server != nullptr, "server did not accept localhost connection");
    require(client.connected(), "client is not connected after TCP handshake");
    require(server->connected(), "server endpoint is not connected after accept");

    SessionHello hello;
    hello.accountHandle = "tcp-test";
    for (std::size_t i = 0; i < hello.authToken.bytes.size(); ++i)
        hello.authToken.bytes[i] = static_cast<std::uint8_t>(0x40u + i);
    hello.intent = AuthenticationIntent::Register;
    client.sendSessionHello(hello);

    SessionHello receivedHello;
    const bool receivedIdentity = spinUntil(
        client,
        *server,
        [&]()
        {
            return server->receiveSessionHello(receivedHello);
        }
    );
    require(receivedIdentity, "server did not receive SessionHello over TCP");
    require(receivedHello.accountHandle == hello.accountHandle,
        "SessionHello account handle changed across TCP");
    require(receivedHello.authToken == hello.authToken,
        "opaque SessionHello auth token changed across TCP");
    require(receivedHello.intent == hello.intent,
        "SessionHello authentication intent changed across TCP");

    SessionReject reject;
    reject.reason = SessionRejectReason::UnknownAccount;
    reject.retryable = true;
    server->publishSessionRejectImmediately(reject);

    SessionReject receivedReject;
    const bool receivedRejectFrame = spinUntil(
        client,
        *server,
        [&]() { return client.receiveSessionReject(receivedReject); }
    );
    require(receivedRejectFrame, "client did not receive SessionReject over TCP");
    require(receivedReject.reason == reject.reason &&
            receivedReject.retryable == reject.retryable,
        "SessionReject changed across TCP");

    SessionWelcome welcome;
    welcome.sessionId.value = 1234u;
    welcome.playerId = PlayerId{55u};
    welcome.controlledShipInstanceId = 7007u;
    welcome.controlledEntityId = EntityId{7u};
    welcome.fixedStepSeconds = 0.02;
    welcome.starAtlasCatalog.schemaVersion = 4u;
    welcome.starAtlasCatalog.contentFingerprint = 0x1122334455667788ull;
    server->publishSessionWelcomeImmediately(welcome);

    const SimulationSnapshot snapshot = makeSnapshot();
    server->publishSnapshotImmediately(snapshot);

    GalaxyMapResponse galaxyResponse;
    galaxyResponse.requestId = 9001u;
    galaxyResponse.metadata = snapshot.metadata;
    galaxyResponse.snapshot.universeTimeSeconds =
        snapshot.metadata.universeTimeSeconds;
    galaxyResponse.snapshot.universeDate = "3026-01-01";
    MapResponse mapResponse = galaxyResponse;
    server->sendMapResponse(mapResponse);

    TimeSyncResponse timeResponse;
    timeResponse.sequence = 88u;
    timeResponse.clientSendTimeSeconds = 5.5;
    timeResponse.serverReceiveTimeSeconds = 5.75;
    server->sendTimeSyncResponse(timeResponse);

    SessionWelcome receivedWelcome;
    SimulationSnapshot receivedSnapshot;
    MapResponse receivedMapResponse;
    TimeSyncResponse receivedTimeResponse;

    bool hasWelcome = false;
    bool hasSnapshot = false;
    bool hasMapResponse = false;
    bool hasTimeResponse = false;
    const bool receivedServerPlane = spinUntil(
        client,
        *server,
        [&]()
        {
            if (!hasWelcome)
                hasWelcome = client.receiveSessionWelcome(receivedWelcome);
            if (!hasSnapshot)
                hasSnapshot = client.receiveSnapshot(receivedSnapshot);
            if (!hasMapResponse)
                hasMapResponse = client.receiveMapResponse(receivedMapResponse);
            if (!hasTimeResponse)
                hasTimeResponse = client.receiveTimeSyncResponse(receivedTimeResponse);
            return hasWelcome && hasSnapshot && hasMapResponse && hasTimeResponse;
        }
    );

    require(receivedServerPlane,
        "client did not receive complete server->client protocol plane");
    require(receivedWelcome.sessionId == welcome.sessionId,
        "SessionWelcome changed across TCP");
    require(receivedWelcome.playerId == welcome.playerId,
        "PlayerId changed across TCP");
    require(receivedWelcome.controlledShipInstanceId == welcome.controlledShipInstanceId,
        "ShipInstanceId changed across TCP");
    require(receivedWelcome.controlledEntityId == welcome.controlledEntityId,
        "controlled entity changed across TCP");
    require(std::abs(receivedWelcome.fixedStepSeconds - welcome.fixedStepSeconds) < 1.0e-12,
        "authoritative fixed step changed across TCP");
    require(receivedSnapshot.metadata.serverTick == snapshot.metadata.serverTick,
        "snapshot metadata changed across TCP");
    require(receivedSnapshot.replication.entitySetMode ==
            ReplicatedEntitySetMode::SparseRetainMissing,
        "sparse replication mode changed across TCP");
    require(receivedSnapshot.replication.removedShipIds.size() == 1u &&
            receivedSnapshot.replication.removedShipIds.front() == EntityId{99u},
        "snapshot lifecycle removals changed across TCP");
    require(receivedSnapshot.ships.size() == 1u &&
            receivedSnapshot.ships.front().id == EntityId{7u} &&
            receivedSnapshot.ships.front().instanceId == 7007u &&
            receivedSnapshot.ships.front().acknowledgedControlTick == 77u,
        "hydrated ship row changed across TCP");
    require(std::holds_alternative<GalaxyMapResponse>(receivedMapResponse) &&
            std::get<GalaxyMapResponse>(receivedMapResponse).requestId == 9001u,
        "MapResponse changed across TCP");
    require(receivedTimeResponse.sequence == 88u &&
            std::abs(receivedTimeResponse.serverReceiveTimeSeconds - 5.75) < 1.0e-12,
        "TimeSyncResponse changed across TCP");

    ClientMessage clientMessage;
    clientMessage.clientTick = 501u;
    ShipControlState control;
    control.controlTick = 501u;
    control.forwardInput = 0.75f;
    control.yawInput = -0.25f;
    clientMessage.payload = control;
    client.sendClientMessage(clientMessage);

    MapRequest request = GalaxyMapRequest{6002u};
    client.sendMapRequest(request);

    TimeSyncRequest timeRequest;
    timeRequest.sequence = 7003u;
    timeRequest.clientSendTimeSeconds = 9.25;
    client.sendTimeSyncRequest(timeRequest);

    ClientMessage receivedClientMessage;
    MapRequest receivedRequest;
    TimeSyncRequest receivedTimeRequest;

    bool hasClientMessage = false;
    bool hasMapRequest = false;
    bool hasTimeRequest = false;
    const bool receivedClientPlane = spinUntil(
        client,
        *server,
        [&]()
        {
            if (!hasClientMessage)
                hasClientMessage = server->receiveClientMessage(receivedClientMessage);
            if (!hasMapRequest)
                hasMapRequest = server->receiveMapRequest(receivedRequest);
            if (!hasTimeRequest)
                hasTimeRequest = server->receiveTimeSyncRequest(receivedTimeRequest);
            return hasClientMessage && hasMapRequest && hasTimeRequest;
        }
    );

    require(receivedClientPlane,
        "server did not receive complete client->server protocol plane");
    require(receivedClientMessage.clientTick == 501u &&
            std::holds_alternative<ShipControlState>(receivedClientMessage.payload),
        "ClientMessage changed across TCP");
    const auto& receivedControl =
        std::get<ShipControlState>(receivedClientMessage.payload);
    require(receivedControl.controlTick == 501u &&
            std::abs(receivedControl.forwardInput - 0.75f) < 1.0e-6f,
        "ShipControlState changed across TCP");
    require(std::holds_alternative<GalaxyMapRequest>(receivedRequest) &&
            std::get<GalaxyMapRequest>(receivedRequest).requestId == 6002u,
        "Galaxy MapRequest changed across TCP");
    require(receivedTimeRequest.sequence == 7003u &&
            std::abs(receivedTimeRequest.clientSendTimeSeconds - 9.25) < 1.0e-12,
        "TimeSyncRequest changed across TCP");

    client.disconnect();
    const bool serverSawDisconnect = spinUntil(
        client,
        *server,
        [&]() { return !server->connected(); }
    );
    require(serverSawDisconnect,
        "server endpoint did not observe peer disconnect");
}

} // namespace

int main()
{
    testFullProtocolAcrossKernelTcp();
    std::cout
        << "[PASS] real localhost TCP carries control + sparse hydrated data plane"
        << '\n';
    return 0;
}
