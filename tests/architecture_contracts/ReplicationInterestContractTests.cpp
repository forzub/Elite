#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "src/game/network/ReplicationSnapshotMerge.h"
#include "src/game/server/ReplicationInterestPolicy.h"
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
    baseline.replication.entitySetMode =
        ReplicatedEntitySetMode::FullAuthoritativeSet;
    baseline.ships.push_back(makeShip(1, 0, 0.0));
    baseline.ships.push_back(makeShip(2, 0, 100.0));

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
        retained.replication.entitySetMode ==
            ReplicatedEntitySetMode::FullAuthoritativeSet,
        "canonical history sample must materialize as a full entity set"
    );

    // Explicit lifecycle remove is the only sparse deletion mechanism.
    SimulationSnapshot remove;
    remove.metadata.serverTick = 12;
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

    std::cerr
        << "[PASS] per-session replication interest + retain/update/remove semantics\n";
    return 0;
}
