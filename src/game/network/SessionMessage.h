#pragma once

#include "src/scene/EntityID.h"
#include "src/game/identity/PlayerId.h"
#include "src/game/ship/ShipRegistry.h"
#include <cstdint>
#include "src/game/network/ProtocolMetadata.h"

namespace game::network
{
struct ServerSessionId
{
    std::uint64_t value = 0;

    constexpr explicit operator bool() const noexcept
    {
        return value != 0;
    }

    friend constexpr bool operator==(
        ServerSessionId a,
        ServerSessionId b
    ) noexcept
    {
        return a.value == b.value;
    }

    friend constexpr bool operator!=(
        ServerSessionId a,
        ServerSessionId b
    ) noexcept
    {
        return !(a == b);
    }
};

/*
    One-time server-assigned session bootstrap data.

    The controlled entity is connection authority, not client input. Keep it
    outside recurring simulation snapshots so stable session metadata is not
    resent at replication cadence.
*/
struct SessionWelcome
{
    ServerSessionId sessionId {};
    PlayerId playerId {};
    ShipInstanceId controlledShipInstanceId = 0;
    EntityId controlledEntityId {0};

    // Authoritative fixed simulation cadence used by remote client prediction.
    // Local sessions can obtain this directly from their host, but a process-
    // remote client must receive it as session bootstrap metadata.
    double fixedStepSeconds = 0.0;

    // Static physical catalog is loaded independently at each endpoint. The
    // server sends only a compatibility fence, never the catalog payload.
    CatalogMetadata starAtlasCatalog;
};
}
