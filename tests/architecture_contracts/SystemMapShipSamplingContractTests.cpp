#include <cmath>
#include <deque>
#include <iostream>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

class ClientWorldState;

#include "src/game/client/ClientMapService.h"
#include "src/game/client/ClientSystemMapShipSampler.h"


static_assert(
    std::is_constructible_v<
        game::client::ClientMapService,
        ITransport&,
        const game::client::ClientCatalogService&,
        const ::ClientWorldState&
    >,
    "ClientMapService must consume the canonical client world-state type; "
    "a nested game::client::ClientWorldState would split the replication/map seam"
);

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipSnapshot makeShip(
    std::uint32_t id,
    ShipRole role,
    int systemId,
    double xMeters
)
{
    ShipSnapshot ship;
    ship.id = EntityId{id};
    ship.role = role;
    ship.typeId = ObjectType::CobraMk1;
    ship.transform.motion.systemId = systemId;
    ship.transform.motion.parentBodyId = "parent";
    ship.transform.motion.hubId = "hub:test";
    ship.transform.setWorldPositionMeters(glm::dvec3(xMeters, 0.0, 0.0));
    return ship;
}

SimulationSnapshot makeSnapshot(
    std::uint64_t tick,
    double serverTime,
    std::initializer_list<ShipSnapshot> ships
)
{
    SimulationSnapshot snapshot;
    snapshot.metadata.serverTick = tick;
    snapshot.metadata.serverTimeSeconds = serverTime;
    snapshot.metadata.universeTimelineRevision = 7;
    snapshot.ships.assign(ships.begin(), ships.end());
    return snapshot;
}
}

int main()
{
    try
    {
        std::deque<SimulationSnapshot> history;
        history.push_back(makeSnapshot(
            3,
            1.00,
            {makeShip(1, ShipRole::Player, 0, 100.0),
             makeShip(2, ShipRole::NPC, 0, 300.0)}
        ));
        history.push_back(makeSnapshot(
            6,
            1.06,
            {makeShip(1, ShipRole::Player, 0, 160.0),
             makeShip(2, ShipRole::NPC, 0, 360.0)}
        ));

        const auto middle =
            game::client::sampleSystemMapShipsAtServerTime(
                history,
                0,
                1.02
            );

        require(
            middle.status == game::client::SystemMapShipSampleStatus::Ready,
            "in-range System-map ship sample was not ready"
        );
        require(middle.ships.size() == 2, "ship set changed during interpolation");
        require(
            middle.ships.front().hubId == "hub:test",
            "System-map ship sample lost replicated Hub membership"
        );

        const double playerX =
            world::coordinates::fullMeters(middle.ships[0].worldPosition).x;
        require(
            std::abs(playerX - 120.0) < 1.0e-6,
            "ship position was not sampled at the requested server-time epoch"
        );

        const auto future =
            game::client::sampleSystemMapShipsAtServerTime(
                history,
                0,
                1.08
            );
        require(
            future.status ==
                game::client::SystemMapShipSampleStatus::AwaitingNewerSnapshot,
            "future map epoch did not wait for newer replication history"
        );

        const auto stale =
            game::client::sampleSystemMapShipsAtServerTime(
                history,
                0,
                0.90
            );
        require(
            stale.status == game::client::SystemMapShipSampleStatus::TooOld,
            "stale map epoch was silently clamped to unrelated history"
        );

        std::deque<SimulationSnapshot> transferHistory;
        transferHistory.push_back(makeSnapshot(
            9,
            2.00,
            {makeShip(7, ShipRole::NPC, 0, 10.0)}
        ));
        transferHistory.push_back(makeSnapshot(
            12,
            2.06,
            {makeShip(7, ShipRole::NPC, 1, 20.0)}
        ));

        const auto betweenSystems =
            game::client::sampleSystemMapShipsAtServerTime(
                transferHistory,
                0,
                2.03
            );
        require(
            betweenSystems.status ==
                game::client::SystemMapShipSampleStatus::Ready,
            "cross-system sample unexpectedly failed as a whole"
        );
        require(
            betweenSystems.ships.empty(),
            "System-map ship sampler interpolated across system-local domains"
        );

        const auto exactNewSystem =
            game::client::sampleSystemMapShipsAtServerTime(
                transferHistory,
                1,
                2.06
            );
        require(
            exactNewSystem.status ==
                game::client::SystemMapShipSampleStatus::Ready &&
            exactNewSystem.ships.size() == 1 &&
            exactNewSystem.ships.front().systemId == 1,
            "exact post-transfer endpoint was not preserved"
        );

        std::cout << "[PASS] System-map ships sample ordinary replication at one server epoch\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return 1;
    }
}
