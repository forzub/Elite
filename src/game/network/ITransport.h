#pragma once

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/MapSnapshotMessage.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/scene/EntityID.h"

class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual void sendInput(
        EntityId id,
        const ShipControlState& control) = 0;

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

    // Read-only presentation data. The atlas is static session catalog data;
    // the celestial snapshot is the current authoritative system state.
    virtual void requestStarAtlas() = 0;
    virtual bool receiveStarAtlas(
        world::celestial::StarAtlasDatabase& outAtlas
    ) = 0;

    virtual void requestCelestialSnapshot() = 0;
    virtual bool receiveCelestialSnapshot(
        world::celestial::CelestialSystemSnapshot& outSnapshot
    ) = 0;
};
