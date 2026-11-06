#pragma once

#include "src/game/navigation/rules/BodyFlightRules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "src/game/client/ClientDetailMapRuntimeSampler.h"
#include "src/game/client/ClientLocalAuthority.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::client
{
/*
    Detail-map trajectories of free ships (map-3, 2026-11-06): the osculating
    Kepler conic of the replicated position + velocity about the ship's
    dominant body (the deepest body whose sphere of influence contains it).
    This is a fact of the current motion, not a fabricated orbit: a ship in a
    stable orbit draws its orbit, a ship under thrust draws the conic it would
    coast on now (it changes as it thrusts). Hubs keep their rail circles.
*/
struct DetailGravityBody
{
    const world::celestial::CelestialBodyState* state = nullptr;
    double muM3s2 = 0.0;
    double radiusMeters = 0.0;
    double soiMeters = 0.0;
};

inline std::vector<DetailGravityBody> detailGravityBodies(
    const world::celestial::StarAtlasDatabase& atlas,
    const world::celestial::CelestialSystemSnapshot& celestial
)
{
    using namespace world::celestial;
    std::vector<DetailGravityBody> out;
    const auto* system = atlas.findSystem(celestial.systemId);
    if (!system)
        return out;
    auto muOf = [&](const std::string& id) {
        for (const auto& definition : system->bodies)
            if (definition.id == id)
                return definition.gravitationalParameterM3s2;
        return 0.0;
    };
    for (const auto& body : celestial.bodies)
    {
        if (body.type == BodyType::AsteroidBelt)
            continue;
        const double mu = muOf(body.id);
        if (mu <= 0.0)
            continue;
        DetailGravityBody g;
        g.state = &body;
        g.muM3s2 = mu;
        g.radiusMeters = body.radiusKm * 1000.0;
        g.soiMeters = std::numeric_limits<double>::infinity();
        for (const auto& parent : celestial.bodies)
            if (parent.id == body.parentId)
            {
                const double parentMu = muOf(parent.id);
                if (parentMu > 0.0)
                    g.soiMeters = glm::length(body.worldMeters - parent.worldMeters) * std::pow(mu / parentMu, 0.4);
                break;
            }
        out.push_back(g);
    }
    return out;
}

// The deepest body whose sphere of influence contains the point (nullptr:
// only the star / nothing - a heliocentric conic is not a local trajectory).
inline const DetailGravityBody* detailDominantBody(
    const std::vector<DetailGravityBody>& bodies,
    const glm::dvec3& positionMeters
)
{
    // ONE rule with the server and the planner (BodyFlightRules.h); the star (root) = nullptr
    using B = DetailGravityBody;
    bool isRoot = false;
    const std::size_t k = game::navigation::rules::dominantBody(
        bodies, positionMeters, [](const B& b) -> const std::string& { return b.state->id; },
        [](const B& b) { return b.state->parentId; }, [](const B& b) { return b.muM3s2; },
        [](const B& b) { return b.state->worldMeters; }, &isRoot);
    return k < bodies.size() && !isRoot ? &bodies[k] : nullptr;
}

// The conic through (r, v) relative to the centre, sampled as offsets.
// maxRadiusMeters bounds an open (or very wide) conic to the arc that stays
// within it. False for a degenerate state (on the ground, radial line).
inline bool buildDetailConicOrbit(
    world::celestial::DetailMapOrbit& orbit,
    const glm::dvec3& rRel,
    const glm::dvec3& vRel,
    double mu,
    double bodyRadiusMeters,
    double maxRadiusMeters,
    int samples = 256
)
{
    const double r = glm::length(rRel);
    if (mu <= 0.0 || r <= bodyRadiusMeters * 1.0005 || r < 1.0)
        return false;
    const glm::dvec3 h = glm::cross(rRel, vRel);
    const double hLen = glm::length(h);
    if (hLen <= 1.0e-6 * r * std::max(1.0, glm::length(vRel)))
        return false;
    const glm::dvec3 eVec = glm::cross(vRel, h) / mu - rRel / r;
    const double e = glm::length(eVec);
    const double p = hLen * hLen / mu;
    const glm::dvec3 nHat = h / hLen;
    const glm::dvec3 pHat = e > 1.0e-7 ? eVec / e : rRel / r;
    const glm::dvec3 qHat = glm::normalize(glm::cross(nHat, pHat));

    const double periapsis = p / (1.0 + e);
    const double apoapsis = e < 1.0 ? p / (1.0 - e) : std::numeric_limits<double>::infinity();
    maxRadiusMeters = std::max(maxRadiusMeters, r * 1.05);

    double nuLimit = glm::pi<double>();
    orbit.pathClosed = e < 1.0 && apoapsis <= maxRadiusMeters;
    if (!orbit.pathClosed)
    {
        // r(nu) <= maxRadius  <=>  cos(nu) >= (p / maxRadius - 1) / e
        const double c = e > 1.0e-9 ? (p / maxRadiusMeters - 1.0) / e : -1.0;
        nuLimit = std::acos(std::clamp(c, -1.0, 1.0));
        if (e >= 1.0)
            nuLimit = std::min(nuLimit, std::acos(-1.0 / e) - 1.0e-3);
    }
    if (nuLimit <= 1.0e-4)
        return false;

    orbit.pathOffsetsMeters.clear();
    orbit.pathOffsetsMeters.reserve(static_cast<std::size_t>(samples) + 1u);
    const int count = orbit.pathClosed ? samples : samples + 1;
    for (int i = 0; i < count; ++i)
    {
        const double nu = orbit.pathClosed
            ? glm::two_pi<double>() * static_cast<double>(i) / static_cast<double>(samples)
            : -nuLimit + 2.0 * nuLimit * static_cast<double>(i) / static_cast<double>(samples);
        const double radius = p / (1.0 + e * std::cos(nu));
        orbit.pathOffsetsMeters.push_back((pHat * std::cos(nu) + qHat * std::sin(nu)) * radius);
    }
    orbit.radiusMeters = r;
    orbit.altitudeMeters = r - bodyRadiusMeters;
    orbit.speedMps = glm::length(vRel);
    orbit.periapsisAltitudeMeters = periapsis - bodyRadiusMeters;
    orbit.apoapsisAltitudeMeters = e < 1.0 ? apoapsis - bodyRadiusMeters : -1.0;
    orbit.radialAxis = rRel / r;
    orbit.normalAxis = nHat;
    orbit.progradeAxis = glm::normalize(glm::cross(nHat, orbit.radialAxis));
    orbit.valid = true;
    return true;
}

// Conic trajectories of the free ships of a Detail scene: the player's into
// playerOrbits, others (bound orbits only) into shipOrbits. inScene decides
// which dominant bodies belong to this map (the map body, its moons ...).
template <typename InScene>
inline void appendDetailShipTrajectories(
    world::celestial::DetailMapSnapshot& out,
    const world::celestial::StarAtlasDatabase& atlas,
    const world::celestial::CelestialSystemSnapshot& celestial,
    const DetailMapRuntimeSampleResult& runtime,
    EntityId localControlledEntityId,
    const InScene& inScene
)
{
    const auto bodies = detailGravityBodies(atlas, celestial);
    if (bodies.empty())
        return;
    for (const auto& ship : runtime.ships)
    {
        if (ship.systemId != out.systemId)
            continue;
        const bool player = game::client::isLocalControlledEntity(ship.id, localControlledEntityId);
        // hub-bound traffic belongs to its Hub map
        if (!player && !ship.hubId.empty())
            continue;
        const glm::dvec3 position = world::coordinates::fullMeters(ship.worldPosition);
        const DetailGravityBody* reference = detailDominantBody(bodies, position);
        if (!reference || !inScene(*reference, position))
            continue;
        world::celestial::DetailMapOrbit orbit;
        orbit.id = player ? std::string("player_trajectory") : "trajectory:entity:" + std::to_string(ship.id.value);
        orbit.name = orbit.id;
        orbit.parentBodyId = reference->state->id;
        orbit.centerMeters = reference->state->worldMeters;
        orbit.positionMeters = position;
        orbit.velocityMps = ship.worldVelocityMps;
        orbit.player = player;
        if (!buildDetailConicOrbit(orbit, position - reference->state->worldMeters,
                                   ship.worldVelocityMps - reference->state->worldVelocityMetersPerSecond,
                                   reference->muM3s2, reference->radiusMeters,
                                   std::min(reference->soiMeters, 1.0e8)))
            continue;
        if (player)
        {
            orbit.label = "player orbit";
            out.playerOrbits.push_back(std::move(orbit));
        }
        else if (orbit.apoapsisAltitudeMeters >= 0.0)
        {
            out.shipOrbits.push_back(std::move(orbit));
        }
    }
}

} // namespace game::client
