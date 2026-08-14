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

inline game::simulation::ObjectGraphSnapshot materializeGraphSnapshot(
    const game::simulation::ObjectGraphSnapshot* previous,
    const game::simulation::ObjectGraphSnapshot& incoming
)
{
    auto canonical = incoming;
    if (!previous)
        return canonical;

    // Entity presence and field payload completeness are separate concepts.
    // Even a FullAuthoritativeSet snapshot intentionally omits heavy graph
    // fields unless they changed, so canonical/hydration state must retain
    // those fields independently from entity-set semantics.
    if (!incoming.hasModules)
    {
        canonical.hasModules = previous->hasModules;
        canonical.modules = previous->modules;
    }

    if (!incoming.hasStructuralLinks)
    {
        canonical.hasStructuralLinks = previous->hasStructuralLinks;
        canonical.structuralLinks = previous->structuralLinks;
    }

    if (!incoming.hasAssemblyModules)
    {
        canonical.hasAssemblyModules = previous->hasAssemblyModules;
        canonical.assemblyModules = previous->assemblyModules;
    }

    if (!incoming.hasDetachedFragments)
    {
        canonical.hasDetachedFragments = previous->hasDetachedFragments;
        canonical.detachedFragments = previous->detachedFragments;
    }

    if (!incoming.hasRepairJobs)
    {
        canonical.hasRepairJobs = previous->hasRepairJobs;
        canonical.repairJobs = previous->repairJobs;
    }

    if (!incoming.hasDebugHitVolumes)
    {
        canonical.hasDebugHitVolumes = previous->hasDebugHitVolumes;
        canonical.debugHitVolumes = previous->debugHitVolumes;
    }

    return canonical;
}

inline ShipSnapshot materializeShipSnapshot(
    const ShipSnapshot* previous,
    const ShipSnapshot& incoming
)
{
    ShipSnapshot canonical = incoming;
    canonical.graph = materializeGraphSnapshot(
        previous ? &previous->graph : nullptr,
        incoming.graph
    );
    return canonical;
}

inline ObjectSnapshot materializeObjectSnapshot(
    const ObjectSnapshot* previous,
    const ObjectSnapshot& incoming
)
{
    ObjectSnapshot canonical = incoming;
    canonical.graph = materializeGraphSnapshot(
        previous ? &previous->graph : nullptr,
        incoming.graph
    );
    return canonical;
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

    if (previousCanonical == nullptr)
    {
        SimulationSnapshot canonical = incoming;
        canonical.replication.entitySetMode = Mode::FullAuthoritativeSet;
        canonical.replication.removedShipIds.clear();
        canonical.replication.removedObjectIds.clear();
        canonical.replication.removedHubIds.clear();
        return canonical;
    }

    if (incoming.replication.entitySetMode == Mode::FullAuthoritativeSet)
    {
        // FullAuthoritativeSet means the entity vectors are a complete presence
        // set. It does NOT mean every heavy nested graph field is present. Build
        // a new complete entity set while retaining omitted graph fields from
        // the previous canonical sample for entities that still exist.
        SimulationSnapshot canonical = incoming;
        canonical.ships.clear();
        canonical.objects.clear();

        canonical.ships.reserve(incoming.ships.size());
        for (const auto& ship : incoming.ships)
        {
            const auto previousIt = std::find_if(
                previousCanonical->ships.begin(),
                previousCanonical->ships.end(),
                [&](const ShipSnapshot& value)
                {
                    return value.id == ship.id;
                }
            );

            canonical.ships.push_back(
                detail::materializeShipSnapshot(
                    previousIt == previousCanonical->ships.end()
                        ? nullptr
                        : &*previousIt,
                    ship
                )
            );
        }

        canonical.objects.reserve(incoming.objects.size());
        for (const auto& object : incoming.objects)
        {
            const auto previousIt = std::find_if(
                previousCanonical->objects.begin(),
                previousCanonical->objects.end(),
                [&](const ObjectSnapshot& value)
                {
                    return value.id == object.id;
                }
            );

            canonical.objects.push_back(
                detail::materializeObjectSnapshot(
                    previousIt == previousCanonical->objects.end()
                        ? nullptr
                        : &*previousIt,
                    object
                )
            );
        }

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
        const auto previousIt = std::find_if(
            canonical.ships.begin(),
            canonical.ships.end(),
            [&](const ShipSnapshot& value)
            {
                return value.id == ship.id;
            }
        );

        const auto materialized = detail::materializeShipSnapshot(
            previousIt == canonical.ships.end() ? nullptr : &*previousIt,
            ship
        );

        detail::upsertById(
            canonical.ships,
            materialized,
            [](const ShipSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& object : incoming.objects)
    {
        const auto previousIt = std::find_if(
            canonical.objects.begin(),
            canonical.objects.end(),
            [&](const ObjectSnapshot& value)
            {
                return value.id == object.id;
            }
        );

        const auto materialized = detail::materializeObjectSnapshot(
            previousIt == canonical.objects.end() ? nullptr : &*previousIt,
            object
        );

        detail::upsertById(
            canonical.objects,
            materialized,
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
