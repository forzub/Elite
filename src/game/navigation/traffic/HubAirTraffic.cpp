#include "src/game/navigation/traffic/HubAirTraffic.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"

namespace game::navigation::traffic
{
namespace
{
double hash01(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<double>(h & 0xFFFFFFu) / static_cast<double>(0x1000000u);
}

glm::mat4 frameOf(const glm::dvec3& position, const glm::dvec3& forward, const glm::dvec3& up)
{
    const glm::dvec3 x = glm::normalize(glm::cross(up, forward));
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(glm::vec3(x), 0.0f);
    m[1] = glm::vec4(glm::vec3(up), 0.0f);
    m[2] = glm::vec4(glm::vec3(forward), 0.0f);
    m[3] = glm::vec4(glm::vec3(position), 1.0f);
    return m;
}
} // namespace

namespace
{
// a lane end / start inside the city (a dead end / no way in): a block ahead of it in a corridor as wide as a block
// and its street (a lane ends in a STREET - a plain ray along it never meets a building, live 2026-11-22: 0 dead ends)
bool facesBuildings(const HubPlan& plan, const glm::dvec3& at, const glm::dvec3& direction)
{
    glm::dvec2 d(direction.x, direction.z);
    if (glm::length(d) < 1.0e-6)
        return false;
    d = glm::normalize(d);
    const glm::dvec2 side(-d.y, d.x);
    for (const auto& b : plan.blocks())
    {
        const glm::dvec2 rel = b.centre - glm::dvec2(at.x, at.z);
        const double along = glm::dot(rel, d), lateral = std::abs(glm::dot(rel, side));
        if (along > -b.halfMeters && along < 1200.0 && lateral < b.halfMeters + 350.0)
            return true;
    }
    return false;
}

void appendResampled(std::vector<glm::dvec3>& pts, std::vector<double>& limit, const glm::dvec3& p, double v)
{
    if (pts.empty())
    {
        pts.push_back(p);
        limit.push_back(v);
        return;
    }
    const glm::dvec3 a = pts.back();
    const double len = glm::length(p - a);
    if (len < 0.5)
    {
        limit.back() = std::min(limit.back(), v);
        return;
    }
    const int n = std::max(1, static_cast<int>(std::ceil(len / 25.0)));
    for (int i = 1; i <= n; ++i)
    {
        pts.push_back(a + (p - a) * (static_cast<double>(i) / n));
        limit.push_back(v);
    }
}

struct RoutePiece
{
    std::size_t lane;
    std::size_t from;
    std::size_t to;           // inclusive station range on the lane
    int connector = -1;       // the connector taken after it (-1: the route ends with this lane)
};
} // namespace

double HubAirLaneTrack::speedAt(double s) const
{
    if (cumulative.empty())
        return cruiseMps;
    const auto it = std::upper_bound(cumulative.begin(), cumulative.end(), s);
    const std::size_t k = it == cumulative.begin() ? 0 : static_cast<std::size_t>(it - cumulative.begin()) - 1;
    return speed[std::min(k, speed.size() - 1)];
}

void HubAirTrafficLayout::build(const game::navigation::HubPlan& plan, const HubLaneNetwork& lanes)
{
    m_built = true;
    m_lanes = &lanes;
    m_slots.clear();
    m_tracks.clear();
    const auto& fleet = plan.airFleet();
    if (fleet.empty())
        return;
    constexpr double ShipsPerMeter = 1.0 / 3500.0;   // «не обязательно слишком много»: one per 3.5 km of route
    constexpr double RampMps2 = 3.0;
    const auto& L = lanes.lanes();
    const auto& C = lanes.connectors();
    std::vector<std::vector<int>> outOf(L.size());   // the connectors leaving each lane
    for (std::size_t c = 0; c < C.size(); ++c)
        if (C[c].fromLane < L.size() && C[c].toLane < L.size())
            outOf[C[c].fromLane].push_back(static_cast<int>(c));
    for (auto& v : outOf)
        std::sort(v.begin(), v.end(), [&](int a, int b) { return C[static_cast<std::size_t>(a)].fromStation < C[static_cast<std::size_t>(b)].fromStation; });
    const auto isPlaza = [&](std::size_t li) { return L[li].id.find(".plaza.") != std::string::npos; };
    const auto direction = [&](std::size_t li, bool atEnd) {
        const auto& st = L[li].stationsLocal;
        return atEnd ? st[st.size() - 1] - st[st.size() - 2] : st[1] - st[0];
    };
    // the lane's end: open sky / the dock (an exit) or buildings (a dead end)
    std::vector<char> exitEnd(L.size(), 0);
    for (std::size_t li = 0; li < L.size(); ++li)
        if (L[li].stationsLocal.size() >= 2)
            exitEnd[li] = L[li].blue || !facesBuildings(plan, L[li].stationsLocal.back(), direction(li, true)) ? 1 : 0;

    const auto addTrack = [&](HubAirLaneTrack track, double density) {
        if (track.points.size() < 2 || track.lengthMeters < 200.0)
            return;
        double total = 0.0;
        for (const auto& f : fleet)
            if (f.lengthMeters <= track.maxShipLengthMeters)
                total += f.weight;
        if (total <= 0.0)
            return;
        const int ti = static_cast<int>(m_tracks.size());
        if (ti >= 1024)
            return;
        track.lane = ti;
        const std::uint32_t seed = static_cast<std::uint32_t>(std::hash<std::string>{}(track.name));
        const int count = std::clamp(static_cast<int>(std::lround(track.lengthMeters * ShipsPerMeter * density)), 1, 63);
        for (int i = 0; i < count; ++i)
        {
            HubAirSlot slot;
            slot.id = (static_cast<std::uint32_t>(ti) << 6) | static_cast<std::uint32_t>(i);
            slot.lane = ti;
            slot.speedMps = track.cruiseMps;
            slot.phaseMeters = (i + 0.7 * hash01(seed, static_cast<std::uint32_t>(i))) * track.totalSeconds / count;   // seconds
            double pick = hash01(seed + 77u, static_cast<std::uint32_t>(i)) * total;
            int index = 0;
            for (std::size_t f = 0; f < fleet.size(); ++f)
            {
                if (fleet[f].lengthMeters > track.maxShipLengthMeters)
                    continue;
                index = static_cast<int>(f);
                pick -= fleet[f].weight;
                if (pick <= 0.0)
                    break;
            }
            slot.fleetIndex = index;
            m_slots.push_back(slot);
        }
        m_tracks.push_back(std::move(track));
    };
    // the speed profile + time table of a track (from its per-point limits)
    const auto finish = [&](HubAirLaneTrack& t, std::vector<double> limit) {
        const std::size_t n = t.points.size();
        t.cumulative.assign(n, 0.0);
        for (std::size_t k = 1; k < n; ++k)
            t.cumulative[k] = t.cumulative[k - 1] + glm::length(t.points[k] - t.points[k - 1]);
        t.lengthMeters = n ? t.cumulative[n - 1] : 0.0;
        if (!t.loop && n)   // in from open sky / out at cruise: no ramp at the ends
            limit.front() = limit.back() = std::min(t.cruiseMps, std::max(limit.front(), limit.back()));
        for (std::size_t k = 1; k < n; ++k)   // accelerate at most RampMps2
            limit[k] = std::min(limit[k], std::sqrt(limit[k - 1] * limit[k - 1] + 2.0 * RampMps2 * (t.cumulative[k] - t.cumulative[k - 1])));
        for (std::size_t k = n; k-- > 1;)       // brake at most RampMps2
            limit[k - 1] = std::min(limit[k - 1], std::sqrt(limit[k] * limit[k] + 2.0 * RampMps2 * (t.cumulative[k] - t.cumulative[k - 1])));
        t.speed = limit;
        t.seconds.assign(n, 0.0);
        for (std::size_t k = 1; k < n; ++k)
            t.seconds[k] = t.seconds[k - 1] + (t.cumulative[k] - t.cumulative[k - 1]) / std::max(1.0, 0.5 * (t.speed[k] + t.speed[k - 1]));
        t.totalSeconds = n ? t.seconds[n - 1] : 0.0;
    };

    // 1. the plaza rings: one lap, looped (as before)
    for (std::size_t li = 0; li < L.size(); ++li)
    {
        if (!isPlaza(li) || L[li].stationsLocal.size() < 2)
            continue;
        HubAirLaneTrack t;
        t.name = L[li].id;
        t.loop = true;
        t.cruiseMps = 0.7 * L[li].maxSpeedMps;
        t.maxShipLengthMeters = L[li].maxShipLengthMeters;
        t.upSign = L[li].stationsLocal[0].y < 0.0 ? -1.0 : 1.0;
        std::vector<double> limit;
        const std::size_t n = std::min<std::size_t>(L[li].stationsLocal.size(), 37);
        for (std::size_t k = 0; k < n; ++k)
            appendResampled(t.points, limit, L[li].stationsLocal[k], t.cruiseMps);
        finish(t, limit);
        addTrack(std::move(t), 1.0);
    }

    // 2. the routes: from every way in (a lane start with open sky behind it), a few deterministic variants
    int routes = 0, straight = 0, deadEnds = 0;
    for (std::size_t li = 0; li < L.size(); ++li)
        deadEnds += !exitEnd[li] && !isPlaza(li) ? 1 : 0;
    for (std::size_t entry = 0; entry < L.size(); ++entry)
    {
        const auto& el = L[entry];
        if (isPlaza(entry) || el.blue || el.stationsLocal.size() < 2)
            continue;
        if (facesBuildings(plan, el.stationsLocal.front(), -direction(entry, false)))
            continue;   // begins among the buildings: only reached by a turn
        const std::uint32_t seed = static_cast<std::uint32_t>(std::hash<std::string>{}(el.id));
        for (int variant = 0; variant < 3; ++variant)
        {
            std::vector<RoutePiece> pieces;
            std::size_t lane = entry, from = 0;
            bool ok = false;
            for (int turns = 0; turns <= 3; ++turns)
            {
                const auto& lv = L[lane];
                // the connectors ahead on this lane (not onto the plaza's loop, not back onto where we came from)
                std::vector<int> ahead;
                for (int c : outOf[lane])
                {
                    const auto& cn = C[static_cast<std::size_t>(c)];
                    if (cn.fromStation <= from || isPlaza(cn.toLane) || cn.toStation + 1 >= L[cn.toLane].stationsLocal.size())
                        continue;
                    bool seen = false;
                    for (const auto& p : pieces)
                        seen = seen || p.lane == cn.toLane;
                    if (!seen && cn.toLane != entry)
                        ahead.push_back(c);
                }
                // a lane that goes on as another (a dock's blue lane starts where it ends): straight on into it
                int continuation = -1;
                if (!exitEnd[lane])
                    for (std::size_t o = 0; o < L.size(); ++o)
                        if (o != lane && !isPlaza(o) && L[o].stationsLocal.size() >= 2 &&
                            glm::length(L[o].stationsLocal.front() - lv.stationsLocal.back()) < 5.0)
                        {
                            bool seen = false;
                            for (const auto& p : pieces)
                                seen = seen || p.lane == o;
                            if (!seen)
                                continuation = static_cast<int>(o);
                        }
                const bool canEnd = exitEnd[lane] != 0 || continuation >= 0;
                // variant 0: straight through when the lane leads out; otherwise / later variants: a hashed turn
                const bool turnHere = !ahead.empty() &&
                                      (!canEnd || (variant > 0 && turns < 2 && hash01(seed + 31u * static_cast<std::uint32_t>(variant), static_cast<std::uint32_t>(turns)) < 0.8));
                if (!turnHere)
                {
                    if (!canEnd)
                        break;   // a dead end with no way on: no route
                    pieces.push_back({lane, from, lv.stationsLocal.size() - 1, -1});
                    if (continuation >= 0 && !exitEnd[lane])
                    {
                        lane = static_cast<std::size_t>(continuation);   // on into the next lane (its first station = here)
                        from = 1;
                        if (exitEnd[lane])
                        {
                            pieces.push_back({lane, from, L[lane].stationsLocal.size() - 1, -1});
                            ok = true;
                            break;
                        }
                        continue;
                    }
                    ok = true;
                    break;
                }
                const int pick = ahead[static_cast<std::size_t>(hash01(seed + 101u * static_cast<std::uint32_t>(variant + 1), static_cast<std::uint32_t>(turns)) * ahead.size()) % ahead.size()];
                const auto& cn = C[static_cast<std::size_t>(pick)];
                pieces.push_back({lane, from, cn.fromStation, pick});
                lane = cn.toLane;
                from = cn.toStation;
            }
            if (!ok)
                continue;
            HubAirLaneTrack t;
            std::vector<double> limit;
            t.cruiseMps = 1.0e9;
            for (const auto& p : pieces)
            {
                const auto& lv = L[p.lane];
                const double v = 0.7 * lv.maxSpeedMps;
                t.cruiseMps = std::min(t.cruiseMps, v);
                t.maxShipLengthMeters = std::min(t.maxShipLengthMeters, lv.maxShipLengthMeters);
                t.name += (t.name.empty() ? "" : ">") + lv.id;
                for (std::size_t k = p.from; k <= p.to; ++k)
                    appendResampled(t.points, limit, lv.stationsLocal[k], v);
                if (p.connector >= 0)
                {
                    const auto& cn = C[static_cast<std::size_t>(p.connector)];
                    t.maxShipLengthMeters = std::min(t.maxShipLengthMeters, cn.maxShipLengthMeters);
                    for (std::size_t k = 1; k + 1 < cn.pointsLocal.size(); ++k)
                        appendResampled(t.points, limit, cn.pointsLocal[k], std::min(v, cn.maxSpeedMps));
                }
            }
            t.upSign = el.stationsLocal[0].y < 0.0 ? -1.0 : 1.0;
            // a repeat of an earlier variant (the same chain): skipped
            bool dup = false;
            for (const auto& other : m_tracks)
                dup = dup || other.name == t.name;
            if (dup)
                continue;
            straight += pieces.size() == 1 ? 1 : 0;
            ++routes;
            finish(t, limit);
            addTrack(std::move(t), el.blue ? 0.5 : 1.0);
        }
    }
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        for (const auto& t : m_tracks)   // one turning route's profile: the cruise, the slowest point, the time
            if (!t.loop && *std::min_element(t.speed.begin(), t.speed.end()) < t.cruiseMps - 1.0)
            {
                std::fprintf(stderr, "[AirTraffic] route %s: %.0f m, %.0f s, cruise %.0f, slowest %.0f m/s, ships to %.0f m\n",
                             t.name.c_str(), t.lengthMeters, t.totalSeconds, t.cruiseMps,
                             *std::min_element(t.speed.begin(), t.speed.end()), t.maxShipLengthMeters);
                break;
            }
        std::fprintf(stderr, "[AirTraffic] %s: %d routes (%d straight, %d turning), %d lanes end among buildings (their last stretch never flown), %zu ships\n",
                     plan.hubId().c_str(), routes, straight, routes - straight, deadEnds, m_slots.size());
    }
}

