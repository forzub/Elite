#include "src/game/navigation/traffic/HubAirTraffic.h"

#include <algorithm>
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

void HubAirTrafficLayout::build(const game::navigation::HubPlan& plan, const HubLaneNetwork& lanes)
{
    m_built = true;
    m_lanes = &lanes;
    m_slots.clear();
    m_tracks.assign(lanes.lanes().size(), {});
    const auto& fleet = plan.airFleet();
    if (fleet.empty())
        return;
    constexpr double ShipsPerMeter = 1.0 / 3500.0;   // «не обязательно слишком много»: one per 3.5 km of lane
    for (std::size_t li = 0; li < lanes.lanes().size(); ++li)
    {
        const auto& lane = lanes.lanes()[li];
        std::size_t n = lane.stationsLocal.size();
        if (n < 2)
            continue;
        // the plaza ring runs 1.75 laps (merges / diverges): one lap is the loop a ship keeps flying
        if (lane.id.find(".plaza.") != std::string::npos)
            n = std::min<std::size_t>(n, 37);
        HubAirLaneTrack track;
        track.lane = static_cast<int>(li);
        track.cumulative.assign(n, 0.0);
        for (std::size_t k = 1; k < n; ++k)
            track.cumulative[k] = track.cumulative[k - 1] + glm::length(lane.stationsLocal[k] - lane.stationsLocal[k - 1]);
        track.lengthMeters = track.cumulative[n - 1];
        track.upSign = lane.stationsLocal[0].y < 0.0 ? -1.0 : 1.0;
        if (track.lengthMeters < 200.0)
            continue;
        double total = 0.0;
        for (const auto& s : fleet)
            if (s.lengthMeters <= lane.maxShipLengthMeters)
                total += s.weight;
        if (total <= 0.0)
            continue;
        const std::uint32_t seed = static_cast<std::uint32_t>(std::hash<std::string>{}(lane.id));
        const int count = std::max(1, static_cast<int>(std::lround(track.lengthMeters * ShipsPerMeter * (lane.blue ? 0.5 : 1.0))));
        const double speed = 0.7 * lane.maxSpeedMps;
        for (int i = 0; i < count && i < 256; ++i)
        {
            HubAirSlot slot;
            slot.id = (static_cast<std::uint32_t>(li) << 8) | static_cast<std::uint32_t>(i);
            slot.lane = static_cast<int>(li);
            slot.speedMps = speed;
            slot.phaseMeters = (i + 0.7 * hash01(seed, static_cast<std::uint32_t>(i))) * track.lengthMeters / count;
            // the hull: a weighted pick, stable for this ship of this lane
            double pick = hash01(seed + 77u, static_cast<std::uint32_t>(i)) * total;
            int index = 0;
            for (std::size_t f = 0; f < fleet.size(); ++f)
            {
                if (fleet[f].lengthMeters > lane.maxShipLengthMeters)
                    continue;
                index = static_cast<int>(f);
                pick -= fleet[f].weight;
                if (pick <= 0.0)
                    break;
            }
            slot.fleetIndex = index;
            m_slots.push_back(slot);
        }
        m_tracks[li] = std::move(track);
    }
}

const HubAirLaneTrack* HubAirTrafficLayout::track(int lane) const noexcept
{
    if (lane < 0 || lane >= static_cast<int>(m_tracks.size()) || m_tracks[static_cast<std::size_t>(lane)].lane < 0)
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

bool HubAirTrafficLayout::poseAt(int laneIndex, double s, double speedMps, HubAirSlotPose& out) const
{
    const HubAirLaneTrack* t = track(laneIndex);
    if (!t || !m_lanes)
        return false;
    const auto& lane = m_lanes->lanes()[static_cast<std::size_t>(laneIndex)];
    const std::size_t n = t->cumulative.size();
    s = std::clamp(s, 0.0, t->lengthMeters);
    std::size_t k = 1;
    while (k < n - 1 && t->cumulative[k] < s)
        ++k;
    const glm::dvec3 a = lane.stationsLocal[k - 1], b = lane.stationsLocal[k];
    const double segment = std::max(1.0e-6, t->cumulative[k] - t->cumulative[k - 1]);
    out.position = a + (b - a) * ((s - t->cumulative[k - 1]) / segment);
    glm::dvec3 forward = b - a;
    out.forward = glm::length(forward) > 1.0e-6 ? glm::normalize(forward) : glm::dvec3(0.0, 0.0, 1.0);
    out.up = glm::abs(out.forward.y) > 0.95 ? glm::dvec3(1.0, 0.0, 0.0) : glm::dvec3(0.0, t->upSign, 0.0);
    out.velocity = out.forward * speedMps;
    out.alongMeters = s;
    out.local = frameOf(out.position, -out.forward, out.up);   // nose -Z
    return true;
}

bool HubAirTrafficLayout::pose(const HubAirSlot& slot, double timeSeconds, HubAirSlotPose& out) const
{
    const HubAirLaneTrack* t = track(slot.lane);
    if (!t)
        return false;
    // the presentation's day-periodic clock (fmod 86400) keeps float precision: identical on server and client
    const double time = std::fmod(timeSeconds, 86400.0);
    const double raw = slot.phaseMeters + slot.speedMps * time;
    const double lap = std::floor(raw / t->lengthMeters);
    if (!poseAt(slot.lane, raw - lap * t->lengthMeters, slot.speedMps, out))
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
