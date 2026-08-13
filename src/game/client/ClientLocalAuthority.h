#pragma once

#include "src/scene/EntityID.h"

namespace game::client
{
/*
    Client-local control identity is assigned by SessionWelcome. ShipRole tells
    us what kind of authoritative actor a replicated ship is; it must never be
    used to decide which human ship receives this client's prediction/input or
    local-player presentation.
*/
inline bool isLocalControlledEntity(
    EntityId candidate,
    EntityId controlledEntityId
) noexcept
{
    return
        controlledEntityId.value != 0 &&
        candidate == controlledEntityId;
}
}
