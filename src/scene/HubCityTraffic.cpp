#include "src/scene/HubCityTraffic.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/traffic/HubAirTraffic.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"

namespace scene
{
namespace
{
// the zones whose streets carry trucks (tools/models/units/hub_plane.py WHEELED + the dock block)
bool wheeledZone(const std::string& zone)
{
    return zone == "storage" || zone == "industry" || zone == "port" || zone == "logistics" || zone == "refuel" ||
           zone == "repair" || zone == "rig" || zone == "dock";
}

// a stable pseudo-random number in [0, 1) from integers (the same on every client)
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

glm::mat4 frame(const glm::dvec3& position, const glm::dvec3& forward, const glm::dvec3& up)
{
    const glm::dvec3 x = glm::normalize(glm::cross(up, forward));
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(glm::vec3(x), 0.0f);
    m[1] = glm::vec4(glm::vec3(up), 0.0f);
    m[2] = glm::vec4(glm::vec3(forward), 0.0f);
    m[3] = glm::vec4(glm::vec3(position), 1.0f);
    return m;
}

double distanceToSegment(const glm::dvec3& p, const glm::dvec3& a, const glm::dvec3& b)
{
    const glm::dvec3 ab = b - a;
    const double t = std::clamp(glm::dot(p - a, ab) / std::max(1.0e-9, glm::dot(ab, ab)), 0.0, 1.0);
    return glm::length(a + ab * t - p);
}

constexpr double CarSpacingPassenger = 95.0;    // m between the cars of a lane beside housing / downtown
constexpr double CarSpacingWheeled = 170.0;
constexpr double TruckSpacing = 210.0;
constexpr double CarSpeedInner = 22.0;          // m/s (~80 km/h)
constexpr double CarSpeedOuter = 17.0;
constexpr double TruckSpeed = 14.0;
constexpr double TrainSpeed = 28.0;
constexpr double TrainDwellSeconds = 25.0;
constexpr double TrainHalfLength = 29.0;
} // namespace

const char* hubTrafficModelId(HubTrafficModel model)
{
    switch (model)
    {
        case HubTrafficModel::CarA: return "hubt_car_a";
        case HubTrafficModel::CarB: return "hubt_car_b";
        case HubTrafficModel::CarC: return "hubt_car_c";
        case HubTrafficModel::Truck: return "hubt_truck";
        case HubTrafficModel::Train: return "hubt_train";
        case HubTrafficModel::PersonA: return "hubt_person_a";
        case HubTrafficModel::PersonB: return "hubt_person_b";
        case HubTrafficModel::PersonC: return "hubt_person_c";
        case HubTrafficModel::PersonDot: return "hubt_person_dot";
        case HubTrafficModel::Count: break;
    }
    return "hubt_car_a";
}

void hubCityMonorailLoop(const game::navigation::HubPlan& plan, double t, const glm::dvec3& camera, double r,
                         std::vector<HubTrafficInstance>& out);

// THE PEDESTRIANS (author 2026-11-22: «пешеходы - комичные, гротескные; издали точки, вблизи фигурки»): on the
// sidewalks of the road bridges (both sides of the plane; under it upside down) and waiting on the monorail platforms;
// each walks to and fro along its street at its own pace with a bouncy gait. Within PedFigureMeters a figure, out to
// PedRadiusMeters a lit speck, none beyond. A pure function of the time.
constexpr double PedRadiusMeters = 450.0;
constexpr double PedFigureMeters = 140.0;
void hubCityPedestrians(const game::navigation::HubPlan& plan, double t, const glm::dvec3& camera, std::vector<HubTrafficInstance>& out)
{
    const auto& roads = plan.roads();
    const auto emit = [&](const glm::dvec3& p, const glm::dvec3& forward, const glm::dvec3& up, std::uint32_t who) {
        const double dist = glm::length(p - camera);
        if (dist > PedRadiusMeters)
            return;
        HubTrafficInstance v;
        v.model = dist > PedFigureMeters ? HubTrafficModel::PersonDot
                                         : static_cast<HubTrafficModel>(static_cast<int>(HubTrafficModel::PersonA) + static_cast<int>(who % 3u));
        v.local = frame(p, forward, up);
        v.distance = static_cast<float>(dist);
        out.push_back(v);
    };
    const auto& streets = plan.streets();
    for (std::size_t si = 0; si < streets.size(); ++si)
    {
        const auto& st = streets[si];
        const glm::dvec2 a2 = st.a, b2 = st.b;
        const glm::dvec2 ab = b2 - a2;
        const double length = glm::length(ab);
        if (length < 50.0)
            continue;
        // the street far from the camera: nobody to draw
        const double u = std::clamp(glm::dot(glm::dvec2(camera.x, camera.z) - a2, ab) / (length * length), 0.0, 1.0);
        if (glm::length(a2 + ab * u - glm::dvec2(camera.x, camera.z)) > PedRadiusMeters + 50.0)
            continue;
        const glm::dvec3 along(ab.x / length, 0.0, ab.y / length);
        const glm::dvec3 side(-along.z, 0.0, along.x);
        for (int layer = 0; layer < 2; ++layer)
        {
            const double y = layer == 0 ? roads.topY : roads.underY;
            if (std::abs(camera.y - y) > PedRadiusMeters)
                continue;
            const glm::dvec3 up(0.0, layer == 0 ? 1.0 : -1.0, 0.0);
            for (int sd = -1; sd <= 1; sd += 2)
            {
                const std::uint32_t seed = static_cast<std::uint32_t>(si * 977u + static_cast<std::uint32_t>(layer) * 31u +
                                                                      static_cast<std::uint32_t>(sd + 1) * 7u);
                const int count = static_cast<int>(length / 24.0);
                for (int i = 0; i < count; ++i)
                {
                    const double h = hash01(seed, static_cast<std::uint32_t>(i));
                    const double pace = 0.9 + 0.9 * hash01(seed + 5u, static_cast<std::uint32_t>(i));   // 0.9..1.8 m/s
                    const double run = length - 20.0;
                    // to and fro: a triangle wave over 2 * run
                    double s = std::fmod(h * 2.0 * run + pace * t, 2.0 * run);
                    const bool back = s > run;
                    if (back)
                        s = 2.0 * run - s;
                    const double bob = 0.15 * std::abs(std::sin(t * pace * 5.5 + h * 40.0));   // the bouncy gait
                    const double lane = sd * (11.0 + 0.9 * hash01(seed + 9u, static_cast<std::uint32_t>(i)));
                    const glm::dvec3 p = glm::dvec3(a2.x, y, a2.y) + along * (10.0 + s) + side * lane + up * bob;
                    // a wobble of the heading (they meander)
                    const double wob = 0.25 * std::sin(t * 1.3 + h * 17.0);
                    const glm::dvec3 f = glm::normalize((back ? -along : along) + side * wob);
                    emit(p, f, up, seed + static_cast<std::uint32_t>(i));
                }
            }
        }
    }
    // the monorail platforms: a few waiting at each station, shuffling about
    for (std::size_t k = 0; k < roads.monorailStations.size(); ++k)
    {
        const glm::dvec2 st = roads.monorailStations[k];
        if (glm::length(glm::dvec2(camera.x, camera.z) - st) > PedRadiusMeters + 60.0)
            continue;
        // the station's line: along X if a loop side through it runs along X
        bool alongX = false;
        const auto& loop = roads.monorailLoop;
        for (std::size_t c = 0; c < loop.size(); ++c)
        {
            const glm::dvec2 a = loop[c], b = loop[(c + 1) % loop.size()];
            if (std::abs(a.y - b.y) < 1.0 && std::abs(st.y - a.y) < 1.0)
                alongX = true;
        }
        const glm::dvec3 along = alongX ? glm::dvec3(1.0, 0.0, 0.0) : glm::dvec3(0.0, 0.0, 1.0);
        const glm::dvec3 side = alongX ? glm::dvec3(0.0, 0.0, 1.0) : glm::dvec3(1.0, 0.0, 0.0);
        for (int i = 0; i < 10; ++i)
        {
            const std::uint32_t seed = 70001u + static_cast<std::uint32_t>(k) * 53u;
            const double h = hash01(seed, static_cast<std::uint32_t>(i));
            const double x = (h - 0.5) * 70.0 + 3.0 * std::sin(t * 0.3 + h * 20.0);
            const double sd = (i % 2) ? 1.0 : -1.0;
            const glm::dvec3 p = glm::dvec3(st.x, roads.monorailLoopY, st.y) + along * x + side * (sd * 6.5);
            const glm::dvec3 f = glm::normalize(-side * sd + along * (0.4 * std::sin(t * 0.7 + h * 9.0)));
            emit(p, f, glm::dvec3(0.0, 1.0, 0.0), seed + static_cast<std::uint32_t>(i));
        }
    }
}

void hubCityTraffic(const game::navigation::HubPlan& plan, double timeSeconds, const glm::vec3& cameraLocal, float radius,
                    std::vector<HubTrafficInstance>& out)
{
    out.clear();
    const auto& roads = plan.roads();
    // a ring hub: the streets are the plan's STRIP - the camera taken into it, the vehicles bent back (HubRingBend.h)
    const auto& ring = plan.ring();
    const glm::dvec3 camera = ring.toStrip(glm::dvec3(cameraLocal));
    const double r = radius;
    // the time folded to a day keeps the float maths of the stream phases exact (the streams repeat anyway)
    const double t = std::fmod(timeSeconds, 86400.0);
    hubCityMonorailLoop(plan, t, camera, r, out);
    static const bool pedestrians = [] {   // DEV A/B of the frame cost: ELITE_DEV_HUB_PEDESTRIANS=0
        const char* v = std::getenv("ELITE_DEV_HUB_PEDESTRIANS");
        return !(v && std::string(v) == "0");
    }();
    if (pedestrians)
        hubCityPedestrians(plan, t, camera, out);
    const auto& streets = plan.streets();
    for (std::size_t si = 0; si < streets.size(); ++si)
    {
        const auto& st = streets[si];
        const glm::dvec3 a(st.a.x, 0.0, st.a.y), b(st.b.x, 0.0, st.b.y);
        const double length = glm::length(b - a);
        if (length < 50.0)
            continue;
        if (distanceToSegment(glm::dvec3(camera.x, 0.0, camera.z), a, b) > r + std::abs(roads.underY) + 100.0)
            continue;
        const glm::dvec3 d = (b - a) / length;
        // the zones on both sides of the street: trucks beside the wheeled ones
        const glm::dvec3 mid = (a + b) * 0.5;
        const glm::dvec3 across(-d.z, 0.0, d.x);
        bool wheeled = false;
        for (const double s : {-400.0, 400.0})
            if (const auto* block = plan.blockAt(mid.x + across.x * s, mid.z + across.z * s))
                wheeled = wheeled || wheeledZone(block->zone);
        for (int layer = 0; layer < 2; ++layer)
        {
            const glm::dvec3 up(0.0, layer == 0 ? 1.0 : -1.0, 0.0);
            const double y = layer == 0 ? roads.topY : roads.underY;
            for (int way = 0; way < 2; ++way)
            {
                const glm::dvec3 forward = way == 0 ? d : -d;
                const glm::dvec3 start = way == 0 ? a : b;
                const glm::dvec3 right = glm::normalize(glm::cross(forward, up));
                for (std::size_t lane = 0; lane < roads.lanesMeters.size(); ++lane)
                {
                    const bool outer = lane + 1 == roads.lanesMeters.size() && lane > 0;
                    const bool trucks = outer && wheeled;
                    const double spacing = trucks ? TruckSpacing : wheeled ? CarSpacingWheeled : CarSpacingPassenger;
                    const double speed = trucks ? TruckSpeed : outer ? CarSpeedOuter : CarSpeedInner;
                    const std::uint32_t seed =
                        static_cast<std::uint32_t>(si * 131u + static_cast<std::uint32_t>(layer) * 17u +
                                                   static_cast<std::uint32_t>(way) * 7u + static_cast<std::uint32_t>(lane));
                    const int count = std::max(1, static_cast<int>(length / spacing));
                    const glm::dvec3 laneOrigin = start + right * roads.lanesMeters[lane] + glm::dvec3(0.0, y, 0.0);
                    for (int i = 0; i < count; ++i)
                    {
                        const double phase = (i + 0.6 * hash01(seed, static_cast<std::uint32_t>(i))) * spacing;
                        const double s = std::fmod(phase + speed * t, length);
                        const glm::dvec3 p = laneOrigin + forward * s;
                        const double dist = glm::length(p - camera);
                        if (dist > r)
                            continue;
                        HubTrafficInstance v;
                        v.model = trucks ? HubTrafficModel::Truck
                                         : static_cast<HubTrafficModel>(
                                               static_cast<int>(hash01(seed + 991u, static_cast<std::uint32_t>(i)) * 3.0));
                        v.local = frame(p, forward, up);
                        v.distance = static_cast<float>(dist);
                        out.push_back(v);
                    }
                }
            }
        }
        // the monorail: one train shuttling along the beam (a dwell at each end, its stations) [air traffic below]
        if (st.monorail)
        {
            const bool alongZ = std::abs(d.z) > std::abs(d.x);
            const glm::dvec3 side = alongZ ? glm::dvec3(1.0, 0.0, 0.0) : glm::dvec3(0.0, 0.0, -1.0);
            const glm::dvec3 lo = (alongZ ? (a.z < b.z ? a : b) : (a.x < b.x ? a : b)) + side * roads.monorailSideMeters;
            const glm::dvec3 axis = alongZ ? glm::dvec3(0.0, 0.0, 1.0) : glm::dvec3(1.0, 0.0, 0.0);
            const double run = std::max(1.0, length - 2.0 * TrainHalfLength);
            const double travel = run / TrainSpeed;
            const double cycle = 2.0 * (travel + TrainDwellSeconds);
            const double c = std::fmod(t + hash01(static_cast<std::uint32_t>(si), 4242u) * cycle, cycle);
            double s = 0.0;
            glm::dvec3 forward = axis;
            if (c < TrainDwellSeconds)
                s = 0.0;
            else if (c < TrainDwellSeconds + travel)
                s = (c - TrainDwellSeconds) * TrainSpeed;
            else if (c < 2.0 * TrainDwellSeconds + travel)
                s = run;
            else
            {
                s = run - (c - 2.0 * TrainDwellSeconds - travel) * TrainSpeed;
                forward = -axis;
            }
            if (c >= TrainDwellSeconds + travel)
                forward = -axis;
            const glm::dvec3 p = lo + axis * (TrainHalfLength + s) + glm::dvec3(0.0, roads.monorailY, 0.0);
            const double dist = glm::length(p - camera);
            if (dist <= r)
            {
                HubTrafficInstance v;
                v.model = HubTrafficModel::Train;
                v.local = frame(p, forward, glm::dvec3(0.0, 1.0, 0.0));
                v.distance = static_cast<float>(dist);
                out.push_back(v);
            }
        }
    }
    if (ring.valid)
        for (auto& v : out)
        {
            const glm::dvec3 p(v.local[3]);
            const glm::dmat3 B = ring.basisAt(p.x);
            const glm::dmat3 rot = B * glm::dmat3(glm::mat3(v.local));
            glm::mat4 m = glm::mat4(glm::mat3(rot));
            m[3] = glm::vec4(glm::vec3(ring.toFrame(p)), 1.0f);
            v.local = m;
        }
}

// THE MONORAIL LOOP: trains round the one closed line (author 2026-11-22: «монорельс - одна петля или зигзаг ...
// станции, поезда»): one train per ~3 km of the loop, all the same timetable - TrainSpeed between the stations, a
// TrainDwellSeconds stop at each, eased in / out over the last / first 60 m; a pure function of the time
void hubCityMonorailLoop(const game::navigation::HubPlan& plan, double t, const glm::dvec3& camera, double r,
                         std::vector<HubTrafficInstance>& out)
{
    const auto& roads = plan.roads();
    const auto& loop = roads.monorailLoop;
    if (loop.size() < 3)
        return;
    std::vector<double> cum(loop.size() + 1, 0.0);
    for (std::size_t k = 0; k < loop.size(); ++k)
        cum[k + 1] = cum[k] + glm::length(loop[(k + 1) % loop.size()] - loop[k]);
    const double perimeter = cum.back();
    if (perimeter < 100.0)
        return;
    const auto alongOf = [&](const glm::dvec2& p) {
        double best = 1.0e30, at = 0.0;
        for (std::size_t k = 0; k < loop.size(); ++k)
        {
            const glm::dvec2 a = loop[k], b = loop[(k + 1) % loop.size()];
            const glm::dvec2 ab = b - a;
            const double u = std::clamp(glm::dot(p - a, ab) / std::max(1.0e-9, glm::dot(ab, ab)), 0.0, 1.0);
            const double d = glm::length(a + ab * u - p);
            if (d < best)
            {
                best = d;
                at = cum[k] + u * glm::length(ab);
            }
        }
        return at;
    };
    std::vector<double> stops;
    for (const auto& s : roads.monorailStations)
        stops.push_back(alongOf(s));
    std::sort(stops.begin(), stops.end());
    if (stops.empty())
        stops.push_back(0.0);
    // the timetable of one lap: from each stop to the next (a run with 60 m ease in / out) + the dwell
    constexpr double Ease = 60.0;
    std::vector<double> legStart(stops.size() + 1, 0.0), legLength(stops.size(), 0.0);
    for (std::size_t k = 0; k < stops.size(); ++k)
    {
        double len = (k + 1 < stops.size() ? stops[k + 1] : stops[0] + perimeter) - stops[k];
        legLength[k] = len;
        // the run time: cruise + the two eased ends (half speed on average over Ease each)
        const double ease = std::min(Ease, 0.5 * len);
        const double run = (len - 2.0 * ease) / TrainSpeed + 2.0 * ease / (0.5 * TrainSpeed);
        legStart[k + 1] = legStart[k] + TrainDwellSeconds + run;
    }
    const double lapSeconds = legStart.back();
    const int trains = std::max(2, static_cast<int>(perimeter / 3000.0));
    for (int i = 0; i < trains; ++i)
    {
        const double c = std::fmod(t + lapSeconds * i / trains, lapSeconds);
        std::size_t k = 0;
        while (k + 1 < stops.size() && c >= legStart[k + 1])
            ++k;
        const double inLeg = c - legStart[k];
        double s = stops[k];
        if (inLeg > TrainDwellSeconds)
        {
            const double len = legLength[k];
            const double ease = std::min(Ease, 0.5 * len);
            double tau = inLeg - TrainDwellSeconds;
            const double easeT = ease / (0.5 * TrainSpeed);
            const double cruiseT = (len - 2.0 * ease) / TrainSpeed;
            double d = 0.0;
            if (tau < easeT)                                   // accelerating: d = a t^2 / 2, a = v / easeT
                d = 0.5 * (TrainSpeed / easeT) * tau * tau;
            else if (tau < easeT + cruiseT)
                d = ease + (tau - easeT) * TrainSpeed;
            else
            {
                const double rest = std::max(0.0, 2.0 * easeT + cruiseT - tau);
                d = len - 0.5 * (TrainSpeed / easeT) * rest * rest;
            }
            s += std::min(d, len);
        }
        s = std::fmod(s, perimeter);
        std::size_t seg = 0;
        while (seg + 1 < loop.size() && cum[seg + 1] <= s)
            ++seg;
        const glm::dvec2 a = loop[seg], b = loop[(seg + 1) % loop.size()];
        const double segLen = std::max(1.0e-6, cum[seg + 1] - cum[seg]);
        const glm::dvec2 p2 = a + (b - a) * ((s - cum[seg]) / segLen);
        const glm::dvec2 d2 = glm::normalize(b - a);
        const glm::dvec3 p(p2.x, roads.monorailLoopY, p2.y);
        const double dist = glm::length(p - camera);
        if (dist > r)
            continue;
        HubTrafficInstance v;
        v.model = HubTrafficModel::Train;
        v.local = frame(p, glm::dvec3(d2.x, 0.0, d2.y), glm::dvec3(0.0, 1.0, 0.0));
        v.distance = static_cast<float>(dist);
        out.push_back(v);
    }
}

void hubCityAirTraffic(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork& lanes,
                       double timeSeconds, const glm::vec3& cameraLocal, float radius, std::vector<HubAirTrafficInstance>& out,
                       const std::vector<std::uint32_t>* promotedSlots)
{
    out.clear();
    if (plan.airFleet().empty())
        return;
    // the slots are shared with the server (game::navigation::traffic::HubAirTrafficLayout): a slot the server promoted
    // to a real ship near a player is drawn as that ship, not here
    const auto& layout = game::navigation::traffic::hubAirTrafficLayoutCached(plan, lanes);
    const glm::dvec3 camera(cameraLocal);
    game::navigation::traffic::HubAirSlotPose pose;
    for (const auto& slot : layout.slots())
    {
        if (promotedSlots && std::find(promotedSlots->begin(), promotedSlots->end(), slot.id) != promotedSlots->end())
            continue;
        if (!layout.pose(slot, timeSeconds, pose))
            continue;
        const double dist = glm::length(pose.position - camera);
        if (dist > radius)
            continue;
        HubAirTrafficInstance v;
        v.fleetIndex = slot.fleetIndex;
        v.local = pose.local;
        v.distance = static_cast<float>(dist);
        out.push_back(v);
    }
}
} // namespace scene
