// Hub city damage (game::simulation::CityDamage.h): the weapons' energy per building of a city object, the building's
// variant by thresholds, the destroyed buildings' volumes switched off; replicated as ObjectSnapshot::buildingStates.
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

#include "src/game/combat/Penetration.h"
#include "src/game/simulation/CityDamage.h"

namespace
{

// the first stage of a building (one heavy missile for a 100 000 m3 block), scaled by the cube root of its volume:
// a 1.3 Mm3 tower takes ~60 MJ (two Needles or one Thermite), a 75 000 m3 block ~23 MJ
constexpr double FirstStageMJ = 25.0;
constexpr double GuttedFactor = 4.0;
constexpr double DestroyedFactor = 12.0;

bool hasDamagedVariants(const std::string& folder)
{
    static std::unordered_map<std::string, bool> cache;
    const auto [it, inserted] = cache.try_emplace(folder, false);
    if (inserted)
    {
        const std::string v = folder + "_dmg_missile";
        std::error_code ec;
        it->second = std::filesystem::exists("assets/compiled/hub_modules/" + v + "/" + v + ".elmodel", ec);
    }
    return it->second;
}

const char* variantName(std::uint8_t v)
{
    switch (static_cast<game::simulation::CityBuildingVariant>(v))
    {
        case game::simulation::CityBuildingVariant::Missile: return "damaged (missile)";
        case game::simulation::CityBuildingVariant::Laser: return "damaged (laser / rail)";
        case game::simulation::CityBuildingVariant::Gutted: return "gutted";
        case game::simulation::CityBuildingVariant::Destroyed: return "destroyed";
        default: return "intact";
    }
}

} // namespace

bool GameSimulation::castCitySegment(const glm::dvec3& startW, const glm::dvec3& endW, double dt, CityRayHit& out) const
{
    bool any = false;
    for (const auto& [oid, obj] : m_staticObjects)
    {
        if (obj.type != ObjectType::HubBeltCity || !obj.packageHitVolumes || obj.hitComponent.volumes.empty())
            continue;
        const glm::dvec3 cpos = world::coordinates::fullMeters(obj.worldPosition);
        const glm::dvec3 cvel(obj.linearVelocity);
        const glm::dvec3 a = startW - (cpos - cvel * dt);    // relative to the city: where it was at the step's start
        const glm::dvec3 b = endW - cpos;
        const glm::dvec3 seg = b - a;
        const double len = glm::length(seg);
        if (len < 1.0e-6)
            continue;
        // the bounding sphere (cached per object and per static-object generation: a construction stage replaces the
        // volumes - SRV-4, the beams missed the new buildings with the first stage's sphere)
        auto& rit = m_cityStrikeRadiusCache[oid.value];   // world-owned (stabilization stage 1)
        if (rit.generation != m_staticObjectsGeneration)
        {
            rit.generation = m_staticObjectsGeneration;
            rit.radius = 0.0;
            for (const auto& v : obj.hitComponent.volumes)
                rit.radius = std::max(rit.radius, static_cast<double>(glm::length(v.center) + glm::length(v.halfSize)));
        }
        const glm::dvec3 closest = a + seg * std::clamp(glm::dot(-a, seg) / (len * len), 0.0, 1.0);
        if (glm::length(closest) > rit.radius)
            continue;
        const glm::dmat3 R(glm::mat3(obj.orientation));
        const glm::vec3 aL(glm::transpose(R) * a);
        const glm::vec3 dL(glm::transpose(R) * (seg / len));
        const auto hits = game::combat::castRay(obj.hitComponent, aL, dL, static_cast<float>(len));
        if (hits.empty())
            continue;
        const double t = hits.front().tEnter;
        if (any && t >= out.t)
            continue;
        any = true;
        out.object = oid;
        out.volume = hits.front().volume;
        out.t = t;
        out.pointW = cpos + R * glm::dvec3(aL + dL * static_cast<float>(t));
    }
    return any;
}

