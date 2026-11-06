#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// Route planner: start -> (intermediate points) -> finish, official or
// unofficial, as a list of legs the route autopilot flies on the J drive.
//
// Zones (assets/data/navigation/route_zones.json):
//   BLUE  - mandatory passage: an official flight between the gate's bodies
//           passes through it (inserted as a pass-through point);
//   RED   - forbidden: no route through it (gone round); entering it is an
//           administrative violation (fine), not a collision;
//   GREEN - preferred corridor: an official route between its ends follows it.
// Official = green corridors, blue gates, around red zones. Unofficial
// (smuggling) = away from every hub's radar / patrol coverage and the blue
// gates, around red zones, no corridors.
//
// Pure geometry on the world positions of this moment (the client resolves
// anchors); the autopilot keeps hubs / orbits exact as they move, pass-through
// points are guides.
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

struct RoutePoint
{
    enum class Kind { Hub, Orbit, Point, Land, Ship };
    Kind kind = Kind::Point;
    std::string hubId, bodyId, label;
    double orbitAltitudeM = 0.0;
    glm::dvec3 worldM {0.0};       // where it is now
    bool stop = false;             // stop here (J off) - hubs / finish; else pass through
    std::string action;            // at a stop: "refuel", "repair", "pause" (joined by '+')
    std::string landingSpec;       // Land: "" = a wild site, "site:<catalog id>" = a settlement pad
    std::uint64_t shipEntityId = 0; // Ship: the rendezvous target (server entity id)
    bool inserted = false;         // inserted by the planner (refuel stop)
};

// J charge of a leg of `distanceM` flown at the route drive (JDrive.h rule): the drive's time at its
// acceleration (a sqrt(d / a) profile, the 0.9 c cap) times its rate
double estimateLegChargeT(double distanceM, double massT);

struct PlanRequest
{
    bool official = true;
    glm::dvec3 startM {0.0};
    std::string startBodyId;              // the body whose sphere holds the start
    std::vector<RoutePoint> points;       // intermediates in order, then the finish (stop)
    std::vector<std::pair<std::string, glm::dvec3>> hubsM;   // every hub now (radar for unofficial)
    std::function<std::string(const glm::dvec3&)> bodyAt;  // the body whose sphere holds a point
};

struct PlannedRoute
{
    std::vector<RoutePoint> legs;          // flown in order
    std::vector<std::string> notes;        // why each extra point is there
    double lengthM = 0.0;
};

PlannedRoute planRoute(const PlanRequest& request, const std::vector<ZoneGeometry>& zones, double hubRadarM);

// Wire form for the server (ClientShipCommand::RouteLeg detail): legs separated by '|',
// "hub:<id>[:stop][:<action>]" | "orbit:<body>:<alt m>[:stop]" | "pt:<x>,<y>,<z>".
std::string encodeRoute(const PlannedRoute& route);
std::vector<RoutePoint> decodeRoute(const std::string& wire);
} // namespace game::navigation::route
