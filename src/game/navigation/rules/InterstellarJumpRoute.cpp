#include "src/game/navigation/rules/InterstellarJumpRoute.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>

#include "src/game/navigation/rules/JumpRules.h"
#include "src/world/celestial/StarAtlasDatabase.h"

#include <nlohmann/json.hpp>

namespace game::navigation::jump
{
namespace
{
constexpr double GravitationalConstant = 6.6743e-11;

struct Mass
{
    std::string name;
    int systemId = -1;     // a catalogue system (excluded when it is the start or the target)
    glm::dvec3 positionLy {0.0};
    double radiusLy = 0.0;
};

// the charted masses of an atlas, built once per atlas
const std::vector<Mass>& chartedMasses(const world::celestial::StarAtlasDatabase& atlas)
{
    static std::mutex lock;
    static std::unordered_map<const void*, std::vector<Mass>> cache;
    std::lock_guard<std::mutex> guard(lock);
    auto [it, inserted] = cache.try_emplace(&atlas);
    if (!inserted)
        return it->second;
    auto& out = it->second;
    const auto addSystems = [&](const std::vector<world::celestial::StarSystemSummary>& systems) {
        for (const auto& s : systems)
        {
            double massKg = 0.0;
            if (const auto* def = atlas.findSystem(s.id))
                for (const auto& b : def->bodies)
                    if (b.type == world::celestial::BodyType::Star)
                        massKg += b.massKg;
            if (massKg <= 0.0)
                massKg = 1.989e30 * std::max(1, s.starsCount);   // no detail: a solar mass per star
            out.push_back({s.name, s.id, s.positionLy, gravityCaptureRadiusLy(GravitationalConstant * massKg)});
        }
    };
    addSystems(atlas.systems());
    addSystems(atlas.distantSystems());
    for (const auto& o : atlas.objects())
    {
        const auto& p = o.properties;
        if (!p.is_object() || !p.value("gameplay_gravity", false) || !p.contains("mu_m3s2"))
            continue;
        double mu = p.value("mu_m3s2", 0.0);
        if (p.contains("companion_mass_kg"))
            mu += GravitationalConstant * p.value("companion_mass_kg", 0.0);
        const double r = gravityCaptureRadiusLy(mu);
        if (r * MetersPerLy < 10.0 * AuM)
            continue;   // a rogue moon or a comet: its sphere is smaller than the step's own error - nothing to catch
        out.push_back({o.name.empty() ? o.id : o.name, -1, o.positionLy, r});
    }
    return out;
}

double segmentDistanceLy(const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& c)
{
    const glm::dvec3 ab = b - a;
    const double len2 = glm::dot(ab, ab);
    const double t = len2 > 0.0 ? std::clamp(glm::dot(c - a, ab) / len2, 0.0, 1.0) : 0.0;
    return glm::length(a + ab * t - c);
}
struct Swarm
{
    std::string name;
    int placeId = -1;
    glm::dvec3 centreLy {0.0};
    glm::dmat3 toUnit {1.0};   // world ly offset -> unit-sphere coordinates
};

const std::vector<Swarm>& swarms(const world::celestial::StarAtlasDatabase& atlas)
{
    static std::mutex lock;
    static std::unordered_map<const void*, std::vector<Swarm>> cache;
    std::lock_guard<std::mutex> guard(lock);
    auto [it, inserted] = cache.try_emplace(&atlas);
    if (!inserted)
        return it->second;
    for (const auto& place : atlas.deepSpacePlaces())
    {
        const auto* o = atlas.deepSpaceObject(place.id);
        if (!o)
            continue;
        Swarm s;
        s.name = o->name.empty() ? o->id : o->name;
        s.placeId = place.id;
        s.centreLy = o->positionLy;
        const auto& p = o->properties;
        if (o->objectType == "icy_debris_field" && p.contains("geometry"))
        {
            const auto& g = p["geometry"];
            glm::dvec3 radii(0.3);
            if (g.contains("radii_ly") && g["radii_ly"].is_array() && g["radii_ly"].size() == 3)
                radii = glm::dvec3(g["radii_ly"][0].get<double>(), g["radii_ly"][1].get<double>(), g["radii_ly"][2].get<double>());
            glm::dmat3 rot(1.0);   // local -> world (columns = the local axes in world)
            if (g.contains("orientation_quat_wxyz") && g["orientation_quat_wxyz"].is_array() && g["orientation_quat_wxyz"].size() == 4)
            {
                const auto& q = g["orientation_quat_wxyz"];
                const double w = q[0].get<double>(), x = q[1].get<double>(), y = q[2].get<double>(), z = q[3].get<double>();
                rot = glm::dmat3(glm::dvec3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)),
                                 glm::dvec3(2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)),
                                 glm::dvec3(2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)));
            }
            const glm::dmat3 scale(glm::dvec3(1.0 / std::max(1e-6, radii.x), 0, 0), glm::dvec3(0, 1.0 / std::max(1e-6, radii.y), 0),
                                   glm::dvec3(0, 0, 1.0 / std::max(1e-6, radii.z)));
            s.toUnit = scale * glm::transpose(rot);
        }
        else
        {
            const double r = std::max(1.0, p.value("extent_au", 10.0)) * AuM / MetersPerLy;
            s.toUnit = glm::dmat3(1.0 / r);
        }
        it->second.push_back(std::move(s));
    }
    return it->second;
}

