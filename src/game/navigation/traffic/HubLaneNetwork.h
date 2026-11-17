#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/traffic/RouteStageCompiler.h"
#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation::traffic
{

/*
    Hub-scale traffic lanes of a generated settlement (author 2026-11-17: «зеленые тунели - зоны через весь город -
    карта быстрых маршрутов сформированная заранее ... в зеленой и синей зонах маршрут должен быть уже прописан
    заранее. тогда кораблю останется только построить маршрут к зеленой или синей зоне, а после выхода из нее или в
    новую зону или уже в док»; «можно какую-то часть вести в центр - там сделать типа площади и из центра уже
    разлетаться по нужным направлениям»).

    Data: assets/data/navigation/hub_lanes/<hub id>.json (schema elite.hub_lanes.v1, written by
    tools/models/hub_belt_world.py from tools/models/units/hub_plane.py), in the frame of the hub module
    frame_module_id (the city assembly):
      - one-way GREEN lanes: station polylines, a radius, a speed limit, the largest ship allowed;
      - CONNECTORS: pre-written polylines from a station of one lane to a station of another - the turns at the
        crossings, the merges into / diverges out of the plaza ring under the canopy;
      - RED zones (cargo berths) as oriented boxes;
      - the hub speed limit and the radius of its zone.

    chooseRoute() picks the cheapest chain by a Dijkstra search over (lane, station): a metre of lane or connector
    costs LaneCostFactor of a free metre. A ship ENTERS the network only at a lane's first station and LEAVES it only
    at a lane's last station - the lanes' ends lie beyond the rim, in open sky; inside the city there is no free leg,
    every turn is pre-written. Between a lane end and another lane's start a free transfer (open sky, <= MaxTransfer)
    is allowed. Each continuous piece of the chain (lanes + connectors) becomes ONE VolumeTransit stage (PreferInside)
    whose centreline is the whole pre-written polyline, with a mandatory tangent straight before its entry and after
    its exit - the same product the authored traffic graph compiles to, so SegmentedRouteComposer flies it unchanged.
    A lane / connector whose max ship length is below the ship's is not used. No chain beats the direct leg -> invalid
    route (the caller plans directly).
*/
class HubLaneNetwork final
{
public:
    struct Lane
    {
        std::string id;
        double radiusMeters = 45.0;
        double maxSpeedMps = 150.0;
        double maxShipLengthMeters = 1.0e9;
        bool blue = false;                       // kind "blue": a pre-written approach onto a dock's axis
        std::vector<glm::dvec3> stationsLocal;
    };
    struct Connector
    {
        std::string id;
        std::size_t fromLane = 0;
        std::size_t fromStation = 0;
        std::size_t toLane = 0;
        std::size_t toStation = 0;
        double radiusMeters = 45.0;
        double maxSpeedMps = 100.0;
        double maxShipLengthMeters = 1.0e9;
        std::vector<glm::dvec3> pointsLocal;   // first / last = the two stations exactly
        double lengthMeters = 0.0;
    };
    struct RedZone
    {
        std::string id;
        glm::dvec3 centerLocal {0.0};
        glm::dvec3 halfExtentsMeters {1.0};
        glm::dvec3 rotationDeg {0.0};
    };

    static constexpr double LaneCostFactor = 0.35;          // a lane metre vs a free metre
    static constexpr double JoinStraightMeters = 300.0;     // mandatory straight before joining a lane
    static constexpr double LeaveStraightMeters = 200.0;    // ... and after leaving it
    static constexpr double MaxTransferMeters = 1600.0;     // lane end -> lane start free transfer limit (open sky)
    // the free legs' geometry in the cost (live 2026-11-17: a lane start 1.2 km from a ship at rest, behind its 1 km
    // start lead -> the inbound leg infeasible on every retry): every radian a free leg turns costs TurnPenaltyMeters,
    // and a free leg shorter than MinFreeLegMeters (after the ship's start lead) is not offered
    // A free leg must also have room for its turns: at least MinFreeLegMeters + FreeTurnRadiusMeters x its total turn
    // (radians; ~100 m/s at the Assisted geometry lateral ~17 m/s^2 -> r ~600 m; live run 2: 940 m with a 70 deg turn
    // at the join -> infeasible again).
    // Run 6 (2026-11-17, the inbound leg's own numbers): before the join straight the composer also demands a hard
    // terminal approach (700 m) - so the leg really ends JoinApproachMeters further out, and an S-curve of 27 + 32 deg
    // with 0.8 km of side offset did not fit 1.66 km -> the radius taken is a conservative 1.5 km (1 km still passed it).
    static constexpr double TurnPenaltyMeters = 400.0;
    static constexpr double MinFreeLegMeters = 600.0;
    static constexpr double FreeTurnRadiusMeters = 1500.0;
    static constexpr double JoinApproachMeters = 700.0;

    bool load(const std::string& path);
    bool loaded() const noexcept { return !m_lanes.empty(); }
    const std::string& failure() const noexcept { return m_failure; }
    const std::string& frameModuleId() const noexcept { return m_frameModuleId; }
    const std::vector<Lane>& lanes() const noexcept { return m_lanes; }
    const std::vector<Connector>& connectors() const noexcept { return m_connectors; }
    const std::vector<RedZone>& redZones() const noexcept { return m_redZones; }
    // the hub speed limit (0 = none) within speedLimitZoneMeters of the frame origin
    double speedLimitMps() const noexcept { return m_speedLimitMps; }
    double speedLimitZoneMeters() const noexcept { return m_speedLimitZoneMeters; }

    // shipWorld / goalWorld and the result in the planning frame; frame = the city module's pose in that frame.
    // summary: a one-line description of the chosen chain (diagnostics).
    CompiledTrafficRoute chooseRoute(
        const NavigationReferenceFramePose& frame,
        const glm::dvec3& shipWorld,
        const glm::dvec3& goalWorld,
        double shipLengthMeters,
        const glm::dvec3& initialForward,          // zero = no committed heading
        double startLeadMeters,                     // the straight the ship flies before it may turn
        const glm::dvec3& goalArriveForward,       // the direction the ship must reach the goal in (zero = free)
        std::string* summary = nullptr
    ) const;

    // the red zones as planner obstacles in the planning frame
    std::vector<world::navigation::NavigationObstacle> redZoneObstacles(
        const NavigationReferenceFramePose& frame
    ) const;

private:
    std::string m_failure;
    std::string m_frameModuleId;
    std::vector<Lane> m_lanes;
    std::vector<Connector> m_connectors;
    std::vector<RedZone> m_redZones;
    double m_speedLimitMps = 0.0;
    double m_speedLimitZoneMeters = 0.0;
};

} // namespace game::navigation::traffic
