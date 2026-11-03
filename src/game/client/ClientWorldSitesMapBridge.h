#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "src/game/system_map/PlanetBodyOrientation.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/celestial/WorldSitesCatalog.h"
#include "src/world/orbits/OrbitalMotion.h"

namespace game::client
{
/*
    Map presentation of the world sites (galaxy_details/world_sites, GENERATED
    from the world model). Sites are drawn as infrastructure markers, never as
    Hub targets: only replicated runtime hubs have a Hub map, and a site bound
    to one (runtimeBinding) is skipped because the replicated hub already
    represents it. System Map: sites with map level "system" (orbital, trade,
    belt, military hubs). Detail Map of a body: every site of that body,
    surface ones at their persistent latitude / longitude in the same planet
    texture frame the renderers use, so a city never moves against the terrain.
    Orbit sites use one presentation phase law on both maps (one-day period
    from a stable id phase) until sites get real ephemerides.
*/
inline double worldSitePhaseRad(const std::string& id)
{
    // stable orbital phase from the site id (FNV-1a), the same on every client
    std::uint32_t h = 2166136261u;
    for (const char c : id)
    {
        h ^= static_cast<std::uint8_t>(c);
        h *= 16777619u;
    }
    return (h % 3600u) / 3600.0 * 2.0 * 3.14159265358979323846;
}

inline const char* worldSiteTypeLabel(const std::string& type)
{
    if (type == "orbital_hub") return "Orbital hub";
    if (type == "trade_hub") return "Trade hub";
    if (type == "belt_hub") return "Belt hub";
    if (type == "mining_hub") return "Mining hub";
    if (type == "industrial_hub") return "Industrial hub";
    if (type == "military_hub") return "Military hub";
    if (type == "science_hub") return "Science hub";
    if (type == "quarantine_hub") return "Quarantine station";
    if (type == "free_port") return "Free port";
    if (type == "planetary_surface_port") return "Surface spaceport";
    if (type == "moon_hub") return "Moon settlement";
    if (type == "deep_space_refuel_point") return "Refuel depot";
    if (type == "abandoned_hub") return "Abandoned hub";
    return "World site";
}

inline glm::dvec3 worldSiteOrbitOffsetMeters(
    const world::celestial::WorldSite& site,
    double parentRadiusMeters,
    double parentMuM3s2,
    double universeTimeSeconds
)
{
    world::orbits::OrbitalMotion motion;
    motion.enabled = true;
    motion.parentRadiusMeters = parentRadiusMeters;
    motion.altitudeMeters = site.altitudeKm * 1000.0;
    motion.inclinationDeg = site.inclinationDeg;
    motion.initialPhaseDeg = worldSitePhaseRad(site.id) * 180.0 / 3.14159265358979323846;
    motion.orbitalPeriodSeconds = parentMuM3s2 > 0.0
        ? world::orbits::computeCircularOrbitPeriodSeconds(parentRadiusMeters, motion.altitudeMeters, parentMuM3s2)
        : 86400.0;
    return world::orbits::computeOrbitPositionMeters(motion, universeTimeSeconds);
}

inline void appendSystemMapWorldSites(
    world::celestial::SystemMapSnapshot& map,
    const world::celestial::WorldSitesCatalog& catalog,
    double universeTimeSeconds
)
{
    using namespace world::celestial;
    for (const auto& site : catalog.sitesInSystem(map.systemId))
    {
        if (site.mapLevel != "system" || !site.runtimeBinding.empty())
            continue;
        SystemMapObject o;
        o.stableId = "site:" + site.id;
        o.name = site.name;
        o.owner = site.polity;
        o.parentBodyId = site.parentBodyId;
        o.kind = SystemMapObjectKind::Station;
        o.systemId = map.systemId;
        o.typeName = worldSiteTypeLabel(site.type);
        o.sizeMeters = glm::dvec3(800.0);
        const SystemMapBody* parent = nullptr;
        for (const auto& b : map.bodies)
        {
            if (b.id == site.parentBodyId)
            {
                parent = &b;
                break;
            }
        }
        if (site.placement == WorldSitePlacementKind::Belt)
        {
            const double l = site.beltLongitudeDeg * 3.14159265358979323846 / 180.0;
            o.positionAu = glm::dvec3(std::cos(l), 0.0, std::sin(l)) * site.beltRadiusAu;
        }
        else if (site.placement == WorldSitePlacementKind::Lagrange)
        {
            // parent planet of the trojan swarm: the reservoir id minus ".trojans"
            const std::string planetId = site.parentBodyId.substr(0, site.parentBodyId.rfind('.'));
            const SystemMapBody* planet = nullptr;
            for (const auto& b : map.bodies)
                if (b.id == planetId)
                    planet = &b;
            if (!planet)
                continue;
            const double a = (site.lagrangePoint == "L5" ? -1.0 : 1.0) * 3.14159265358979323846 / 3.0;
            const glm::dvec3 p = planet->positionAu;
            o.positionAu = glm::dvec3(p.x * std::cos(a) - p.z * std::sin(a), p.y, p.x * std::sin(a) + p.z * std::cos(a));
        }
        else if (parent)
        {
            if (site.placement == WorldSitePlacementKind::Orbit)
            {
                const glm::dvec3 off = worldSiteOrbitOffsetMeters(site, parent->radiusKm * 1000.0, 0.0, universeTimeSeconds);
                o.positionAu = parent->positionAu + off / MetersPerAu;
                o.hasOrbit = true;
                o.orbitCenterAu = parent->positionAu;
                o.orbitRadiusAu = (parent->radiusKm + site.altitudeKm) * 1000.0 / MetersPerAu;
                o.orbitInclinationDeg = site.inclinationDeg;
            }
            else
            {
                o.positionAu = parent->positionAu;
            }
        }
        else
        {
            continue;
        }
        map.objects.push_back(std::move(o));
    }
}

inline void appendDetailMapWorldSites(
    world::celestial::DetailMapSnapshot& out,
    const world::celestial::WorldSitesCatalog& catalog,
    double universeTimeSeconds
)
{
    using namespace world::celestial;
    if (!out.valid || !out.hasCentralBody || out.planetBodyId.empty())
        return;
    const auto texture = game::system_map::makePlanetTextureOrientation(
        out.planetAxialTiltDeg, out.planetAxisNodeDeg, out.planetRotationPhaseRad, out.planetTextureLongitudeOffsetDeg);
    for (const auto& site : catalog.sitesInSystem(out.systemId))
    {
        if (site.parentBodyId != out.planetBodyId || !site.runtimeBinding.empty() || site.mapLevel == "none")
            continue;
        LocalSceneObject o;
        o.stableId = "site:" + site.id;
        o.parentStableId = out.planetBodyId;
        o.name = site.name;
        o.kind = "world_site:" + site.type;
        o.typeName = worldSiteTypeLabel(site.type);
        o.objectClass = DetailObjectClass::Hub;
        o.origin = DetailObjectOrigin::Authored;
        o.role = LocalSceneObjectRole::Context;
        o.coordinateSpace = LocalSceneCoordinateSpace::SystemWorldMeters;
        if (site.placement == WorldSitePlacementKind::Surface)
        {
            const double lat = site.latitudeDeg * 3.14159265358979323846 / 180.0;
            const double lon = site.longitudeDeg * 3.14159265358979323846 / 180.0;
            const glm::dvec3 dir = texture.prime * (std::cos(lat) * std::cos(lon)) + texture.north * std::sin(lat) +
                                   texture.east * (std::cos(lat) * std::sin(lon));
            o.positionMeters = out.planetCenterMeters + dir * out.planetRadiusMeters;
            o.sizeMeters = glm::dvec3(2000.0);
        }
        else if (site.placement == WorldSitePlacementKind::Orbit)
        {
            o.positionMeters = out.planetCenterMeters +
                worldSiteOrbitOffsetMeters(site, out.planetRadiusMeters, 0.0, universeTimeSeconds);
            o.sizeMeters = glm::dvec3(800.0);
        }
        else
        {
            continue;
        }
        o.velocityMps = out.planetVelocityMps;
        o.globalVelocityMps = out.planetVelocityMps;
        o.hasGlobalVelocity = true;
        o.boundingRadiusMeters = o.sizeMeters.x * 0.5;
        o.valid = true;
        out.scene.objects.push_back(std::move(o));
    }
}
}
