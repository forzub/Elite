#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// Independent execution safety monitor (Step G). Not a second autopilot: it
// plans nothing and never changes the route or the program. Every tick it
// takes the command the pilot produced and predicts, over a short horizon,
// where that command takes the craft relative to its corridor (or narrow
// zone). It only intervenes when the command makes things unsafe:
//  - WARN: the predicted cross-track leaves the tolerance -> a main burn
//    whose side push points outward is inhibited;
//  - UNRECOVERABLE: even the full lateral authority can barely stop the
//    outward motion before the wall -> the full RCS pushes inward (the main
//    keeps only an inward-helping burn);
//  - inside a narrow zone an attitude error beyond the tolerance that is
//    still growing has its rotation stopped (no swing of the hull box toward
//    the walls).
// The pilot's own corridor logic acts first and earlier; in healthy flight
// the monitor stays silent.
struct ExecutionSafetyInput
{
    glm::dvec3 crossTrack {0.0};      // actual - reference, path-normal
    glm::dvec3 lateralVelocity {0.0}; // path-normal velocity
    glm::dvec3 tangent {0.0, 0.0, 1.0};
    glm::dvec3 commandedAcceleration {0.0}; // main delivered + RCS commanded
    // The reference's own path-normal acceleration (v^2 * curvature): the
    // prediction is relative to the curving reference.
    glm::dvec3 referenceLateralAcceleration {0.0};
    glm::dvec3 mainDelivered {0.0};
    double toleranceMeters = 0.0;
    // Full lateral authority available now (all RCS; plus main where the
    // hull can push sideways) - what an emergency could use.
    double lateralAuthorityMps2 = 0.0;
    double horizonSeconds = 4.0;
    bool narrow = false;
    double attitudeErrorRad = 0.0;
    double attitudeToleranceRad = 0.0;
    double attitudeErrorRateRadPerSec = 0.0; // > 0: growing
};

enum class ExecutionSafetyVerdict : int
{
    Safe = 0,
    Warn = 1,
    Unrecoverable = 2,
};

struct ExecutionSafetyResult
{
    ExecutionSafetyVerdict verdict = ExecutionSafetyVerdict::Safe;
    double predictedCrossTrackMeters = 0.0;
    double stoppingDistanceMeters = 0.0;
    double marginMeters = 0.0;
    bool inhibitMain = false;
    bool forceInwardRcs = false;
    bool stopRotation = false;
    glm::dvec3 inward {0.0};
};

[[nodiscard]] inline ExecutionSafetyResult evaluateExecutionSafety(
    const ExecutionSafetyInput& in) noexcept
{
    ExecutionSafetyResult out;
    if (!(in.toleranceMeters > 0.0))
        return out;
    const glm::dvec3 t = in.tangent;
    const glm::dvec3 lateralCommand =
        in.commandedAcceleration - t * glm::dot(in.commandedAcceleration, t) -
        in.referenceLateralAcceleration;
    const double T = std::max(0.0, in.horizonSeconds);
    const glm::dvec3 predicted =
        in.crossTrack + in.lateralVelocity * T + 0.5 * lateralCommand * T * T;
    out.predictedCrossTrackMeters = glm::length(predicted);
    const double distance = glm::length(in.crossTrack);
    out.marginMeters = in.toleranceMeters - distance;
    if (distance > 1.0e-6)
        out.inward = -in.crossTrack / distance;
    else if (out.predictedCrossTrackMeters > 1.0e-6)
        out.inward = -predicted / out.predictedCrossTrackMeters;
    const double outward =
        std::max(0.0, -glm::dot(in.lateralVelocity, out.inward));
    out.stoppingDistanceMeters =
        in.lateralAuthorityMps2 > 1.0e-9
            ? outward * outward / (2.0 * in.lateralAuthorityMps2)
            : (outward > 1.0e-6 ? 1.0e9 : 0.0);

    // Margin share the stopping distance may use before it counts as
    // unrecoverable (the pilot's own emergency acts long before).
    constexpr double UnrecoverableShare = 0.9;
    // Only a command that is predicted to leave the tolerance is judged; one
    // already bringing the craft back is left alone.
    if (out.predictedCrossTrackMeters > in.toleranceMeters)
        out.verdict =
            outward > 1.0e-6 &&
                    out.stoppingDistanceMeters >=
                        UnrecoverableShare * out.marginMeters
                ? ExecutionSafetyVerdict::Unrecoverable
                : ExecutionSafetyVerdict::Warn;

    if (out.verdict != ExecutionSafetyVerdict::Safe)
    {
        // A main burn pushing outward makes it worse.
        const glm::dvec3 mainLateral =
            in.mainDelivered - t * glm::dot(in.mainDelivered, t);
        out.inhibitMain = glm::dot(mainLateral, out.inward) < 0.0 &&
            glm::length(mainLateral) > 1.0e-6;
        out.forceInwardRcs =
            out.verdict == ExecutionSafetyVerdict::Unrecoverable;
    }
    out.stopRotation = in.narrow &&
        in.attitudeErrorRad > in.attitudeToleranceRad &&
        in.attitudeErrorRateRadPerSec > 0.0;
    return out;
}

} // namespace game::navigation::autopilot
