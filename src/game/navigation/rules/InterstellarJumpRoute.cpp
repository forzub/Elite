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
} // namespace

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
