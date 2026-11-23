#pragma once

// The continuous optimizer's request as a binary record: the game dumps the real ones (ELITE_ROUTE_DRAFT_DUMP=<file>,
// appended, one record per optimizer call) and EliteRouteDraftLab replays their scenes (the real hub obstacles in the
// planning frame) with sampled start / goal / ship states.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h"

namespace game::navigation::planner
{
namespace draft_io_detail
{
template <class T> void put(std::vector<char>& b, const T& v)
{
    const char* p = reinterpret_cast<const char*>(&v);
    b.insert(b.end(), p, p + sizeof(T));
}
template <class T> bool get(std::FILE* f, T& v) { return std::fread(&v, sizeof(T), 1, f) == 1; }
inline void putV(std::vector<char>& b, const glm::dvec3& v) { put(b, v.x); put(b, v.y); put(b, v.z); }
inline bool getV(std::FILE* f, glm::dvec3& v) { return get(f, v.x) && get(f, v.y) && get(f, v.z); }
constexpr std::uint32_t Magic = 0x52445231u;   // "RDR1"
} // namespace draft_io_detail

inline void appendRouteDraftRequest(const char* path, const assisted::lab::Request& r)
{
    using namespace draft_io_detail;
    std::vector<char> b;
    put(b, Magic);
    for (const auto* s : {&r.start, &r.goal})
    {
        putV(b, s->positionMeters);
        putV(b, s->tangent);
        put(b, s->speedMps);
    }
    for (double d : {r.hullRadiusMeters, r.designSpeedMps, r.acceleratingMps2, r.brakingMps2, r.maxLateralAccelerationMps2,
                     r.maxAngularVelocityRadPerSecond, r.maxAngularAccelerationRadPerSecond2, r.minimumComfortRadiusMeters,
                     r.protectedStartStraightMeters, r.protectedGoalStraightMeters})
        put(b, d);
    put(b, static_cast<std::uint32_t>(r.obstacles.size()));
    for (const auto& o : r.obstacles)
    {
        put(b, static_cast<std::uint8_t>(o.shape));
        putV(b, o.centerMeters);
        for (int c = 0; c < 3; ++c)
            putV(b, glm::dvec3(o.localToWorldBasis[c]));
        put(b, o.radiusMeters);
        putV(b, o.halfExtentsMeters);
        put(b, o.capsuleHalfLengthMeters);
        put(b, o.requiredClearanceMeters);
    }
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    if (std::FILE* f = std::fopen(path, "ab"))
    {
        std::fwrite(b.data(), 1, b.size(), f);
        std::fclose(f);
    }
}

inline std::vector<assisted::lab::Request> readRouteDraftRequests(const char* path)
{
    using namespace draft_io_detail;
    std::vector<assisted::lab::Request> out;
    std::FILE* f = std::fopen(path, "rb");
    if (!f)
        return out;
    for (;;)
    {
        std::uint32_t magic = 0;
        if (!get(f, magic) || magic != Magic)
            break;
        assisted::lab::Request r;
        bool ok = true;
        for (auto* s : {&r.start, &r.goal})
            ok = ok && getV(f, s->positionMeters) && getV(f, s->tangent) && get(f, s->speedMps);
        for (double* d : {&r.hullRadiusMeters, &r.designSpeedMps, &r.acceleratingMps2, &r.brakingMps2,
                          &r.maxLateralAccelerationMps2, &r.maxAngularVelocityRadPerSecond,
                          &r.maxAngularAccelerationRadPerSecond2, &r.minimumComfortRadiusMeters,
                          &r.protectedStartStraightMeters, &r.protectedGoalStraightMeters})
            ok = ok && get(f, *d);
        std::uint32_t n = 0;
        ok = ok && get(f, n);
        for (std::uint32_t i = 0; ok && i < n; ++i)
        {
            world::navigation::NavigationObstacle o;
            std::uint8_t shape = 0;
            glm::dvec3 c0, c1, c2;
            ok = get(f, shape) && getV(f, o.centerMeters) && getV(f, c0) && getV(f, c1) && getV(f, c2) &&
                 get(f, o.radiusMeters) && getV(f, o.halfExtentsMeters) && get(f, o.capsuleHalfLengthMeters) &&
                 get(f, o.requiredClearanceMeters);
            o.shape = static_cast<world::navigation::NavigationObstacleShape>(shape);
            o.localToWorldBasis = glm::dmat3(c0, c1, c2);
            r.obstacles.push_back(std::move(o));
        }
        if (!ok)
            break;
        out.push_back(std::move(r));
    }
    std::fclose(f);
    return out;
}
} // namespace game::navigation::planner
