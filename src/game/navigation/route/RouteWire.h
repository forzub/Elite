#pragma once

// The wire form of a planned route (client planner -> RouteLeg command -> server): the legs and
// their encode / decode. Neutral: the server includes this, never the client planner
// (architecture audit 2026-11-06, gate check_manual_docking_advisory & co).
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::route
{

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

// "hub:<id>:stop|pass:<action>" | "orbit:<body>:<alt m>:stop|pass:<action>" | "land:<body>:<spec ':'->'~'>:<action>"
// | "ship:<entity>:<action>" | "pt:x,y,z:stop|pass:<action>", legs joined by '|'
std::string encodeRouteLegs(const std::vector<RoutePoint>& legs);
std::vector<RoutePoint> decodeRoute(const std::string& wire);

} // namespace game::navigation::route