// the part of a segment inside a swarm's unit sphere; integral of (1 - |x(t)|^2) dt x the world length (ly)
struct Chord
{
    double A = 0, B = 0, C = 0, len = 0, lo = 0, hi = 0;
    bool any = false;
    double integral(double a, double b) const
    {
        const auto F = [&](double t) { return (1.0 - C) * t - B * t * t / 2.0 - A * t * t * t / 3.0; };
        return (F(b) - F(a)) * len;
    }
};

Chord chordOf(const Swarm& s, const glm::dvec3& a, const glm::dvec3& b)
{
    Chord c;
    const glm::dvec3 x0 = s.toUnit * (a - s.centreLy);
    const glm::dvec3 d = s.toUnit * (b - a);
    c.A = glm::dot(d, d);
    c.B = 2.0 * glm::dot(x0, d);
    c.C = glm::dot(x0, x0);
    c.len = glm::length(b - a);
    if (c.A <= 0.0)
        return c;
    const double disc = c.B * c.B - 4.0 * c.A * (c.C - 1.0);
    if (disc <= 0.0)
        return c;
    const double q = std::sqrt(disc);
    c.lo = std::max(0.0, (-c.B - q) / (2.0 * c.A));
    c.hi = std::min(1.0, (-c.B + q) / (2.0 * c.A));
    c.any = c.hi > c.lo;
    return c;
}
constexpr double YearS = 365.25 * 86400.0;
constexpr int ShellSamples = 600;

double shellProfile(const ApproachShell& s, double r)
{
    if (r <= s.innerLy || r >= s.outerLy)
        return 0.0;
    if (r <= s.peakLy)
        return (r - s.innerLy) / std::max(1e-12, s.peakLy - s.innerLy);
    return (s.outerLy - r) / std::max(1e-12, s.outerLy - s.peakLy);
}

// the part [lo, hi] of the segment a..b inside the shell's outer sphere
bool shellSpan(const ApproachShell& s, const glm::dvec3& a, const glm::dvec3& b, double& lo, double& hi)
{
    const glm::dvec3 d = b - a, x0 = a - s.centreLy;
    const double A = glm::dot(d, d), B = 2.0 * glm::dot(x0, d), C = glm::dot(x0, x0) - s.outerLy * s.outerLy;
    if (A <= 0.0)
        return false;
    const double disc = B * B - 4.0 * A * C;
    if (disc <= 0.0)
        return false;
    const double q = std::sqrt(disc);
    lo = std::max(0.0, (-B - q) / (2.0 * A));
    hi = std::min(1.0, (-B + q) / (2.0 * A));
    return hi > lo;
}

