#include "src/game/navigation/traffic/HubLaneNetwork.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <queue>
#include <sstream>

#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

namespace game::navigation::traffic
{

namespace
{

glm::dvec3 vec3Of(const nlohmann::json& a)
{
    return {a.at(0).get<double>(), a.at(1).get<double>(), a.at(2).get<double>()};
}

void appendUnique(std::vector<glm::dvec3>& points, const glm::dvec3& p)
{
    if (points.empty() || glm::length(points.back() - p) > 1.0e-6)
        points.push_back(p);
}

} // namespace

bool HubLaneNetwork::load(const std::string& path)
{
    m_lanes.clear();
    m_redZones.clear();
    m_failure.clear();
    std::ifstream in(path);
    if (!in)
    {
        m_failure = "no lane file " + path;
        return false;
    }
    try
    {
        nlohmann::json doc;
        in >> doc;
        if (doc.value("schema", std::string()) != "elite.hub_lanes.v1")
        {
            m_failure = "unknown lane schema in " + path;
            return false;
        }
        m_frameModuleId = doc.at("frame_module_id").get<std::string>();
        for (const auto& item : doc.at("lanes"))
        {
            Lane lane;
            lane.id = item.at("id").get<std::string>();
            lane.radiusMeters = item.value("radius_m", 45.0);
            lane.maxSpeedMps = item.value("max_speed_mps", 150.0);
            for (const auto& p : item.at("stations_local_m"))
                lane.stationsLocal.push_back(vec3Of(p));
            if (lane.stationsLocal.size() >= 2)
                m_lanes.push_back(std::move(lane));
        }
        for (const auto& item : doc.value("red_zones", nlohmann::json::array()))
        {
            RedZone zone;
            zone.id = item.at("id").get<std::string>();
            zone.centerLocal = vec3Of(item.at("center_local_m"));
            zone.halfExtentsMeters = vec3Of(item.at("half_extents_m"));
            zone.rotationDeg = vec3Of(item.at("rotation_deg"));
            m_redZones.push_back(std::move(zone));
        }
    }
    catch (const std::exception& e)
    {
        m_failure = std::string("lane file parse error: ") + e.what();
        m_lanes.clear();
        return false;
    }
    if (m_lanes.empty())
        m_failure = "lane file has no lanes: " + path;
    return !m_lanes.empty();
}

CompiledTrafficRoute HubLaneNetwork::chooseRoute(
    const NavigationReferenceFramePose& frame,
    const glm::dvec3& shipWorld,
    const glm::dvec3& goalWorld,
    std::string* summary
) const
{
    CompiledTrafficRoute out;
    // world stations and per-station travel directions
    struct LaneW
    {
        std::vector<glm::dvec3> p;
        std::vector<glm::dvec3> fwd;    // fwd[k]: direction of the segment k -> k+1 (last: the previous one)
    };
    std::vector<LaneW> lw(m_lanes.size());
    for (std::size_t l = 0; l < m_lanes.size(); ++l)
    {
        for (const auto& s : m_lanes[l].stationsLocal)
            lw[l].p.push_back(frame.pointToWorld(s));
        const auto n = lw[l].p.size();
        lw[l].fwd.resize(n);
        for (std::size_t k = 0; k + 1 < n; ++k)
            lw[l].fwd[k] = glm::normalize(lw[l].p[k + 1] - lw[l].p[k]);
        lw[l].fwd[n - 1] = lw[l].fwd[n - 2];
    }
    const auto joinStart = [&](std::size_t l, std::size_t k) { return lw[l].p[k] - lw[l].fwd[k] * JoinStraightMeters; };
    const auto leaveDir = [&](std::size_t l, std::size_t k) { return lw[l].fwd[k == 0 ? 0 : k - 1]; };
    const auto leaveEnd = [&](std::size_t l, std::size_t k) { return lw[l].p[k] + leaveDir(l, k) * LeaveStraightMeters; };

    // states: (lane, station, ridden) + the goal
    std::vector<std::size_t> base(m_lanes.size() + 1, 0);
    for (std::size_t l = 0; l < m_lanes.size(); ++l)
        base[l + 1] = base[l] + 2 * lw[l].p.size();
    const std::size_t goal = base.back();
    const std::size_t count = goal + 1;
    const auto state = [&](std::size_t l, std::size_t k, int ridden) { return base[l] + 2 * k + ridden; };
    constexpr double Inf = std::numeric_limits<double>::infinity();
    std::vector<double> dist(count, Inf);
    std::vector<std::size_t> prev(count, count);   // count = from the ship
    using Item = std::pair<double, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    const auto relax = [&](std::size_t to, double d, std::size_t from)
    {
        if (d < dist[to])
        {
            dist[to] = d;
            prev[to] = from;
            open.push({d, to});
        }
    };
    const double direct = glm::length(goalWorld - shipWorld);
    relax(goal, direct, count);
    for (std::size_t l = 0; l < m_lanes.size(); ++l)
        for (std::size_t k = 0; k + 1 < lw[l].p.size(); ++k)
            relax(state(l, k, 0), glm::length(joinStart(l, k) - shipWorld) + JoinStraightMeters, count);
    while (!open.empty())
    {
        const auto [d, s] = open.top();
        open.pop();
        if (d > dist[s] || s == goal)
            continue;
        const std::size_t l = static_cast<std::size_t>(std::upper_bound(base.begin(), base.end(), s) - base.begin()) - 1;
        const std::size_t k = (s - base[l]) / 2;
        const int ridden = static_cast<int>((s - base[l]) % 2);
        if (k + 1 < lw[l].p.size())
            relax(state(l, k + 1, 1), d + LaneCostFactor * glm::length(lw[l].p[k + 1] - lw[l].p[k]), s);
        if (!ridden)
            continue;
        const glm::dvec3 out0 = leaveEnd(l, k);
        relax(goal, d + LeaveStraightMeters + glm::length(goalWorld - out0), s);
        for (std::size_t m = 0; m < m_lanes.size(); ++m)
        {
            if (m == l)
                continue;
            for (std::size_t j = 0; j + 1 < lw[m].p.size(); ++j)
            {
                const double t = glm::length(joinStart(m, j) - out0);
                if (t <= MaxTransferMeters)
                    relax(state(m, j, 0), d + LeaveStraightMeters + t + JoinStraightMeters, s);
            }
        }
    }
    // the chain: walk back from the goal
    struct Span
    {
        std::size_t lane, from, to;
    };
    std::vector<Span> spans;
    for (std::size_t s = prev[goal]; s != count;)
    {
        const std::size_t l = static_cast<std::size_t>(std::upper_bound(base.begin(), base.end(), s) - base.begin()) - 1;
        const std::size_t to = (s - base[l]) / 2;
        std::size_t from = to;
        std::size_t c = s;
        // walk back along the lane to its join state (ridden = 0)
        while ((c - base[l]) % 2 == 1)
        {
            c = prev[c];
            from = (c - base[l]) / 2;
        }
        spans.push_back({l, from, to});
        s = prev[c];
    }
    std::reverse(spans.begin(), spans.end());
    std::ostringstream text;
    text << "direct_m=" << direct << " chain_cost=" << dist[goal] << " spans=" << spans.size();
    if (spans.empty())
    {
        out.failure = "no lane chain beats the direct leg";
        if (summary)
            *summary = text.str();
        return out;
    }
    const glm::dvec3 up = frame.directionToWorld(glm::dvec3(0.0, 1.0, 0.0));
    for (const auto& sp : spans)
    {
        const auto& lane = m_lanes[sp.lane];
        const auto& w = lw[sp.lane];
        text << " [" << lane.id << " " << sp.from << "->" << sp.to << "]";
        CompiledTrafficStage stage;
        stage.kind = TrafficRouteStageKind::VolumeTransit;
        stage.laneId = lane.id;
        stage.fromPortalId = lane.id + "@" + std::to_string(sp.from);
        stage.toPortalId = lane.id + "@" + std::to_string(sp.to);
        stage.fromWorldMeters = w.p[sp.from];
        stage.toWorldMeters = w.p[sp.to];
        stage.volumeConstraint.volumeId = lane.id;
        stage.volumeConstraint.policy = world::navigation::NavigationVolumePolicy::PreferInside;
        stage.volumeConstraint.maxSpeedMps = lane.maxSpeedMps;
        for (std::size_t k = sp.from; k <= sp.to; ++k)
        {
            CompiledNavigationVolumeSection section;
            section.centerWorldMeters = w.p[k];
            section.forwardWorld = k < sp.to ? w.fwd[k] : w.fwd[sp.to - 1];
            glm::dvec3 u = up - section.forwardWorld * glm::dot(up, section.forwardWorld);
            section.upWorld = glm::length(u) > 1.0e-6 ? glm::normalize(u) : glm::dvec3(0.0, 1.0, 0.0);
            section.crossSection = world::navigation::NavigationCrossSectionKind::Circle;
            section.radiusMeters = lane.radiusMeters;
            stage.volumeSections.push_back(section);
            if (k > sp.from && k < sp.to)
                stage.requiredCenterlineMeters.push_back(w.p[k]);
        }
        CompiledMandatoryTangentStraight in;
        in.portalId = stage.fromPortalId;
        in.inbound = true;
        in.forwardWorld = w.fwd[sp.from];
        in.lengthMeters = JoinStraightMeters;
        in.endWorldMeters = stage.fromWorldMeters;
        in.startWorldMeters = stage.fromWorldMeters - in.forwardWorld * JoinStraightMeters;
        CompiledMandatoryTangentStraight leave;
        leave.portalId = stage.toPortalId;
        leave.inbound = false;
        leave.forwardWorld = w.fwd[sp.to - 1];
        leave.lengthMeters = LeaveStraightMeters;
        leave.startWorldMeters = stage.toWorldMeters;
        leave.endWorldMeters = stage.toWorldMeters + leave.forwardWorld * LeaveStraightMeters;
        out.mandatoryTangentStraights.push_back(in);
        out.mandatoryTangentStraights.push_back(leave);
        appendUnique(out.requiredViaPointsMeters, in.startWorldMeters);
        appendUnique(out.requiredViaPointsMeters, stage.fromWorldMeters);
        for (const auto& p : stage.requiredCenterlineMeters)
            appendUnique(out.requiredViaPointsMeters, p);
        appendUnique(out.requiredViaPointsMeters, stage.toWorldMeters);
        appendUnique(out.requiredViaPointsMeters, leave.endWorldMeters);
        out.stages.push_back(std::move(stage));
    }
    out.valid = true;
    if (summary)
        *summary = text.str();
    return out;
}

std::vector<world::navigation::NavigationObstacle> HubLaneNetwork::redZoneObstacles(
    const NavigationReferenceFramePose& frame
) const
{
    std::vector<world::navigation::NavigationObstacle> out;
    for (const auto& zone : m_redZones)
    {
        glm::dmat4 r(1.0);
        r = glm::rotate(r, glm::radians(zone.rotationDeg.x), glm::dvec3(1, 0, 0));
        r = glm::rotate(r, glm::radians(zone.rotationDeg.y), glm::dvec3(0, 1, 0));
        r = glm::rotate(r, glm::radians(zone.rotationDeg.z), glm::dvec3(0, 0, 1));
        world::navigation::NavigationObstacle o;
        o.id = "red:" + zone.id;
        o.shape = world::navigation::NavigationObstacleShape::Box;
        o.centerMeters = frame.pointToWorld(zone.centerLocal);
        o.localToWorldBasis = frame.localToWorldBasis * glm::dmat3(r);
        o.halfExtentsMeters = zone.halfExtentsMeters;
        out.push_back(std::move(o));
    }
    return out;
}

} // namespace game::navigation::traffic