const HubAirLaneTrack* HubAirTrafficLayout::track(int lane) const noexcept
{
    if (lane < 0 || lane >= static_cast<int>(m_tracks.size()))
        return nullptr;
    return &m_tracks[static_cast<std::size_t>(lane)];
}

const HubAirSlot* HubAirTrafficLayout::slot(std::uint32_t id) const noexcept
{
    for (const auto& s : m_slots)
        if (s.id == id)
            return &s;
    return nullptr;
}

bool HubAirTrafficLayout::poseAt(int trackIndex, double s, double speedMps, HubAirSlotPose& out) const
{
    const HubAirLaneTrack* t = track(trackIndex);
    if (!t || t->points.size() < 2)
        return false;
    const std::size_t n = t->cumulative.size();
    s = std::clamp(s, 0.0, t->lengthMeters);
    const auto it = std::upper_bound(t->cumulative.begin(), t->cumulative.end(), s);
    std::size_t k = std::clamp<std::size_t>(static_cast<std::size_t>(it - t->cumulative.begin()), 1, n - 1);
    const glm::dvec3 a = t->points[k - 1], b = t->points[k];
    const double segment = std::max(1.0e-6, t->cumulative[k] - t->cumulative[k - 1]);
    out.position = a + (b - a) * ((s - t->cumulative[k - 1]) / segment);
    // the heading: smoothed over the neighbouring segment (the turn's polyline)
    glm::dvec3 forward = b - a;
    if (k + 1 < n)
        forward = glm::normalize(forward) * 0.6 + glm::normalize(t->points[k + 1] - b) * 0.4;
    out.forward = glm::length(forward) > 1.0e-6 ? glm::normalize(forward) : glm::dvec3(0.0, 0.0, 1.0);
    const double upSign = out.position.y < 0.0 ? -1.0 : 1.0;
    out.up = glm::abs(out.forward.y) > 0.95 ? glm::dvec3(1.0, 0.0, 0.0) : glm::dvec3(0.0, upSign, 0.0);
    out.velocity = out.forward * speedMps;
    out.alongMeters = s;
    out.local = frameOf(out.position, -out.forward, out.up);   // nose -Z
    return true;
}

