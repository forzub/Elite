#pragma once

// Surface settlements on solid bodies (author 2026-11-05 cc): the deterministic layout of a
// settlement around its catalog point - one source for the terrain (the flattened pads and
// footprints, server and client alike), the landing autopilot (its pads) and the renderer (the
// structures). Pure function of the catalog site (id, type, population, lat / lon) and the body.
//
// Airless rocky bodies (the Moon): an entrance into the ground (the settlement lives under it -
// meteorites), two or three small domes, an antenna mast and a row of landing pads beside it.
// Small bodies (asteroids): a passage into the rock and a pad. Bodies with an atmosphere use the
// same layout until their own style exists (only shuttles land there).
//
// Player role (every element needs one): the pads are where the player lands to trade, take
// missions or rescue; the entrance / domes / antenna mark the settlement from the approach.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/WorldSitesCatalog.h"

namespace world::celestial::settlements
{

enum class StructureKind : std::uint8_t
{
    Pad,          // a landing pad (flat disc, markings, lights)
    Entrance,     // the portal into the underground settlement
    Dome,         // a small pressurised dome
    Antenna,      // a communication mast
    RockPassage   // a small body: the passage cut into the rock
};

struct Structure
{
    StructureKind kind = StructureKind::Pad;
    glm::dvec3 bodyDirection {0.0, 1.0, 0.0};   // body-fixed (prime, north, east)
    double radiusM = 0.0;                         // footprint radius
    double heightM = 0.0;                         // above the ground
    double headingRad = 0.0;                      // bearing of the structure's axis (from north)
    double flattenRadiusM = 0.0;                  // the terrain is flat out to here
    int number = 0;                               // pads: 1, 2, 3 ...
};

struct SettlementLayout
{
    std::string siteId;
    std::string name;
    std::string bodyId;
    glm::dvec3 centreDirection {0.0, 1.0, 0.0};
    std::vector<Structure> structures;

