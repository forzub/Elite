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
    costs LaneCostFactor of a free metre. A ship ENTERS a lane at ANY of its stations (merging along the lane's course
    after the mandatory join straight) and LEAVES it at any station it rode to (author 2026-11-22: diving straight into
    a green zone, not through its entry point - joining only at a lane's first station cost 8-11 km loops); a free
    transfer (<= MaxTransfer) joins any station of another lane. The free legs are planned against the obstacles. Each continuous piece of the chain (lanes + connectors) becomes ONE VolumeTransit stage (PreferInside)
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
    // the free legs' cost (live 2026-11-17: a lane start 1.2 km from a ship at rest, behind its 1 km start lead ->
    // the inbound leg infeasible on every retry): every radian a free leg turns costs TurnPenaltyMeters, and a free leg
    // shorter than MinFreeLegMeters + the turn radius x its total turn is not offered
    static constexpr double TurnPenaltyMeters = 400.0;
    static constexpr double MinFreeLegMeters = 600.0;

    // What the planner will demand of the free legs, taken from the SAME policy the composer applies
    // (freeLegModelFor in HubLaneRouting <- OrdinaryRoutePolicy): live runs 6..14 failed whenever the search assumed less.
    struct FreeLegModel
    {
        double joinApproachMeters = 700.0;     // the hard terminal approach before a lane's join straight
        double turnRadiusMeters = 1500.0;      // the free legs' turn radius at the route speed
        double exitMinimumMeters = 0.0;        // the shortest final leg (its start straight + turn budget + goal axis)
    };

    bool load(const std::string& path);
    bool loaded() const noexcept { return !m_lanes.empty(); }
    const std::string& failure() const noexcept { return m_failure; }
    const std::string& hubId() const noexcept { return m_hubId; }
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
        const FreeLegModel& freeLegs,
        std::string* summary = nullptr,
        // the buildings + the red zones (the planning frame): a free leg into / out of a lane that crosses one is not
        // offered (author 2026-11-22: «при манёврах входа в зелёную зону не попадать в красную зону. и в здания»)
        const std::vector<world::navigation::NavigationObstacle>* obstacles = nullptr,
        double agentRadiusMeters = 0.0
    ) const;

    // the red zones as planner obstacles in the planning frame
    std::vector<world::navigation::NavigationObstacle> redZoneObstacles(
        const NavigationReferenceFramePose& frame
    ) const;

private:
    std::string m_failure;
    std::string m_hubId;
    std::string m_frameModuleId;
    std::vector<Lane> m_lanes;
    std::vector<Connector> m_connectors;
    std::vector<RedZone> m_redZones;
    double m_speedLimitMps = 0.0;
    double m_speedLimitZoneMeters = 0.0;
};

/*
    The lane networks of all hubs: assets/data/navigation/hub_lanes/<hub>.json, loaded once on first use (the docking
    planner, the flight HUD and the Hub map share them - audit 2026-11-18: each kept its own copy and resolved the city
    frame its own way). Lookup by hub id or by the network's frame module id (the module whose pose is the lanes'
    frame). The first call must not race another: the callers are the client main thread.
*/
class HubLaneCatalog final
{
public:
    static const HubLaneNetwork* forHub(const std::string& hubId);
    static const HubLaneNetwork* forFrameModule(const std::string& moduleId);
};

} // namespace game::navigation::traffic
