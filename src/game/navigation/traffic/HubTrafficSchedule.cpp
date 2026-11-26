#include "src/game/navigation/traffic/HubTrafficSchedule.h"
#include "src/game/navigation/HubPlanCatalog.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>

#include <nlohmann/json.hpp>

namespace game::navigation::traffic
{
namespace
{
glm::dvec3 vec3(const nlohmann::json& j, const glm::dvec3& fallback)
{
    return j.is_array() && j.size() == 3 ? glm::dvec3(j[0].get<double>(), j[1].get<double>(), j[2].get<double>()) : fallback;
}

double smooth(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// a basis with the nose (-Z) along noseDir and +Y as close to up as it can be
glm::dmat3 noseBasis(const glm::dvec3& noseDir, const glm::dvec3& up)
{
    const glm::dvec3 z = -glm::normalize(noseDir);
    glm::dvec3 x = glm::cross(up, z);
    if (glm::length(x) < 1.0e-6)
        x = glm::cross(glm::dvec3(0.0, 0.0, 1.0), z);
    x = glm::normalize(x);
    return glm::dmat3(x, glm::cross(z, x), z);
}

glm::dvec3 rotateAbout(const glm::dvec3& v, const glm::dvec3& axis, double angle)
{
    const glm::dvec3 k = glm::normalize(axis);
    return v * std::cos(angle) + glm::cross(k, v) * std::sin(angle) + k * glm::dot(k, v) * (1.0 - std::cos(angle));
}
// a point at arc length s along a polyline, and the index of its segment
glm::dvec3 alongPolyline(const std::vector<glm::dvec3>& p, double s, std::size_t& segment)
{
    for (std::size_t i = 0; i + 1 < p.size(); ++i)
    {
        const double len = glm::length(p[i + 1] - p[i]);
        if (s <= len || i + 2 == p.size())
        {
            segment = i;
            return len > 1.0e-9 ? p[i] + (p[i + 1] - p[i]) * std::clamp(s / len, 0.0, 1.0) : p[i + 1];
        }
        s -= len;
    }
    segment = 0;
    return p.empty() ? glm::dvec3(0.0) : p.back();
}

double polylineLength(const std::vector<glm::dvec3>& p)
{
    double l = 0.0;
    for (std::size_t i = 0; i + 1 < p.size(); ++i)
        l += glm::length(p[i + 1] - p[i]);
    return l;
}
} // namespace

bool HubTrafficSchedule::load(const std::string& path)
{
    *this = HubTrafficSchedule();
    std::ifstream in(path);
    if (!in)
        return false;
    const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || j.value("schema", std::string()) != "elite.hub_schedule.v1")
        return false;
    m_hubId = j.value("hub_id", std::string());
    m_frameModuleId = j.value("frame_module_id", std::string());
    m_cycle = j.value("cycle_s", 86400.0);
    m_approach = j.value("approach_s", 150.0);
    m_depart = j.value("depart_s", 150.0);
    for (const auto& d : j.value("docks", nlohmann::json::array()))
    {
        HubScheduleDock dock;
        dock.id = d.value("id", std::string());
        dock.kind = d.value("kind", std::string());
        dock.position = vec3(d.value("pos", nlohmann::json()), glm::dvec3(0.0));
        dock.axis = glm::normalize(vec3(d.value("axis", nlohmann::json()), glm::dvec3(0.0, 0.0, 1.0)));
        dock.up = glm::normalize(vec3(d.value("up", nlohmann::json()), glm::dvec3(0.0, 1.0, 0.0)));
        dock.heading = glm::normalize(vec3(d.value("heading", nlohmann::json()), dock.axis));
        dock.maxLengthMeters = d.value("max_len", 100.0);
        dock.vertical = d.value("vertical", false);
        m_docks.push_back(std::move(dock));
    }
    for (const auto& v : j.value("visits", nlohmann::json::array()))
    {
        HubScheduleVisit visit;
        visit.id = v.value("id", std::string());
        visit.kind = v.value("kind", std::string());
        visit.model = v.value("model", std::string());
        visit.name = v.value("name", std::string());
        visit.passport = v.value("passport", std::string());
        visit.lengthMeters = v.value("length_m", 30.0);
        visit.cargoTonnes = v.value("cargo_t", 0.0);
        const std::string dockId = v.value("dock", std::string());
        for (std::size_t k = 0; k < m_docks.size(); ++k)
            if (m_docks[k].id == dockId)
                visit.dock = static_cast<int>(k);
        visit.arriveSeconds = v.value("arrive_s", 0.0);
        visit.dwellSeconds = v.value("dwell_s", 0.0);
        if (visit.dock >= 0)
            m_visits.push_back(std::move(visit));
    }
    return !m_frameModuleId.empty();
}

bool HubTrafficSchedule::pose(const HubScheduleVisit& visit, double timeSeconds, HubVisitPose& out) const
{
    if (visit.dock < 0 || visit.dock >= static_cast<int>(m_docks.size()))
        return false;
    const HubScheduleDock& d = m_docks[static_cast<std::size_t>(visit.dock)];
    const double tc = std::fmod(std::fmod(timeSeconds, m_cycle) + m_cycle, m_cycle);
    // this cycle's visit or the previous one still docked / leaving across midnight
    for (const double day : {0.0, -m_cycle, m_cycle})
    {
        const double rel = tc - (visit.arriveSeconds + day);
        if (rel < -m_approach || rel > visit.dwellSeconds + m_depart)
            continue;
        if (!d.vertical)
        {
            // a berth along a rim pier: in along the pier's axis from open space, out the same way after a turn
            const glm::dvec3 entry = d.position + d.axis * ApproachEntryMeters + d.up * 150.0;
            if (rel < 0.0)
            {
                const double f = 1.0 - std::pow(1.0 - (rel + m_approach) / m_approach, 2.0);   // decelerating
                out.position = entry + (d.position - entry) * f;
                out.basis = noseBasis(-d.axis, d.up);
                out.phase = 1;
            }
            else if (rel <= visit.dwellSeconds)
            {
                out.position = d.position;
                out.basis = noseBasis(-d.axis, d.up);
                out.phase = 2;
            }
            else
            {
                const double q = (rel - visit.dwellSeconds) / m_depart;
                const double lift = 80.0 * smooth(q / 0.3);
                const double turn = 3.14159265358979 * smooth(q / 0.3);
                const double run = q < 0.3 ? 0.0 : std::pow((q - 0.3) / 0.7, 2.0) * ApproachEntryMeters;
                out.position = d.position + d.up * lift + d.axis * run;
                out.basis = noseBasis(rotateAbout(-d.axis, d.up, turn), d.up);
                out.phase = 3;
            }
        }
        else if (const auto* plan = HubPlanCatalog::forFrameModule(m_frameModuleId);
                 plan && plan->ring().valid && plan->ring().periodSeconds > 0.0 && plan->ring().hasSyncGate)
        {
            // a pad of a TURNING RING (2026-11-26): the way the player's autopilot flies - in along the spin axis from
            // the open end, through the sync gate on the axis, out over the pad, down; back the same way. The poses are
            // the city's frame, so the visit turns with the ring from the gate on (the gate is on the axis: the turn is
            // a roll there). The old pad path left "away from the centre" in the city's x / z - 3 km through the rim.
            const glm::dvec3 gate = plan->ring().syncGate;
            const glm::dvec3 into(0.0, 0.0, gate.z < 0.0 ? 1.0 : -1.0);
            const double over = std::min(PadEntryMeters, 0.5 * plan->ring().radius);   // over the pad, inside the ring
            const std::vector<glm::dvec3> path {gate - into * ApproachEntryMeters, gate, d.position + d.up * over,
                                                d.position};
            const double length = polylineLength(path);
            const auto basisOn = [&](std::size_t segment, bool arriving) {
                const bool atPad = segment + 2 == path.size();   // the descent / the climb: the parked heading
                if (atPad)
                    return noseBasis(d.heading, d.up);
                glm::dvec3 dir = path[segment + 1] - path[segment];
                if (!arriving)
                    dir = -dir;
                return noseBasis(glm::length(dir) > 1.0e-9 ? dir : into, d.up);
            };
            std::size_t segment = 0;
            if (rel < 0.0)
            {
                const double f = 1.0 - std::pow(1.0 - (rel + m_approach) / m_approach, 2.0);   // decelerating
                out.position = alongPolyline(path, f * length, segment);
                out.basis = basisOn(segment, true);
                out.phase = 1;
            }
            else if (rel <= visit.dwellSeconds)
            {
                out.position = d.position;
                out.basis = noseBasis(d.heading, d.up);
                out.phase = 2;
            }
            else
            {
                const double q = std::clamp((rel - visit.dwellSeconds) / m_depart, 0.0, 1.0);   // accelerating
                out.position = alongPolyline(path, (1.0 - q * q) * length, segment);
                out.basis = basisOn(segment, false);
                out.phase = 3;
            }
        }
        else
        {
            // a pad: down from above, nose along the pad's heading; up and away from the city centre
            glm::dvec3 away(d.position.x, 0.0, d.position.z);
            away = glm::length(away) > 1.0 ? glm::normalize(away) : d.heading;
            if (rel < 0.0)
            {
                const double f = 1.0 - std::pow(1.0 - (rel + m_approach) / m_approach, 2.0);
                out.position = d.position + d.up * (PadEntryMeters * (1.0 - f));
                out.basis = noseBasis(d.heading, d.up);
                out.phase = 1;
            }
            else if (rel <= visit.dwellSeconds)
            {
                out.position = d.position;
                out.basis = noseBasis(d.heading, d.up);
                out.phase = 2;
            }
            else
            {
                const double q = (rel - visit.dwellSeconds) / m_depart;
                const double climb = PadEntryMeters * smooth(q / 0.5);
                const double run = q < 0.5 ? 0.0 : std::pow((q - 0.5) / 0.5, 2.0) * ApproachEntryMeters;
                out.position = d.position + d.up * climb + away * run;
                const double blend = smooth(q / 0.5);
                out.basis = noseBasis(glm::normalize(d.heading * (1.0 - blend) + away * blend + d.up * 1.0e-3), d.up);
                out.phase = 3;
            }
        }
        return true;
    }
    return false;
}

namespace
{
struct CatalogData
{
    std::vector<std::unique_ptr<HubTrafficSchedule>> schedules;
    bool scanned = false;
};

CatalogData& catalog()
{
    // thread-safe first use (a function-local static's initializer): the server thread (hub security, promoted air
    // traffic) and the client main thread (the renderer) may ask first at the same time
    static CatalogData data = [] {
        CatalogData data;
        data.scanned = true;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("assets/data/navigation/hub_schedules", ec))
        {
            if (entry.path().extension() != ".json")
                continue;
            auto s = std::make_unique<HubTrafficSchedule>();
            if (s->load(entry.path().string()))
                data.schedules.push_back(std::move(s));
        }
        return data;
    }();
    return data;
}
} // namespace

const HubTrafficSchedule* HubTrafficScheduleCatalog::forHub(const std::string& hubId)
{
    for (const auto& s : catalog().schedules)
        if (s->hubId() == hubId)
            return s.get();
    return nullptr;
}

const HubTrafficSchedule* HubTrafficScheduleCatalog::forFrameModule(const std::string& moduleId)
{
    for (const auto& s : catalog().schedules)
        if (s->frameModuleId() == moduleId)
            return s.get();
    return nullptr;
}
} // namespace game::navigation::traffic
