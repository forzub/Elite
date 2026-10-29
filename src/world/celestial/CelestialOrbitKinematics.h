#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"

namespace world::celestial
{

inline constexpr double OrbitTwoPi =
    6.28318530717958647692;

inline double circularOrbitPhaseRad(
    double simTimeSeconds,
    double orbitalPeriodDays,
    int orbitalDirection,
    double phaseOffsetRad = 0.0
)
{
    if (orbitalPeriodDays <= 0.0)
        return phaseOffsetRad;

    const double turns =
        std::fmod(
            simTimeSeconds /
                (orbitalPeriodDays * SecondsPerDay),
            1.0
        );

    return
        phaseOffsetRad +
        static_cast<double>(
            orbitalDirection < 0 ? -1 : 1
        ) *
        turns *
        OrbitTwoPi;
}

inline glm::dvec3 circularOrbitPositionAu(
    double radiusAu,
    double phaseRad
)
{
    /*
        +Y is the north pole.

        A positive (prograde) phase therefore moves from +X toward -Z.
        Its angular-momentum vector points toward +Y, matching the
        positive body-rotation convention used by the surface renderer.
    */
    return glm::dvec3(
        std::cos(phaseRad) * radiusAu,
        0.0,
        -std::sin(phaseRad) * radiusAu
    );
}

/*
    Keplerian orbit of a body relative to its parent, in the engine frame of
    the orbit's reference plane (+Y north, prograde motion from +X toward -Z).

    The elements use the standard right-handed frame (x, y, z_north); the
    engine maps it as X = x, Y = z, Z = -y. Mean motion comes from the
    stored period (derived from mu of parent + body by the generator).

    This is the one orbital contract: the runtime state, the System Map orbit
    path and ephemeris queries all go through keplerPerifocalToEngine.
*/
struct KeplerPerifocalBasis
{
    double a = 0.0;
    double e = 0.0;
    // Columns of R_z(O) R_x(i) R_z(w) (perifocal P and Q axes), standard frame.
    double r11 = 1.0, r12 = 0.0, r21 = 0.0, r22 = 1.0, r31 = 0.0, r32 = 0.0;
};

inline KeplerPerifocalBasis keplerPerifocalBasis(const CelestialBodyDefinition& body)
{
    constexpr double degToRad = OrbitTwoPi / 360.0;
    KeplerPerifocalBasis b;
    b.a = body.distanceAu;
    b.e = std::min(std::max(body.eccentricity, 0.0), 0.999);
    const double O = body.longitudeAscendingNodeDeg * degToRad;
    const double i = body.inclinationDeg * degToRad;
    const double w = body.argumentOfPeriapsisDeg * degToRad;
    const double cO = std::cos(O), sO = std::sin(O);
    const double ci = std::cos(i), si = std::sin(i);
    const double cw = std::cos(w), sw = std::sin(w);
    b.r11 = cO * cw - sO * sw * ci;
    b.r12 = -cO * sw - sO * cw * ci;
    b.r21 = sO * cw + cO * sw * ci;
    b.r22 = -sO * sw + cO * cw * ci;
    b.r31 = sw * si;
    b.r32 = cw * si;
    return b;
}

inline glm::dvec3 keplerPerifocalToEngine(const KeplerPerifocalBasis& b, double xp, double yp)
{
    const double x = b.r11 * xp + b.r12 * yp;
    const double y = b.r21 * xp + b.r22 * yp;
    const double z = b.r31 * xp + b.r32 * yp;
    return glm::dvec3(x, z, -y);
}

// Position on the orbit at eccentric anomaly E (perifocal ellipse).
inline glm::dvec3 keplerPositionAtEccentricAnomalyAu(const KeplerPerifocalBasis& b, double E)
{
    return keplerPerifocalToEngine(
        b, b.a * (std::cos(E) - b.e), b.a * std::sqrt(1.0 - b.e * b.e) * std::sin(E));
}

inline void keplerStateAu(
    const CelestialBodyDefinition& body,
    double simTimeSeconds,
    glm::dvec3& positionAu,
    glm::dvec3& velocityAuPerSecond
)
{
    positionAu = glm::dvec3(0.0);
    velocityAuPerSecond = glm::dvec3(0.0);
    if (body.distanceAu <= 0.0 || body.orbitalPeriodDays <= 0.0)
        return;

    constexpr double degToRad = OrbitTwoPi / 360.0;
    const KeplerPerifocalBasis basis = keplerPerifocalBasis(body);
    const double a = basis.a;
    const double e = basis.e;
    const double n = OrbitTwoPi / (body.orbitalPeriodDays * SecondsPerDay);
    double meanAnomaly = std::fmod(
        body.meanAnomalyAtEpochDeg * degToRad + n * (simTimeSeconds - body.orbitEpochSeconds),
        OrbitTwoPi);
    if (meanAnomaly < 0.0)
        meanAnomaly += OrbitTwoPi;

    double E = e < 0.8 ? meanAnomaly : 3.14159265358979323846;
    for (int k = 0; k < 32; ++k)
    {
        const double f = E - e * std::sin(E) - meanAnomaly;
        const double step = f / (1.0 - e * std::cos(E));
        E -= step;
        if (std::abs(step) < 1.0e-14)
            break;
    }
    const double cosE = std::cos(E);
    const double sinE = std::sin(E);
    const double root = std::sqrt(1.0 - e * e);
    const double rate = n / (1.0 - e * cosE);
    positionAu = keplerPositionAtEccentricAnomalyAu(basis, E);
    velocityAuPerSecond = keplerPerifocalToEngine(basis, -a * sinE * rate, a * root * cosE * rate);
}

// Rotates a vector from a parent-equatorial frame (+Y = spin axis) into the
// system frame, using the same spin axis as PlanetBodyOrientation.
inline glm::dvec3 rotateFromParentEquator(
    const glm::dvec3& v,
    double axialTiltDeg,
    double axisNodeDeg
)
{
    constexpr double degToRad = OrbitTwoPi / 360.0;
    const double tilt = axialTiltDeg * degToRad;
    const double node = axisNodeDeg * degToRad;
    if (std::abs(tilt) < 1.0e-12)
        return v;
    const glm::dvec3 north(std::sin(tilt) * std::cos(node), std::cos(tilt), std::sin(tilt) * std::sin(node));
    glm::dvec3 axis = glm::cross(glm::dvec3(0.0, 1.0, 0.0), north);
    const double s = glm::length(axis);
    if (s < 1.0e-12)
        return north.y < 0.0 ? glm::dvec3(v.x, -v.y, -v.z) : v;
    axis /= s;
    const double c = north.y;
    // Rodrigues rotation taking +Y onto the spin axis.
    return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0 - c);
}