bool HubAirTrafficLayout::pose(const HubAirSlot& slot, double timeSeconds, HubAirSlotPose& out) const
{
    const HubAirLaneTrack* t = track(slot.lane);
    if (!t || t->totalSeconds <= 0.0)
        return false;
    // the presentation's day-periodic clock (fmod 86400) keeps float precision: identical on server and client
    const double time = std::fmod(timeSeconds, 86400.0);
    const double raw = slot.phaseMeters + time;   // phaseMeters holds the slot's phase in SECONDS along its route
    const double lap = std::floor(raw / t->totalSeconds);
    const double tt = raw - lap * t->totalSeconds;
    const auto it = std::upper_bound(t->seconds.begin(), t->seconds.end(), tt);
    const std::size_t n = t->seconds.size();
    const std::size_t k = std::clamp<std::size_t>(static_cast<std::size_t>(it - t->seconds.begin()), 1, n - 1);
    const double dtSeg = std::max(1.0e-6, t->seconds[k] - t->seconds[k - 1]);
    const double f = std::clamp((tt - t->seconds[k - 1]) / dtSeg, 0.0, 1.0);
    const double s = t->cumulative[k - 1] + f * (t->cumulative[k] - t->cumulative[k - 1]);
    const double v = t->speed[k - 1] + f * (t->speed[k] - t->speed[k - 1]);
    if (!poseAt(slot.lane, s, v, out))
        return false;
    out.lap = static_cast<std::int64_t>(lap) + static_cast<std::int64_t>(std::floor(timeSeconds / 86400.0)) * 1000000;
    return true;
}

const HubAirTrafficLayout& hubAirTrafficLayoutCached(const game::navigation::HubPlan& plan, const HubLaneNetwork& lanes)
{
    thread_local std::unordered_map<const HubLaneNetwork*, std::unique_ptr<HubAirTrafficLayout>> cache;
    auto& slot = cache[&lanes];
    if (!slot)
    {
        slot = std::make_unique<HubAirTrafficLayout>();
        slot->build(plan, lanes);
    }
    return *slot;
}
} // namespace game::navigation::traffic
