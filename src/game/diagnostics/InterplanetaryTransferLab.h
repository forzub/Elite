#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

namespace game::diagnostics
{

// A deterministic server-side interplanetary test actor.  The trajectory is
// the ideal two-body transfer ellipse whose apsides touch the circular Earth
// and Mars orbital radii used by the current Sol runtime.  It is deliberately
// diagnostic state: no planner/AI owns this motion.
inline constexpr bool InterplanetaryTransferLabEnabled = true;
inline constexpr std::uint64_t InterplanetaryTransferLabInstanceId = 9020;
inline constexpr const char* InterplanetaryTransferLabLabel =
    "MARS-EARTH TRANSFER TEST";
inline constexpr const char* InterplanetaryTransferLabSunBodyId =
    "system_0.Sol";
inline constexpr const char* InterplanetaryTransferLabMarsBodyId =
    "system_0.Sol.Марс";

inline constexpr double InterplanetaryAuMeters = 149597870700.0;
inline constexpr double InterplanetarySunMuM3s2 = 1.3271244e20;
inline constexpr double InterplanetaryEarthOrbitAu = 1.0;
inline constexpr double InterplanetaryMarsOrbitAu = 1.52;
inline constexpr double InterplanetaryMarsOrbitPeriodDays = 687.0;
inline constexpr double InterplanetaryInitialElapsedDays = 120.0;
inline constexpr double InterplanetarySecondsPerDay = 86400.0;

struct InterplanetaryTransferLabState
{
    glm::dvec3 relativePositionMeters {0.0};
    glm::dvec3 relativeVelocityMetersPerSecond {0.0};
    double heliocentricRadiusMeters = 0.0;
    double heliocentricSpeedMetersPerSecond = 0.0;
    double transferElapsedSeconds = 0.0;
    double transferDurationSeconds = 0.0;
    bool arrived = false;
};

inline double solveTransferEccentricAnomaly(
    double meanAnomaly,
    double eccentricity
)
{
    double eccentricAnomaly = meanAnomaly;

    // The transfer eccentricity is only ~0.206, so Newton converges very
    // quickly from E=M.  Eight iterations are ample and remain deterministic.
    for (int i = 0; i < 8; ++i)
    {
        const double f =
            eccentricAnomaly -
            eccentricity * std::sin(eccentricAnomaly) -
            meanAnomaly;
        const double derivative =
            1.0 - eccentricity * std::cos(eccentricAnomaly);

        eccentricAnomaly -= f / derivative;
    }

    return eccentricAnomaly;
}

inline glm::dvec3 rotateTransferPhase(
    const glm::dvec3& value,
    double phaseOffsetRad
)
{
    const double c = std::cos(phaseOffsetRad);
    const double s = std::sin(phaseOffsetRad);
    return glm::dvec3(
        c * value.x + s * value.z,
        value.y,
        -s * value.x + c * value.z
    );
}

inline InterplanetaryTransferLabState evaluateInterplanetaryTransferLab(
    double elapsedSinceSpawnSeconds,
    double departurePhaseRad = 3.14159265358979323846
)
{
    constexpr double pi = 3.14159265358979323846;

    const double earthRadiusMeters =
        InterplanetaryEarthOrbitAu * InterplanetaryAuMeters;
    const double marsRadiusMeters =
        InterplanetaryMarsOrbitAu * InterplanetaryAuMeters;
    const double semiMajorAxisMeters =
        0.5 * (earthRadiusMeters + marsRadiusMeters);
    const double eccentricity =
        (marsRadiusMeters - earthRadiusMeters) /
        (marsRadiusMeters + earthRadiusMeters);
    const double meanMotionRadPerSecond =
        std::sqrt(
            InterplanetarySunMuM3s2 /
            (semiMajorAxisMeters * semiMajorAxisMeters * semiMajorAxisMeters)
        );
    const double transferDurationSeconds =
        pi / meanMotionRadPerSecond;

    const double elapsedSeconds = std::clamp(
        InterplanetaryInitialElapsedDays * InterplanetarySecondsPerDay +
            std::max(0.0, elapsedSinceSpawnSeconds),
        0.0,
        transferDurationSeconds
    );

    // Aphelion (Mars radius) is E=pi.  Advancing one half orbital period to
    // E=2*pi reaches perihelion (Earth radius).
    const double meanAnomaly =
        pi + meanMotionRadPerSecond * elapsedSeconds;
    const double eccentricAnomaly =
        solveTransferEccentricAnomaly(meanAnomaly, eccentricity);
    const double oneMinusECosE =
        1.0 - eccentricity * std::cos(eccentricAnomaly);
    const double eccentricAnomalyRate =
        meanMotionRadPerSecond / oneMinusECosE;
    const double minorScale =
        std::sqrt(1.0 - eccentricity * eccentricity);

    InterplanetaryTransferLabState out;
    // CelestialSystemRuntime defines prograde motion as increasing orbital
    // phase: +X -> -Z.  The diagnostic transfer must use the same handedness;
    // the old +sin(E) Z term made the Mars->Earth actor retrograde.
    const glm::dvec3 unrotatedPosition(
        semiMajorAxisMeters *
            (std::cos(eccentricAnomaly) - eccentricity),
        0.0,
        -semiMajorAxisMeters * minorScale *
            std::sin(eccentricAnomaly)
    );
    const glm::dvec3 unrotatedVelocity(
        -semiMajorAxisMeters *
            std::sin(eccentricAnomaly) * eccentricAnomalyRate,
        0.0,
        -semiMajorAxisMeters * minorScale *
            std::cos(eccentricAnomaly) * eccentricAnomalyRate
    );

    // The canonical ellipse starts at aphelion phase pi. Rotate it so its
    // synthetic departure point coincides with Mars' phase at departure.
    const double phaseOffsetRad = departurePhaseRad - pi;
    out.relativePositionMeters =
        rotateTransferPhase(unrotatedPosition, phaseOffsetRad);
    out.relativeVelocityMetersPerSecond =
        rotateTransferPhase(unrotatedVelocity, phaseOffsetRad);
    out.heliocentricRadiusMeters =
        glm::length(out.relativePositionMeters);
    out.heliocentricSpeedMetersPerSecond =
        glm::length(out.relativeVelocityMetersPerSecond);
    out.transferElapsedSeconds = elapsedSeconds;
    out.transferDurationSeconds = transferDurationSeconds;
    out.arrived = elapsedSeconds >= transferDurationSeconds - 1.0e-6;
    return out;
}

} // namespace game::diagnostics
