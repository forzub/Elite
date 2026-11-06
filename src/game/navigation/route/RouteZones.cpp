#include "src/game/navigation/route/RouteZones.h"

#include <algorithm>
#include <cmath>
#include <fstream>

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

} // namespace game::navigation::route
