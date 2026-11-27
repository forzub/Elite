#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "src/scene/EntityID.h"

namespace game::simulation
{
/*
    What the canonical replication snapshot has already PUBLISHED about each ship's module graph (author's review
    2026-11-27: the network publication history is not ship state).

    The ship / module runtime owns the facts and their revisions (ObjectModuleRuntime::revision(), the simulation's
    structural graph revision bumped by markShipGraphDirty). This ledger only remembers what was sent:
      - a detailed graph was published (the next demotion publishes ONE explicit empty graph);
      - the module revision last published (small module-row deltas);
      - the structural revision last seen and how many full-graph publications it still owes (2: the old delivery
        redundancy, counted in publications, not ticks);
      - detached-fragment / repair-job payloads last published non-empty (one explicit empty list after they end).
    Its lifetime is the replication's: entries of entities the snapshot no longer has a ship for are pruned by
    retainOnly(), never by the simulation's ship removal. A late-joining client is served by the server's hydration
    path from the canonical snapshot, not by this ledger.
*/
class ShipGraphPublicationLedger
{
public:
    static constexpr int kStructuralGraphPublications = 2;

    bool detailedGraphPublished(EntityId id) const { return m_detailed.count(id) != 0; }

    // the structure changed since the last publication: owe kStructuralGraphPublications full graphs
    void observeStructureRevision(EntityId id, std::uint64_t structureRevision)
    {
        // first sight: a ship dirtied before its first publication (revision != 0) owes the redundancy too (the old
        // per-ship counter was set at the dirty event, before any publication)
        auto [it, inserted] = m_structureRevision.try_emplace(id, structureRevision);
        if (inserted ? structureRevision != 0 : it->second != structureRevision)
        {
            it->second = structureRevision;
            m_fullGraphsOwed[id] = kStructuralGraphPublications;
        }
    }
    bool fullGraphOwed(EntityId id) const { return m_fullGraphsOwed.count(id) != 0; }

    bool moduleRevisionPublished(EntityId id, std::uint64_t revision) const
    {
        const auto it = m_moduleRevision.find(id);
        return it != m_moduleRevision.end() && it->second == revision;
    }

    // a full structural graph went out
    void fullGraphPublished(EntityId id, std::uint64_t moduleRevision)
    {
        m_detailed.insert(id);
        m_moduleRevision[id] = moduleRevision;
        if (const auto it = m_fullGraphsOwed.find(id); it != m_fullGraphsOwed.end() && --it->second <= 0)
            m_fullGraphsOwed.erase(it);
    }
    void moduleRowsPublished(EntityId id, std::uint64_t moduleRevision) { m_moduleRevision[id] = moduleRevision; }

    // the ship left detailed replication: returns true when an explicit empty graph must clear the old one
    bool demote(EntityId id)
    {
        m_fullGraphsOwed.erase(id);
        m_moduleRevision.erase(id);
        return m_detailed.erase(id) != 0;
    }

    bool fragmentsPublished(EntityId id) const { return m_fragments.count(id) != 0; }
    void setFragmentsPublished(EntityId id, bool nonEmpty) { nonEmpty ? (void)m_fragments.insert(id) : (void)m_fragments.erase(id); }
    bool repairJobsPublished(EntityId id) const { return m_repairJobs.count(id) != 0; }
    void setRepairJobsPublished(EntityId id, bool nonEmpty) { nonEmpty ? (void)m_repairJobs.insert(id) : (void)m_repairJobs.erase(id); }

    // drop the history of every entity `alive` rejects (its ship left the simulation)
    template <class Alive>
    void retainOnly(const Alive& alive)
    {
        prune(m_detailed, alive);
        prune(m_fragments, alive);
        prune(m_repairJobs, alive);
        prune(m_fullGraphsOwed, alive);
        prune(m_moduleRevision, alive);
        prune(m_structureRevision, alive);
    }

    std::size_t trackedEntities() const { return m_structureRevision.size(); }

private:
    template <class C, class Alive>
    static void prune(C& c, const Alive& alive)
    {
        for (auto it = c.begin(); it != c.end();)
        {
            if (alive(keyOf(*it)))
                ++it;
            else
                it = c.erase(it);
        }
    }
    static EntityId keyOf(const EntityId& id) { return id; }
    template <class V>
    static EntityId keyOf(const std::pair<const EntityId, V>& kv) { return kv.first; }

    std::unordered_set<EntityId> m_detailed;
    std::unordered_map<EntityId, int> m_fullGraphsOwed;
    std::unordered_map<EntityId, std::uint64_t> m_moduleRevision;
    std::unordered_map<EntityId, std::uint64_t> m_structureRevision;
    std::unordered_set<EntityId> m_fragments;
    std::unordered_set<EntityId> m_repairJobs;
};
} // namespace game::simulation
