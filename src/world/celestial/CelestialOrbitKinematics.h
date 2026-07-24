#pragma once

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
