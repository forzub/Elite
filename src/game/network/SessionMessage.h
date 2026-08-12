#pragma once

#include "src/scene/EntityID.h"

namespace game::network
{
/*
    One-time server-assigned session bootstrap data.

    The controlled entity is connection authority, not client input. Keep it
    outside recurring simulation snapshots so stable session metadata is not
    resent at replication cadence.
*/
struct SessionWelcome
{
    EntityId controlledEntityId {0};
};
}
