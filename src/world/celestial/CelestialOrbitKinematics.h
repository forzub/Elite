#pragma once

#include <algorithm>
#include <cmath>

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
    Keplerian state relative to the parent, in the engine frame of the
    orbit's reference plane (+Y north, prograde motion from +X toward -Z).

    The elements use the standard right-handed frame (x, y, z_north); the
    engine maps it as X = x, Y = z, Z = -y. Mean motion comes from the
    stored period (derived from mu of parent + body by the generator).
*/
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
    const double a = body.distanceAu;
    const double e = std::min(std::max(body.eccentricity, 0.0), 0.999);
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
    const double xp = a * (cosE - e);
    const double yp = a * root * sinE;
    const double rate = n / (1.0 - e * cosE);
    const double vxp = -a * sinE * rate;
    const double vyp = a * root * cosE * rate;

    const double O = body.longitudeAscendingNodeDeg * degToRad;
    const double i = body.inclinationDeg * degToRad;
    const double w = body.argumentOfPeriapsisDeg * degToRad;
    const double cO = std::cos(O), sO = std::sin(O);
    const double ci = std::cos(i), si = std::sin(i);
    const double cw = std::cos(w), sw = std::sin(w);
    // Perifocal -> reference frame rotation R_z(O) R_x(i) R_z(w).
    const double r11 = cO * cw - sO * sw * ci, r12 = -cO * sw - sO * cw * ci;
    const double r21 = sO * cw + cO * sw * ci, r22 = -sO * sw + cO * cw * ci;
    const double r31 = sw * si, r32 = cw * si;
    const double x = r11 * xp + r12 * yp, y = r21 * xp + r22 * yp, z = r31 * xp + r32 * yp;
    const double vx = r11 * vxp + r12 * vyp, vy = r21 * vxp + r22 * vyp, vz = r31 * vxp + r32 * vyp;
    positionAu = glm::dvec3(x, z, -y);
    velocityAuPerSecond = glm::dvec3(vx, vz, -vy);
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
