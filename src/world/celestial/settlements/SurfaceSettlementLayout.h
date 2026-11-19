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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/WorldSitesCatalog.h"
#include "src/world/time/ConstructionClock.h"

namespace world::celestial::settlements
{

enum class StructureKind : std::uint8_t
{
    Pad,          // a landing pad (flat disc, markings, lights)
    Entrance,     // the portal into the underground settlement
    Dome,         // a small pressurised dome
    Antenna,      // a communication mast
    RockPassage,  // a small body: the passage cut into the rock
    // construction (docs/HUB_CONSTRUCTION.md; the site's projects of WorldSitesCatalog, by the game day)
    BuildFrame,   // a building going up: stage 1 foundation .. 4 nearly clad, 5 finished (heightM = its final height)
    Crane,        // a tower crane (only where there is gravity): heightM = hook height, headingRad = the jib's bearing
    Barracks,     // the workers' camp: a row of two-level container huts
    TempPad       // a temporary loading / unloading deck on legs
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
    int stage = 0;                                // BuildFrame: 1..5
    bool stopped = false;                         // construction: halted / abandoned (no work lights)
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

// the game day (0 = 3126-01-01) of the universe time the construction follows (src/world/time/ConstructionClock.h:
// the server's clock); the layout is taken when the body's settlements are built (server terrain / client mesh)
inline int constructionDayNow()
{
    return world::time::constructionDay(world::time::constructionUniverseTime());
}

// progress 0..1 of a site project on a day; status 0 planned, 1 building, 2 halted, 3 abandoned, 4 done (the rule of
// src/model_asset/HubExpansion.cpp projectProgress)
inline double siteProjectProgress(const WorldSite::Project& p, int day, int& status)
{
    if (day < p.startDay)
    {
        status = 0;
        return 0.0;
    }
    status = 1;
    int worked = day - p.startDay;
    for (const auto& [from, to] : p.halts)
    {
        if (day <= from)
            continue;
        if (to < 0)
        {
            worked -= day - from;
            status = 3;
        }
        else
        {
            worked -= std::min(day, to) - from;
            if (day < to)
                status = 2;
        }
    }
    const double prog = std::clamp(static_cast<double>(worked) / static_cast<double>(std::max(1, p.workDays)), 0.0, 1.0);
    if (prog >= 1.0)
        status = 4;
    return prog;
}

// the construction sites of a surface settlement at `day`: the frames of the buildings (inner ones first for the
// settlement's own build: the core is ready, the edge only starts), cranes where the gravity holds a hook load
// (bodies of >= 600 km radius: >= ~0.3 m/s2; on a small body tugs and tethers do the job), the barracks of the
// workers, a temporary deck; finished expansions stay as finished blocks (the settlement grows)
inline void appendSurfaceConstruction(SettlementLayout& out, const WorldSite& site, double bodyRadiusM, double heading, int day)
{
    constexpr double Pi = 3.14159265358979323846;
    if (site.projects.empty())
        return;
    const std::uint64_t h = layoutHash(site.id + "#construction");
    const bool gravity = bodyRadiusM >= 600000.0;
    const auto at = [&](double distanceM, double bearing) {
        return offsetDirection(out.centreDirection, bodyRadiusM, distanceM, bearing);
    };
    struct Kind { double r, h; };
    const auto kindOf = [](const std::string& k, int i) -> Kind {
        if (k == "tower") return {28.0, 90.0};
        if (k == "slab") return {40.0, 45.0};
        if (k == "hall" || k == "minehead") return {45.0, 28.0};
        if (k == "dock" || k == "yard") return {38.0, 12.0};
        if (k == "mast") return {8.0, 60.0};
        if (k == "hab") return {18.0, 30.0};
        static const Kind build[3] = {{18.0, 30.0}, {45.0, 28.0}, {28.0, 70.0}};   // the hub's own: hab / hall / tower
        return build[i % 3];
    };
    int expansionSlot = 0, finishedShown = 0;
    for (std::size_t pi = 0; pi < site.projects.size(); ++pi)
    {
        const auto& p = site.projects[pi];
        int status = 0;
        const double prog = siteProjectProgress(p, day, status);
        if (status == 0)
            continue;
        const bool own = p.kind == "build";
        const bool stopped = status == 2 || status == 3;
        if (status == 4 && (own || ++finishedShown > 4))
            continue;
        const int frames = own ? 3 + static_cast<int>(layoutUnit(h, 3) * 3.0) : 1;
        const double baseBearing = own ? heading + Pi : heading + Pi * 0.5 + expansionSlot * 0.9;
        const double baseDist = own ? 230.0 : 470.0 + 70.0 * (expansionSlot % 4);
        if (!own)
            ++expansionSlot;
        for (int i = 0; i < frames; ++i)
        {
            const Kind k = kindOf(p.kind, i);
            const double bearing = baseBearing + (i - (frames - 1) * 0.5) * 0.55 +
                                   (layoutUnit(h, 40 + static_cast<int>(pi) * 8 + i) - 0.5) * 0.15;
            const double dist = baseDist + (own ? 70.0 * (i % 2) : 0.0);
            // the readiness window of this frame in the project (the settlement's own build: inner / earlier first)
            const double a = own ? 0.12 * i : 0.0, b = own ? std::min(1.0, a + 0.45) : 1.0;
            const double u = (prog - a) / std::max(1e-6, b - a);
            const int stage = status == 4 || u >= 1.0 ? 5 : u < 0.0 ? 1 : 1 + std::min(3, static_cast<int>(u * 4.0));
            Structure f {StructureKind::BuildFrame, at(dist, bearing), k.r, k.h, bearing, k.r + 6.0, 0};
            f.stage = stage;
            f.stopped = stopped;
            out.structures.push_back(f);
            if (status == 4)
                continue;
            if (gravity && stage < 5)
            {
                Structure c {StructureKind::Crane, at(dist + k.r + 10.0, bearing + 0.02), 4.0, k.h * 1.2 + 15.0,
                             layoutUnit(h, 60 + static_cast<int>(pi) * 8 + i) * 2.0 * Pi, 7.0, 0};
                c.stopped = stopped;
                out.structures.push_back(c);
            }
        }
        if (status == 4)
            continue;
        // the camp and the temporary deck beside the site
        const int rows = own ? 3 : (p.kind == "mast" || p.kind == "yard" || p.kind == "hab") ? 0 : 1;
        for (int r = 0; r < rows; ++r)
        {
            Structure br {StructureKind::Barracks, at(baseDist + 150.0 + 14.0 * r, baseBearing + 0.42), 38.0, 6.0,
                          baseBearing + Pi * 0.5, 42.0, r + 1};
            br.stopped = stopped;
            out.structures.push_back(br);
        }
        Structure tp {StructureKind::TempPad, at(baseDist + 140.0, baseBearing - 0.4), 24.0, 8.0, baseBearing, 30.0, 0};
        tp.stopped = stopped;
        out.structures.push_back(tp);
    }
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
    appendSurfaceConstruction(out, site, bodyRadiusM, heading, constructionDayNow());
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
