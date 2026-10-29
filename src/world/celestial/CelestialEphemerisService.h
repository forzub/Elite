#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"

namespace world::celestial
{

/*
    Read-only ephemerides of one star system.

    state(bodyId, t) answers where a body is and how it moves at any universe
    time t (past or future) without touching the runtime clock or the
    CelestialSystemRuntime snapshot. It evaluates the same catalog elements
    through the same relative-state function (relativeBodyStateAu) as the
    production runtime, chained through the parent / barycentre hierarchy:
    there is no second orbital physics.

    Frame: the system's engine frame (+Y north of the reference plane),
    origin at the system root (barycentre or single star); AU and AU/s.

    Static per-body data for trajectory planning (mu, physical and
    atmosphere radius, sphere of influence, Hill radius, hierarchy) is
    published once per body.
*/
struct EphemerisState
{
    glm::dvec3 positionAu {0.0};
    glm::dvec3 velocityAuPerSecond {0.0};

    glm::dvec3 positionMeters() const { return positionAu * MetersPerAu; }
    glm::dvec3 velocityMetersPerSecond() const { return velocityAuPerSecond * MetersPerAu; }
};

struct EphemerisBody
{
    std::string id;
    std::string parentId;
    BodyType type = BodyType::Unknown;

    double gravitationalParameterM3s2 = 0.0;
    double radiusMeters = 0.0;
    // Catalog atmosphere outer radius; equals radiusMeters for airless bodies.
    double atmosphereOuterRadiusMeters = 0.0;

    // Orbit around the parent (zero for the system root).
    double semiMajorAxisMeters = 0.0;
    double eccentricity = 0.0;
    double orbitalPeriodSeconds = 0.0;
    // mu of what the body orbits (a barycentre carries the mu of its members).
    double centralGravitationalParameterM3s2 = 0.0;

    // Laplace sphere of influence a (m/M)^(2/5) - the patched-conic switch
    // radius; Hill radius a (1 - e) (m / 3M)^(1/3) - the useful limit of
    // bound satellite orbits. Both are 0 for the system root.
    double sphereOfInfluenceMeters = 0.0;
    double hillRadiusMeters = 0.0;

    std::vector<std::string> childIds;
};

class CelestialEphemerisService
{
public:
    explicit CelestialEphemerisService(const CelestialSystemDefinition& system);

    int systemId() const { return m_systemId; }

    const EphemerisBody* body(const std::string& bodyId) const;
    const std::vector<EphemerisBody>& bodies() const { return m_bodies; }

    // System-root-relative state at universe time t.
    std::optional<EphemerisState> state(const std::string& bodyId, double universeTimeSeconds) const;

    // State of bodyId relative to referenceId (for example a moon relative
    // to its planet, or a planet relative to the star) at time t.
    std::optional<EphemerisState> stateRelativeTo(
        const std::string& bodyId,
        const std::string& referenceId,
        double universeTimeSeconds
    ) const;

    // Parent chain from the body's parent up to the system root.
    std::vector<std::string> ancestors(const std::string& bodyId) const;

private:
    const CelestialBodyDefinition* definition(const std::string& bodyId) const;

    int m_systemId = -1;
    std::vector<CelestialBodyDefinition> m_definitions;
    std::unordered_map<std::string, std::size_t> m_index;
    std::vector<EphemerisBody> m_bodies;
};

} // namespace world::celestial