const char* const ShellFiles[] = {"assets/data/galaxy/jump_approach_shells.json", "../assets/data/galaxy/jump_approach_shells.json",
                                  "../src/assets/data/galaxy/jump_approach_shells.json", "src/assets/data/galaxy/jump_approach_shells.json"};

glm::dvec3 vec3Of(const nlohmann::json& j, const glm::dvec3& fallback)
{
    if (!j.is_array() || j.size() != 3)
        return fallback;
    return glm::dvec3(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
}

double shellDepthTau(const ApproachShell& s, const glm::dvec3& a, const glm::dvec3& b, double lo, double hi, double t);
} // namespace

const std::vector<ApproachShell>& approachShells(const world::celestial::StarAtlasDatabase& atlas)
{
    static std::mutex lock;
    static std::unordered_map<const void*, std::vector<ApproachShell>> cache;
    std::lock_guard<std::mutex> guard(lock);
    auto [it, inserted] = cache.try_emplace(&atlas);
    if (!inserted)
        return it->second;
    for (const char* path : ShellFiles)
    {
        std::ifstream in(path);
        if (!in)
            continue;
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.contains("shells"))
            continue;
        const double epoch = j.value("epochUniverseS", 0.0);
        constexpr double DegToRad = 3.14159265358979323846 / 180.0;
        const double auLy = AuM / MetersPerLy;
        for (const auto& e : j["shells"])
        {
            ApproachShell s;
            s.systemId = e.value("systemId", -1);
            const auto* sys = atlas.findSystemSummary(s.systemId);
            if (!sys)
                continue;
            s.name = e.value("name", sys->name);
            s.cause = e.value("cause", std::string());
            s.centreLy = sys->positionLy;
            s.innerLy = e.value("innerAu", 0.0) * auLy;
            s.peakLy = e.value("peakAu", 0.0) * auLy;
            s.outerLy = e.value("outerAu", 0.0) * auLy;
            s.tau = e.value("tau", 0.0);
            s.epochUniverseS = epoch;
            if (s.outerLy <= s.innerLy || s.tau <= 0.0)
                continue;
            if (e.contains("corridors"))
                for (const auto& c : e["corridors"])
                {
                    ApproachCorridor k;
                    k.name = c.value("name", std::string());
                    k.dir = glm::normalize(vec3Of(c.contains("dir") ? c["dir"] : nlohmann::json(), glm::dvec3(1.0, 0.0, 0.0)));
                    k.axis = glm::normalize(vec3Of(c.contains("axis") ? c["axis"] : nlohmann::json(), glm::dvec3(0.0, 0.0, 1.0)));
                    k.halfWidthRad = c.value("halfWidthDeg", 1.0) * DegToRad;
                    k.driftRadPerS = c.value("driftDegPerYear", 0.0) * DegToRad / YearS;
                    s.corridors.push_back(k);
                }
            it->second.push_back(std::move(s));
        }
        std::cerr << "[Jump] approach shells: " << it->second.size() << " from " << path << std::endl;
        break;
    }
    return it->second;
}

const ApproachShell* approachShellOf(const world::celestial::StarAtlasDatabase& atlas, int systemId)
{
    for (const auto& s : approachShells(atlas))
        if (s.systemId == systemId)
            return &s;
    return nullptr;
}

glm::dvec3 corridorDirAt(const ApproachShell& shell, const ApproachCorridor& c, double universeTimeS)
{
    // Rodrigues: dir turned about the axis (perpendicular to it) by the drift since the epoch
    const double a = c.driftRadPerS * (universeTimeS - shell.epochUniverseS);
    return glm::normalize(c.dir * std::cos(a) + glm::cross(c.axis, c.dir) * std::sin(a) +
                          c.axis * glm::dot(c.axis, c.dir) * (1.0 - std::cos(a)));
}

