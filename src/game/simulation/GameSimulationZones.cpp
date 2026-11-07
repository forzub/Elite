// Navigation-zone violations on the server (architecture audit 2026-11-06 item 9): a red zone
// entered or a blue gate skipped between its two bodies fines the ship - authoritative, replicated
// (motion.zoneFinesTotalCr / zoneViolation*), the client only shows the notice.
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <iostream>

#include "src/game/navigation/route/RouteZones.h"
#include "src/game/navigation/rules/BodyFlightRules.h"

namespace
{
const game::navigation::route::ZoneCatalog& zones()
{
    static const game::navigation::route::ZoneCatalog catalog = [] {
        auto c = game::navigation::route::loadZoneCatalog();
        std::cerr << "[Zones] server: " << c.zones.size() << " navigation zones" << std::endl;
        return c;
    }();
    return catalog;
}
}

void GameSimulation::updateZoneViolations(double dt)
{
    namespace route = game::navigation::route;
    m_zoneCheckSeconds -= dt;
    if (m_zoneCheckSeconds > 0.0)
        return;
    m_zoneCheckSeconds = 0.25;
    const auto& catalog = zones();
    if (catalog.zones.empty())
        return;
    const auto resolver = [this](const route::ZoneAnchor& a, glm::dvec3& out) -> bool {
        if (a.kind == "hub")
        {
            const auto* hub = hubNavigationFrame(a.id);
            if (!hub || !hub->valid)
                return false;
            out = hub->originMeters;
            return true;
        }
        const auto body = [&](const std::string& id, glm::dvec3& p) {
            for (const auto& b : m_gravityBodies)
                if (b.id == id)
                {
                    p = b.centerMeters;
                    return true;
                }
            return false;
        };
        if (a.kind == "body")
            return body(a.id, out);
        if (a.kind == "lagrange")
        {
            glm::dvec3 p, s;
            if (!body(a.primary, p) || !body(a.secondary, s))
                return false;
            out = p + (s - p) * a.fraction;
            return true;
        }
        return false;
    };
    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr || shipPtr->core().role() != ShipRole::Player)
            continue;
        auto& motion = shipPtr->core().transform().motion;
        const auto geometry = route::resolveZones(catalog, motion.systemId, resolver);
        if (geometry.empty())
            continue;
        const glm::dvec3 p = world::coordinates::fullMeters(shipPtr->core().transform().worldPosition);
        using B = game::navigation::GravityBody;
        const std::size_t k = game::navigation::rules::dominantBody(
            m_gravityBodies, p, [](const B& b) -> const std::string& { return b.id; }, [](const B&) { return std::string(); },
            [](const B& b) { return b.gravitationalParameterM3s2; }, [](const B& b) { return b.centerMeters; });
        std::string body = k < m_gravityBodies.size() ? m_gravityBodies[k].id : std::string();
        // the star dominates the space between a planet and its moon (its pull exceeds the planet's
        // there): it is not a step of the body chain, or every Earth -> Moon flight "came from the Sun"
        // and a skipped gate was never fined (live 2026-11-07)
        {
            std::size_t root = m_gravityBodies.size();
            for (std::size_t i = 0; i < m_gravityBodies.size(); ++i)
                if (root == m_gravityBodies.size() || m_gravityBodies[i].gravitationalParameterM3s2 > m_gravityBodies[root].gravitationalParameterM3s2)
                    root = i;
            if (root < m_gravityBodies.size() && body == m_gravityBodies[root].id && !m_zoneWatch[id].lastBody.empty())
                body = m_zoneWatch[id].lastBody;
        }
        auto& watch = m_zoneWatch[id];
        const auto fine = [&](const route::RouteZone& z, int kind) {
            motion.zoneFinesTotalCr += z.fineCr;
            motion.zoneViolationKind = static_cast<std::uint8_t>(kind);
            motion.zoneViolationZoneId = z.id;
            ++motion.zoneViolationSerial;
            std::cerr << "[Zones] ship " << id.value << " VIOLATION: " << (kind == 1 ? "entered red zone " : "skipped gate ") << z.id
                      << ", fine " << z.fineCr << " cr (total " << motion.zoneFinesTotalCr << ")" << std::endl;
        };
        for (const auto& g : geometry)
        {
            const bool inside = g.contains(p);
            bool& was = watch.inside[g.zone->id];
            if (inside && !was && g.zone->colour == route::ZoneColour::Red)
                fine(*g.zone, 1);
            if (inside && g.zone->colour == route::ZoneColour::Blue)
                watch.gatePassed[g.zone->id] = true;
            was = inside;
            // a blue gate skipped: from one of its bodies to the other without passing it
            if (g.zone->colour == route::ZoneColour::Blue && !watch.lastBody.empty() && body != watch.lastBody)
            {
                const auto& gb = g.zone->gateBetween;
                const bool between = std::find(gb.begin(), gb.end(), watch.lastBody) != gb.end() &&
                                     std::find(gb.begin(), gb.end(), body) != gb.end();
                if (between && !watch.gatePassed[g.zone->id])
                    fine(*g.zone, 2);
                if (between)
                    watch.gatePassed[g.zone->id] = false;
            }
        }
        watch.lastBody = body;
    }
}
