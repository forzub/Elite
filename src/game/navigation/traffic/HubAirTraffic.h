#pragma once

// The hub city's AIR traffic slots, shared by the client presentation (src/scene/HubCityTraffic) and the server's
// promotion of near slots to real ships (GameSimulationHubSecurity.cpp; author 2026-11-19: «любой корабль попавший в
// близкую зону видимости игрока становится настоящим»). A slot is one ship of one lane: it flies the lane's stations at
// 70 % of the lane's speed limit and wraps from the lane's end (open sky beyond the rim) back to its start. Its pose is
// a PURE function of the universe time, so the server spawns a promoted ship exactly where every client draws it.
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{
class HubPlan;
}

namespace game::navigation::traffic
{
class HubLaneNetwork;

struct HubAirSlot
{
    std::uint32_t id = 0;          // stable: (lane index << 8) | ship index
    int lane = -1;
    int fleetIndex = 0;            // the plan's air_fleet entry (its hull)
    double phaseMeters = 0.0;      // the slot's place on the lane at t = 0
    double speedMps = 0.0;
};

struct HubAirLaneTrack
{
    int lane = -1;
    std::vector<double> cumulative;   // metres along the stations (the plaza ring: one lap)
    double lengthMeters = 0.0;
    double upSign = 1.0;              // lanes under the plane fly upside down
};

struct HubAirSlotPose
{
    glm::dvec3 position {0.0};   // city frame
    glm::dvec3 forward {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    glm::dvec3 velocity {0.0};
    double alongMeters = 0.0;
    std::int64_t lap = 0;         // how many times the slot has wrapped (a promoted ship leaves at its lap's end)
    glm::mat4 local {1.0f};       // the presentation matrix (nose -Z), as drawn
};

// The slots of one hub (built once per plan + lane network and cached by the caller).
class HubAirTrafficLayout
{
public:
    void build(const game::navigation::HubPlan& plan, const HubLaneNetwork& lanes);
    bool built() const noexcept { return m_built; }
    const std::vector<HubAirSlot>& slots() const noexcept { return m_slots; }
    const HubAirLaneTrack* track(int lane) const noexcept;
    const HubAirSlot* slot(std::uint32_t id) const noexcept;
    bool pose(const HubAirSlot& slot, double timeSeconds, HubAirSlotPose& out) const;
    // the lane's pose at an arbitrary distance along it (no wrap)
    bool poseAt(int lane, double alongMeters, double speedMps, HubAirSlotPose& out) const;

private:
    bool m_built = false;
    const HubLaneNetwork* m_lanes = nullptr;
    std::vector<HubAirSlot> m_slots;
    std::vector<HubAirLaneTrack> m_tracks;   // index = lane index (empty track = no traffic)
};

// the cached layout of a plan + lane network (the client main thread and the server thread each keep their own)
const HubAirTrafficLayout& hubAirTrafficLayoutCached(const game::navigation::HubPlan& plan, const HubLaneNetwork& lanes);
} // namespace game::navigation::traffic
