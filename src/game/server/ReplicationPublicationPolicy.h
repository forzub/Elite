#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/game/server/ReplicationInterestPolicy.h"
#include "src/game/simulation/SimulationSnapshot.h"

namespace game::server
{

/*
    Per-connection publication memory for sparse replication.

    Interest answers "how often should this destination receive this ship?".
    Publication state answers the different transport question "what has this
    client already been hydrated with, and when was its last update sent?".

    This state is deliberately owned by ServerRunner's transport binding. It is
    neither authoritative simulation state nor gameplay visibility/knowledge.
*/
struct ReplicationPublicationState
{
    std::unordered_set<std::uint32_t> replicatedShipIds;
    std::unordered_map<std::uint32_t, double> lastShipPublicationTimeSeconds;

    // Objects/hubs remain full-cadence in Stage M7, but once the envelope is
    // SparseRetainMissing their lifecycle still needs explicit remove rows.
    std::unordered_set<std::uint32_t> replicatedObjectIds;
    std::unordered_set<std::string> replicatedHubIds;
    // the source object / hub id sets the two sets above were last made from (0: unknown); equal
    // fingerprints = nothing appeared or left, no per-destination set rebuild
    std::uint64_t replicatedObjectsFingerprint = 0;
    std::uint64_t replicatedHubsFingerprint = 0;

    bool hasBootstrapBaseline = false;
};

struct ReplicationPublicationSelection
{
    // Every id in shipUpdateIds is included in this transport packet.
    // shipHydrationIds is a subset that must use the server's canonical/full
    // retained runtime row rather than the current sparse-field ship row.
    std::vector<EntityId> shipUpdateIds;
    std::vector<EntityId> shipHydrationIds;

