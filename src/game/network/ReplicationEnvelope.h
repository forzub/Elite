#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/scene/EntityID.h"

namespace game::network
{

/*
    Semantics of the entity vectors carried by one SimulationSnapshot.

    FullAuthoritativeSet preserves the legacy contract: if an entity is absent
    from a snapshot, it no longer belongs to the client's replicated world.

    SparseRetainMissing is the future sparse-replication contract: omission is
    only "no update in this packet". Destruction/interest exit is represented
    explicitly through removed* lists. This distinction must exist before the
    server is allowed to decimate per-entity publication cadence.
*/
enum class ReplicatedEntitySetMode : std::uint8_t
{
    FullAuthoritativeSet = 0,
    SparseRetainMissing = 1
};

struct ReplicationEnvelope
{
    ReplicatedEntitySetMode entitySetMode =
        ReplicatedEntitySetMode::FullAuthoritativeSet;

    // Explicit lifecycle removals are meaningful only for SparseRetainMissing.
    // They are present in the protocol now so later omission can never be
    // confused with destruction or interest exit.
    std::vector<EntityId> removedShipIds;
    std::vector<EntityId> removedObjectIds;
    std::vector<std::string> removedHubIds;

};

} // namespace game::network
