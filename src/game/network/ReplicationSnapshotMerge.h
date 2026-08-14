#pragma once

#include <algorithm>
#include <string>
#include <utility>

#include "src/game/simulation/SimulationSnapshot.h"

namespace game::network
{
namespace detail
{
template <typename T, typename IdFn, typename IdT>
inline void eraseById(
    std::vector<T>& values,
    const IdT& id,
    IdFn&& idOf
)
{
    values.erase(
        std::remove_if(
            values.begin(),
            values.end(),
            [&](const T& value)
            {
                return idOf(value) == id;
            }
        ),
        values.end()
    );
}

template <typename T, typename IdFn>
inline void upsertById(
    std::vector<T>& values,
    const T& incoming,
    IdFn&& idOf
)
{
    const auto incomingId = idOf(incoming);
    const auto it = std::find_if(
        values.begin(),
        values.end(),
        [&](const T& value)
        {
            return idOf(value) == incomingId;
        }
    );

    if (it == values.end())
        values.push_back(incoming);
    else
        *it = incoming;
}
} // namespace detail

/*
    Materialize one canonical/full-presence history sample from a possibly
    sparse transport snapshot.

    ClientWorldState applies only the incoming updates/removals to live entity
    state, but map/interpolation history needs a complete retained set at each
    server epoch. Sparse omission therefore inherits the previous canonical
    entity row, while explicit removals erase it.

    The server protocol guarantees a FullAuthoritativeSet bootstrap before any
    SparseRetainMissing packet. If no baseline is supplied here, the incoming
    packet is returned as-is; callers should treat that as a bootstrap fault if
    sparse publication is ever enabled without a prior full sample.
*/
inline SimulationSnapshot materializeCanonicalReplicationSnapshot(
    const SimulationSnapshot* previousCanonical,
    const SimulationSnapshot& incoming
)
{
    using Mode = ReplicatedEntitySetMode;

    if (incoming.replication.entitySetMode == Mode::FullAuthoritativeSet ||
        previousCanonical == nullptr)
    {
        SimulationSnapshot canonical = incoming;
        canonical.replication.entitySetMode = Mode::FullAuthoritativeSet;
        canonical.replication.removedShipIds.clear();
        canonical.replication.removedObjectIds.clear();
        canonical.replication.removedHubIds.clear();
        return canonical;
    }

    SimulationSnapshot canonical = *previousCanonical;
    canonical.metadata = incoming.metadata;
    canonical.session = incoming.session;
    canonical.signals = incoming.signals;

    for (const auto& ship : incoming.ships)
    {
        detail::upsertById(
            canonical.ships,
            ship,
            [](const ShipSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& object : incoming.objects)
    {
        detail::upsertById(
            canonical.objects,
            object,
            [](const ObjectSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& hub : incoming.hubs)
    {
        detail::upsertById(
            canonical.hubs,
            hub,
            [](const game::simulation::OrbitalHubSnapshot& value)
                -> const std::string&
            {
                return value.id;
            }
        );
    }

    for (const auto id : incoming.replication.removedShipIds)
    {
        detail::eraseById(
            canonical.ships,
            id.value,
            [](const ShipSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto id : incoming.replication.removedObjectIds)
    {
        detail::eraseById(
            canonical.objects,
            id.value,
            [](const ObjectSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& id : incoming.replication.removedHubIds)
    {
        detail::eraseById(
            canonical.hubs,
            id,
            [](const game::simulation::OrbitalHubSnapshot& value)
                -> const std::string&
            {
                return value.id;
            }
        );
    }

    // The history sample is now materialized/full even though the transport
    // packet that produced it was sparse.
    canonical.replication = incoming.replication;
    canonical.replication.entitySetMode = Mode::FullAuthoritativeSet;
    canonical.replication.removedShipIds.clear();
    canonical.replication.removedObjectIds.clear();
    canonical.replication.removedHubIds.clear();

    return canonical;
}

} // namespace game::network
