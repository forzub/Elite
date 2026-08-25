#include "src/world/navigation/TrajectoryGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/gtc/constants.hpp>

#include "src/world/navigation/SmoothPathOptimizer.h"
#include "src/world/navigation/NavigationOrientation.h"

namespace world::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;

struct CurvePoint
{
    glm::dvec3 positionMeters {0.0};
    double sourceProgressMeters = 0.0;
};

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite3(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double length2 = glm::dot(value, value);
    if (!finite(length2) || length2 <= Epsilon)
        return fallback;
    return value / std::sqrt(length2);
}

std::vector<double> sourceProgress(const std::vector<glm::dvec3>& points)
{
    std::vector<double> progress(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        progress[i] = progress[i - 1] +
            glm::length(points[i] - points[i - 1]);
    }
    return progress;
}

std::vector<CurvePoint> globalSmoothCurve(
    const TrajectoryGenerationRequest& request,
    TrajectoryGenerationDiagnostics& diagnostics
)
{
    SmoothPathRequest smooth;
    smooth.pathPointsMeters = request.pathPointsMeters;
    smooth.obstacles = request.obstacles;
    smooth.vehicle = request.vehicle;
    smooth.maxSampleSpacingMeters = request.sampleSpacingMeters;
    smooth.maxChordErrorMeters = request.maxCurveChordErrorMeters;
    smooth.maxSupportLevel = request.maxSmoothSupportLevel;
    smooth.terminalAllowedObstacleId = request.terminalAllowedObstacleId;
    smooth.terminalObstacleEntrySourceProgressMeters =
        request.terminalObstacleEntrySourceProgressMeters;

    const auto result = SmoothPathOptimizer::optimize(smooth);
    diagnostics.smoothCandidatesEvaluated =
        result.diagnostics.candidatesEvaluated;
    diagnostics.smoothSafeCandidates = result.diagnostics.safeCandidates;
    diagnostics.selectedSmoothSupportLevel =
        result.diagnostics.selectedSupportLevel;
    diagnostics.coarsePathLengthMeters =
        result.diagnostics.coarseLengthMeters;
    diagnostics.optimizedPathLengthMeters =
        result.diagnostics.optimizedLengthMeters;
    diagnostics.maxCurvaturePerMeter =
        result.diagnostics.maxCurvaturePerMeter;
    diagnostics.curvatureVariation =
        result.diagnostics.curvatureVariation;
    diagnostics.smoothingFellBackToPolyline =
        result.diagnostics.fellBackToPolyline;

    std::vector<CurvePoint> curve;
    curve.reserve(result.points.size());
    for (const auto& point : result.points)
        curve.push_back({point.positionMeters, point.sourceProgressMeters});
    return curve;
}

std::vector<double> curveProgress(const std::vector<CurvePoint>& points)
{
    std::vector<double> progress(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        progress[i] = progress[i - 1] +
            glm::length(points[i].positionMeters - points[i - 1].positionMeters);
    }
    return progress;
}

std::vector<glm::dvec3> tangents(const std::vector<CurvePoint>& points)
{
    std::vector<glm::dvec3> out(points.size(), glm::dvec3(1.0, 0.0, 0.0));
    if (points.size() < 2)
        return out;

    for (std::size_t i = 0; i < points.size(); ++i)
    {
        glm::dvec3 delta(0.0);
        if (i == 0)
            delta = points[1].positionMeters - points[0].positionMeters;
        else if (i + 1 == points.size())
            delta = points[i].positionMeters - points[i - 1].positionMeters;
        else
            delta = points[i + 1].positionMeters - points[i - 1].positionMeters;
        out[i] = normalizedOr(
            delta,
            i > 0 ? out[i - 1] : glm::dvec3(1.0, 0.0, 0.0)
        );
    }
    return out;
}