// Kepler state relative to the parent including the parent-equator frame of
// moons (orbitInParentEquator): exactly what CelestialSystemRuntime uses.
inline void relativeKeplerStateAu(
    const CelestialBodyDefinition& body,
    const CelestialBodyDefinition* parent,
    double simTimeSeconds,
    glm::dvec3& positionAu,
    glm::dvec3& velocityAuPerSecond
)
{
    keplerStateAu(body, simTimeSeconds, positionAu, velocityAuPerSecond);
    if (body.orbitInParentEquator && parent)
    {
        positionAu = rotateFromParentEquator(positionAu, parent->axialTiltDeg, parent->axisNodeDeg);
        velocityAuPerSecond =
            rotateFromParentEquator(velocityAuPerSecond, parent->axialTiltDeg, parent->axisNodeDeg);
    }
}

// The orbit ellipse relative to the parent (closed polyline, first point =
// periapsis), in the same frame as relativeKeplerStateAu.
inline std::vector<glm::dvec3> keplerOrbitPathAu(
    const CelestialBodyDefinition& body,
    const CelestialBodyDefinition* parent,
    int samples
)
{
    std::vector<glm::dvec3> out;
    if (!body.keplerOrbit || body.distanceAu <= 0.0 || samples < 8)
        return out;
    const KeplerPerifocalBasis basis = keplerPerifocalBasis(body);
    out.reserve(static_cast<std::size_t>(samples) + 1);
    for (int k = 0; k <= samples; ++k)
    {
        // Uniform in eccentric anomaly: denser near apoapsis is acceptable,
        // the ellipse shape is exact at every sample.
        const double E = OrbitTwoPi * static_cast<double>(k) / static_cast<double>(samples);
        glm::dvec3 p = keplerPositionAtEccentricAnomalyAu(basis, E);
        if (body.orbitInParentEquator && parent)
            p = rotateFromParentEquator(p, parent->axialTiltDeg, parent->axisNodeDeg);
        out.push_back(p);
    }
    return out;
}

inline double bodyRotationPhaseRad(
    double simTimeSeconds,
    double dayLengthHours,
    int rotationDirection,
    double rotationOffsetRad = 0.0
)
{
    if (dayLengthHours <= 0.0)
        return rotationOffsetRad;

    const double turns =
        std::fmod(
            simTimeSeconds /
                (dayLengthHours * 3600.0),
            1.0
        );

    return
        rotationOffsetRad +
        static_cast<double>(
            rotationDirection < 0 ? -1 : 1
        ) *
        turns *
        OrbitTwoPi;
}

} // namespace world::celestial
