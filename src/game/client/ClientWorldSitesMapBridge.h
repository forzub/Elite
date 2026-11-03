#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "src/game/system_map/PlanetBodyOrientation.h"
#include "src/world/celestial/CelestialOrbitKinematics.h"
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

// Position of a site at a belt anchor: the anchor's Kepler orbit about the star,
// through the same contract as the catalog planets (keplerStateAu).
inline glm::dvec3 worldSiteHeliocentricAu(const world::celestial::WorldOrbitElements& e, double universeTimeSeconds)
{
    world::celestial::CelestialBodyDefinition body;
    body.distanceAu = e.aAu;
    body.eccentricity = e.e;
    body.inclinationDeg = e.iDeg;
    body.longitudeAscendingNodeDeg = e.nodeDeg;
    body.argumentOfPeriapsisDeg = e.periDeg;
    body.meanAnomalyAtEpochDeg = e.meanAnomalyEpochDeg;
    body.orbitEpochSeconds = 0.0;
    body.orbitalPeriodDays = 365.25 * std::pow(std::max(1.0e-6, e.aAu), 1.5);
    glm::dvec3 p(0.0), v(0.0);
    world::celestial::keplerStateAu(body, universeTimeSeconds, p, v);
    return p;
}

// Lagrange point of a primary-secondary pair at the bodies' current positions.
inline glm::dvec3 worldSiteLagrangeAu(const glm::dvec3& primary, const glm::dvec3& secondary, const std::string& point, double hillRatio)
{
    const glm::dvec3 d = secondary - primary;
    const double h = hillRatio > 0.0 ? hillRatio : 0.01;
    if (point == "L1")
        return secondary - d * h;
    if (point == "L2")
        return secondary + d * h;
    const double a = (point == "L5" ? -1.0 : 1.0) * 3.14159265358979323846 / 3.0;
    return primary + glm::dvec3(d.x * std::cos(a) - d.z * std::sin(a), d.y, d.x * std::sin(a) + d.z * std::cos(a));
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
        if (!site.runtimeBinding.empty())
        {
            // the replicated hub represents this site: give its marker the world model's operator and type
            for (auto& object : map.objects)
                if (object.kind == SystemMapObjectKind::Hub && site.runtimeBinding.find("[" + object.stableId + "]") != std::string::npos)
                {
                    if (!site.operatorName.empty())
                        object.owner = site.operatorName;
                    if (!site.typeLabel.empty())
                        object.typeName = site.typeLabel;
                    object.name = site.name;
                }
            continue;
        }
        if (site.mapLevel != "system")
            continue;
        SystemMapObject o;
        o.stableId = "site:" + site.id;
        o.name = site.name;
        o.owner = !site.operatorName.empty() ? site.operatorName : !site.polityName.empty() ? site.polityName : site.polity;
        o.parentBodyId = site.parentBodyId;
        o.kind = site.type == "nav_relay" ? SystemMapObjectKind::Relay
                 : site.type == "belt_anchor" ? SystemMapObjectKind::Buoy
                 : SystemMapObjectKind::Station;
        o.systemId = map.systemId;
        o.typeName = !site.typeLabel.empty() ? site.typeLabel : worldSiteTypeLabel(site.type);
        o.sizeMeters = glm::dvec3(800.0);
        auto bodyById = [&map](const std::string& id) -> const SystemMapBody* {
            for (const auto& b : map.bodies)
                if (b.id == id)
                    return &b;
            return nullptr;
        };
        const SystemMapBody* parent = bodyById(site.parentBodyId);
        if (site.placement == WorldSitePlacementKind::Heliocentric)
        {
            const SystemMapBody* star = nullptr;
            for (const auto& b : map.bodies)
                if (b.type == BodyType::Star)
                {
                    star = &b;
                    break;
                }
            o.positionAu = (star ? star->positionAu : glm::dvec3(0.0)) + worldSiteHeliocentricAu(site.elements, universeTimeSeconds);
        }
        else if (site.placement == WorldSitePlacementKind::LagrangePoint)
        {
            const SystemMapBody* p = bodyById(site.lagrangePrimaryId);
            const SystemMapBody* q = bodyById(site.lagrangeSecondaryId);
            if (!q)
                continue;
            o.positionAu = worldSiteLagrangeAu(p ? p->positionAu : glm::dvec3(0.0), q->positionAu, site.lagrangePoint, site.lagrangeHillRatio);
        }
        else if (site.placement == WorldSitePlacementKind::Belt)
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
