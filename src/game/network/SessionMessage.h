#pragma once

#include "src/scene/EntityID.h"
#include "src/game/network/ProtocolMetadata.h"

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

    // Static physical catalog is loaded independently at each endpoint. The
    // server sends only a compatibility fence, never the catalog payload.
    CatalogMetadata starAtlasCatalog;
};
}
