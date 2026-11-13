#include "src/game/navigation/rules/JumpSumps.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <mutex>

#include <nlohmann/json.hpp>

#include "src/game/navigation/rules/JumpRules.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/WorldSitesCatalog.h"

namespace game::navigation::jump
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
constexpr double EarthMassKg = 5.9722e24;
constexpr double TrojanHalfArcRad = 17.0 * Pi / 180.0;
constexpr int SegmentSamples = 400;

// radial profile 0..1: rises from inner to peak, falls to outer
double radialProfile(const JumpSump& s, double r)
{
    if (r <= s.innerM || r >= s.outerM)
        return 0.0;
    if (r <= s.peakM)
        return s.peakM > s.innerM ? (r - s.innerM) / (s.peakM - s.innerM) : 1.0;
    return s.outerM > s.peakM ? (s.outerM - r) / (s.outerM - s.peakM) : 1.0;
}

double angularProfile(const JumpSump& s, double azimuthRad)
{
    if (s.kind != "trojans")
        return 1.0;
    double best = 0.0;
    for (const double lagrange : {Pi / 3.0, -Pi / 3.0})
    {
        double d = std::remainder(azimuthRad - lagrange, 2.0 * Pi);
        best = std::max(best, std::exp(-(d * d) / (TrojanHalfArcRad * TrojanHalfArcRad)));
    }
    return best;
}

// mass of the profile with a unit peak density (numeric: radial x height x azimuth)
double profileVolumeM3(const JumpSump& s)
{
    double v = 0.0;
    constexpr int N = 400;
    const double dr = (s.outerM - s.innerM) / N;
    for (int i = 0; i < N; ++i)
    {
        const double r = s.innerM + (i + 0.5) * dr;
        v += radialProfile(s, r) * r * dr;
    }
    const double height = s.halfThicknessM * std::sqrt(Pi);
    double azimuth = 2.0 * Pi;
    if (s.kind == "trojans")
        azimuth = 2.0 * TrojanHalfArcRad * std::sqrt(Pi);   // two gaussian clouds
    return v * height * azimuth;
}

double numberOr(const nlohmann::json& j, const char* key, double fallback)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_number() ? it->get<double>() : fallback;
}
} // namespace

const std::vector<JumpSump>& jumpSumpsOfSystem(const world::celestial::CelestialSystemDefinition& system)
{
    static std::mutex lock;
    static std::unordered_map<int, std::vector<JumpSump>> cache;
    std::lock_guard<std::mutex> guard(lock);
    auto [it, inserted] = cache.try_emplace(system.systemId);
    if (!inserted)
        return it->second;
    for (const auto& b : system.bodies)
    {
        if (b.type != world::celestial::BodyType::AsteroidBelt || b.sourcePath.empty())
            continue;
        std::ifstream in(b.sourcePath);
        if (!in)
            continue;
        nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object())
            continue;
        JumpSump s;
        s.id = b.id;
        s.name = b.name.empty() ? b.id : b.name;
        s.kind = j.value("kind", std::string());
        if (s.kind != "asteroid_belt" && s.kind != "kuiper_belt" && s.kind != "debris_disk" && s.kind != "trojans")
            continue;
        s.innerM = numberOr(j, "inner_au", 0.0) * AuM;
        s.outerM = numberOr(j, "outer_au", 0.0) * AuM;
        s.peakM = std::clamp(numberOr(j, "peak_au", 0.0) * AuM, s.innerM, s.outerM);
        if (s.peakM <= s.innerM)
            s.peakM = 0.5 * (s.innerM + s.outerM);
        const double incl = numberOr(j, "inclination_mean_deg", 5.0) * Pi / 180.0;
        s.halfThicknessM = std::max(0.01 * s.peakM, s.peakM * std::sin(incl));
        s.massKg = numberOr(j, "total_mass_earth", 0.0) * EarthMassKg;
        if (s.outerM <= s.innerM || s.massKg <= 0.0)
            continue;
        // the swarm orbits the parent's parent for trojans (the planet's star), else its parent
        if (s.kind == "trojans")
        {
            s.planetId = b.parentId;
            for (const auto& p : system.bodies)
                if (p.id == b.parentId)
                    s.centreBodyId = p.parentId;
        }
        else
            s.centreBodyId = b.parentId;
        if (s.centreBodyId.empty())
            continue;
        s.densityScale = s.massKg / std::max(1.0, profileVolumeM3(s));
        it->second.push_back(std::move(s));
    }
    return it->second;
}

bool systemHasBeacons(int systemId)
{
    static world::celestial::WorldSitesCatalog sites;
    static const bool loaded = sites.loadFromRuntimeOrSource();
    return loaded && systemId >= 0 && !sites.sitesInSystem(systemId).empty();
}

double sumpDensityKgM3(const JumpSump& s, const glm::dvec3& p, const std::unordered_map<std::string, glm::dvec3>& centres)
{
    const auto c = centres.find(s.centreBodyId);
    if (c == centres.end())
        return 0.0;
    const glm::dvec3 local = p - c->second;
    const double r = std::sqrt(local.x * local.x + local.z * local.z);
    const double radial = radialProfile(s, r);
    if (radial <= 0.0)
        return 0.0;
    const double y = local.y / s.halfThicknessM;
    if (std::abs(y) > 4.0)
        return 0.0;
    double angular = 1.0;
    if (s.kind == "trojans")
    {
        const auto planet = centres.find(s.planetId);
        if (planet == centres.end())
            return 0.0;
        const glm::dvec3 pl = planet->second - c->second;
        const double azimuth = std::atan2(local.z, local.x) - std::atan2(pl.z, pl.x);
        angular = angularProfile(s, azimuth);
    }
    return s.densityScale * radial * angular * std::exp(-y * y);
}

std::vector<SumpCrossing> sumpCrossings(const std::vector<JumpSump>& sumps, const std::unordered_map<std::string, glm::dvec3>& centres,
                                        const glm::dvec3& from, const glm::dvec3& to, double lockFactor)
{
    std::vector<SumpCrossing> out;
    const double length = glm::length(to - from);
    if (length <= 0.0)
        return out;
    const double scale = SumpColumnKgM2 * std::sqrt(std::max(1.0, lockFactor));
    for (const auto& s : sumps)
    {
        double column = 0.0;
        for (int i = 0; i < SegmentSamples; ++i)
            column += sumpDensityKgM3(s, from + (to - from) * ((i + 0.5) / SegmentSamples), centres);
        column *= length / SegmentSamples;
        if (column <= 0.0)
            continue;
        out.push_back({s.id, s.name, column, 1.0 - std::exp(-column / scale)});
    }
    return out;
}

bool sumpCatch(const std::vector<JumpSump>& sumps, const std::unordered_map<std::string, glm::dvec3>& centres,
               const glm::dvec3& from, const glm::dvec3& to, double lockFactor, double e, glm::dvec3& point, const JumpSump*& sump)
{
    const double length = glm::length(to - from);
    if (length <= 0.0 || sumps.empty())
        return false;
    const double scale = SumpColumnKgM2 * std::sqrt(std::max(1.0, lockFactor));
    const double dl = length / SegmentSamples;
    double depth = 0.0;
    for (int i = 0; i < SegmentSamples; ++i)
    {
        const glm::dvec3 p = from + (to - from) * ((i + 0.5) / SegmentSamples);
        for (const auto& s : sumps)
        {
            depth += sumpDensityKgM3(s, p, centres) * dl / scale;
            if (depth >= e)
            {
                point = p;
                sump = &s;
                return true;
            }
        }
    }
    return false;
}
} // namespace game::navigation::jump
