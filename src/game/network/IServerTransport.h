#pragma once

#include "src/game/network/ClientMessage.h"
#include "src/game/network/MapSnapshotMessage.h"
#include "src/game/network/TimeSyncMessage.h"
#include "src/game/network/SessionMessage.h"
#include "src/game/simulation/SimulationSnapshot.h"

/*
    Server-side endpoint of a transport connection.

    This interface deliberately contains messages and replicated snapshots only.
    It must not expose GameServer or any mutable authoritative runtime object.
    The local loopback implementation is mutex-protected and may be consumed
    from the authoritative worker thread while the client endpoint is used by
    the application thread. The protocol surface remains suitable for a later
    process/socket transport without changing GameClient or GameServer ownership.
*/
class IServerTransport
{
public:
    virtual ~IServerTransport() = default;

    // Advances transport-local delivery/latency state. A real socket-backed
    // implementation may make this a no-op and feed the queues asynchronously.
    virtual void update(float dt) = 0;

    virtual bool receiveSessionHello(
        game::network::SessionHello& outHello
    ) = 0;

    virtual bool receiveClientMessage(
        game::network::ClientMessage& outMessage
    ) = 0;

    virtual bool receiveMapRequest(
        game::network::MapRequest& outRequest
    ) = 0;

    virtual bool receiveTimeSyncRequest(
        game::network::TimeSyncRequest& outRequest
    ) = 0;

    virtual void publishSessionWelcomeImmediately(
        const game::network::SessionWelcome& welcome
    ) = 0;

    virtual void publishSnapshot(
        const SimulationSnapshot& snapshot
    ) = 0;

    // Bootstrap is intentionally explicit: the first authoritative snapshot
    // must be available before the in-process client starts synchronization.
    virtual void publishSnapshotImmediately(
        const SimulationSnapshot& snapshot
    ) = 0;

    virtual void sendMapResponse(
        game::network::MapResponse response
    ) = 0;

    virtual void sendTimeSyncResponse(
        game::network::TimeSyncResponse response
    ) = 0;
};
