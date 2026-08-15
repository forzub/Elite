#pragma once

#include "src/galaxy/Actors.h"
#include "src/game/identity/PlayerId.h"
#include "src/game/ship/ShipRegistry.h"

struct PlayerState
{
    PlayerId       id {};
    ActorId        actor = 0;       // persistent social/faction actor identity
    ActorCode      activeCode = 0;  // current cryptographic/transponder code
    int            credits = 0;
    ShipInstanceId currentShipId = 0; // persistent ship assignment, not EntityId
};
