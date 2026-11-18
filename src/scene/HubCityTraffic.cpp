#include "src/scene/HubCityTraffic.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

#include "src/game/navigation/HubPlanCatalog.h"
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
        case HubTrafficModel::Count: break;
    }
    return "hubt_car_a";
}

void hubCityTraffic(const game::navigation::HubPlan& plan, double timeSeconds, const glm::vec3& cameraLocal, float radius,
                    std::vector<HubTrafficInstance>& out)
{
    out.clear();
    const auto& roads = plan.roads();
    const glm::dvec3 camera(cameraLocal);
    const double r = radius;
    // the time folded to a day keeps the float maths of the stream phases exact (the streams repeat anyway)
    const double t = std::fmod(timeSeconds, 86400.0);
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
}

void hubCityAirTraffic(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork& lanes,
                       double timeSeconds, const glm::vec3& cameraLocal, float radius, std::vector<HubAirTrafficInstance>& out)
{
    out.clear();
    const auto& fleet = plan.airFleet();
    if (fleet.empty())
        return;
    const glm::dvec3 camera(cameraLocal);
    const double t = std::fmod(timeSeconds, 86400.0);
    constexpr double ShipsPerMeter = 1.0 / 3500.0;   // «не обязательно слишком много»: one per 3.5 km of lane
    std::vector<double> cum;
    for (std::size_t li = 0; li < lanes.lanes().size(); ++li)
    {
        const auto& lane = lanes.lanes()[li];
        std::size_t n = lane.stationsLocal.size();
        if (n < 2)
            continue;
        // the plaza ring runs 1.75 laps (merges / diverges): one lap is the loop a ship keeps flying
        if (lane.id.find(".plaza.") != std::string::npos)
            n = std::min<std::size_t>(n, 37);
        cum.assign(n, 0.0);
        for (std::size_t k = 1; k < n; ++k)
            cum[k] = cum[k - 1] + glm::length(lane.stationsLocal[k] - lane.stationsLocal[k - 1]);
        const double length = cum[n - 1];
        if (length < 200.0)
            continue;
        // the ships this lane carries: the fleet's hulls short enough for it, by weight
        double total = 0.0;
        for (const auto& s : fleet)
            if (s.lengthMeters <= lane.maxShipLengthMeters)
                total += s.weight;
        if (total <= 0.0)
            continue;
        const std::uint32_t seed = static_cast<std::uint32_t>(std::hash<std::string>{}(lane.id));
        const int count = std::max(1, static_cast<int>(std::lround(length * ShipsPerMeter * (lane.blue ? 0.5 : 1.0))));
        const double speed = 0.7 * lane.maxSpeedMps;
        const glm::dvec3 up(0.0, lane.stationsLocal[0].y < 0.0 ? -1.0 : 1.0, 0.0);
        for (int i = 0; i < count; ++i)
        {
            const double s = std::fmod((i + 0.7 * hash01(seed, static_cast<std::uint32_t>(i))) * length / count + speed * t, length);
            std::size_t k = 1;
            while (k < n - 1 && cum[k] < s)
                ++k;
            const glm::dvec3 a = lane.stationsLocal[k - 1], b = lane.stationsLocal[k];
            const double segment = std::max(1.0e-6, cum[k] - cum[k - 1]);
            const glm::dvec3 p = a + (b - a) * ((s - cum[k - 1]) / segment);
            const double dist = glm::length(p - camera);
            if (dist > radius)
                continue;
            glm::dvec3 forward = b - a;
            forward = glm::length(forward) > 1.0e-6 ? glm::normalize(forward) : glm::dvec3(0.0, 0.0, 1.0);
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
            HubAirTrafficInstance v;
            v.fleetIndex = index;
            v.local = frame(p, -forward, glm::abs(forward.y) > 0.95 ? glm::dvec3(1.0, 0.0, 0.0) : up);   // nose -Z
            v.distance = static_cast<float>(dist);
            out.push_back(v);
        }
    }
}
} // namespace scene
