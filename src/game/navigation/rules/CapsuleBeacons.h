#pragma once

// Capsule beacons (author 2026-11-24, memory cockpit-capsule-rescue; data assets/data/navigation/capsule_beacons.json):
// automatic radio buoys on the surveyed lanes. A capsule that HEARS one (within its hear radius) learns its position and
// the rescue service it belongs to (a hub) - it flies to that hub, or, when the hub is out of its reach, to the beacon
// itself to wait for the pick-up. Positions are anchored like the navigation zones (route::ZoneAnchor).

#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/game/navigation/route/RouteZones.h"

namespace game::navigation::beacons
{

struct CapsuleBeacon
{
    std::string id, name, rescueHubId, operatorName;
    int systemId = 0;
    route::ZoneAnchor anchor;
    double hearRadiusM = 0.0;
};

inline const std::vector<CapsuleBeacon>& capsuleBeacons()
{
    static const std::vector<CapsuleBeacon> all = [] {
        std::vector<CapsuleBeacon> out;
        std::ifstream in("assets/data/navigation/capsule_beacons.json");
        if (!in)
            in.open("src/assets/data/navigation/capsule_beacons.json");
        if (!in)
            return out;
        nlohmann::json j;
        try
        {
            in >> j;
        }
        catch (...)
        {
            return out;
        }
        for (const auto& b : j.value("beacons", nlohmann::json::array()))
        {
            CapsuleBeacon c;
            c.id = b.value("id", std::string());
            c.name = b.value("name", c.id);
            c.systemId = b.value("system_id", 0);
            c.rescueHubId = b.value("rescue_hub", std::string());
            c.operatorName = b.value("operator", std::string());
            c.hearRadiusM = b.value("hear_radius_km", 0.0) * 1000.0;
            if (b.contains("anchor"))
            {
                const auto& a = b["anchor"];
                c.anchor.kind = a.value("kind", std::string());
                c.anchor.id = a.value("id", std::string());
                c.anchor.primary = a.value("primary", std::string());
                c.anchor.secondary = a.value("secondary", std::string());
                c.anchor.fraction = a.value("fraction", 0.5);
            }
            if (!c.id.empty())
                out.push_back(std::move(c));
        }
        return out;
    }();
    return all;
}

} // namespace game::navigation::beacons