std::size_t nearestSourceProgressIndex(
    const std::vector<CurvePoint>& points,
    double sourceProgressMeters
)
{
    std::size_t best = 0;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const double distance = std::abs(
            points[i].sourceProgressMeters - sourceProgressMeters
        );
        if (distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

std::vector<double> speedLimits(
    const TrajectoryGenerationRequest& request,
    const std::vector<CurvePoint>& points,
    const std::vector<double>& pathProgress,
    const std::vector<glm::dvec3>& tangent
)
{
    std::vector<double> limits(points.size(), request.vehicle.maxSpeedMps);

    // Curvature speed ceiling from centripetal acceleration a=v^2*kappa.
    for (std::size_t i = 1; i + 1 < points.size(); ++i)
    {
        const double dsA = pathProgress[i] - pathProgress[i - 1];
        const double dsB = pathProgress[i + 1] - pathProgress[i];
        const double span = std::max(Epsilon, 0.5 * (dsA + dsB));
        const double dotValue = std::clamp(
            glm::dot(tangent[i - 1], tangent[i + 1]),
            -1.0,
            1.0
        );
        const double angle = std::acos(dotValue);
        const double curvature = angle / std::max(Epsilon, 2.0 * span);
        if (curvature > 1.0e-9)
        {
            const double curveLimit = std::sqrt(
                std::max(0.0, request.vehicle.maxLateralAccelerationMps2) /
                curvature
            );
            // Small reserve keeps finite-difference acceleration below the
            // authored lateral envelope instead of exactly grazing it.
            limits[i] = std::min(limits[i], curveLimit * 0.92);
        }
    }

    for (const auto& range : request.speedLimitRanges)
    {
        const double lo = std::min(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        const double hi = std::max(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            if (points[i].sourceProgressMeters >= lo - 1.0e-7 &&
                points[i].sourceProgressMeters <= hi + 1.0e-7)
            {
                limits[i] = std::min(
                    limits[i],
                    std::max(0.0, range.maxSpeedMps)
                );
            }
        }
    }

    for (const auto& constraint : request.pointSpeedConstraints)
    {
        if (points.empty())
            break;
        const std::size_t index = nearestSourceProgressIndex(
            points,
            constraint.sourcePathProgressMeters
        );
        limits[index] = std::min(
            limits[index],
            std::max(0.0, constraint.maxSpeedMps)
        );
    }

    return limits;
}

glm::dvec3 angularVelocityBetween(
    glm::dquat a,
    glm::dquat b,
    double dt
)
{
    if (dt <= Epsilon)
        return glm::dvec3(0.0);
    a = glm::normalize(a);
    b = glm::normalize(b);
    glm::dquat delta = glm::normalize(b * glm::conjugate(a));
    if (delta.w < 0.0)
        delta = -delta;

    const double w = std::clamp(delta.w, -1.0, 1.0);
    const double angle = 2.0 * std::acos(w);
    const double sinHalf = std::sqrt(std::max(0.0, 1.0 - w * w));
    if (angle <= Epsilon || sinHalf <= Epsilon)
        return glm::dvec3(0.0);
    const glm::dvec3 axis(delta.x, delta.y, delta.z);
    return axis / sinHalf * (angle / dt);
}

bool validateRequest(const TrajectoryGenerationRequest& request)
{
    if (request.systemId < 0 || request.frameId.empty() ||
        !finite(request.startUniverseTimeSeconds) ||
        !finite(request.universeTimeScale) || request.universeTimeScale < 0.0 ||
        request.pathPointsMeters.size() < 2 || !request.vehicle.valid() ||
        !finite3(request.initialVelocityMps) ||
        !finite(request.sampleSpacingMeters) ||
        request.sampleSpacingMeters <= 0.0 ||
        !finite(request.maxCurveChordErrorMeters) ||
        request.maxCurveChordErrorMeters <= 0.0)
    {
        return false;
    }

    for (const auto& point : request.pathPointsMeters)
    {
        if (!finite3(point))
            return false;
    }
    return true;
}

} // namespace

TrajectoryGenerationResult TrajectoryGenerator::generate(
    const TrajectoryGenerationRequest& request
)
{
    TrajectoryGenerationResult out;
    out.trajectory.systemId = request.systemId;
    out.trajectory.frameId = request.frameId;
    out.trajectory.startUniverseTimeSeconds = request.startUniverseTimeSeconds;

    if (!validateRequest(request))
    {
        out.trajectory.status = TrajectoryStatus::InvalidRequest;
        out.trajectory.message = "invalid trajectory generation request";
        return out;
    }

    const auto rawProgress = sourceProgress(request.pathPointsMeters);
    const double rawLength = rawProgress.back();
    if (!finite(rawLength) || rawLength <= Epsilon)
    {
        out.trajectory.status = TrajectoryStatus::InvalidRequest;
        out.trajectory.message = "trajectory source path has zero length";
        return out;
    }

    const auto curve = globalSmoothCurve(request, out.diagnostics);
    if (curve.size() < 2)
    {
        out.trajectory.status = TrajectoryStatus::NoSafePath;
        out.trajectory.message = "no collision-free globally smooth trajectory path";
        return out;
    }

    const auto progress = curveProgress(curve);
    const auto tangent = tangents(curve);
    auto limits = speedLimits(request, curve, progress, tangent);
    std::vector<double> speeds(curve.size(), 0.0);

    // Backward pass: every point receives the maximum speed from which the
    // next downstream constraint can still be reached with real braking.
    std::vector<double> downstream = limits;
    const double braking = request.vehicle.maxBrakingAccelerationMps2;
    for (std::size_t i = curve.size() - 1; i-- > 0; )
    {
        const double ds = progress[i + 1] - progress[i];
        const double reachable = std::sqrt(std::max(
            0.0,
            downstream[i + 1] * downstream[i + 1] + 2.0 * braking * ds
        ));
        downstream[i] = std::min(downstream[i], reachable);
    }

    const double along = glm::dot(request.initialVelocityMps, tangent.front());
    const double initialAlong = std::max(0.0, along);
    const glm::dvec3 alongVector = tangent.front() * along;
    const double crossTrack = glm::length(
        request.initialVelocityMps - alongVector
    );
    out.diagnostics.initialAlongPathSpeedMps = initialAlong;
    out.diagnostics.initialCrossTrackSpeedMps = crossTrack;
    out.diagnostics.pathCaptureRequired =
        crossTrack > 0.25 || along < -0.25;

    // Reverse motion relative to the route or a large side-slip is a follower
    // capture problem. Stage 5A does not pretend that projecting the velocity
    // changes physics; it exposes the diagnostic and parameterizes the forward
    // route from the non-negative along-path component.
    if (initialAlong > downstream.front() + 1.0e-6)
    {
        out.trajectory.status = TrajectoryStatus::InitialStateInfeasible;
        out.trajectory.message =
            "initial along-path speed cannot meet downstream constraints";
        return out;
    }

    speeds.front() = std::min(initialAlong, downstream.front());
    const double acceleration = request.vehicle.maxForwardAccelerationMps2;
    for (std::size_t i = 1; i < curve.size(); ++i)
    {
        const double ds = progress[i] - progress[i - 1];
        const double reachable = std::sqrt(std::max(
            0.0,
            speeds[i - 1] * speeds[i - 1] + 2.0 * acceleration * ds
        ));
        speeds[i] = std::min(downstream[i], reachable);
    }

    out.trajectory.samples.resize(curve.size());
    double timeOffset = 0.0;
    for (std::size_t i = 0; i < curve.size(); ++i)
    {
        auto& sample = out.trajectory.samples[i];
        if (i > 0)
        {
            const double ds = progress[i] - progress[i - 1];
            const double speedSum = speeds[i - 1] + speeds[i];
            double dt = 0.0;
            if (speedSum > 1.0e-7)
            {
                dt = 2.0 * ds / speedSum;
            }
            else
            {
                // This can only occur for two authored zero-speed hard points.
                // Allocate a finite conservative dwell/translation time rather
                // than creating NaN/Inf timestamps.
                const double fallbackAccel = std::max(
                    Epsilon,
                    std::min(acceleration, braking)
                );
                dt = 2.0 * std::sqrt(std::max(0.0, ds) / fallbackAccel);
            }
            if (!finite(dt) || dt <= 0.0)
            {
                out.trajectory.samples.clear();
                out.trajectory.status = TrajectoryStatus::NumericalFailure;
                out.trajectory.message = "trajectory time parameterization failed";
                return out;
            }
            timeOffset += dt;
        }

        sample.timeOffsetSeconds = timeOffset;
        sample.universeTimeSeconds = request.startUniverseTimeSeconds +
            timeOffset * request.universeTimeScale;
        sample.pathProgressMeters = progress[i];
        sample.sourcePathProgressMeters = curve[i].sourceProgressMeters;
        sample.positionMeters = curve[i].positionMeters;
        sample.speedMps = speeds[i];
        sample.velocityMps = tangent[i] * speeds[i];
    }

    // Deterministic visible attitude: nose follows the local trajectory. A
    // terminal docking pose can blend in near the end, while angular control
    // feasibility remains a Stage 5B/follower concern rather than being hidden.
    glm::dvec3 upHint(0.0, 1.0, 0.0);
    const glm::dquat terminalOrientation = orientationForForwardUp(
        request.terminalForward,
        request.terminalUp
    );
    for (std::size_t i = 0; i < out.trajectory.samples.size(); ++i)
    {
        auto& sample = out.trajectory.samples[i];
        glm::dquat orientation = orientationForForwardUp(tangent[i], upHint);
        upHint = orientation * glm::dvec3(0.0, 1.0, 0.0);

        if (request.hasTerminalOrientation &&
            request.terminalOrientationBlendDistanceMeters > Epsilon)
        {
            const double remaining =
                progress.back() - sample.pathProgressMeters;
            const double u = std::clamp(
                1.0 - remaining /
                    request.terminalOrientationBlendDistanceMeters,
                0.0,
                1.0
            );
            const double smooth = u * u * (3.0 - 2.0 * u);
            orientation = glm::normalize(glm::slerp(
                orientation,
                terminalOrientation,
                smooth
            ));
        }
        sample.orientation = orientation;
    }
    if (request.hasTerminalOrientation)
        out.trajectory.samples.back().orientation = terminalOrientation;

    for (std::size_t i = 0; i < out.trajectory.samples.size(); ++i)
    {
        auto& sample = out.trajectory.samples[i];
        if (i == 0)
        {
            if (out.trajectory.samples.size() > 1)
            {
                const double dt = out.trajectory.samples[1].timeOffsetSeconds -
                    sample.timeOffsetSeconds;
                sample.accelerationMps2 = dt > Epsilon
                    ? (out.trajectory.samples[1].velocityMps - sample.velocityMps) / dt
                    : glm::dvec3(0.0);
                sample.angularVelocityRadPerSecond = dt > Epsilon
                    ? angularVelocityBetween(
                        sample.orientation,
                        out.trajectory.samples[1].orientation,
                        dt
                      )
                    : glm::dvec3(0.0);
            }
        }
        else
        {
            const auto& previous = out.trajectory.samples[i - 1];
            const double dt = sample.timeOffsetSeconds -
                previous.timeOffsetSeconds;
            sample.accelerationMps2 = dt > Epsilon
                ? (sample.velocityMps - previous.velocityMps) / dt
                : glm::dvec3(0.0);
            sample.angularVelocityRadPerSecond = dt > Epsilon
                ? angularVelocityBetween(previous.orientation, sample.orientation, dt)
                : glm::dvec3(0.0);
        }

        if (!finite3(sample.positionMeters) || !finite3(sample.velocityMps) ||
            !finite3(sample.accelerationMps2) ||
            !finite3(sample.angularVelocityRadPerSecond) ||
            !finite(sample.universeTimeSeconds) || !finite(sample.speedMps))
        {
            out.trajectory.samples.clear();
            out.trajectory.status = TrajectoryStatus::NumericalFailure;
            out.trajectory.message = "trajectory contains non-finite sample";
            return out;
        }

        out.diagnostics.maxSpeedMps = std::max(
            out.diagnostics.maxSpeedMps,
            sample.speedMps
        );
        out.diagnostics.maxAccelerationMps2 = std::max(
            out.diagnostics.maxAccelerationMps2,
            glm::length(sample.accelerationMps2)
        );
        out.diagnostics.maxAngularVelocityRadPerSecond = std::max(
            out.diagnostics.maxAngularVelocityRadPerSecond,
            glm::length(sample.angularVelocityRadPerSecond)
        );
    }

    out.trajectory.durationSeconds = timeOffset;
    out.trajectory.lengthMeters = progress.back();
    out.trajectory.status = TrajectoryStatus::Ready;
    out.trajectory.message = out.diagnostics.smoothingFellBackToPolyline
        ? "safe time-parameterized polyline trajectory"
        : "safe global B-spline time-parameterized trajectory";
    return out;
}

} // namespace world::navigation