double approachShellFactor(const ApproachShell& shell, const glm::dvec3& pointLy, double universeTimeS)
{
    const glm::dvec3 rel = pointLy - shell.centreLy;
    const double r = glm::length(rel);
    const double radial = shellProfile(shell, r);
    if (radial <= 0.0)
        return 0.0;
    const glm::dvec3 u = rel / r;
    double f = radial;
    for (const auto& c : shell.corridors)
    {
        const double angle = std::acos(std::clamp(glm::dot(u, corridorDirAt(shell, c, universeTimeS)), -1.0, 1.0)) / c.halfWidthRad;
        f *= 1.0 - std::exp(-angle * angle);
    }
    return f;
}

namespace
{
// tau units: one radial crossing of the whole shell outside the corridors = tau
double shellDepthTau(const ApproachShell& s, const glm::dvec3& a, const glm::dvec3& b, double lo, double hi, double t)
{
    const double len = glm::length(b - a) * (hi - lo);
    const double half = 0.5 * (s.outerLy - s.innerLy);
    double sum = 0.0;
    for (int i = 0; i < ShellSamples; ++i)
        sum += approachShellFactor(s, a + (b - a) * (lo + (hi - lo) * (i + 0.5) / ShellSamples), t);
    return s.tau * sum * len / ShellSamples / std::max(1e-12, half);
}
} // namespace

std::vector<InterstellarSumpCrossing> interstellarSumpCrossings(const world::celestial::StarAtlasDatabase& atlas,
                                                                const InterstellarRoute& route, double lockFactor,
                                                                double universeTimeS)
{
    std::vector<InterstellarSumpCrossing> out;
    const double scale = InterstellarSumpScaleLy * std::sqrt(std::max(1.0, lockFactor));
    for (const auto& s : approachShells(atlas))
    {
        double tau = 0.0;
        for (std::size_t i = 1; i < route.pointsLy.size(); ++i)
        {
            double lo = 0.0, hi = 0.0;
            if (shellSpan(s, route.pointsLy[i - 1], route.pointsLy[i], lo, hi))
                tau += shellDepthTau(s, route.pointsLy[i - 1], route.pointsLy[i], lo, hi, universeTimeS);
        }
        if (tau > 0.0)
        {
            InterstellarSumpCrossing c;
            c.name = s.name;
            c.placeId = world::celestial::StarAtlasDatabase::approachIdOf(s.systemId);
            c.shell = true;
            c.depth = tau * InterstellarSumpScaleLy;
            c.probability = 1.0 - std::exp(-c.depth / scale);
            out.push_back(c);
        }
    }
    for (const auto& s : swarms(atlas))
    {
        double depth = 0.0;
        for (std::size_t i = 1; i < route.pointsLy.size(); ++i)
        {
            const Chord c = chordOf(s, route.pointsLy[i - 1], route.pointsLy[i]);
            if (c.any)
                depth += c.integral(c.lo, c.hi);
        }
        if (depth > 0.0)
            out.push_back({s.name, s.placeId, false, depth, 1.0 - std::exp(-depth / scale)});
    }
    return out;
}

