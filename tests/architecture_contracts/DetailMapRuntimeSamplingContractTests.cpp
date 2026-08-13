#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>

#include "src/game/client/ClientDetailMapRuntimeSampler.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(1);
    }
}

bool near(double a, double b, double epsilon = 1.0e-9)
{
    return std::abs(a - b) <= epsilon;
}

ShipSnapshot makeShip(
    std::uint32_t idValue,
    int systemId,
    double xMeters,
    double velocityXMps
)
{
    ShipSnapshot ship;
    ship.id = EntityId{idValue};
    ship.role = ShipRole::NPC;
    ship.typeId = ObjectType::CobraMk1;
    ship.transform.motion.systemId = systemId;
    ship.transform.motion.parentBodyId = "planet";
    ship.transform.setWorldPositionMeters(glm::dvec3(xMeters, 0.0, 0.0));
    ship.transform.motion.worldVelocityMps = glm::dvec3(velocityXMps, 0.0, 0.0);
    return ship;
}

ObjectSnapshot makeObject(
    std::uint32_t idValue,
    int systemId,
    double xMeters,
    double velocityXMps,
    bool navigationVisible
)
{
    ObjectSnapshot object;
    object.id = EntityId{idValue};
    object.type = ObjectType::Station;
    object.systemId = systemId;
    object.setWorldPosition(
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(xMeters, 0.0, 0.0)
        )
    );
    object.linearVelocityMps = glm::dvec3(velocityXMps, 0.0, 0.0);
    object.displayName = "Detail Module";
    object.navigationVisible = navigationVisible;
    object.navigationParentBodyId = "planet";
    object.hubAttachment.valid = true;
    object.hubAttachment.systemId = systemId;
    object.hubAttachment.hubId = "hub-alpha";
    object.hubAttachment.moduleId = "module-alpha";
    return object;
}

game::simulation::OrbitalHubSnapshot makeHub(
    int systemId,
    double xMeters,
    double velocityXMps
)
{
    game::simulation::OrbitalHubSnapshot hub;
    hub.id = "hub-alpha";
    hub.name = "Alpha Hub";
    hub.systemId = systemId;
    hub.parentBodyId = "planet";
    hub.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(xMeters, 0.0, 0.0)
        );
    hub.worldVelocityMps = glm::dvec3(velocityXMps, 0.0, 0.0);
    return hub;
}

SimulationSnapshot makeSnapshot(
    double serverTime,
    int systemId,
    double positionScale
)
{
    SimulationSnapshot snapshot;
    snapshot.metadata.serverTimeSeconds = serverTime;
    snapshot.ships.push_back(makeShip(
        1,
        systemId,
        10.0 * positionScale,
        1.0 * positionScale
    ));
    // navigationVisible=false is deliberate: Details must still retain hub
    // child modules that are not top-level System-map markers.
    snapshot.objects.push_back(makeObject(
        42,
        systemId,
        100.0 * positionScale,
        10.0 * positionScale,
        false
    ));
    snapshot.hubs.push_back(makeHub(
        systemId,
        1000.0 * positionScale,
        100.0 * positionScale
    ));
    return snapshot;
}

} // namespace

int main()
{
    constexpr int SystemId = 7;

    std::deque<SimulationSnapshot> history;
    history.push_back(makeSnapshot(10.0, SystemId, 1.0));
    history.push_back(makeSnapshot(20.0, SystemId, 3.0));

    const auto sample =
        game::client::sampleDetailMapRuntimeAtServerTime(
            history,
            SystemId,
            15.0
        );

    require(
        sample.status == game::client::DetailMapRuntimeSampleStatus::Ready,
        "Details runtime sampler did not resolve exact-epoch history"
    );
    require(sample.ships.size() == 1, "Details ship sampling changed entity set");
    require(sample.objects.size() == 1, "non-map-visible Detail module was filtered out");
    require(sample.hubs.size() == 1, "Details hub sampling changed entity set");

    require(
        near(
            world::coordinates::fullMeters(sample.ships.front().worldPosition).x,
            20.0
        ),
        "Details ship position was not interpolated at response epoch"
    );
    require(
        near(sample.ships.front().worldVelocityMps.x, 2.0),
        "Details ship velocity was not interpolated at response epoch"
    );
    require(
        near(
            world::coordinates::fullMeters(sample.objects.front().worldPosition).x,
            200.0
        ),
        "Details static-object position was not interpolated"
    );
    require(
        near(sample.objects.front().linearVelocityMps.x, 20.0),
        "Details static-object velocity was not interpolated"
    );
    require(
        near(
            world::coordinates::fullMeters(sample.hubs.front().worldPosition).x,
            2000.0
        ),
        "Details hub position was not interpolated"
    );
    require(
        near(sample.hubs.front().worldVelocityMps.x, 200.0),
        "Details hub velocity was not interpolated"
    );

    const auto future =
        game::client::sampleDetailMapRuntimeAtServerTime(
            history,
            SystemId,
            21.0
        );
    require(
        future.status ==
            game::client::DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot,
        "future Details epoch did not wait for newer replication history"
    );

    const auto stale =
        game::client::sampleDetailMapRuntimeAtServerTime(
            history,
            SystemId,
            9.0
        );
    require(
        stale.status == game::client::DetailMapRuntimeSampleStatus::TooOld,
        "stale Details epoch was silently clamped"
    );

    std::deque<SimulationSnapshot> transferHistory;
    transferHistory.push_back(makeSnapshot(30.0, SystemId, 1.0));
    transferHistory.push_back(makeSnapshot(40.0, SystemId + 1, 2.0));

    const auto crossing =
        game::client::sampleDetailMapRuntimeAtServerTime(
            transferHistory,
            SystemId,
            35.0
        );
    require(
        crossing.status == game::client::DetailMapRuntimeSampleStatus::Ready,
        "cross-system Details bracket failed as a whole"
    );
    require(
        crossing.ships.empty() && crossing.objects.empty() && crossing.hubs.empty(),
        "Details sampler interpolated across system-local coordinate domains"
    );

    std::cout
        << "[PASS] Details runtime facts use exact-epoch ordinary replication"
        << '\n';
    return 0;
}
