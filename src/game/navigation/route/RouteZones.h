#pragma once

// Navigation zones (assets/data/navigation/route_zones.json): blue gates, red forbidden zones, green
// corridors - their catalog and their world geometry now. Neutral: the client planner draws routes
// with them, the SERVER judges violations with them (architecture audit 2026-11-06: fines are
// authoritative, not client memory).
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::route
{
enum class ZoneColour { Blue, Red, Green };

struct ZoneAnchor
{
    std::string kind;        // hub | body | lagrange
    std::string id;          // hub / body id
    std::string primary, secondary;
    double fraction = 0.5;
};

struct RouteZone
{
    std::string id, name, authority;
    ZoneColour colour = ZoneColour::Red;
    int systemId = 0;
    bool capsule = false;
    ZoneAnchor a, b;
    double radiusM = 0.0;
    std::vector<std::string> gateBetween;   // blue: the bodies the gate stands between
    double fineCr = 0.0;
};

struct ZoneCatalog
{
    std::vector<RouteZone> zones;
    double hubRadarM = 3.0e6;
};

// Loads assets/data/navigation/route_zones.json (or src/assets/...). Empty on failure.
ZoneCatalog loadZoneCatalog();

// World geometry of a zone now (false: an anchor is unknown)
using AnchorResolver = std::function<bool(const ZoneAnchor&, glm::dvec3& worldM)>;
struct ZoneGeometry
{
    const RouteZone* zone = nullptr;
    glm::dvec3 a {0.0}, b {0.0};
    double radiusM = 0.0;
    bool contains(const glm::dvec3& p) const;
    double distance(const glm::dvec3& p) const;   // to the zone's surface (< 0 inside)
};
std::vector<ZoneGeometry> resolveZones(const ZoneCatalog& catalog, int systemId, const AnchorResolver& resolve);

} // namespace game::navigation::route