InterstellarSumpCatch interstellarSumpCatch(const world::celestial::StarAtlasDatabase& atlas, const InterstellarRoute& route,
                                            double lockFactor, double e, double universeTimeS)
{
    InterstellarSumpCatch out;
    const double scale = InterstellarSumpScaleLy * std::sqrt(std::max(1.0, lockFactor));
    double need = e * scale;   // the depth (ly) at which the step is caught
    const auto& all = swarms(atlas);
    const auto& shells = approachShells(atlas);
    for (std::size_t i = 1; i < route.pointsLy.size(); ++i)
    {
        const glm::dvec3 a = route.pointsLy[i - 1], b = route.pointsLy[i];
        struct Met
        {
            double lo;
            const Swarm* swarm;
            const ApproachShell* shell;
            double hi;
        };
        std::vector<Met> met;   // the swarms and the shells on this leg in the order the line meets them
        for (const auto& s : all)
            if (const Chord c = chordOf(s, a, b); c.any)
                met.push_back({c.lo, &s, nullptr, c.hi});
        for (const auto& s : shells)
            if (double lo = 0.0, hi = 0.0; shellSpan(s, a, b, lo, hi))
                met.push_back({lo, nullptr, &s, hi});
        std::sort(met.begin(), met.end(), [](const auto& x, const auto& y) { return x.lo < y.lo; });
        for (const auto& entry : met)
        {
            if (entry.shell)
            {
                // walk the samples to the catch point
                const ApproachShell& sh = *entry.shell;
                const double half = 0.5 * (sh.outerLy - sh.innerLy);
                const double dl = glm::length(b - a) * (entry.hi - entry.lo) / ShellSamples;
                for (int k = 0; k < ShellSamples; ++k)
                {
                    const glm::dvec3 p = a + (b - a) * (entry.lo + (entry.hi - entry.lo) * (k + 0.5) / ShellSamples);
                    need -= sh.tau * approachShellFactor(sh, p, universeTimeS) * dl / std::max(1e-12, half) * InterstellarSumpScaleLy;
                    if (need <= 0.0)
                    {
                        out.caught = true;
                        out.shell = true;
                        out.placeId = world::celestial::StarAtlasDatabase::approachIdOf(sh.systemId);
                        out.name = sh.name;
                        out.pointLy = p;
                        out.centreLy = sh.centreLy;
                        return out;
                    }
                }
                continue;
            }
            const Swarm* s = entry.swarm;
            const Chord c = chordOf(*s, a, b);
            const double total = c.integral(c.lo, c.hi);
            if (total < need)
            {
                need -= total;
                continue;
            }
            double lo = c.lo, hi = c.hi;   // bisect the catch point
            for (int k = 0; k < 60; ++k)
            {
                const double mid = 0.5 * (lo + hi);
                if (c.integral(c.lo, mid) < need)
                    lo = mid;
                else
                    hi = mid;
            }
            out.caught = true;
            out.placeId = s->placeId;
            out.name = s->name;
            out.pointLy = a + (b - a) * (0.5 * (lo + hi));
            out.centreLy = s->centreLy;
            return out;
        }
    }
    return out;
}

double gravityCaptureRadiusLy(double muM3s2)
{
    return std::sqrt(std::max(0.0, muM3s2) / GalacticFieldMps2) / MetersPerLy;
}

glm::dvec3 jumpStartPointLy(const world::celestial::StarAtlasDatabase& atlas, int systemId, const glm::dvec3& shipPositionM)
{
    const auto* s = atlas.findSystemSummary(systemId);
    if (!s)
        return glm::dvec3(0.0);
    if (world::celestial::StarAtlasDatabase::isDeepSpace(systemId))
        return s->positionLy + shipPositionM / MetersPerLy;
    return s->positionLy;
}

