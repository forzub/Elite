#include "src/world/celestial/CelestialEphemerisService.h"

#include <algorithm>
#include <cmath>

#include "src/world/celestial/CelestialOrbitKinematics.h"

namespace world::celestial
{

CelestialEphemerisService::CelestialEphemerisService(const CelestialSystemDefinition& system)
    : m_systemId(system.systemId),
      m_definitions(system.bodies)
{
    m_bodies.reserve(m_definitions.size());
    for (std::size_t i = 0; i < m_definitions.size(); ++i)
        m_index[m_definitions[i].id] = i;

    for (const auto& def : m_definitions)
    {
        EphemerisBody b;
        b.id = def.id;
        b.parentId = def.parentId;
        b.type = def.type;
        b.gravitationalParameterM3s2 = def.gravitationalParameterM3s2;
        b.radiusMeters = def.radiusKm * 1000.0;
        b.atmosphereOuterRadiusMeters = std::max(b.radiusMeters, def.atmosphereOuterRadiusKm * 1000.0);
        b.semiMajorAxisMeters = def.distanceAu * MetersPerAu;
        b.eccentricity = def.keplerOrbit ? def.eccentricity : 0.0;
        b.orbitalPeriodSeconds = def.orbitalPeriodDays * SecondsPerDay;
        m_bodies.push_back(std::move(b));
    }
    for (auto& b : m_bodies)
        if (const auto it = m_index.find(b.parentId); it != m_index.end())
            m_bodies[it->second].childIds.push_back(b.id);

    // A barycentre has no mass of its own: its mu is the sum of its members.
    std::vector<double> systemMu(m_bodies.size(), -1.0);
    const auto massOf = [&](auto&& self, std::size_t i, int depth) -> double {
        if (systemMu[i] >= 0.0)
            return systemMu[i];
        double mu = m_bodies[i].gravitationalParameterM3s2;
        if (m_bodies[i].type == BodyType::Barycenter && depth < 16)
            for (const auto& child : m_bodies[i].childIds)
            {
                const std::size_t c = m_index.at(child);
                if (m_bodies[c].type == BodyType::Star || m_bodies[c].type == BodyType::Barycenter)
                    mu += self(self, c, depth + 1);
            }
        return systemMu[i] = mu;
    };
    for (auto& b : m_bodies)
    {
        const auto parent = m_index.find(b.parentId);
        if (parent == m_index.end() || b.semiMajorAxisMeters <= 0.0)
            continue;
        const double central = massOf(massOf, parent->second, 0);
        const double own = b.type == BodyType::Barycenter ? massOf(massOf, m_index.at(b.id), 0)
                                                          : b.gravitationalParameterM3s2;
        b.centralGravitationalParameterM3s2 = central;
        if (central > 0.0 && own > 0.0)
        {
            b.sphereOfInfluenceMeters = b.semiMajorAxisMeters * std::pow(own / central, 0.4);
            b.hillRadiusMeters =
                b.semiMajorAxisMeters * (1.0 - b.eccentricity) * std::cbrt(own / (3.0 * central));
        }
    }
}

const CelestialBodyDefinition* CelestialEphemerisService::definition(const std::string& bodyId) const
{
    const auto it = m_index.find(bodyId);
    return it == m_index.end() ? nullptr : &m_definitions[it->second];
}

const EphemerisBody* CelestialEphemerisService::body(const std::string& bodyId) const
{
    const auto it = m_index.find(bodyId);
    return it == m_index.end() ? nullptr : &m_bodies[it->second];
}

std::optional<EphemerisState> CelestialEphemerisService::state(
    const std::string& bodyId,
    double universeTimeSeconds
) const
{
    const CelestialBodyDefinition* def = definition(bodyId);
    if (!def)
        return std::nullopt;

    EphemerisState out;
    for (int depth = 0; def && depth < 32; ++depth)
    {
        const CelestialBodyDefinition* parent = definition(def->parentId);
        glm::dvec3 p {0.0}, v {0.0};
        relativeBodyStateAu(*def, parent, universeTimeSeconds, p, v);
        out.positionAu += p;
        out.velocityAuPerSecond += v;
        def = parent;
    }
    return out;
}

std::optional<EphemerisState> CelestialEphemerisService::stateRelativeTo(
    const std::string& bodyId,
    const std::string& referenceId,
    double universeTimeSeconds
) const
{
    const auto a = state(bodyId, universeTimeSeconds);
    const auto b = state(referenceId, universeTimeSeconds);
    if (!a || !b)
        return std::nullopt;
    return EphemerisState {a->positionAu - b->positionAu, a->velocityAuPerSecond - b->velocityAuPerSecond};
}

std::vector<std::string> CelestialEphemerisService::ancestors(const std::string& bodyId) const
{
    std::vector<std::string> out;
    const CelestialBodyDefinition* def = definition(bodyId);
    while (def && !def->parentId.empty() && out.size() < 32)
    {
        out.push_back(def->parentId);
        def = definition(def->parentId);
    }
    return out;
}

} // namespace world::celestial
