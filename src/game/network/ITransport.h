#pragma once

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/simulation/SimulationSnapshot.h"
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
};
