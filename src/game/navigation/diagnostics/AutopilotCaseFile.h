#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipParamsRecord.h"

namespace game::navigation::diagnostics
{

// DEV diagnostic: one accepted autopilot start (route plan + start
// arguments + client physics) written by the game and replayed by the
// offline harness. Only the plan fields the route autopilot consumes are
// stored. Doubles use hex-float text so the replay is bit-exact.
struct AutopilotCase
{
    planner::RoutePlan plan;
    autopilot::AutopilotAgentState agent;
    ShipParams physics {};
    double acceptedAtUniverseTimeSeconds = 0.0;
    std::uint64_t requestSerial = 0;
    double trackingToleranceMeters = 0.0;
    glm::dvec3 routeUpReference {0.0};
    bool holdAtTerminal = true;
};

namespace autopilot_case_detail
{
inline void put(std::FILE* f, double v) { std::fprintf(f, " %a", v); }
inline void put(std::FILE* f, const glm::dvec3& v)
{
    put(f, v.x); put(f, v.y); put(f, v.z);
}
inline bool get(std::FILE* f, double& v) { return std::fscanf(f, " %la", &v) == 1; }
inline bool get(std::FILE* f, glm::dvec3& v)
{
    return get(f, v.x) && get(f, v.y) && get(f, v.z);
}
inline bool getCount(std::FILE* f, const char* tag, std::size_t& n)
{
    char word[32] = {};
    unsigned long long value = 0;
    if (std::fscanf(f, " %31s %llu", word, &value) != 2 ||
        std::strcmp(word, tag) != 0)
        return false;
    n = static_cast<std::size_t>(value);
    return true;
}
inline void putGates(
    std::FILE* f, const char* tag, const std::vector<planner::RouteGate>& gates)
{
    std::fprintf(f, "%s %zu\n", tag, gates.size());
    for (const auto& g : gates)
    {
        put(f, g.positionMeters); put(f, g.forward); put(f, g.speedMps);
        std::fputc('\n', f);
    }
}
inline bool getGates(
    std::FILE* f, const char* tag, std::vector<planner::RouteGate>& gates)
{
    std::size_t n = 0;
    if (!getCount(f, tag, n))
        return false;
    gates.resize(n);
    for (auto& g : gates)
        if (!get(f, g.positionMeters) || !get(f, g.forward) ||
            !get(f, g.speedMps))
            return false;
    return true;
}
// ShipParams values by name (physics_v2): exact (hex floats), one line per field
inline void putParamsValue(std::FILE* f, double v) { std::fprintf(f, " %a", v); }
inline void putParamsValue(std::FILE* f, float v) { std::fprintf(f, " %a", static_cast<double>(v)); }
inline void putParamsValue(std::FILE* f, bool v) { std::fprintf(f, " %d", v ? 1 : 0); }
inline void putParamsValue(std::FILE* f, int v) { std::fprintf(f, " %d", v); }
inline void putParamsValue(std::FILE* f, std::uint64_t v) { std::fprintf(f, " %llu", static_cast<unsigned long long>(v)); }
inline void putParamsValue(std::FILE* f, const glm::dvec3& v) { std::fprintf(f, " %a %a %a", v.x, v.y, v.z); }
inline bool getParamsValue(std::FILE* f, double& v) { return std::fscanf(f, " %la", &v) == 1; }
inline bool getParamsValue(std::FILE* f, float& v)
{
    double d = 0.0;
    if (std::fscanf(f, " %la", &d) != 1)
        return false;
    v = static_cast<float>(d);
    return true;
}
inline bool getParamsValue(std::FILE* f, bool& v)
{
    int i = 0;
    if (std::fscanf(f, " %d", &i) != 1)
        return false;
    v = i != 0;
    return true;
}
inline bool getParamsValue(std::FILE* f, int& v) { return std::fscanf(f, " %d", &v) == 1; }
inline bool getParamsValue(std::FILE* f, std::uint64_t& v)
{
    unsigned long long u = 0;
    if (std::fscanf(f, " %llu", &u) != 1)
        return false;
    v = static_cast<std::uint64_t>(u);
    return true;
}
inline bool getParamsValue(std::FILE* f, glm::dvec3& v)
{
    return std::fscanf(f, " %la %la %la", &v.x, &v.y, &v.z) == 3;
}

} // namespace autopilot_case_detail

inline bool writeAutopilotCase(const char* path, const AutopilotCase& c)
{
    using namespace autopilot_case_detail;
    std::FILE* f = std::fopen(path, "w");
    if (!f)
        return false;
    std::fprintf(f, "ELITE_AUTOPILOT_CASE 1\n");
    std::fprintf(f, "curves %zu\n", c.plan.routeCurves.size());
    for (const auto& s : c.plan.routeCurves)
    {
        std::fprintf(f, "%d", static_cast<int>(s.kind));
        put(f, s.startProgressMeters); put(f, s.endProgressMeters);
        put(f, s.maxSpeedMps);
        put(f, s.startMeters); put(f, s.endMeters);
        put(f, s.startForward); put(f, s.endForward);
        put(f, s.startUp); put(f, s.endUp);
        put(f, s.arcCenterMeters); put(f, s.arcNormal);
        put(f, s.arcRadiusMeters); put(f, s.arcSweepRadians);
        put(f, s.bezierControl1Meters); put(f, s.bezierControl2Meters);
        std::fprintf(f, " %zu", s.arcLengthKnots.size());
        for (const auto& k : s.arcLengthKnots)
        {
            put(f, k.parameter01); put(f, k.localProgressMeters);
        }
        std::fputc('\n', f);
    }
    putGates(f, "gates", c.plan.gates);
    putGates(f, "execution_gates", c.plan.executionGates);
    std::fprintf(f, "anchors %zu\n", c.plan.routeFrameAnchors.size());
    for (const auto& a : c.plan.routeFrameAnchors)
    {
        put(f, a.progressMeters); put(f, a.upReference);
        put(f, a.dynamicRollPhaseWeight);
        std::fputc('\n', f);
    }
    std::fprintf(f, "narrow %zu\n", c.plan.narrowZones.size());
    for (const auto& z : c.plan.narrowZones)
    {
        put(f, z.startProgressMeters); put(f, z.endProgressMeters);
        put(f, z.axisWorld); put(f, z.upWorld);
        put(f, z.lateralToleranceRightMeters);
        put(f, z.lateralToleranceUpMeters); put(f, z.maxSpeedMps);
        std::fputc('\n', f);
    }
    std::fprintf(f, "agent");
    put(f, c.agent.positionMapMeters); put(f, c.agent.velocityMapMetersPerSecond);
    put(f, c.agent.forwardMap); put(f, c.agent.rightMap); put(f, c.agent.upMap);
    put(f, c.agent.pitchRateRadPerSec); put(f, c.agent.yawRateRadPerSec);
    put(f, c.agent.rollRateRadPerSec);
    std::fprintf(f, "\nstart");
    put(f, c.acceptedAtUniverseTimeSeconds);
    std::fprintf(f, " %llu", static_cast<unsigned long long>(c.requestSerial));
    put(f, c.trackingToleranceMeters); put(f, c.routeUpReference);
    // physics_v2: ShipParams by field NAME (ShipParamsRecord.h), never the struct's bytes
    std::size_t fieldCount = 0;
    forEachShipParamsField(c.physics, [&](const char*, const auto&) { ++fieldCount; });
    std::fprintf(f, " %d\nphysics_v2 %zu", c.holdAtTerminal ? 1 : 0, fieldCount);
    forEachShipParamsField(c.physics, [&](const char* name, const auto& value) {
        std::fprintf(f, "\n%s", name);
        putParamsValue(f, value);
    });
    std::fprintf(f, "\nend\n");
    std::fclose(f);
    return true;
}

inline bool readAutopilotCase(const char* path, AutopilotCase& c)
{
    using namespace autopilot_case_detail;
    std::FILE* f = std::fopen(path, "r");
    if (!f)
        return false;
    bool ok = false;
    do
    {
        char magic[32] = {};
        int version = 0;
        if (std::fscanf(f, " %31s %d", magic, &version) != 2 ||
            std::strcmp(magic, "ELITE_AUTOPILOT_CASE") != 0 || version != 1)
            break;
        std::size_t n = 0;
        if (!getCount(f, "curves", n))
            break;
        c.plan = {};
        c.plan.disposition = planner::RoutePlanDisposition::Ready;
        c.plan.failureCode = planner::RoutePlanFailureCode::None;
        c.plan.routeCurves.resize(n);
        bool curvesOk = true;
        for (auto& s : c.plan.routeCurves)
        {
            int kind = 0;
            unsigned long long knots = 0;
            curvesOk = std::fscanf(f, " %d", &kind) == 1 &&
                get(f, s.startProgressMeters) && get(f, s.endProgressMeters) &&
                get(f, s.maxSpeedMps) && get(f, s.startMeters) &&
                get(f, s.endMeters) && get(f, s.startForward) &&
                get(f, s.endForward) && get(f, s.startUp) && get(f, s.endUp) &&
                get(f, s.arcCenterMeters) && get(f, s.arcNormal) &&
                get(f, s.arcRadiusMeters) && get(f, s.arcSweepRadians) &&
                get(f, s.bezierControl1Meters) &&
                get(f, s.bezierControl2Meters) &&
                std::fscanf(f, " %llu", &knots) == 1;
            if (!curvesOk)
                break;
            s.kind = static_cast<planner::RouteCurveKind>(kind);
            s.arcLengthKnots.resize(static_cast<std::size_t>(knots));
            for (auto& k : s.arcLengthKnots)
                curvesOk = curvesOk && get(f, k.parameter01) &&
                    get(f, k.localProgressMeters);
            if (!curvesOk)
                break;
        }
        if (!curvesOk || !getGates(f, "gates", c.plan.gates) ||
            !getGates(f, "execution_gates", c.plan.executionGates) ||
            !getCount(f, "anchors", n))
            break;
        c.plan.routeFrameAnchors.resize(n);
        bool anchorsOk = true;
        for (auto& a : c.plan.routeFrameAnchors)
            anchorsOk = anchorsOk && get(f, a.progressMeters) &&
                get(f, a.upReference) && get(f, a.dynamicRollPhaseWeight);
        char word[32] = {};
        if (!anchorsOk || std::fscanf(f, " %31s", word) != 1)
            break;
        if (std::strcmp(word, "narrow") == 0)
        {
            unsigned long long zones = 0;
            if (std::fscanf(f, " %llu", &zones) != 1)
                break;
            c.plan.narrowZones.resize(static_cast<std::size_t>(zones));
            bool zonesOk = true;
            for (auto& z : c.plan.narrowZones)
                zonesOk = zonesOk && get(f, z.startProgressMeters) &&
                    get(f, z.endProgressMeters) && get(f, z.axisWorld) &&
                    get(f, z.upWorld) &&
                    get(f, z.lateralToleranceRightMeters) &&
                    get(f, z.lateralToleranceUpMeters) &&
                    get(f, z.maxSpeedMps);
            if (!zonesOk || std::fscanf(f, " %31s", word) != 1)
                break;
        }
        if (std::strcmp(word, "agent") != 0)
            break;
        auto& g = c.agent;
        if (!get(f, g.positionMapMeters) ||
            !get(f, g.velocityMapMetersPerSecond) || !get(f, g.forwardMap) ||
            !get(f, g.rightMap) || !get(f, g.upMap) ||
            !get(f, g.pitchRateRadPerSec) || !get(f, g.yawRateRadPerSec) ||
            !get(f, g.rollRateRadPerSec))
            break;
        unsigned long long serial = 0;
        int hold = 0;
        if (std::fscanf(f, " %31s", word) != 1 ||
            std::strcmp(word, "start") != 0 ||
            !get(f, c.acceptedAtUniverseTimeSeconds) ||
            std::fscanf(f, " %llu", &serial) != 1 ||
            !get(f, c.trackingToleranceMeters) ||
            !get(f, c.routeUpReference) || std::fscanf(f, " %d", &hold) != 1)
            break;
        c.requestSerial = serial;
        c.holdAtTerminal = hold != 0;
        std::size_t size = 0;
        if (std::fscanf(f, " %31s", word) != 1)
            break;
        if (std::strcmp(word, "physics_v2") == 0)
        {
            // by name: unknown names (a removed field) are skipped, missing ones keep their defaults
            if (std::fscanf(f, " %zu", &size) != 1)
                break;
            bool fieldsOk = true;
            for (std::size_t i = 0; i < size && fieldsOk; ++i)
            {
                char name[96] = {};
                if (std::fscanf(f, " %95s", name) != 1)
                {
                    fieldsOk = false;
                    break;
                }
                bool known = false;
                forEachShipParamsField(c.physics, [&](const char* field, auto& value) {
                    if (!known && std::strcmp(field, name) == 0)
                    {
                        known = true;
                        fieldsOk = getParamsValue(f, value);
                    }
                });
                if (!known)
                {
                    // an unknown field: skip the rest of its line
                    int ch = 0;
                    while ((ch = std::fgetc(f)) != EOF && ch != '\n') {}
                }
            }
            ok = fieldsOk;
            break;
        }
        // LEGACY raw-byte cases ("physics <n> <hex>"): read only as a prefix of the CURRENT layout - valid only while
        // no field was inserted before the dumped size. Re-dump such cases as physics_v2.
        if (std::strcmp(word, "physics") != 0 || std::fscanf(f, " %zu", &size) != 1 || size > sizeof(ShipParams))
            break;
        auto* bytes = reinterpret_cast<unsigned char*>(&c.physics);
        bool bytesOk = true;
        for (std::size_t i = 0; i < size && bytesOk; ++i)
        {
            unsigned value = 0;
            bytesOk = std::fscanf(f, "%2x", &value) == 1;
            bytes[i] = static_cast<unsigned char>(value);
        }
        ok = bytesOk;
    } while (false);
    std::fclose(f);
    return ok;
}

} // namespace game::navigation::diagnostics
