#pragma once

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/MapSnapshotMessage.h"
#include "src/game/network/PresentationDataMessage.h"
#include "src/game/network/TimeSyncMessage.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/scene/EntityID.h"

class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual bool receiveSnapshot(
        SimulationSnapshot& outSnapshot) = 0;

    virtual void update(float dt) = 0;

    virtual void sendClientMessage(
        EntityId playerId,
        const game::network::ClientMessage& msg
    ) = 0;

    virtual void sendMapRequest(
        const game::network::MapRequest& request
    ) = 0;

    virtual bool receiveMapResponse(
        game::network::MapResponse& outResponse
    ) = 0;

    virtual void sendPresentationDataRequest(
        const game::network::PresentationDataRequest& request
    ) = 0;

    virtual bool receivePresentationDataResponse(
        game::network::PresentationDataResponse& outResponse
    ) = 0;

    virtual void sendTimeSyncRequest(
        const game::network::TimeSyncRequest& request
    ) = 0;

    virtual bool receiveTimeSyncResponse(
        game::network::TimeSyncResponse& outResponse
    ) = 0;
};