InterstellarRoute planInterstellarRoute(const world::celestial::StarAtlasDatabase& atlas, int fromSystemId, int toSystemId,
                                        const glm::dvec3* startLy)
{
    InterstellarRoute route;
    const auto* from = atlas.findSystemSummary(fromSystemId);
    const auto* to = atlas.findSystemSummary(toSystemId);
    if (!from || !to)
        return route;
    const auto& masses = chartedMasses(atlas);
    // from a system's approaches its own star is the start's neighbour, not an obstacle
    const int fromStar = world::celestial::StarAtlasDatabase::isApproach(fromSystemId)
                             ? world::celestial::StarAtlasDatabase::systemOfApproach(fromSystemId)
                             : fromSystemId;
    // the first mass a segment passes inside the capture sphere of (closest to the segment's start)
    const auto blockerOf = [&](const glm::dvec3& a, const glm::dvec3& b) -> const Mass* {
        const Mass* best = nullptr;
        double bestAlong = 0.0;
        for (const auto& m : masses)
        {
            if (m.systemId == fromSystemId || m.systemId == fromStar || m.systemId == toSystemId)
                continue;
            if (segmentDistanceLy(a, b, m.positionLy) >= m.radiusLy)
                continue;
            const double along = glm::dot(m.positionLy - a, b - a);
            if (!best || along < bestAlong)
            {
                best = &m;
                bestAlong = along;
            }
        }
        return best;
    };
    route.pointsLy = {startLy ? *startLy : from->positionLy};
    const glm::dvec3 target = to->positionLy;
    for (int pass = 0; pass < 6; ++pass)
    {
        const glm::dvec3 a = route.pointsLy.back();
        const Mass* m = blockerOf(a, target);
        if (!m)
        {
            route.ok = true;
            break;
        }
        // the correction point beside the mass, on the side the line already passes
        const glm::dvec3 line = glm::normalize(target - a);
        glm::dvec3 side = (a + line * glm::dot(m->positionLy - a, line)) - m->positionLy;
        if (glm::length(side) < 1.0e-9)
            side = glm::cross(line, glm::dvec3(0.0, 0.0, 1.0));
        if (glm::length(side) < 1.0e-9)
            side = glm::cross(line, glm::dvec3(0.0, 1.0, 0.0));
        const glm::dvec3 w = m->positionLy + glm::normalize(side) * m->radiusLy * InterstellarDetourFactor;
        // the leg to the correction point must itself be clear
        if (blockerOf(a, w))
            break;
        route.pointsLy.push_back(w);
        route.avoidedNames.push_back(m->name);
    }
    route.pointsLy.push_back(target);
    for (std::size_t i = 1; i < route.pointsLy.size(); ++i)
        route.lengthLy += glm::length(route.pointsLy[i] - route.pointsLy[i - 1]);
    return route;
}

std::vector<LaneRecord> parseLaneRecords(const std::string& text)
{
    std::vector<LaneRecord> out;
    std::size_t pos = 0;
    while (pos < text.size())
    {
        std::size_t end = text.find(';', pos);
        if (end == std::string::npos)
            end = text.size();
        const std::string item = text.substr(pos, end - pos);
        pos = end + 1;
        LaneRecord r;
        char source[128] = {0};
        if (std::sscanf(item.c_str(), "%d,%d,%lf,%lf,%lf,%lf,%lf,%127[^;]", &r.systemId, &r.corridor, &r.surveyUniverseS, &r.dir.x,
                        &r.dir.y, &r.dir.z, &r.errorDeg, source) >= 7 &&
            glm::length(r.dir) > 0.0)
        {
            r.dir = glm::normalize(r.dir);
            r.source = source;
            out.push_back(r);
        }
    }
    return out;
}

std::string formatLaneRecords(const std::vector<LaneRecord>& records)
{
    std::string out;
    for (const auto& r : records)
    {
        char b[320];
        std::string src = r.source;
        for (char& ch : src)
            if (ch == ',' || ch == ';')
                ch = ' ';
        std::snprintf(b, sizeof b, "%d,%d,%.1f,%.9f,%.9f,%.9f,%.4f,%s", r.systemId, r.corridor, r.surveyUniverseS, r.dir.x, r.dir.y,
                      r.dir.z, r.errorDeg, src.c_str());
        out += (out.empty() ? "" : ";") + std::string(b);
    }
    return out;
}

