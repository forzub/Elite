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
    новую зону или уже в док»).

    Data: assets/data/navigation/hub_lanes/<hub id>.json (schema elite.hub_lanes.v1, written by
    tools/models/hub_belt_world.py): one-way GREEN lanes as station polylines in the frame of the hub module
    frame_module_id (the city assembly), a radius and a speed limit; RED zones (cargo berths) as oriented boxes.

    chooseRoute() picks the cheapest chain ship -> [lane span] -> [lane span] ... -> goal by a Dijkstra search: a free
    leg costs its straight distance, a metre inside a lane costs LaneCostFactor of it, a ship may join a lane at any
    station and leave it at any later station, and may change to another lane whose join point is near. The chain
    becomes a CompiledTrafficRoute: every lane span a VolumeTransit stage (GREEN = PreferInside) with its sections /
    centreline, a mandatory tangent straight before the join and after the leave - the same product the authored
    traffic graph compiles to, so SegmentedRouteComposer flies it unchanged (lane interiors pre-written, only the free
    legs planned). No lane chain beats the direct leg -> invalid route (the caller plans directly).
*/
class HubLaneNetwork final
{
public:
    struct Lane
    {
        std::string id;
        double radiusMeters = 45.0;
        double maxSpeedMps = 150.0;
        std::vector<glm::dvec3> stationsLocal;
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
    static constexpr double MaxTransferMeters = 1600.0;     // lane -> lane free transfer limit

    bool load(const std::string& path);
    bool loaded() const noexcept { return !m_lanes.empty(); }
    const std::string& failure() const noexcept { return m_failure; }
    const std::string& frameModuleId() const noexcept { return m_frameModuleId; }
    const std::vector<Lane>& lanes() const noexcept { return m_lanes; }
    const std::vector<RedZone>& redZones() const noexcept { return m_redZones; }

    // shipWorld / goalWorld and the result in the planning frame; frame = the city module's pose in that frame.
    // summary: a one-line description of the chosen chain (diagnostics).
    CompiledTrafficRoute chooseRoute(
        const NavigationReferenceFramePose& frame,
        const glm::dvec3& shipWorld,
        const glm::dvec3& goalWorld,
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
    std::vector<RedZone> m_redZones;
};

} // namespace game::navigation::traffic
