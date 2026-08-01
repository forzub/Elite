#pragma once

#include <cmath>

#include <glm/glm.hpp>

namespace game::system_map
{
struct PlanetBodyOrientation
{
    glm::dvec3 north {0.0, 1.0, 0.0};
    glm::dvec3 prime {1.0, 0.0, 0.0};
    glm::dvec3 east {0.0, 0.0, 1.0};
};

inline double planetOrientationDegreesToRadians(double degrees)
{
    constexpr double pi =
        3.141592653589793238462643383279502884;

    return degrees * pi / 180.0;
}

inline glm::dvec3 normalizePlanetOrientationVector(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double lengthSquared = glm::dot(value, value);

    if (lengthSquared <= 1.0e-18)
        return fallback;

    return value / std::sqrt(lengthSquared);
}

inline glm::dvec3 planetPrimeAxisFromNorth(
    const glm::dvec3& north
)
{
    glm::dvec3 reference(1.0, 0.0, 0.0);

    if (std::abs(glm::dot(reference, north)) > 0.92)
        reference = glm::dvec3(0.0, 0.0, 1.0);

    return normalizePlanetOrientationVector(
        reference - north * glm::dot(reference, north),
        glm::dvec3(1.0, 0.0, 0.0)
    );
}

inline glm::dvec3 planetEastAxisFromNorthAndPrime(
    const glm::dvec3& north,
    const glm::dvec3& prime
)
{
    return normalizePlanetOrientationVector(
        glm::cross(north, prime),
        glm::dvec3(0.0, 0.0, 1.0)
    );
}

inline PlanetBodyOrientation makePlanetBodyOrientation(
    double axialTiltDeg,
    double axisNodeDeg
)
{
    const double tilt =
        planetOrientationDegreesToRadians(axialTiltDeg);
    const double node =
        planetOrientationDegreesToRadians(axisNodeDeg);

    PlanetBodyOrientation orientation;

    orientation.north =
        normalizePlanetOrientationVector(
            glm::dvec3(
                std::sin(tilt) * std::cos(node),
                std::cos(tilt),
                std::sin(tilt) * std::sin(node)
            ),
            glm::dvec3(0.0, 1.0, 0.0)
        );

    orientation.prime =
        planetPrimeAxisFromNorth(orientation.north);

    orientation.east =
        planetEastAxisFromNorthAndPrime(
            orientation.north,
            orientation.prime
        );

    return orientation;
}

inline PlanetBodyOrientation makePlanetTextureOrientation(
    double axialTiltDeg,
    double axisNodeDeg,
    double rotationPhaseRad,
    double textureLongitudeOffsetDeg
)
{
    PlanetBodyOrientation orientation =
        makePlanetBodyOrientation(
            axialTiltDeg,
            axisNodeDeg
        );

    const double spin =
        rotationPhaseRad +
        planetOrientationDegreesToRadians(
            textureLongitudeOffsetDeg
        );

    const double cosine = std::cos(spin);
    const double sine = std::sin(spin);

    const glm::dvec3 prime0 = orientation.prime;
    const glm::dvec3 east0 = orientation.east;

    orientation.prime =
        normalizePlanetOrientationVector(
            prime0 * cosine + east0 * sine,
            prime0
        );

    orientation.east =
        normalizePlanetOrientationVector(
            -prime0 * sine + east0 * cosine,
            east0
        );

    return orientation;
}

inline glm::dvec3 worldDirectionToPlanetBody(
    const PlanetBodyOrientation& orientation,
    const glm::dvec3& worldDirection
)
{
    const glm::dvec3 direction =
        normalizePlanetOrientationVector(
            worldDirection,
            glm::dvec3(0.0, 0.0, 1.0)
        );

    return glm::dvec3(
        glm::dot(direction, orientation.prime),
        glm::dot(direction, orientation.north),
        glm::dot(direction, orientation.east)
    );
}
}
