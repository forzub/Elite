#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>

#include "src/game/client/ClientDetailMapRuntimeSampler.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/navigation/HubFrameBasis.h"
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

ShipSnapshot makeHubShip(
    double worldX,
    double localX,
    double localVelocityX
)
{
    ShipSnapshot ship;
    ship.id = EntityId{1};
    ship.role = ShipRole::Player;
    ship.typeId = ObjectType::CobraMk1;
    ship.transform.motion.systemId = 7;
    ship.transform.motion.hubId = "hub-alpha";
    ship.transform.motion.mode = game::navigation::MotionMode::HubTactical;
    ship.transform.motion.localPositionMeters = glm::dvec3(localX, 0.0, 0.0);
    ship.transform.motion.localVelocityMps = glm::dvec3(localVelocityX, 0.0, 0.0);
    ship.transform.motion.worldVelocityMps = glm::dvec3(100.0 + localVelocityX, 0.0, 0.0);
    ship.transform.setWorldPositionMeters(glm::dvec3(worldX, 0.0, 0.0));
    return ship;
}

game::simulation::OrbitalHubSnapshot makeHub(
    double worldX,
    double worldVelocityX,
    double angularZ
)
{
    game::simulation::OrbitalHubSnapshot hub;
    hub.id = "hub-alpha";
    hub.name = "Alpha Hub";
    hub.systemId = 7;
    hub.parentBodyId = "planet";
    hub.worldPosition = world::coordinates::makeWorldPositionFromMeters(
        glm::dvec3(worldX, 0.0, 0.0)
    );
    hub.worldVelocityMps = glm::dvec3(worldVelocityX, 0.0, 0.0);
    hub.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, angularZ);
    hub.primeModuleId = "prime";
    hub.orientation = game::navigation::hubVisualOrientation(
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0)
    );
    return hub;
}

SimulationSnapshot makeSnapshot(
    double serverTime,
    double scale
)
{
    SimulationSnapshot snapshot;
    snapshot.metadata.serverTimeSeconds = serverTime;
    snapshot.ships.push_back(makeHubShip(
        1000.0 * scale + 10.0 * scale,
        10.0 * scale,
        2.0 * scale
    ));
    snapshot.hubs.push_back(makeHub(
        1000.0 * scale,
        100.0 * scale,
        0.01 * scale
    ));
    return snapshot;
}

} // namespace

int main()
{
    std::deque<SimulationSnapshot> history;
    history.push_back(makeSnapshot(10.0, 1.0));
    history.push_back(makeSnapshot(20.0, 3.0));

    const auto sample = game::client::sampleDetailMapRuntimeAtServerTime(
        history,
        7,
        15.0
    );

    require(
        sample.status == game::client::DetailMapRuntimeSampleStatus::Ready,
        "Hub Map runtime sampler did not resolve the response epoch"
    );
    require(sample.ships.size() == 1, "Hub Map ship set changed during sampling");
    require(sample.hubs.size() == 1, "Hub Map hub set changed during sampling");

    require(
        near(sample.ships.front().localPositionMeters.x, 20.0),
        "authoritative HubTactical local position was not interpolated"
    );
    require(
        near(sample.ships.front().localVelocityMps.x, 4.0),
        "authoritative HubTactical local velocity was not interpolated"
    );
    require(
        sample.ships.front().motionMode == game::navigation::MotionMode::HubTactical,
        "HubTactical mode was lost from exact-epoch sampling"
    );
    require(
        near(sample.hubs.front().angularVelocityWorldRadPerSecond.z, 0.02),
        "hub rotating-frame angular velocity was not interpolated"
    );
    require(
        sample.hubs.front().primeModuleId == "prime",
        "hub prime-module identity was lost from ordinary replication"
    );

    const auto future = game::client::sampleDetailMapRuntimeAtServerTime(
        history,
        7,
        21.0
    );
    require(
        future.status == game::client::DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot,
        "future Hub Map epoch did not wait for replication history"
    );

    const auto stale = game::client::sampleDetailMapRuntimeAtServerTime(
        history,
        7,
        9.0
    );
    require(
        stale.status == game::client::DetailMapRuntimeSampleStatus::TooOld,
        "stale Hub Map epoch was silently clamped"
    );

    std::cout
        << "[PASS] Hub Map runtime facts use exact-epoch ordinary replication"
        << '\n';
    return 0;
}