    std::vector<EntityId> removedShipIds;
    std::vector<EntityId> removedObjectIds;
    std::vector<std::string> removedHubIds;
};

inline void sortEntityIds(std::vector<EntityId>& ids)
{
    std::sort(
        ids.begin(),
        ids.end(),
        [](EntityId a, EntityId b)
        {
            return a.value < b.value;
        }
    );

    ids.erase(
        std::unique(
            ids.begin(),
            ids.end(),
            [](EntityId a, EntityId b)
            {
                return a.value == b.value;
            }
        ),
        ids.end()
    );
}

inline void seedReplicationPublicationState(
    ReplicationPublicationState& state,
    const SimulationSnapshot& bootstrapSnapshot
)
{
    state = {};

    const double bootstrapTime =
        bootstrapSnapshot.metadata.serverTimeSeconds;

    for (const auto& ship : bootstrapSnapshot.ships)
    {
        state.replicatedShipIds.insert(ship.id.value);
        state.lastShipPublicationTimeSeconds[ship.id.value] = bootstrapTime;
    }

    for (const auto& object : bootstrapSnapshot.objects)
        state.replicatedObjectIds.insert(object.id.value);

    for (const auto& hub : bootstrapSnapshot.hubs)
        state.replicatedHubIds.insert(hub.id);

    state.hasBootstrapBaseline = true;
}

// The id sets of one source snapshot, built ONCE per publication and shared by every destination
// (load test 2026-11-08: rebuilding them per session - 153 sessions x 161 ships / 110 objects /
// 107 hub-id strings - cost 8.7 ms per publication).
struct ReplicationSourceIds
{
    std::unordered_set<std::uint32_t> ships;
    std::unordered_set<std::uint32_t> objects;
    std::unordered_set<std::string> hubs;
    std::uint64_t objectsFingerprint = 0;
    std::uint64_t hubsFingerprint = 0;
};

inline ReplicationSourceIds buildReplicationSourceIds(const SimulationSnapshot& snapshot)
{
    ReplicationSourceIds ids;
    ids.ships.reserve(snapshot.ships.size());
    for (const auto& ship : snapshot.ships)
        ids.ships.insert(ship.id.value);
    // order-independent fingerprints (sum of mixed hashes) + the count, never 0
    const auto mix = [](std::uint64_t h) {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL;
        return h ^ (h >> 33);
    };
    ids.objects.reserve(snapshot.objects.size());
    std::uint64_t fo = 0;
    for (const auto& object : snapshot.objects)
    {
        ids.objects.insert(object.id.value);
        fo += mix(object.id.value + 0x9e3779b97f4a7c15ULL);
    }
    ids.objectsFingerprint = (fo ^ mix(snapshot.objects.size())) | 1u;
    ids.hubs.reserve(snapshot.hubs.size());
    std::uint64_t fh = 0;
    for (const auto& hub : snapshot.hubs)
    {
        ids.hubs.insert(hub.id);
        fh += mix(std::hash<std::string> {}(hub.id));
    }
    ids.hubsFingerprint = (fh ^ mix(snapshot.hubs.size() + 7u)) | 1u;
    return ids;
}

inline ReplicationPublicationSelection selectReplicationPublications(
    const ShipReplicationInterestPlan& shipInterest,
    const SimulationSnapshot& currentSnapshot,
    ReplicationPublicationState& state,
    const ReplicationSourceIds& source
);

inline ReplicationPublicationSelection selectReplicationPublications(
    const ShipReplicationInterestPlan& shipInterest,
    const SimulationSnapshot& currentSnapshot,
    ReplicationPublicationState& state
)
{
    return selectReplicationPublications(shipInterest, currentSnapshot, state, buildReplicationSourceIds(currentSnapshot));
}

inline ReplicationPublicationSelection selectReplicationPublications(
    const ShipReplicationInterestPlan& shipInterest,
    const SimulationSnapshot& currentSnapshot,
    ReplicationPublicationState& state,
    const ReplicationSourceIds& source
)
{
    ReplicationPublicationSelection selection;

    // Sparse packets are not legal before one full bootstrap baseline has been
    // delivered to this transport. The caller keeps publication full until the
    // binding has been seeded explicitly.
    if (!state.hasBootstrapBaseline)
        return selection;

    const double now = currentSnapshot.metadata.serverTimeSeconds;
    constexpr double TimeToleranceSeconds = 1.0e-9;

    const auto& currentShipIds = source.ships;

    for (const auto& decision : shipInterest.decisions)
    {
        const std::uint32_t id = decision.id.value;
        const bool alreadyReplicated =
            state.replicatedShipIds.find(id) !=
                state.replicatedShipIds.end();

        if (decision.tier == ShipReplicationInterestTier::None)
        {
            if (alreadyReplicated)
            {
                selection.removedShipIds.push_back(decision.id);
                state.replicatedShipIds.erase(id);
                state.lastShipPublicationTimeSeconds.erase(id);
            }
            continue;
        }

        bool due = !alreadyReplicated;

        if (alreadyReplicated)
        {
            if (decision.targetIntervalSeconds <= 0.0)
            {
                due = true;
            }
            else
            {
                const auto lastIt =
                    state.lastShipPublicationTimeSeconds.find(id);

                if (lastIt == state.lastShipPublicationTimeSeconds.end() ||
                    !std::isfinite(lastIt->second) ||
                    now + TimeToleranceSeconds < lastIt->second)
                {
                    // Missing/corrupt cadence memory or a discontinuity must
                    // fail toward sending state, never toward starving it.
                    due = true;
                }
                else
                {
                    due =
                        now - lastIt->second + TimeToleranceSeconds >=
                        decision.targetIntervalSeconds;
                }
            }
        }

        if (!due)
            continue;

        selection.shipUpdateIds.push_back(decision.id);

        if (!alreadyReplicated)
        {
            // First publication after interest entry/re-entry is a hydration,
            // not an ordinary sparse-field update.
            selection.shipHydrationIds.push_back(decision.id);
            state.replicatedShipIds.insert(id);
        }

        state.lastShipPublicationTimeSeconds[id] = now;
    }

    // A ship can disappear from the authoritative full source set without ever
    // receiving another interest decision. That is lifecycle destruction/removal,
    // not sparse omission, so clear it immediately for this destination.
    std::vector<std::uint32_t> staleShipIds;
    staleShipIds.reserve(state.replicatedShipIds.size());
    for (const auto id : state.replicatedShipIds)
    {
        if (currentShipIds.find(id) == currentShipIds.end())
            staleShipIds.push_back(id);
    }

    for (const auto id : staleShipIds)
    {
        selection.removedShipIds.push_back(EntityId{id});
        state.replicatedShipIds.erase(id);
        state.lastShipPublicationTimeSeconds.erase(id);
    }

    // Stage M7 only decimates ships. Objects and hubs continue to be included
    // in every packet, but SparseRetainMissing makes their disappearance
    // ambiguous unless we publish explicit lifecycle removals too.
    if (state.replicatedObjectsFingerprint != source.objectsFingerprint)
    {
        for (const auto id : state.replicatedObjectIds)
        {
            if (source.objects.find(id) == source.objects.end())
                selection.removedObjectIds.push_back(EntityId{id});
        }
        state.replicatedObjectIds = source.objects;
        state.replicatedObjectsFingerprint = source.objectsFingerprint;
    }

    if (state.replicatedHubsFingerprint != source.hubsFingerprint)
    {
        for (const auto& id : state.replicatedHubIds)
        {
            if (source.hubs.find(id) == source.hubs.end())
                selection.removedHubIds.push_back(id);
        }
        state.replicatedHubIds = source.hubs;
        state.replicatedHubsFingerprint = source.hubsFingerprint;
    }

    sortEntityIds(selection.shipUpdateIds);
    sortEntityIds(selection.shipHydrationIds);
    sortEntityIds(selection.removedShipIds);
    sortEntityIds(selection.removedObjectIds);
    std::sort(selection.removedHubIds.begin(), selection.removedHubIds.end());
    selection.removedHubIds.erase(
        std::unique(
            selection.removedHubIds.begin(),
            selection.removedHubIds.end()
        ),
        selection.removedHubIds.end()
    );

    return selection;
}

} // namespace game::server
