#include "src/game/navigation/rules/JDrive.h"
#include <algorithm>
#include "src/game/navigation/route/RoutePlanner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace game::navigation::route
{
namespace
{
using json = nlohmann::json;


glm::dvec3 closestOnSegment(const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& p)
{
    const glm::dvec3 ab = b - a;
    const double len2 = glm::dot(ab, ab);
    const double t = len2 > 0.0 ? std::clamp(glm::dot(p - a, ab) / len2, 0.0, 1.0) : 0.0;
    return a + ab * t;
}

glm::dvec3 anyPerpendicular(const glm::dvec3& v)
{
    return glm::normalize(glm::cross(v, std::abs(v.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0)));
}

RoutePoint passPoint(const glm::dvec3& p, const std::string& label)
{
    RoutePoint r;
    r.kind = RoutePoint::Kind::Point;
    r.worldM = p;
    r.label = label;
    return r;
}

struct Obstacle
{
    glm::dvec3 a, b;
    double radiusM;
    std::string label;
};

// detour round every obstacle the polyline cuts (a few passes, each inserting
// a point on the far side of the obstacle's surface from the cut)
void avoid(std::vector<glm::dvec3>& path, std::vector<std::string>& labels, const std::vector<Obstacle>& obstacles,
           std::vector<std::string>& notes)
{
    for (int pass = 0; pass < 6; ++pass)
    {
        bool changed = false;
        for (std::size_t i = 0; i + 1 < path.size() && !changed; ++i)
            for (const Obstacle& o : obstacles)
            {
                // the obstacle's nearest point to the segment, then the segment's nearest to it
                const glm::dvec3 c = o.a == o.b ? o.a : closestOnSegment(o.a, o.b, 0.5 * (path[i] + path[i + 1]));
                const glm::dvec3 q = closestOnSegment(path[i], path[i + 1], c);
                if (glm::length(q - c) >= o.radiusM)
                    continue;
                // an end inside the obstacle (a start / finish there): cannot avoid - keep
                if (glm::length(path[i] - c) < o.radiusM || glm::length(path[i + 1] - c) < o.radiusM)
                    continue;
                glm::dvec3 out = q - c;
                if (glm::length(out) < 1.0)
                    out = anyPerpendicular(path[i + 1] - path[i]);
                const glm::dvec3 detour = c + glm::normalize(out) * o.radiusM * 1.3;
                path.insert(path.begin() + static_cast<std::ptrdiff_t>(i + 1), detour);
                labels.insert(labels.begin() + static_cast<std::ptrdiff_t>(i + 1), "обход: " + o.label);
                notes.push_back("обход: " + o.label);
                changed = true;
                break;
            }
        if (!changed)
            return;
    }
}
} // namespace

std::vector<glm::dvec3> corridorPath(const glm::dvec3& a, const glm::dvec3& b, const std::vector<BodyObstacle>& bodies)
{
    std::vector<glm::dvec3> path {a, b};
    std::vector<std::string> labels {"", ""};
    std::vector<Obstacle> obstacles;
    for (const BodyObstacle& o : bodies)
        obstacles.push_back(Obstacle {o.centerM, o.centerM, o.clearM, o.name});
    std::vector<std::string> notes;
    avoid(path, labels, obstacles, notes);
    return path;
}

PlannedRoute planRoute(const PlanRequest& request, const std::vector<ZoneGeometry>& zones, double hubRadarM)
{
    PlannedRoute route;
    glm::dvec3 cur = request.startM;
    std::string curBody = request.startBodyId;
    for (const RoutePoint& target : request.points)
    {
        const std::string targetBody = request.bodyAt ? request.bodyAt(target.worldM) : std::string();
        std::vector<glm::dvec3> path {cur};
        std::vector<std::string> labels {""};

        bool onCorridor = false;
        if (request.official)
        {
            // a corridor of the scheduled network between the leg's ends: follow its path
            for (const CorridorPath& c : request.corridors)
            {
                if (c.points.size() < 2)
                    continue;
                const double near = std::max(c.radiusM * 3.0, 50000.0);
                const bool forward = glm::length(c.points.front() - cur) < near && glm::length(c.points.back() - target.worldM) < near;
                const bool backward = glm::length(c.points.back() - cur) < near && glm::length(c.points.front() - target.worldM) < near;
                if (!forward && !backward)
                    continue;
                std::vector<glm::dvec3> inner(c.points.begin() + 1, c.points.end() - 1);
                if (backward)
                    std::reverse(inner.begin(), inner.end());
                for (const auto& p : inner)
                {
                    path.push_back(p);
                    labels.push_back("зелёный коридор сети: " + c.name);
                }
                route.notes.push_back("по коридору сети: " + c.name);
                onCorridor = true;
                break;
            }
        }
        if (request.official && !onCorridor)
        {
            // green corridor running this way: both of its ends near the leg's ends
            for (const ZoneGeometry& g : zones)
            {
                if (g.zone->colour != ZoneColour::Green || !g.zone->capsule)
                    continue;
                const double near = std::max(g.radiusM * 3.0, 50000.0);
                const bool forward = glm::length(g.a - cur) < near && glm::length(g.b - target.worldM) < near;
                const bool backward = glm::length(g.b - cur) < near && glm::length(g.a - target.worldM) < near;
                if (forward || backward)
                {
                    // fly the corridor axis: enter a little after its start, leave before its end
                    const glm::dvec3 s = forward ? g.a : g.b, e = forward ? g.b : g.a;
                    path.push_back(s + (e - s) * 0.15);
                    labels.push_back("зелёный коридор: " + g.zone->name);
                    path.push_back(s + (e - s) * 0.85);
                    labels.push_back("зелёный коридор: " + g.zone->name);
                    route.notes.push_back("по зелёному коридору: " + g.zone->name);
                    break;
                }
            }
        }
        if (request.official)
        {
            // blue gate between the bodies this leg goes between (on a network corridor too)
            if (!curBody.empty() && !targetBody.empty() && curBody != targetBody)
                for (const ZoneGeometry& g : zones)
                {
                    if (g.zone->colour != ZoneColour::Blue)
                        continue;
                    const auto& gb = g.zone->gateBetween;
                    const bool between = std::find(gb.begin(), gb.end(), curBody) != gb.end() &&
                                         std::find(gb.begin(), gb.end(), targetBody) != gb.end();
                    if (between)
                    {
                        path.push_back(g.a);
                        labels.push_back("синий створ: " + g.zone->name);
                        route.notes.push_back("обязательный пролёт: " + g.zone->name);
                    }
                }
        }
        path.push_back(target.worldM);
        labels.push_back(target.label);

        // obstacles: red zones always; unofficial also every hub's radar and the blue gates
        std::vector<Obstacle> obstacles;
        for (const ZoneGeometry& g : zones)
            if (g.zone->colour == ZoneColour::Red || (!request.official && g.zone->colour == ZoneColour::Blue))
                obstacles.push_back(Obstacle {g.a, g.b, g.radiusM,
                                              (g.zone->colour == ZoneColour::Red ? "красная зона " : "синий створ (без досмотра) ") +
                                                  g.zone->name});
        for (const BodyObstacle& b : request.bodies)
            obstacles.push_back(Obstacle {b.centerM, b.centerM, b.clearM, b.name});
        if (!request.official)
            for (const auto& [hubId, p] : request.hubsM)
            {
                // the start and the destination hubs cannot be avoided
                if (glm::length(p - cur) < hubRadarM || glm::length(p - target.worldM) < hubRadarM)
                    continue;
                obstacles.push_back(Obstacle {p, p, hubRadarM, "радар хаба " + hubId});
            }
        avoid(path, labels, obstacles, route.notes);

        for (std::size_t i = 1; i + 1 < path.size(); ++i)
            route.legs.push_back(passPoint(path[i], labels[i]));
        route.legs.push_back(target);
        for (std::size_t i = 1; i < path.size(); ++i)
            route.lengthM += glm::length(path[i] - path[i - 1]);
        cur = target.worldM;
        curBody = targetBody;
    }
    return route;
}

double estimateLegChargeT(double distanceM, double massT, double coreRatingT)
{
    namespace jd = game::navigation::jdrive;
    const double a = jd::MaxAccelMps2;
    // accelerate to the middle (or to the cap), brake the rest
    double t = 2.0 * std::sqrt(std::max(0.0, distanceM) / a);
    const double vPeak = a * t * 0.5;
    if (vPeak > jd::SpeedCapMps)
    {
        const double tRamp = jd::SpeedCapMps / a;
        const double dRamp = jd::SpeedCapMps * tRamp;   // both ramps
        t = 2.0 * tRamp + (distanceM - dRamp) / jd::SpeedCapMps;
        // the coast at the cap is not driven: only the ramps burn
        return jd::jChargeRateTps(a, massT, coreRatingT) * 2.0 * tRamp;
    }
    return jd::jChargeRateTps(a, massT, coreRatingT) * t;
}

} // namespace game::navigation::route
