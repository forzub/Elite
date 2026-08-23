#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "src/game/network/ReplicationSnapshotMerge.h"
#include "src/game/server/ReplicationInterestPolicy.h"
#include "src/game/server/ReplicationPublicationPolicy.h"
#include "src/world/coordinates/WorldPosition.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(2);
    }
}

ShipSnapshot makeShip(
    std::uint32_t id,
    int systemId,
    double xMeters
)
{
    ShipSnapshot ship;
    ship.id = EntityId{id};
    ship.transform.motion.systemId = systemId;
    ship.transform.setWorldPosition(
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(xMeters, 0.0, 0.0)
        )
    );
    return ship;
}

const game::server::ShipReplicationInterestDecision& requireDecision(
    const game::server::ShipReplicationInterestPlan& plan,
    std::uint32_t id
)
{
    const auto it = std::find_if(
        plan.decisions.begin(),
        plan.decisions.end(),
        [&](const auto& decision)
        {
            return decision.id.value == id;
        }
    );

    require(it != plan.decisions.end(), "missing replication-interest decision");
    return *it;
}

bool hasShip(const SimulationSnapshot& snapshot, std::uint32_t id)
{
    return std::any_of(
        snapshot.ships.begin(),
        snapshot.ships.end(),
        [&](const ShipSnapshot& ship)
        {
            return ship.id.value == id;
        }
    );
}

bool hasObject(const SimulationSnapshot& snapshot, std::uint32_t id)
{
    return std::any_of(
        snapshot.objects.begin(),
        snapshot.objects.end(),
        [&](const ObjectSnapshot& object)
        {
            return object.id.value == id;
        }
    );
}

bool hasHub(const SimulationSnapshot& snapshot, const std::string& id)
{
    return std::any_of(
        snapshot.hubs.begin(),
        snapshot.hubs.end(),
        [&](const auto& hub)
        {
            return hub.id == id;
        }
    );
}

bool hasEntityId(const std::vector<EntityId>& ids, std::uint32_t id)
{
    return std::any_of(
        ids.begin(),
        ids.end(),
        [&](EntityId candidate)
        {
            return candidate.value == id;
        }
    );
}

const ShipSnapshot& requireShipSnapshot(
    const SimulationSnapshot& snapshot,
    std::uint32_t id
)
{
    const auto it = std::find_if(
        snapshot.ships.begin(),
        snapshot.ships.end(),
        [&](const ShipSnapshot& ship)
        {
            return ship.id.value == id;
        }
    );
    require(it != snapshot.ships.end(), "missing expected ship snapshot");
    return *it;
}
}

