#pragma once

// The daily ship schedule of a hub city (author 2026-11-18: «прописать товарооборот и сделать тупо расписание, которое
// будет лежать и на сервере и на клиенте ... все доки сделать доками - но на клиенте ... что серверу надо знать -
// игрок нападает на корабль. тот становится настоящим и сервер начинает его симуляцию»).
// assets/data/navigation/hub_schedules/<hub>.json (elite.hub_schedule.v1, tools/models/hub_schedule.py): the client-side
// docks (rim pier berths, fuel berths, frontier pads) and the visits of a 24 h cycle (hull, dock, arrival, dwell). A
// visit's pose is a pure function of the universe time - the client draws it, the server computes the same pose when a
// player attacks it (then the ship becomes a real one).
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::traffic
{
struct HubScheduleDock
{
    std::string id;
    std::string kind;            // cargo | fuel | pad
    glm::dvec3 position {0.0};   // the hull's centre docked (city frame)
    glm::dvec3 axis {0.0, 0.0, 1.0};      // the approach axis: out of the city (berths) / up (pads)
    glm::dvec3 up {0.0, 1.0, 0.0};
    glm::dvec3 heading {0.0, 0.0, 1.0};   // pads: the parked hull's nose direction
    double maxLengthMeters = 100.0;
    bool vertical = false;       // a pad: the ship comes down from above
};

struct HubScheduleVisit
{
    std::string id;
    std::string kind;            // bulk | fuel | frontier
    std::string model;           // "ships/ocs-4-lastochka" (under assets/compiled)
    std::string name;
    double lengthMeters = 30.0;
    double cargoTonnes = 0.0;
    int dock = -1;
    double arriveSeconds = 0.0;  // second of the cycle the hull is at its dock
    double dwellSeconds = 0.0;
};

// a visit at a moment: where the hull is (city frame; its basis columns X, Y, Z with the nose to -Z) and what it does
struct HubVisitPose
{
    glm::dvec3 position {0.0};
    glm::dmat3 basis {1.0};
    int phase = 0;               // 1 approach, 2 docked, 3 departure
};

class HubTrafficSchedule
{
public:
    bool load(const std::string& path);
    const std::string& hubId() const { return m_hubId; }
    const std::string& frameModuleId() const { return m_frameModuleId; }
    double cycleSeconds() const { return m_cycle; }
    const std::vector<HubScheduleDock>& docks() const { return m_docks; }
    const std::vector<HubScheduleVisit>& visits() const { return m_visits; }
    // the visit's pose at timeSeconds (the universe time; the cycle repeats), false: not in the city's sky then
    bool pose(const HubScheduleVisit& visit, double timeSeconds, HubVisitPose& out) const;

    static constexpr double ApproachEntryMeters = 3000.0;   // a berth: the approach starts this far out along its axis
    static constexpr double PadEntryMeters = 900.0;         // a pad: the descent starts this high over it

private:
    std::string m_hubId;
    std::string m_frameModuleId;
    double m_cycle = 86400.0;
    double m_approach = 150.0;
    double m_depart = 150.0;
    std::vector<HubScheduleDock> m_docks;
    std::vector<HubScheduleVisit> m_visits;
};

class HubTrafficScheduleCatalog final
{
public:
    static const HubTrafficSchedule* forHub(const std::string& hubId);
    static const HubTrafficSchedule* forFrameModule(const std::string& moduleId);
};
} // namespace game::navigation::traffic
