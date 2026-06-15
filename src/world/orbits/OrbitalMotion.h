#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace world::orbits
{

struct OrbitalMotion
{
    bool enabled = false;

    glm::dvec3 centerMeters {0.0};

    double parentRadiusMeters = 0.0;
    double altitudeMeters = 0.0;

    double orbitalPeriodSeconds = 1.0;
    double selfRotationPeriodSeconds = 1.0;

    double inclinationDeg = 0.0;
    double longitudeOfAscendingNodeDeg = 0.0;
    double argumentOfPeriapsisDeg = 0.0;
    double initialPhaseDeg = 0.0;

    double epochSeconds = 0.0;
};

inline double degToRad(double deg)
{
    return deg * 3.14159265358979323846 / 180.0;
}




inline double computeCircularOrbitPeriodSeconds(
    double parentRadiusMeters,
    double altitudeMeters,
    double parentGravitationalParameterM3s2
)
{
    if (parentGravitationalParameterM3s2 <= 0.0)
        return 1.0;

    const double orbitRadiusMeters =
        std::max(
            1.0,
            parentRadiusMeters + altitudeMeters
        );

    return
        2.0 *
        3.14159265358979323846 *
        std::sqrt(
            (orbitRadiusMeters * orbitRadiusMeters * orbitRadiusMeters) /
            parentGravitationalParameterM3s2
        );
}





inline glm::dvec3 computeOrbitPositionMeters(
    const OrbitalMotion& motion,
    double universeTimeSeconds
)
{
    const double radius =
        motion.parentRadiusMeters + motion.altitudeMeters;

    const double period =
        std::max(1.0, motion.orbitalPeriodSeconds);

    const double phase =
        degToRad(motion.initialPhaseDeg) +
        2.0 * 3.14159265358979323846 *
        ((universeTimeSeconds - motion.epochSeconds) / period);

    const glm::dvec3 local {
        std::cos(phase) * radius,
        0.0,
        std::sin(phase) * radius
    };

    glm::dmat4 rot(1.0);

    rot = glm::rotate(
        rot,
        degToRad(motion.longitudeOfAscendingNodeDeg),
        glm::dvec3(0.0, 1.0, 0.0)
    );

    rot = glm::rotate(
        rot,
        degToRad(motion.inclinationDeg),
        glm::dvec3(1.0, 0.0, 0.0)
    );

    rot = glm::rotate(
        rot,
        degToRad(motion.argumentOfPeriapsisDeg),
        glm::dvec3(0.0, 1.0, 0.0)
    );

    return motion.centerMeters + glm::dvec3(rot * glm::dvec4(local, 1.0));
}







inline glm::dvec3 computeOrbitVelocityMetersPerSecond(
    const OrbitalMotion& motion,
    double universeTimeSeconds
)
{
    // Численная производная.
    // Так мы не дублируем математику наклонов/узлов/аргумента перицентра.
    // Для игровой симуляции этого достаточно и архитектурно безопасно.
    constexpr double h = 0.25;

    const glm::dvec3 p0 =
        computeOrbitPositionMeters(
            motion,
            universeTimeSeconds - h
        );

    const glm::dvec3 p1 =
        computeOrbitPositionMeters(
            motion,
            universeTimeSeconds + h
        );

    return (p1 - p0) / (2.0 * h);
}
















inline glm::mat4 computeSelfRotation(
    const OrbitalMotion& motion,
    double universeTimeSeconds
)
{
    const double period =
        std::max(1.0, motion.selfRotationPeriodSeconds);

    const double angle =
        2.0 * 3.14159265358979323846 *
        ((universeTimeSeconds - motion.epochSeconds) / period);

    return glm::rotate(
        glm::mat4(1.0f),
        static_cast<float>(angle),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

} // namespace world::orbits