#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>

#include "src/game/client/ClientSystemMapInfrastructureBridge.h"
#include "src/game/client/ClientSystemMapInfrastructureSampler.h"
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

ObjectSnapshot makeStation(
    std::uint32_t idValue,
    int systemId,
    double xMeters,
    const char* name,
    const char* owner
)
{
    ObjectSnapshot object;
    object.id.value = idValue;
    object.type = ObjectType::Station;
    object.systemId = systemId;
    object.setWorldPosition(
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(xMeters, 0.0, 0.0)
        )
    );
    object.displayName = name;
    object.ownerName = owner;
    object.navigationVisible = true;
    object.navigationParentBodyId = "planet";
    object.hubAttachment.valid = true;
    object.hubAttachment.systemId = systemId;
    object.hubAttachment.hubId = "hub-alpha";
    object.hubAttachment.moduleId = "station-module";
    return object;
}

game::simulation::OrbitalHubSnapshot makeHub(
    int systemId,
    double xMeters,
    double centerXMeters
)
{
    game::simulation::OrbitalHubSnapshot hub;
    hub.id = "hub-alpha";
    hub.name = "Alpha Hub";
    hub.owner = "Contract Authority";
    hub.systemId = systemId;
    hub.parentBodyId = "planet";
    hub.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(xMeters, 0.0, 0.0)
        );
    hub.motion.enabled = true;
    hub.motion.centerMeters = {centerXMeters, 0.0, 0.0};
    hub.motion.parentRadiusMeters = 1000.0;
    hub.motion.altitudeMeters = 500.0;
    hub.motion.inclinationDeg = 12.0;
    hub.motion.longitudeOfAscendingNodeDeg = 34.0;
    hub.motion.argumentOfPeriapsisDeg = 56.0;
    return hub;
}

} // namespace

int main()
{
    constexpr int SystemId = 7;

    SimulationSnapshot older;
    older.metadata.serverTimeSeconds = 10.0;
    older.objects.push_back(
        makeStation(42, SystemId, 100.0, "Station Alpha", "Contract Authority")
    );
    older.hubs.push_back(makeHub(SystemId, 1000.0, 10.0));

    SimulationSnapshot newer;
    newer.metadata.serverTimeSeconds = 20.0;
    newer.objects.push_back(
        makeStation(42, SystemId, 300.0, "Station Alpha", "Contract Authority")
    );
    newer.hubs.push_back(makeHub(SystemId, 3000.0, 30.0));

    // Foreign-system infrastructure must never leak into this System map.
    newer.objects.push_back(
        makeStation(99, 8, 9999.0, "Foreign", "Foreign")
    );
    newer.hubs.push_back(makeHub(8, 9999.0, 9999.0));
    newer.hubs.back().id = "foreign-hub";

    std::deque<SimulationSnapshot> history {older, newer};

    const auto sample =
        game::client::sampleSystemMapInfrastructureAtServerTime(
            history,
            SystemId,
            15.0
        );

    require(
        sample.status ==
            game::client::SystemMapInfrastructureSampleStatus::Ready,
        "infrastructure sampler did not resolve an exact-epoch bracket"
    );
    require(sample.objects.size() == 1, "station system filtering failed");
    require(sample.hubs.size() == 1, "hub system filtering failed");

    const double stationX =
        world::coordinates::fullMeters(sample.objects.front().worldPosition).x;
    const double hubX =
        world::coordinates::fullMeters(sample.hubs.front().worldPosition).x;

    require(near(stationX, 200.0), "station transform was not interpolated");
    require(near(hubX, 2000.0), "hub transform was not interpolated");
    require(
        near(sample.hubs.front().motion.centerMeters.x, 20.0),
        "moving orbit center was not sampled at the same epoch"
    );

    world::celestial::SystemMapSnapshot map;
    map.systemId = SystemId;

    world::celestial::SystemMapObject diagnostic;
    diagnostic.stableId = "diagnostic:keep";
    diagnostic.kind = world::celestial::SystemMapObjectKind::Ship;
    map.objects.push_back(diagnostic);

    game::client::rebuildSystemMapInfrastructureLayer(map, sample);

    require(map.objects.size() == 3, "client infrastructure composition count changed");
    require(
        map.objects[0].stableId == "diagnostic:keep",
        "explicit diagnostic map probe was not preserved"
    );

    const auto& station = map.objects[1];
    const auto& hub = map.objects[2];

    require(
        station.kind == world::celestial::SystemMapObjectKind::Station,
        "replicated station facts did not become a Station map object"
    );
    require(station.stableId == "entity:42", "station stable key changed");
    require(station.name == "Station Alpha", "station instance name was lost");
    require(station.owner == "Contract Authority", "station owner was lost");
    require(station.hasOrbit, "hub-attached station lost hub orbit presentation");
    require(
        near(
            station.orbitCenterAu.x,
            20.0 / world::celestial::MetersPerAu
        ),
        "station orbit did not use exact-epoch replicated hub motion"
    );

    require(
        hub.kind == world::celestial::SystemMapObjectKind::Hub,
        "replicated hub facts did not become a Hub map object"
    );
    require(hub.stableId == "hub-alpha", "hub identity changed");
    require(near(hub.positionAu.x, 2000.0 / world::celestial::MetersPerAu),
        "hub map position did not use sampled replication state");

    const auto awaiting =
        game::client::sampleSystemMapInfrastructureAtServerTime(
            history,
            SystemId,
            21.0
        );
    require(
        awaiting.status ==
            game::client::SystemMapInfrastructureSampleStatus::AwaitingNewerSnapshot,
        "future map epoch did not wait for replication history"
    );

    const auto stale =
        game::client::sampleSystemMapInfrastructureAtServerTime(
            history,
            SystemId,
            9.0
        );
    require(
        stale.status ==
            game::client::SystemMapInfrastructureSampleStatus::TooOld,
        "stale map epoch did not request a fresh response"
    );

    std::cout
        << "[PASS] System-map infrastructure/hubs use exact-epoch ordinary replication"
        << '\n';
    return 0;
}
