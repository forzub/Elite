#include "src/game/navigation/rules/InterstellarJumpRoute.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

#include "src/game/navigation/rules/JumpRules.h"
#include "src/world/celestial/StarAtlasDatabase.h"

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
} // namespace

std::vector<InterstellarSumpCrossing> interstellarSumpCrossings(const world::celestial::StarAtlasDatabase& atlas,
                                                                const InterstellarRoute& route, double lockFactor)
{
    std::vector<InterstellarSumpCrossing> out;
    const double scale = InterstellarSumpScaleLy * std::sqrt(std::max(1.0, lockFactor));
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
            out.push_back({s.name, s.placeId, depth, 1.0 - std::exp(-depth / scale)});
    }
    return out;
}

InterstellarSumpCatch interstellarSumpCatch(const world::celestial::StarAtlasDatabase& atlas, const InterstellarRoute& route,
                                            double lockFactor, double e)
{
    InterstellarSumpCatch out;
    const double scale = InterstellarSumpScaleLy * std::sqrt(std::max(1.0, lockFactor));
    double need = e * scale;   // the depth (ly) at which the step is caught
    const auto& all = swarms(atlas);
    for (std::size_t i = 1; i < route.pointsLy.size(); ++i)
    {
        const glm::dvec3 a = route.pointsLy[i - 1], b = route.pointsLy[i];
        std::vector<std::pair<double, const Swarm*>> met;   // the swarms on this leg in the order the line meets them
        for (const auto& s : all)
            if (const Chord c = chordOf(s, a, b); c.any)
                met.push_back({c.lo, &s});
        std::sort(met.begin(), met.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
        for (const auto& entry : met)
        {
            const Swarm* s = entry.second;
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

InterstellarRoute planInterstellarRoute(const world::celestial::StarAtlasDatabase& atlas, int fromSystemId, int toSystemId)
{
    InterstellarRoute route;
    const auto* from = atlas.findSystemSummary(fromSystemId);
    const auto* to = atlas.findSystemSummary(toSystemId);
    if (!from || !to)
        return route;
    const auto& masses = chartedMasses(atlas);
    // the first mass a segment passes inside the capture sphere of (closest to the segment's start)
    const auto blockerOf = [&](const glm::dvec3& a, const glm::dvec3& b) -> const Mass* {
        const Mass* best = nullptr;
        double bestAlong = 0.0;
        for (const auto& m : masses)
        {
            if (m.systemId == fromSystemId || m.systemId == toSystemId)
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
    route.pointsLy = {from->positionLy};
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
} // namespace game::navigation::jump