void GameSimulation::damageCity(const CityRayHit& hit, double energyMJ, int kind)
{
    namespace gs = game::simulation;
    const auto it = m_staticObjects.find(hit.object);
    if (it == m_staticObjects.end() || energyMJ <= 0.0)
        return;
    StaticObject& obj = it->second;
    if (hit.volume < 0 || static_cast<std::size_t>(hit.volume) >= obj.hitComponent.volumes.size())
        return;
    const auto& vol = obj.hitComponent.volumes[static_cast<std::size_t>(hit.volume)];
    const std::int32_t node = vol.assemblyNode;
    if (node < 0)
        return;
    CityDamageState& state = m_cityDamage[hit.object.value];
    auto [fit, newFolder] = state.folder.try_emplace(node);
    if (newFolder)
        fit->second = gs::cityModuleFolderOfCollision(vol.m_label);
    const std::string& folder = fit->second;
    if (folder.empty() || gs::cityInfrastructureFolder(folder))
        return;   // the lattice the city hangs on takes no damage here
    auto [tit, newThreshold] = state.thresholdMJ.try_emplace(node, FirstStageMJ);
    if (newThreshold)
    {
        double volume = 0.0;
        for (const auto& v : obj.hitComponent.volumes)
            if (v.assemblyNode == node)
                volume += 8.0 * static_cast<double>(v.halfSize.x) * v.halfSize.y * v.halfSize.z;
        tit->second = FirstStageMJ * std::clamp(std::cbrt(volume / 1.0e5), 0.5, 4.0);
    }
    const double first = tit->second;
    const double energy = (state.energyMJ[node] += energyMJ);
    const std::uint8_t before = state.variant.count(node) ? state.variant[node] : 0;
    std::uint8_t after = before;
    const bool variants = hasDamagedVariants(folder);
    {
        static const bool accountLog = std::getenv("ELITE_DEV_CITY_DAMAGE_LOG") != nullptr;
        static double lastLog = -10.0;   // the running account (DEV), at most once a second
        if (accountLog && m_combatClockS - lastLog >= 1.0)
        {
            lastLog = m_combatClockS;
            std::cerr << "[CityDamage] node " << node << " (" << folder << "): " << energy << " MJ of " << first
                      << " (first stage), variants " << variants << std::endl;
        }
    }
    if (energy >= DestroyedFactor * first)
        after = static_cast<std::uint8_t>(gs::CityBuildingVariant::Destroyed);
    else if (variants && energy >= GuttedFactor * first)
        after = std::max<std::uint8_t>(before, static_cast<std::uint8_t>(gs::CityBuildingVariant::Gutted));
    else if (variants && energy >= first && before == 0)
        after = static_cast<std::uint8_t>(kind == 1 ? gs::CityBuildingVariant::Missile : gs::CityBuildingVariant::Laser);
    if (after == before)
        return;
    state.variant[node] = after;
    if (after == static_cast<std::uint8_t>(gs::CityBuildingVariant::Destroyed))
        for (auto& v : obj.hitComponent.volumes)
            if (v.assemblyNode == node)
                v.destroyed = true;     // ships fly through where it stood; the weapons pass on
    std::cerr << "[CityDamage] " << obj.hubModuleId << " node " << node << " (" << folder << "): " << variantName(after)
              << " - " << energy << " MJ taken (first stage " << first << " MJ)" << std::endl;
}

game::simulation::CityPadPresence GameSimulation::cityPadPresenceOf(EntityId cityId, const std::string& address) const
{
    const auto it = m_staticObjects.find(cityId);
    if (it == m_staticObjects.end())
        return {};
    // the pad nodes of this city's CURRENT package (a construction stage replaces the volumes: the generation)
    auto& c = m_cityPadNodesCache[cityId.value];   // world-owned (stabilization stage 1)
    if (c.generation != m_staticObjectsGeneration)
    {
        c.generation = m_staticObjectsGeneration;
        c.nodes = game::simulation::cityPadNodes(it->second.hitComponent.volumes);
    }
    return game::simulation::cityPadPresence(c.nodes, cityBuildingStates(cityId), address);
}

std::vector<game::simulation::CityBuildingState> GameSimulation::cityBuildingStates(EntityId objectId) const
{
    std::vector<game::simulation::CityBuildingState> out;
    const auto it = m_cityDamage.find(objectId.value);
    if (it == m_cityDamage.end())
        return out;
    out.reserve(it->second.variant.size());
    for (const auto& [node, variant] : it->second.variant)
        out.push_back({static_cast<std::uint32_t>(node), variant});
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.node < b.node; });
    return out;
}