double laneExpectedOffset(const ApproachShell& shell, const LaneRecord& r, double universeTimeS)
{
    if (r.corridor < 1 || r.corridor > static_cast<int>(shell.corridors.size()))
        return 1.0e9;
    const auto& c = shell.corridors[static_cast<std::size_t>(r.corridor - 1)];
    constexpr double DegToRad = 3.14159265358979323846 / 180.0;
    const double ageS = std::max(0.0, universeTimeS - r.surveyUniverseS);
    return (r.errorDeg * DegToRad + c.driftRadPerS * ageS) / std::max(1e-12, c.halfWidthRad);
}

LaneState laneState(const ApproachShell& shell, const LaneRecord& r, double universeTimeS)
{
    const double k = laneExpectedOffset(shell, r, universeTimeS);
    if (k < 0.3)
        return LaneState::Known;
    if (k < 1.0)
        return LaneState::Estimate;
    if (k < 2.5)
        return LaneState::Outdated;
    return LaneState::Unknown;
}

const char* laneStateText(LaneState s)
{
    switch (s)
    {
    case LaneState::Known: return "известна";
    case LaneState::Estimate: return "оценка";
    case LaneState::Outdated: return "устарела";
    default: return "неизвестна";
    }
}

double laneExpectedRisk(const ApproachShell& shell, const LaneRecord& r, double universeTimeS, double lockFactor)
{
    const double k = laneExpectedOffset(shell, r, universeTimeS);
    const double g = 1.0 - std::exp(-k * k);   // the cone's factor at the expected offset (the other corridors ignored)
    return 1.0 - std::exp(-shell.tau * g / std::sqrt(std::max(1.0, lockFactor)));
}

std::vector<LaneUse> applyLaneRecords(const world::celestial::StarAtlasDatabase& atlas, InterstellarRoute& route, int fromSystemId,
                                      int toSystemId, const std::vector<LaneRecord>& records, double universeTimeS)
{
    std::vector<LaneUse> used;
    if (route.pointsLy.size() < 2)
        return used;
    const auto best = [&](int systemId, LaneUse& out) {
        const ApproachShell* shell = approachShellOf(atlas, systemId);
        if (!shell)
            return false;
        bool any = false;
        double bestK = 1.0e18;
        for (const auto& r : records)
        {
            if (r.systemId != systemId || laneState(*shell, r, universeTimeS) == LaneState::Unknown)
                continue;
            const double k = laneExpectedOffset(*shell, r, universeTimeS);
            if (k < bestK)
            {
                bestK = k;
                out.shell = shell;
                out.record = r;
                out.state = laneState(*shell, r, universeTimeS);
                any = true;
            }
        }
        return any;
    };
    // the start's own shell: only from the system itself (from its approaches the ship is already in the shell)
    LaneUse start;
    if (!world::celestial::StarAtlasDatabase::isDeepSpace(fromSystemId) && fromSystemId != toSystemId && best(fromSystemId, start))
    {
        start.arrival = false;
        route.pointsLy.insert(route.pointsLy.begin() + 1, start.shell->centreLy + start.record.dir * start.shell->outerLy * LaneEntryFactor);
        route.avoidedNames.insert(route.avoidedNames.begin(), "выход по трассе " + start.shell->corridors[start.record.corridor - 1].name);
        used.push_back(start);
    }
    LaneUse arrive;
    if (fromSystemId != toSystemId && best(toSystemId, arrive))
    {
        // the entry point; from inside the shell (its approaches) only when the ship is not already deeper than it
        const glm::dvec3 entry = arrive.shell->centreLy + arrive.record.dir * arrive.shell->outerLy * LaneEntryFactor;
        route.pointsLy.insert(route.pointsLy.end() - 1, entry);
        route.avoidedNames.push_back("вход по трассе " + arrive.shell->corridors[arrive.record.corridor - 1].name);
        used.push_back(arrive);
    }
    if (!used.empty())
    {
        route.lengthLy = 0.0;
        for (std::size_t i = 1; i < route.pointsLy.size(); ++i)
            route.lengthLy += glm::length(route.pointsLy[i] - route.pointsLy[i - 1]);
    }
    return used;
}
} // namespace game::navigation::jump