    const Structure* pad(int number = 1) const
    {
        for (const auto& s : structures)
            if (s.kind == StructureKind::Pad && s.number == number)
                return &s;
        return nullptr;
    }
};

// the convention of the System Map and the landing autopilot ("pad:<lat>,<lon>")
inline glm::dvec3 bodyDirectionFromLatLon(double latDeg, double lonDeg)
{
    const double la = glm::radians(latDeg), lo = glm::radians(lonDeg);
    return glm::dvec3(std::cos(la) * std::cos(lo), std::sin(la), std::cos(la) * std::sin(lo));
}

inline std::uint64_t layoutHash(std::string_view text)
{
    std::uint64_t h = 1469598103934665603ull;
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ull;
    return h;
}

// 0..1, the k-th deterministic number of a hash (splitmix64)
inline double layoutUnit(std::uint64_t h, int k)
{
    std::uint64_t z = h + 0x9E3779B97F4A7C15ull * static_cast<std::uint64_t>(k + 1);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
}

// the direction `distanceM` from `dir` along the surface at a bearing (0 = north, pi / 2 = east)
inline glm::dvec3 offsetDirection(const glm::dvec3& dir, double bodyRadiusM, double distanceM, double bearingRad)
{
    const glm::dvec3 up = glm::normalize(dir);
    glm::dvec3 north = glm::dvec3(0.0, 1.0, 0.0) - up * up.y;
    if (glm::length(north) < 1.0e-9)
        north = glm::dvec3(1.0, 0.0, 0.0) - up * up.x;   // at a pole: any meridian
    north = glm::normalize(north);
    const glm::dvec3 east = glm::normalize(glm::cross(north, up));
    const glm::dvec3 along = north * std::cos(bearingRad) + east * std::sin(bearingRad);
    const double angle = distanceM / std::max(1.0, bodyRadiusM);
    return glm::normalize(up * std::cos(angle) + along * std::sin(angle));
}

inline SettlementLayout layoutSurfaceSettlement(const WorldSite& site, double bodyRadiusM)
{
    constexpr double Pi = 3.14159265358979323846;
    SettlementLayout out;
    out.siteId = site.id;
    out.name = site.name;
    out.bodyId = site.parentBodyId;
    out.centreDirection = bodyDirectionFromLatLon(site.latitudeDeg, site.longitudeDeg);
    const std::uint64_t h = layoutHash(site.id);
    const double heading = layoutUnit(h, 0) * 2.0 * Pi;   // the settlement's axis
    const auto at = [&](double distanceM, double bearing) {
        return offsetDirection(out.centreDirection, bodyRadiusM, distanceM, bearing);
    };
    const bool smallBody = bodyRadiusM < 400000.0;
    if (smallBody)
    {
        out.structures.push_back({StructureKind::RockPassage, out.centreDirection, 18.0, 24.0, heading, 30.0, 0});
    }
    else
    {
        out.structures.push_back({StructureKind::Entrance, out.centreDirection, 20.0, 9.0, heading, 34.0, 0});
        const int domes = 2 + (layoutUnit(h, 1) > 0.5 ? 1 : 0);
        for (int i = 0; i < domes; ++i)
        {
            const double bearing = heading + Pi * 0.5 + (i - (domes - 1) * 0.5) * 0.55 + (layoutUnit(h, 10 + i) - 0.5) * 0.2;
            const double radius = 10.0 + 12.0 * layoutUnit(h, 20 + i);
            out.structures.push_back({StructureKind::Dome, at(75.0 + 50.0 * layoutUnit(h, 30 + i), bearing), radius, radius * 0.62,
                                      bearing, radius + 8.0, 0});
        }
        out.structures.push_back({StructureKind::Antenna, at(120.0, heading + Pi * 1.1), 4.0, 48.0 + 12.0 * layoutUnit(h, 2),
                                  heading, 10.0, 0});
    }
    // the pads: a row along the settlement's axis, the first 320 m from the centre; more for
    // larger settlements (traffic)
    const int pads = site.population > 10000000 ? 3 : site.population > 100000 ? 2 : 1;
    for (int i = 0; i < pads; ++i)
        out.structures.push_back({StructureKind::Pad, at(smallBody ? 150.0 + 80.0 * i : 320.0 + 95.0 * i, heading), 30.0, 0.4, heading,
                                  36.0, i + 1});
    return out;
}

// The catalog, loaded once for the process (the same file on server and client).
inline const WorldSitesCatalog* settlementCatalog()
{
    static std::once_flag once;
    static WorldSitesCatalog catalog;
    static bool loaded = false;
    std::call_once(once, [] {
        try
        {
            loaded = catalog.loadFromRuntimeOrSource();
        }
        catch (...)
        {
            loaded = false;   // never throw out of a call_once (it locks the flag under MinGW)
        }
        if (!loaded)
            std::fprintf(stderr, "[Settlements] world sites catalog not found: no surface settlements\n");
    });
    return loaded ? &catalog : nullptr;
}

inline int systemIdOfBody(const std::string& bodyId)
{
    int id = -1;
    return std::sscanf(bodyId.c_str(), "system_%d", &id) == 1 ? id : -1;
}

// every surface settlement of one body
inline std::vector<SettlementLayout> surfaceSettlementsOn(const std::string& bodyId, double bodyRadiusM)
{
    std::vector<SettlementLayout> out;
    const auto* catalog = settlementCatalog();
    const int systemId = systemIdOfBody(bodyId);
    if (!catalog || systemId < 0)
        return out;
    for (const auto& site : catalog->sitesInSystem(systemId))
        if (site.parentBodyId == bodyId && site.placement == WorldSitePlacementKind::Surface)
            out.push_back(layoutSurfaceSettlement(site, bodyRadiusM));
    return out;
}

// a surface settlement by its catalog id (any system): its body id, empty when unknown
inline std::string surfaceSettlementBody(const std::string& siteId)
{
    const auto* catalog = settlementCatalog();
    if (!catalog)
        return {};
    int systemId = -1;
    if (std::sscanf(siteId.c_str(), "site.%d", &systemId) != 1)
        return {};
    if (const auto* site = catalog->findSite(systemId, siteId); site && site->placement == WorldSitePlacementKind::Surface)
        return site->parentBodyId;
    return {};
}

} // namespace world::celestial::settlements