int main()
{
    using game::network::ReplicatedEntitySetMode;
    using game::server::ShipReplicationInterestTier;

    game::server::ReplicationInterestPolicy policy;
    policy.tacticalRadiusMeters = 1000.0;
    policy.nearbyRadiusMeters = 10000.0;
    policy.nearbyIntervalSeconds = 0.25;
    policy.coarseIntervalSeconds = 2.0;

    SimulationSnapshot world;
    world.ships.push_back(makeShip(1, 0, 0.0));
    world.ships.push_back(makeShip(2, 0, 500.0));
    world.ships.push_back(makeShip(3, 0, 5000.0));
    world.ships.push_back(makeShip(4, 0, 50000.0));
    world.ships.push_back(makeShip(5, 1, 100.0));

    const auto plan = game::server::buildShipReplicationInterestPlan(
        EntityId{1},
        world,
        policy
    );

    require(
        requireDecision(plan, 1).tier == ShipReplicationInterestTier::Controlled,
        "controlled ship is not pinned to Controlled replication interest"
    );
    require(
        requireDecision(plan, 2).tier == ShipReplicationInterestTier::Tactical,
        "same-system tactical ship interest is wrong"
    );
    require(
        requireDecision(plan, 3).tier == ShipReplicationInterestTier::Nearby,
        "same-system nearby ship interest is wrong"
    );
    require(
        requireDecision(plan, 4).tier == ShipReplicationInterestTier::Coarse,
        "same-system far ship interest is wrong"
    );
    require(
        requireDecision(plan, 5).tier == ShipReplicationInterestTier::None,
        "different-system ship must be out of current replication scope"
    );
    require(
        std::abs(requireDecision(plan, 3).targetIntervalSeconds - 0.25) < 1e-9 &&
        std::abs(requireDecision(plan, 4).targetIntervalSeconds - 2.0) < 1e-9,
        "interest tier does not carry its target publication cadence"
    );
    require(
        plan.summary.controlledShips == 1 &&
        plan.summary.tacticalShips == 1 &&
        plan.summary.nearbyShips == 1 &&
        plan.summary.coarseShips == 1 &&
        plan.summary.outOfScopeShips == 1,
        "replication-interest summary counts are wrong"
    );

    // Full-authoritative-set preserves the legacy omission=remove contract.
    SimulationSnapshot baseline;
    baseline.metadata.serverTick = 10;
    baseline.metadata.serverTimeSeconds = 10.0;
    baseline.replication.entitySetMode =
        ReplicatedEntitySetMode::FullAuthoritativeSet;
    baseline.ships.push_back(makeShip(1, 0, 0.0));
    baseline.ships.push_back(makeShip(2, 0, 100.0));
    baseline.ships.back().transform.motion.worldVelocityMps =
        glm::dvec3(40.0, 0.0, 0.0);

    // Bootstrap carries the heavy structural graph. Later full-presence
    // publications may omit these fields while the entity itself remains.
    baseline.ships.front().graph.hasModules = true;
    game::simulation::ObjectModuleSnapshot engineModule;
    engineModule.moduleId = "engine";
    engineModule.health = 77.0f;
    baseline.ships.front().graph.modules.push_back(engineModule);

    ObjectSnapshot objectA;
    objectA.id = EntityId{100};
    baseline.objects.push_back(objectA);

    game::simulation::OrbitalHubSnapshot hubA;
    hubA.id = "hub-A";
    baseline.hubs.push_back(hubA);

    const auto canonicalBaseline =
        game::network::materializeCanonicalReplicationSnapshot(
            nullptr,
            baseline
        );

    require(
        hasShip(canonicalBaseline, 1) && hasShip(canonicalBaseline, 2),
        "full bootstrap did not remain canonical"
    );

    // Sparse omission retains entity 2/object/hub while ship 1 is updated.
    SimulationSnapshot sparse;
    sparse.metadata.serverTick = 11;
    sparse.metadata.serverTimeSeconds = 10.1;
    sparse.replication.entitySetMode =
        ReplicatedEntitySetMode::SparseRetainMissing;
    sparse.ships.push_back(makeShip(1, 0, 250.0));

    const auto retained =
        game::network::materializeCanonicalReplicationSnapshot(
            &canonicalBaseline,
            sparse
        );

    require(
        hasShip(retained, 1) && hasShip(retained, 2),
        "sparse omission deleted a retained ship"
    );
    require(
        hasObject(retained, 100) && hasHub(retained, "hub-A"),
        "sparse omission deleted retained infrastructure"
    );
    require(
        std::abs(
            world::coordinates::fullMeters(
                requireShipSnapshot(retained, 2).transform.worldPosition
            ).x - 104.0
        ) < 1.0e-6,
        "sparse omission stamped an old ship position onto a newer packet epoch"
    );
    require(
        retained.replication.entitySetMode ==
            ReplicatedEntitySetMode::FullAuthoritativeSet,
        "canonical history sample must materialize as a full entity set"
    );
    require(
        requireShipSnapshot(retained, 1).graph.hasModules &&
        requireShipSnapshot(retained, 1).graph.modules.size() == 1 &&
        requireShipSnapshot(retained, 1).graph.modules.front().moduleId == "engine",
        "canonical sparse history lost an omitted nested ship graph payload"
    );

    // Explicit lifecycle remove is the only sparse deletion mechanism.
    SimulationSnapshot remove;
    remove.metadata.serverTick = 12;
    remove.metadata.serverTimeSeconds = 10.2;
    remove.replication.entitySetMode =
        ReplicatedEntitySetMode::SparseRetainMissing;
    remove.replication.removedShipIds.push_back(EntityId{2});
    remove.replication.removedObjectIds.push_back(EntityId{100});
    remove.replication.removedHubIds.push_back("hub-A");

    const auto removed =
        game::network::materializeCanonicalReplicationSnapshot(
            &retained,
            remove
        );

    require(!hasShip(removed, 2), "explicit sparse ship removal was ignored");
    require(!hasObject(removed, 100), "explicit sparse object removal was ignored");
    require(!hasHub(removed, "hub-A"), "explicit sparse hub removal was ignored");

    // A later full set intentionally returns to omission=remove semantics.
    SimulationSnapshot fullAgain;
    fullAgain.metadata.serverTick = 13;
    fullAgain.metadata.serverTimeSeconds = 10.3;
    fullAgain.replication.entitySetMode =
        ReplicatedEntitySetMode::FullAuthoritativeSet;
    fullAgain.ships.push_back(makeShip(1, 0, 300.0));

    const auto canonicalFullAgain =
        game::network::materializeCanonicalReplicationSnapshot(
            &removed,
            fullAgain
        );
    require(
        canonicalFullAgain.ships.size() == 1 && hasShip(canonicalFullAgain, 1),
        "full-authoritative-set did not replace the previous retained set"
    );
    require(
        requireShipSnapshot(canonicalFullAgain, 1).graph.hasModules &&
        requireShipSnapshot(canonicalFullAgain, 1).graph.modules.size() == 1 &&
        requireShipSnapshot(canonicalFullAgain, 1).graph.modules.front().health == 77.0f,
        "full-presence publication incorrectly discarded omitted nested graph state"
    );

    // Stage M7: cadence is now consumed per destination after a full bootstrap.
    SimulationSnapshot cadenceWorld = world;
    cadenceWorld.metadata.serverTimeSeconds = 0.0;

    game::server::ReplicationPublicationState publicationState;
    game::server::seedReplicationPublicationState(
        publicationState,
        cadenceWorld
    );

    cadenceWorld.metadata.serverTimeSeconds = 0.06;
    auto cadencePlan = game::server::buildShipReplicationInterestPlan(
        EntityId{1},
        cadenceWorld,
        policy
    );
    auto firstSparse = game::server::selectReplicationPublications(
        cadencePlan,
        cadenceWorld,
        publicationState
    );

    require(
        hasEntityId(firstSparse.shipUpdateIds, 1) &&
        hasEntityId(firstSparse.shipUpdateIds, 2),
        "controlled/tactical ships were not selected at normal snapshot cadence"
    );
    require(
        !hasEntityId(firstSparse.shipUpdateIds, 3) &&
        !hasEntityId(firstSparse.shipUpdateIds, 4),
        "nearby/coarse ships ignored their lower publication cadence"
    );
    require(
        hasEntityId(firstSparse.removedShipIds, 5),
        "out-of-scope bootstrap ship did not receive explicit interest-exit removal"
    );

    cadenceWorld.metadata.serverTimeSeconds = 0.26;
    cadencePlan = game::server::buildShipReplicationInterestPlan(
        EntityId{1},
        cadenceWorld,
        policy
    );
    auto nearbyDue = game::server::selectReplicationPublications(
        cadencePlan,
        cadenceWorld,
        publicationState
    );
    require(
        hasEntityId(nearbyDue.shipUpdateIds, 3) &&
        !hasEntityId(nearbyDue.shipUpdateIds, 4),
        "nearby/coarse cadence gates do not mature independently"
    );

    cadenceWorld.metadata.serverTimeSeconds = 2.10;
    cadencePlan = game::server::buildShipReplicationInterestPlan(
        EntityId{1},
        cadenceWorld,
        policy
    );
    auto coarseDue = game::server::selectReplicationPublications(
        cadencePlan,
        cadenceWorld,
        publicationState
    );
    require(
        hasEntityId(coarseDue.shipUpdateIds, 4),
        "coarse ship never matured to its target publication interval"
    );

    // Re-entry after an explicit interest exit must hydrate before ordinary
    // sparse-field updates are allowed again.
    cadenceWorld.ships.back().transform.motion.systemId = 0;
    cadenceWorld.metadata.serverTimeSeconds = 2.16;
    cadencePlan = game::server::buildShipReplicationInterestPlan(
        EntityId{1},
        cadenceWorld,
        policy
    );
    auto reentry = game::server::selectReplicationPublications(
        cadencePlan,
        cadenceWorld,
        publicationState
    );
    require(
        hasEntityId(reentry.shipUpdateIds, 5) &&
        hasEntityId(reentry.shipHydrationIds, 5),
        "interest re-entry did not request a full hydration row"
    );

    // Destruction/removal must not wait for a coarse cadence deadline.
    cadenceWorld.ships.erase(
        std::remove_if(
            cadenceWorld.ships.begin(),
            cadenceWorld.ships.end(),
            [](const ShipSnapshot& ship)
            {
                return ship.id.value == 4;
            }
        ),
        cadenceWorld.ships.end()
    );
    cadenceWorld.metadata.serverTimeSeconds = 2.20;
    cadencePlan = game::server::buildShipReplicationInterestPlan(
        EntityId{1},
        cadenceWorld,
        policy
    );
    auto destroyed = game::server::selectReplicationPublications(
        cadencePlan,
        cadenceWorld,
        publicationState
    );
    require(
        hasEntityId(destroyed.removedShipIds, 4),
        "authoritative ship removal waited for sparse publication cadence"
    );

    // Objects/hubs are not decimated yet, but sparse envelope semantics still
    // require explicit lifecycle removals when their full source set changes.
    SimulationSnapshot infrastructureBaseline;
    infrastructureBaseline.metadata.serverTimeSeconds = 5.0;
    ObjectSnapshot trackedObject;
    trackedObject.id = EntityId{100};
    infrastructureBaseline.objects.push_back(trackedObject);
    game::simulation::OrbitalHubSnapshot trackedHub;
    trackedHub.id = "hub-A";
    infrastructureBaseline.hubs.push_back(trackedHub);

    game::server::ReplicationPublicationState infrastructureState;
    game::server::seedReplicationPublicationState(
        infrastructureState,
        infrastructureBaseline
    );

    SimulationSnapshot infrastructureGone;
    infrastructureGone.metadata.serverTimeSeconds = 5.1;
    const auto infrastructureRemoval =
        game::server::selectReplicationPublications(
            {},
            infrastructureGone,
            infrastructureState
        );
    require(
        hasEntityId(infrastructureRemoval.removedObjectIds, 100) &&
        std::find(
            infrastructureRemoval.removedHubIds.begin(),
            infrastructureRemoval.removedHubIds.end(),
            "hub-A"
        ) != infrastructureRemoval.removedHubIds.end(),
        "sparse envelope lost object/hub lifecycle removal while they remain full-cadence"
    );

    std::cerr
        << "[PASS] per-session interest + real sparse cadence + hydration/lifecycle semantics\n";
    return 0;
}
