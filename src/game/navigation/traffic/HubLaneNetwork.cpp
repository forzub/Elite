#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>

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

constexpr int EdgeFromShip = -1;
constexpr int EdgeLane = -2;
constexpr int EdgeTransfer = -3;
constexpr int EdgeExit = -4;

} // namespace

bool HubLaneNetwork::load(const std::string& path)
{
    m_lanes.clear();
    m_connectors.clear();
    m_redZones.clear();
    m_failure.clear();
    m_speedLimitMps = 0.0;
    m_speedLimitZoneMeters = 0.0;
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
        m_hubId = doc.value("hub_id", std::string());
        m_speedLimitMps = doc.value("speed_limit_mps", 0.0);
        m_speedLimitZoneMeters = doc.value("speed_limit_zone_m", 0.0);
        std::unordered_map<std::string, std::size_t> index;
        for (const auto& item : doc.at("lanes"))
        {
            Lane lane;
            lane.id = item.at("id").get<std::string>();
            lane.radiusMeters = item.value("radius_m", 45.0);
            lane.maxSpeedMps = item.value("max_speed_mps", 150.0);
            lane.maxShipLengthMeters = item.value("max_ship_length_m", 1.0e9);
            lane.blue = item.value("kind", std::string("green")) == "blue";
            for (const auto& p : item.at("stations_local_m"))
                lane.stationsLocal.push_back(vec3Of(p));
            if (lane.stationsLocal.size() >= 2)
            {
                index[lane.id] = m_lanes.size();
                m_lanes.push_back(std::move(lane));
            }
        }
        for (const auto& item : doc.value("connectors", nlohmann::json::array()))
        {
            const auto from = index.find(item.at("from_lane").get<std::string>());
            const auto to = index.find(item.at("to_lane").get<std::string>());
            if (from == index.end() || to == index.end())
                continue;
            Connector c;
            c.id = item.at("id").get<std::string>();
            c.fromLane = from->second;
            c.toLane = to->second;
            c.fromStation = item.at("from_station").get<std::size_t>();
            c.toStation = item.at("to_station").get<std::size_t>();
            if (c.fromStation >= m_lanes[c.fromLane].stationsLocal.size() ||
                c.toStation >= m_lanes[c.toLane].stationsLocal.size())
                continue;
            c.radiusMeters = item.value("radius_m", 45.0);
            c.maxSpeedMps = item.value("max_speed_mps", 100.0);
            c.maxShipLengthMeters = item.value("max_ship_length_m", 1.0e9);
            for (const auto& p : item.at("points_local_m"))
                c.pointsLocal.push_back(vec3Of(p));
            if (c.pointsLocal.size() < 2)
                continue;
            // the ends ARE the stations (the file rounds the points coarser than the stations)
            c.pointsLocal.front() = m_lanes[c.fromLane].stationsLocal[c.fromStation];
            c.pointsLocal.back() = m_lanes[c.toLane].stationsLocal[c.toStation];
            for (std::size_t i = 1; i < c.pointsLocal.size(); ++i)
                c.lengthMeters += glm::length(c.pointsLocal[i] - c.pointsLocal[i - 1]);
            m_connectors.push_back(std::move(c));
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
        m_connectors.clear();
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
    double shipLengthMeters,
    const glm::dvec3& initialForward,
    double startLeadMeters,
    const glm::dvec3& goalArriveForward,
    const FreeLegModel& freeLegs,
    std::string* summary,
    const std::vector<world::navigation::NavigationObstacle>* obstacles,
    double agentRadiusMeters
) const
{
    const auto searchStart = std::chrono::steady_clock::now();
    // a straight free piece clear of the buildings and the red zones (the hull + 10 m); no obstacle list = not checked
    int blockedLegs = 0;
    const auto clear = [&](const glm::dvec3& a, const glm::dvec3& b) {
        if (!obstacles || obstacles->empty())
            return true;
        const bool ok = world::navigation::segmentClearOfNavigationObstacles(a, b, *obstacles, agentRadiusMeters, 10.0);
        blockedLegs += ok ? 0 : 1;
        return ok;
    };
    CompiledTrafficRoute out;
    const std::size_t laneCount = m_lanes.size();
    // world stations, per-station travel directions, the global station index base of every lane
    std::vector<std::vector<glm::dvec3>> P(laneCount), F(laneCount);
    std::vector<char> usable(laneCount, 0);
    std::vector<std::size_t> sbase(laneCount + 1, 0);
    for (std::size_t l = 0; l < laneCount; ++l)
    {
        for (const auto& s : m_lanes[l].stationsLocal)
            P[l].push_back(frame.pointToWorld(s));
        const auto n = P[l].size();
        F[l].resize(n);
        for (std::size_t k = 0; k + 1 < n; ++k)
            F[l][k] = glm::normalize(P[l][k + 1] - P[l][k]);
        F[l][n - 1] = F[l][n - 2];
        usable[l] = m_lanes[l].maxShipLengthMeters >= shipLengthMeters ? 1 : 0;
        sbase[l + 1] = sbase[l] + n;
    }
    std::vector<std::vector<glm::dvec3>> C(m_connectors.size());
    std::vector<std::vector<std::size_t>> outgoing(sbase[laneCount]);
    for (std::size_t c = 0; c < m_connectors.size(); ++c)
    {
        const auto& cn = m_connectors[c];
        if (!usable[cn.fromLane] || !usable[cn.toLane] || cn.maxShipLengthMeters < shipLengthMeters)
            continue;
        for (const auto& p : cn.pointsLocal)
            C[c].push_back(frame.pointToWorld(p));
        outgoing[sbase[cn.fromLane] + cn.fromStation].push_back(c);
    }
    // a lane is joined at ANY of its stations, merging from the side along its course, and left at any station it rode
    // to (author 2026-11-22: «возможность планировщика нырять прямо в зелёную зону без пересечения её точки входа - это
    // существенно бы ему всё упростило»; joining only at a lane's first station cost 8-11 km loops through the city)
    const auto joinStartAt = [&](std::size_t l, std::size_t k) { return P[l][k] - F[l][k] * JoinStraightMeters; };
    const auto joinApproachAt = [&](std::size_t l, std::size_t k) {
        return joinStartAt(l, k) - F[l][k] * freeLegs.joinApproachMeters;
    };
    const auto leaveEndAt = [&](std::size_t l, std::size_t k) { return P[l][k] + F[l][k] * LeaveStraightMeters; };
    // the join straight of every station checked ONCE (the search reuses it; per pop it froze the game - live 2026-11-22)
    constexpr double OpenSkyMeters = 1500.0;
    std::vector<std::vector<char>> joinClear(laneCount);
    for (std::size_t l = 0; l < laneCount; ++l)
    {
        joinClear[l].assign(P[l].size(), 0);
        if (usable[l])
            for (std::size_t k = 0; k + 1 < P[l].size(); ++k)
            {
                // ... and joined only from open sky (author 2026-11-22: «в зелёную зону входим из открытого
                // пространства»): straight out of the city (up over the top tier, down under the lower one) from the
                // approach's start nothing stands - a join under the roofs made the inbound leg dive between the
                // buildings (50 s of search, a 1.3 km turn radius among 26-40 m streets)
                const glm::dvec3 a = joinApproachAt(l, k);
                const glm::dvec3 up = frame.localToWorldBasis[1] *
                                      (m_lanes[l].stationsLocal[k].y >= 0.0 ? 1.0 : -1.0);
                joinClear[l][k] = clear(a, P[l][k]) && clear(a, a + up * OpenSkyMeters) ? 1 : 0;
            }
    }

    // states: (global station, ridden) + the goal
    const std::size_t goal = 2 * sbase[laneCount];
    const std::size_t count = goal + 1;
    const auto state = [&](std::size_t l, std::size_t k, int ridden) { return 2 * (sbase[l] + k) + ridden; };
    const auto laneOf = [&](std::size_t globalStation) {
        return static_cast<std::size_t>(std::upper_bound(sbase.begin(), sbase.end(), globalStation) - sbase.begin()) - 1;
    };
    constexpr double Inf = std::numeric_limits<double>::infinity();
    std::vector<double> dist(count, Inf);
    std::vector<std::size_t> prevState(count, count);   // count = from the ship
    std::vector<int> prevEdge(count, EdgeFromShip);
    using Item = std::pair<double, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    const auto relax = [&](std::size_t to, double d, std::size_t from, int edge)
    {
        if (d < dist[to])
        {
            dist[to] = d;
            prevState[to] = from;
            prevEdge[to] = edge;
            open.push({d, to});
        }
    };
    const auto angle = [](const glm::dvec3& a, const glm::dvec3& b) {
        const double la = glm::length(a), lb = glm::length(b);
        return la < 1.0e-9 || lb < 1.0e-9 ? 0.0 : std::acos(std::clamp(glm::dot(a, b) / (la * lb), -1.0, 1.0));
    };
    // the ship's own start: its committed heading flown for the start lead first
    const bool hasHeading = glm::length(initialForward) > 1.0e-9;
    const glm::dvec3 heading = hasHeading ? glm::normalize(initialForward) : glm::dvec3(0.0);
    const glm::dvec3 leadEnd = hasHeading ? shipWorld + heading * startLeadMeters : shipWorld;
    const double leadLength = hasHeading ? startLeadMeters : 0.0;
    // a free leg from `from` (heading fromForward, or none) to `target` (arriving along arriveForward, or free):
    // its length + the turn penalty, or Inf when it has no room for its turns
    const auto freeLeg = [&](const glm::dvec3& from, const glm::dvec3& fromForward, const glm::dvec3& target,
                             const glm::dvec3& arriveForward) {
        const glm::dvec3 d = target - from;
        const double turn = (glm::length(fromForward) > 1.0e-9 ? angle(fromForward, d) : 0.0) +
                            (glm::length(arriveForward) > 1.0e-9 ? angle(d, arriveForward) : 0.0);
        // a leg that is (nearly) one straight - e.g. a blue approach's end on the dock axis -> the goal - needs no room
        if (turn > glm::radians(5.0) && glm::length(d) < MinFreeLegMeters + freeLegs.turnRadiusMeters * turn)
            return Inf;
        if (!clear(from, target))
            return Inf;
        // in a hub (its obstacles given) a free metre costs FreeInHubFactor: the green lanes win (author 2026-11-22:
        // «игнорирование зелёных зон» - a clear straight over the city was as cheap as any lane chain)
        const double freeFactor = obstacles && !obstacles->empty() ? FreeInHubFactor : 1.0;
        return freeFactor * glm::length(d) + TurnPenaltyMeters * turn;
    };
    // the goal is reached along goalArriveForward after a hard straight: a free leg into it ends that far out
    const bool goalHasDirection = glm::length(goalArriveForward) > 1.0e-9;
    const glm::dvec3 goalDirection = goalHasDirection ? glm::normalize(goalArriveForward) : glm::dvec3(0.0);
    const glm::dvec3 goalApproach = goalWorld - goalDirection * (goalHasDirection ? freeLegs.joinApproachMeters : 0.0);
    const double goalApproachLength = goalHasDirection ? freeLegs.joinApproachMeters : 0.0;
    const double direct = leadLength + freeLeg(leadEnd, heading, goalApproach, goalDirection) + goalApproachLength;
    // a goal reached by a service (a city pad): the lanes only - in the hub the green lanes come before open space
    // (author 2026-11-22: «на территории хаба зелёная зона приоритетнее пустого пространства из соображений
    // безопасности»; the clear straight over the roofs beat every lane chain and was never feasible)
    // a goal reached by a service (a city pad): the straight way costs FreeInHubFactor per metre whether or not a free
    // leg fits it (the service flies it, not the planner) - balanced against the lane chains (author 2026-11-22: «надо
    // иметь баланс»)
    if (freeLegs.goalByService && freeLegs.serviceDirect)
        relax(goal, (obstacles && !obstacles->empty() ? FreeInHubFactor : 1.0) * glm::length(goalWorld - shipWorld), count,
              EdgeFromShip);
    else   // the planner flies the straight way itself: its own free-leg cost (Inf when it has no room)
    {
        // ... and in a city it never cuts through a lane's tube (author 2026-11-22: «пролетает сквозь тоннели. Это не
        // безопасно»): a straight that passes inside any tube (but near its own ends) is no way - the lanes are taken
        bool cutsTube = false;
        if (freeLegs.goalByService && std::isfinite(direct))
        {
            const glm::dvec3 a = leadEnd, b = goalApproach;
            const double len = glm::length(b - a);
            const int n = std::max(1, static_cast<int>(len / 25.0));
            for (int i = 1; i < n && !cutsTube; ++i)
            {
                const glm::dvec3 q = a + (b - a) * (static_cast<double>(i) / n);
                if (glm::length(q - shipWorld) < 300.0 || glm::length(q - goalWorld) < 300.0)
                    continue;
                for (std::size_t l = 0; l < laneCount && !cutsTube; ++l)
                    for (std::size_t k = 0; k + 1 < P[l].size(); ++k)
                    {
                        const glm::dvec3 s0 = P[l][k], s1 = P[l][k + 1], d = s1 - s0;
                        const double t = std::clamp(glm::dot(q - s0, d) / std::max(glm::dot(d, d), 1.0e-9), 0.0, 1.0);
                        if (glm::length(q - (s0 + d * t)) < m_lanes[l].radiusMeters + agentRadiusMeters)
                        {
                            cutsTube = true;
                            break;
                        }
                    }
            }
        }
        if (!cutsTube)
            relax(goal, std::isfinite(direct) ? direct : glm::length(goalWorld - shipWorld) + 1.0e6, count, EdgeFromShip);
    }
    int entriesOk = 0, exitsTried = 0, exitsOk = 0;
    for (std::size_t l = 0; l < laneCount; ++l)
        if (usable[l])
            for (std::size_t k = 0; k + 1 < P[l].size(); ++k)
            {
                const double c = joinClear[l][k] ? freeLeg(leadEnd, heading, joinApproachAt(l, k), F[l][k]) : Inf;
                entriesOk += std::isfinite(c) ? 1 : 0;
                if (std::isfinite(c))
                    relax(state(l, k, 0), leadLength + c + freeLegs.joinApproachMeters + JoinStraightMeters, count,
                          EdgeFromShip);
            }
    while (!open.empty())
    {
        const auto [d, s] = open.top();
        open.pop();
        if (d > dist[s] || s == goal)
            continue;
        const std::size_t g = s / 2;
        const int ridden = static_cast<int>(s % 2);
        const std::size_t l = laneOf(g);
        const std::size_t k = g - sbase[l];
        const std::size_t n = P[l].size();
        if (k + 1 < n)
            relax(state(l, k + 1, 1), d + LaneCostFactor * glm::length(P[l][k + 1] - P[l][k]), s, EdgeLane);
        if (!ridden)
            continue;
        for (const std::size_t c : outgoing[g])
        {
            const auto& cn = m_connectors[c];
            relax(state(cn.toLane, cn.toStation, 1), d + LaneCostFactor * cn.lengthMeters, s, static_cast<int>(c));
        }
        // leave at this station (any station it rode to; the lane's end beyond the rim included): for the goal, or a
        // transfer (open sky) into another lane at any of its stations
        const glm::dvec3 out0 = leaveEndAt(l, k);
        // the final leg has its own fixed straights (start + turn budget + the goal's axis): shorter -> it loops back
        // (live run 14: 1.1 km -> assisted reverse flight)
        // a goal reached by a service (a pad): the route stops at this station; the rest costs its straight distance
        // (the service flies it over the roofs) - so the exit nearest the pad wins
        const double exitLeg = freeLegs.goalByService
                                   ? FreeInHubFactor * glm::length(goalWorld - P[l][k])
                                   : glm::length(goalWorld - out0) < freeLegs.exitMinimumMeters || !clear(P[l][k], out0)
                                   ? Inf
                                   : freeLeg(out0, F[l][k], goalApproach, goalDirection);
        ++exitsTried;
        exitsOk += std::isfinite(exitLeg) ? 1 : 0;
        if (std::isfinite(exitLeg))
            relax(goal, d + LeaveStraightMeters + exitLeg + goalApproachLength, s, EdgeExit);
        if (k + 1 != n)
            continue;   // a transfer to another lane: from this lane's end only (from every station it froze the game)
        for (std::size_t m = 0; m < laneCount; ++m)
        {
            if (m == l || !usable[m])
                continue;
            for (std::size_t j = 0; j + 1 < P[m].size(); ++j)
            {
                if (!joinClear[m][j] || glm::length(joinStartAt(m, j) - out0) > MaxTransferMeters)
                    continue;
                const double hop = freeLeg(out0, F[l][k], joinApproachAt(m, j), F[m][j]);
                if (std::isfinite(hop))
                    relax(state(m, j, 0), d + LeaveStraightMeters + hop + freeLegs.joinApproachMeters + JoinStraightMeters,
                          s, EdgeTransfer);
            }
        }
    }
    std::ostringstream text;
    const glm::dmat3 toLocal = glm::transpose(frame.localToWorldBasis);
    const glm::dvec3 goalLocal = toLocal * (goalWorld - frame.originWorldMeters);
    const glm::dvec3 arriveLocal = toLocal * goalDirection;
    text << "search_ms=" << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - searchStart).count()
         << " blocked_legs=" << blockedLegs << " ";
    text << "direct_m=" << direct << " chain_cost=" << dist[goal] << " entries_ok=" << entriesOk << " exits_ok=" << exitsOk
         << "/" << exitsTried << " goal_local=(" << goalLocal.x << "," << goalLocal.y << "," << goalLocal.z << ") arrive_local=("
         << arriveLocal.x << "," << arriveLocal.y << "," << arriveLocal.z << ")";
    if (prevState[goal] == count)
    {
        out.failure = "no lane chain beats the direct leg";
        if (summary)
            *summary = text.str();
        return out;
    }
    // the chain, ship -> goal: (state, the edge that reached it)
    std::vector<std::pair<std::size_t, int>> chain;
    for (std::size_t s = prevState[goal]; s != count; s = prevState[s])
        chain.push_back({s, prevEdge[s]});
    std::reverse(chain.begin(), chain.end());

    // continuous pieces (a piece starts where the ship joins from open sky) -> one stage each
    struct Piece
    {
        std::vector<glm::dvec3> points;
        std::vector<double> radii;
        double speed = Inf;
        std::string from, to, ids;
    };
    std::vector<Piece> pieces;
    for (const auto& [s, edge] : chain)
    {
        const std::size_t g = s / 2;
        const std::size_t l = laneOf(g);
        const std::size_t k = g - sbase[l];
        const auto addPoint = [&](Piece& piece, const glm::dvec3& p, double radius) {
            if (piece.points.empty() || glm::length(piece.points.back() - p) > 1.0e-6)
            {
                piece.points.push_back(p);
                piece.radii.push_back(radius);
            }
        };
        if (edge == EdgeFromShip || edge == EdgeTransfer)
        {
            pieces.emplace_back();
            pieces.back().from = m_lanes[l].id + "@" + std::to_string(k);
            pieces.back().ids = m_lanes[l].id;
            addPoint(pieces.back(), P[l][k], m_lanes[l].radiusMeters);
        }
        else if (edge == EdgeLane)
            addPoint(pieces.back(), P[l][k], m_lanes[l].radiusMeters);
        else if (edge >= 0)
        {
            const auto& cn = m_connectors[static_cast<std::size_t>(edge)];
            for (const auto& p : C[static_cast<std::size_t>(edge)])
                addPoint(pieces.back(), p, cn.radiusMeters);
            pieces.back().speed = std::min(pieces.back().speed, cn.maxSpeedMps);
            pieces.back().ids += " > " + cn.id + " > " + m_lanes[l].id;
        }
        pieces.back().speed = std::min(pieces.back().speed, m_lanes[l].maxSpeedMps);
        pieces.back().to = m_lanes[l].id + "@" + std::to_string(k);
    }
    text << " stages=" << pieces.size();
    const glm::dvec3 up = frame.directionToWorld(glm::dvec3(0.0, 1.0, 0.0));
    for (const auto& piece : pieces)
    {
        if (piece.points.size() < 2)
        {
            out.failure = "degenerate lane piece " + piece.from;
            if (summary)
                *summary = text.str();
            return out;
        }
        text << " [" << piece.ids << "]";
        const std::size_t n = piece.points.size();
        CompiledTrafficStage stage;
        stage.kind = TrafficRouteStageKind::VolumeTransit;
        stage.laneId = piece.from;
        stage.fromPortalId = piece.from;
        stage.toPortalId = piece.to;
        stage.fromWorldMeters = piece.points.front();
        stage.toWorldMeters = piece.points.back();
        stage.volumeConstraint.volumeId = piece.from + ">" + piece.to;
        stage.volumeConstraint.policy = world::navigation::NavigationVolumePolicy::PreferInside;
        stage.volumeConstraint.maxSpeedMps = piece.speed;
        for (std::size_t i = 0; i < n; ++i)
        {
            CompiledNavigationVolumeSection section;
            section.centerWorldMeters = piece.points[i];
            section.forwardWorld = glm::normalize(i + 1 < n ? piece.points[i + 1] - piece.points[i] : piece.points[i] - piece.points[i - 1]);
            glm::dvec3 u = up - section.forwardWorld * glm::dot(up, section.forwardWorld);
            section.upWorld = glm::length(u) > 1.0e-6 ? glm::normalize(u) : glm::dvec3(0.0, 1.0, 0.0);
            section.crossSection = world::navigation::NavigationCrossSectionKind::Circle;
            section.radiusMeters = piece.radii[i];
            stage.volumeSections.push_back(section);
            if (i > 0 && i + 1 < n)
                stage.requiredCenterlineMeters.push_back(piece.points[i]);
        }
        CompiledMandatoryTangentStraight in;
        in.portalId = stage.fromPortalId;
        in.inbound = true;
        in.forwardWorld = glm::normalize(piece.points[1] - piece.points[0]);
        in.lengthMeters = JoinStraightMeters;
        in.endWorldMeters = stage.fromWorldMeters;
        in.startWorldMeters = stage.fromWorldMeters - in.forwardWorld * JoinStraightMeters;
        CompiledMandatoryTangentStraight leave;
        leave.portalId = stage.toPortalId;
        leave.inbound = false;
        leave.forwardWorld = glm::normalize(piece.points[n - 1] - piece.points[n - 2]);
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

namespace
{

struct CatalogData
{
    std::vector<std::unique_ptr<HubLaneNetwork>> networks;
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
        for (const auto& entry : std::filesystem::directory_iterator("assets/data/navigation/hub_lanes", ec))
        {
            if (entry.path().extension() != ".json")
                continue;
            auto network = std::make_unique<HubLaneNetwork>();
            if (network->load(entry.path().string()))
                data.networks.push_back(std::move(network));
        }
        return data;
    }();
    return data;
}

} // namespace

const HubLaneNetwork* HubLaneCatalog::forHub(const std::string& hubId)
{
    for (const auto& network : catalog().networks)
        if (network->hubId() == hubId)
            return network.get();
    return nullptr;
}

const HubLaneNetwork* HubLaneCatalog::forFrameModule(const std::string& moduleId)
{
    for (const auto& network : catalog().networks)
        if (network->frameModuleId() == moduleId)
            return network.get();
    return nullptr;
}

} // namespace game::navigation::traffic