void GameSimulation::devCityFireStep()
{
    // DEV: ELITE_DEV_CITY_STRIKE="<combat s>:x,y,z:<MJ>:<kind 0 laser|1 missile|2 slug>;..." puts the energy straight into
    // the building of the first city at that point of its frame (the stage / variant trials without aiming a ship)
    {
        struct Strike
        {
            double at = 0.0;
            glm::dvec3 p {0.0};
            double mj = 0.0;
            int kind = 1;
            bool done = false;
        };
        static std::vector<Strike> strikes = [] {
            std::vector<Strike> out;
            const char* v = std::getenv("ELITE_DEV_CITY_STRIKE");
            std::stringstream all(v ? v : "");
            std::string item;
            while (std::getline(all, item, ';'))
            {
                Strike k;
                if (std::sscanf(item.c_str(), "%lf:%lf,%lf,%lf:%lf:%d", &k.at, &k.p.x, &k.p.y, &k.p.z, &k.mj, &k.kind) >= 5)
                    out.push_back(k);
            }
            return out;
        }();
        for (auto& k : strikes)
        {
            if (k.done || m_combatClockS < k.at)
                continue;
            k.done = true;
            for (const auto& [oid, obj] : m_staticObjects)
            {
                if (obj.type != ObjectType::HubBeltCity || !obj.packageHitVolumes)
                    continue;
                // the volume nearest the point (its box distance), in the object frame of the volumes
                CityRayHit hit;
                hit.object = oid;
                double best = 1.0e30;
                for (std::size_t i = 0; i < obj.hitComponent.volumes.size(); ++i)
                {
                    const auto& v = obj.hitComponent.volumes[i];
                    if (v.destroyed)
                        continue;
                    const glm::dvec3 rel = glm::transpose(glm::dmat3(v.orientation)) * (k.p - glm::dvec3(v.center));
                    const glm::dvec3 out = glm::max(glm::abs(rel) - glm::dvec3(v.halfSize), glm::dvec3(0.0));
                    const double d = glm::length(out);
                    if (d < best)
                    {
                        best = d;
                        hit.volume = static_cast<int>(i);
                    }
                }
                std::cerr << "[CityDamage] DEV strike " << k.mj << " MJ at (" << k.p.x << "," << k.p.y << "," << k.p.z << "): volume "
                          << hit.volume << " " << (hit.volume >= 0 ? obj.hitComponent.volumes[hit.volume].m_label : std::string())
                          << " (" << best << " m off)" << std::endl;
                if (hit.volume >= 0)
                    damageCity(hit, k.mj, k.kind);
                break;
            }
        }
    }

    // DEV: ELITE_DEV_CITY_FIRE="<combat s>:laser:<seconds>;<combat s>:missile;..." fires the PLAYER ship's weapons on a
    // schedule (the city damage trials: the nose aimed at a block by ELITE_DEV_START_AT)
    struct Event
    {
        double at = 0.0;
        bool laser = false;
        double seconds = 0.0;
        bool started = false, stopped = false;
    };
    static std::vector<Event> events = [] {
        std::vector<Event> out;
        const char* v = std::getenv("ELITE_DEV_CITY_FIRE");
        if (!v || !*v)
            return out;
        std::stringstream all(v);
        std::string item;
        while (std::getline(all, item, ';'))
        {
            Event e;
            std::stringstream one(item);
            std::string a, kind, b;
            std::getline(one, a, ':');
            std::getline(one, kind, ':');
            std::getline(one, b, ':');
            e.at = std::atof(a.c_str());
            e.laser = kind == "laser";
            e.seconds = b.empty() ? 0.0 : std::atof(b.c_str());
            out.push_back(e);
        }
        return out;
    }();
    if (events.empty())
        return;
    EntityId player {};
    for (const auto& [id, s] : m_ships)
        if (s->core().role() == ShipRole::Player)
            player = id;
    if (!player.value)
        return;
    for (auto& e : events)
    {
        if (!e.started && m_combatClockS >= e.at)
        {
            e.started = true;
            std::cerr << "[CityDamage] DEV fire: " << (e.laser ? "laser" : "missile") << " at combat t=" << m_combatClockS;
            if (const Ship* ps = getShip(player))
                for (const auto& [oid, obj] : m_staticObjects)
                    if (obj.type == ObjectType::HubBeltCity && obj.hubId == ps->core().transform().motion.hubId)
                    {
                        const auto& ptr = ps->core().transform();
                        const glm::dmat3 R(glm::mat3(obj.orientation));
                        const glm::dvec3 rel = glm::transpose(R) * (world::coordinates::fullMeters(ptr.worldPosition) -
                                                                    world::coordinates::fullMeters(obj.worldPosition));
                        const glm::dvec3 fwd = glm::transpose(R) * (glm::dmat3(glm::mat3(ptr.orientation)) * glm::dvec3(0, 0, -1));
                        std::cerr << " ship@city (" << rel.x << "," << rel.y << "," << rel.z << ") nose@city (" << fwd.x << ","
                                  << fwd.y << "," << fwd.z << ")";
                    }
            std::cerr << std::endl;
            if (e.laser)
                setLaserFiring(player, true);
            else
                (void)launchMissile(player);
        }
        if (e.laser && e.started && !e.stopped && m_combatClockS >= e.at + e.seconds)
        {
            e.stopped = true;
            setLaserFiring(player, false);
        }
    }
}
