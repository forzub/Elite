#include "src/game/simulation/JDrive.h"
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

ZoneAnchor anchorFrom(const json& j)
{
    ZoneAnchor a;
    a.kind = j.value("kind", std::string());
    a.id = j.value("id", std::string());
    a.primary = j.value("primary", std::string());
    a.secondary = j.value("secondary", std::string());
    a.fraction = j.value("fraction", 0.5);
    return a;
}

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

ZoneCatalog loadZoneCatalog()
{
    ZoneCatalog cat;
    std::ifstream in("assets/data/navigation/route_zones.json");
    if (!in)
        in.open("src/assets/data/navigation/route_zones.json");
    if (!in)
        return cat;
    json j;
    try
    {
        in >> j;
    }
    catch (...)
    {
        return cat;
    }
    for (const auto& z : j.value("zones", json::array()))
    {
        RouteZone r;
        r.id = z.value("id", std::string());
        r.name = z.value("name", r.id);
        r.authority = z.value("authority", std::string());
        const std::string c = z.value("colour", std::string("red"));
        r.colour = c == "blue" ? ZoneColour::Blue : c == "green" ? ZoneColour::Green : ZoneColour::Red;
        r.systemId = z.value("system_id", 0);
        r.capsule = z.value("shape", std::string("sphere")) == "capsule";
        if (z.contains("anchor"))
            r.a = anchorFrom(z["anchor"]);
        if (z.contains("anchor_b"))
            r.b = anchorFrom(z["anchor_b"]);
        r.radiusM = z.value("radius_km", 0.0) * 1000.0;
        for (const auto& g : z.value("gate_between", json::array()))
            r.gateBetween.push_back(g.get<std::string>());
        r.fineCr = z.value("fine_cr", 0.0);
        cat.zones.push_back(std::move(r));
    }
    if (j.contains("watch"))
        cat.hubRadarM = j["watch"].value("hub_radar_km", 3000.0) * 1000.0;
    return cat;
}

bool ZoneGeometry::contains(const glm::dvec3& p) const
{
    return distance(p) < 0.0;
}

double ZoneGeometry::distance(const glm::dvec3& p) const
{
    return glm::length(p - closestOnSegment(a, b, p)) - radiusM;
}

std::vector<ZoneGeometry> resolveZones(const ZoneCatalog& catalog, int systemId, const AnchorResolver& resolve)
{
    std::vector<ZoneGeometry> out;
    for (const auto& z : catalog.zones)
    {
        if (z.systemId != systemId)
            continue;
        ZoneGeometry g;
        g.zone = &z;
        g.radiusM = z.radiusM;
        if (!resolve(z.a, g.a))
            continue;
        g.b = g.a;
        if (z.capsule && !resolve(z.b, g.b))
            continue;
        out.push_back(g);
    }
    return out;
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

        if (request.official)
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
            // blue gate between the bodies this leg goes between
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

double estimateLegChargeT(double distanceM, double massT)
{
    namespace jd = game::simulation::jdrive;
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
        return jd::jChargeRateTps(a, massT, jd::CobrettCoreRatingT) * 2.0 * tRamp;
    }
    return jd::jChargeRateTps(a, massT, jd::CobrettCoreRatingT) * t;
}

std::string encodeRoute(const PlannedRoute& route)
{
    std::ostringstream o;
    o.precision(17);
    for (std::size_t i = 0; i < route.legs.size(); ++i)
    {
        const RoutePoint& p = route.legs[i];
        if (i)
            o << '|';
        // ':' separates the fields; a landing spec "site:<id>" travels with '~' for its ':'
        std::string spec = p.landingSpec;
        std::replace(spec.begin(), spec.end(), ':', '~');
        if (p.kind == RoutePoint::Kind::Hub)
            o << "hub:" << p.hubId << ':' << (p.stop ? "stop" : "pass") << ':' << p.action;
        else if (p.kind == RoutePoint::Kind::Orbit)
            o << "orbit:" << p.bodyId << ':' << p.orbitAltitudeM << ':' << (p.stop ? "stop" : "pass") << ':' << p.action;
        else if (p.kind == RoutePoint::Kind::Land)
            o << "land:" << p.bodyId << ':' << spec << ':' << p.action;
        else if (p.kind == RoutePoint::Kind::Ship)
            o << "ship:" << p.shipEntityId << ':' << p.action;
        else
            o << "pt:" << p.worldM.x << ',' << p.worldM.y << ',' << p.worldM.z << ':' << (p.stop ? "stop" : "pass") << ':' << p.action;
    }
    return o.str();
}

std::vector<RoutePoint> decodeRoute(const std::string& wire)
{
    std::vector<RoutePoint> out;
    std::stringstream ss(wire);
    std::string item;
    while (std::getline(ss, item, '|'))
    {
        std::vector<std::string> f;
        std::stringstream is(item);
        std::string part;
        while (std::getline(is, part, ':'))
            f.push_back(part);
        if (f.empty())
            continue;
        RoutePoint p;
        if (f[0] == "hub" && f.size() >= 2)
        {
            p.kind = RoutePoint::Kind::Hub;
            p.hubId = f[1];
            p.stop = f.size() < 3 || f[2] == "stop";
            p.action = f.size() >= 4 ? f[3] : std::string();
        }
        else if (f[0] == "orbit" && f.size() >= 3)
        {
            p.kind = RoutePoint::Kind::Orbit;
            p.bodyId = f[1];
            p.orbitAltitudeM = std::atof(f[2].c_str());
            p.stop = f.size() < 4 || f[3] == "stop";
            p.action = f.size() >= 5 ? f[4] : std::string();
        }
        else if (f[0] == "ship" && f.size() >= 2)
        {
            p.kind = RoutePoint::Kind::Ship;
            p.shipEntityId = std::strtoull(f[1].c_str(), nullptr, 10);
            p.action = f.size() >= 3 ? f[2] : std::string();
            p.stop = true;
        }
        else if (f[0] == "land" && f.size() >= 2)
        {
            p.kind = RoutePoint::Kind::Land;
            p.bodyId = f[1];
            p.landingSpec = f.size() >= 3 ? f[2] : std::string();
            std::replace(p.landingSpec.begin(), p.landingSpec.end(), '~', ':');
            p.action = f.size() >= 4 ? f[3] : std::string();
            p.stop = true;
        }
        else if (f[0] == "pt" && f.size() >= 2)
        {
            p.kind = RoutePoint::Kind::Point;
            if (std::sscanf(f[1].c_str(), "%lf,%lf,%lf", &p.worldM.x, &p.worldM.y, &p.worldM.z) != 3)
                continue;
            p.stop = f.size() >= 3 && f[2] == "stop";
            p.action = f.size() >= 4 ? f[3] : std::string();
        }
        else
            continue;
        out.push_back(p);
    }
    return out;
}
} // namespace game::navigation::route
