#include "src/game/navigation/route/RouteWire.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace game::navigation::route
{

std::string encodeRouteLegs(const std::vector<RoutePoint>& legs)
{
    std::ostringstream o;
    o.precision(17);
    for (std::size_t i = 0; i < legs.size(); ++i)
    {
        const RoutePoint& p = legs[i];
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
