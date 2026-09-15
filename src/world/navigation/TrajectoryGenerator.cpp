#include "src/world/navigation/TrajectoryGenerator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string_view>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/RuckigRoutePlanner.h"
#include "src/game/navigation/RuckigTrajectorySolver.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/world/navigation/NavigationOrientation.h"
#include "src/world/navigation/NavigationPerfLog.h"

namespace
{
constexpr double Epsilon = 1.0e-9;
using Clock = std::chrono::steady_clock;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double magnitude(const glm::dvec3& value) noexcept
{
    return std::sqrt(glm::dot(value, value));
}

double elapsedMilliseconds(Clock::time_point begin, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

world::navigation::TrajectoryGenerationResult failure(
    const world::navigation::TrajectoryGenerationRequest& request,
    world::navigation::TrajectoryStatus status,
    const char* message
)
{
    world::navigation::TrajectoryGenerationResult out;
    out.trajectory.status = status;
    out.trajectory.systemId = request.systemId;
    out.trajectory.frameId = request.frameId;
    out.trajectory.startUniverseTimeSeconds = request.startUniverseTimeSeconds;
    out.trajectory.message = message ? message : "Ruckig route planning failed";
    return out;
}

bool validRequest(
    const world::navigation::TrajectoryGenerationRequest& request
) noexcept
{
    if (request.systemId < 0 || request.frameId.empty() ||
        !finite(request.startUniverseTimeSeconds) ||
        !finite(request.universeTimeScale) || request.universeTimeScale <= 0.0 ||
        !request.vehicle.valid() || request.pathPointsMeters.size() < 2 ||
        !finite(request.initialVelocityMps))
    {
        return false;
    }

    for (const auto& point : request.pathPointsMeters)
    {
        if (!finite(point))
            return false;
    }
    return true;
}

std::vector<double> sourceProgressTable(
    const std::vector<glm::dvec3>& points
)
{
    std::vector<double> out(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
        out[i] = out[i - 1] + magnitude(points[i] - points[i - 1]);
    return out;
}

double positiveMinimum(std::initializer_list<double> values)
{
    double out = std::numeric_limits<double>::infinity();
    for (double value : values)
    {
        if (finite(value) && value > Epsilon)
            out = std::min(out, value);
    }
    return finite(out) ? out : 0.0;
}

double accelerationBudget(
    const world::navigation::NavigationVehicleProfile& vehicle
)
{
    return positiveMinimum({
        vehicle.maxForwardAccelerationMps2,
        vehicle.maxBrakingAccelerationMps2,
        vehicle.maxLateralAccelerationMps2
    });
}

double segmentSpeedLimit(
    const world::navigation::TrajectoryGenerationRequest& request,
    double sourceStart,
    double sourceEnd
)
{
    double limit = request.vehicle.maxSpeedMps;
    for (const auto& range : request.speedLimitRanges)
    {
        if (!finite(range.sourcePathStartMeters) ||
            !finite(range.sourcePathEndMeters) ||
            !finite(range.maxSpeedMps) || range.maxSpeedMps <= 0.0)
        {
            continue;
        }

        const double begin = std::min(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        const double end = std::max(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        if (end + Epsilon < sourceStart || begin - Epsilon > sourceEnd)
            continue;
        limit = std::min(limit, range.maxSpeedMps);
    }
    return std::max(0.1, limit);
}

double estimateStopToStopSeconds(
    double distanceMeters,
    double maxSpeedMps,
    double accelerationMps2,
    double currentSpeedMps
)
{
    const double distance = std::max(0.0, distanceMeters);
    const double speed = std::max(0.1, maxSpeedMps);
    const double acceleration = std::max(0.1, accelerationMps2);

    const double distanceForAccelAndBrake = speed * speed / acceleration;
    double ideal = 0.0;
    if (distance <= distanceForAccelAndBrake)
        ideal = 2.0 * std::sqrt(distance / acceleration);
    else
        ideal = 2.0 * speed / acceleration +
            (distance - distanceForAccelAndBrake) / speed;

    // The first leg may inherit arbitrary player/NPC velocity. Give Ruckig
    // explicit room to capture that state instead of hiding a second path
    // smoother in front of the solver.
    ideal += std::max(0.0, currentSpeedMps) / acceleration;
    return std::max(0.5, ideal * 1.20 + 0.25);
}

bool violatesKnownStopDistance(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& sourceProgress
)
{
    if (request.pathPointsMeters.size() < 2)
        return true;

    double stopProgress = std::numeric_limits<double>::infinity();
    for (const auto& constraint : request.pointSpeedConstraints)
    {
        if (finite(constraint.sourcePathProgressMeters) &&
            finite(constraint.maxSpeedMps) &&
            constraint.maxSpeedMps <= Epsilon &&
            constraint.sourcePathProgressMeters >= 0.0)
        {
            stopProgress = std::min(
                stopProgress,
                constraint.sourcePathProgressMeters
            );
        }
    }
    if (!finite(stopProgress))
        return false;

    const glm::dvec3 firstDelta =
        request.pathPointsMeters[1] - request.pathPointsMeters[0];
    const double firstLength = magnitude(firstDelta);
    if (firstLength <= Epsilon)
        return false;

    const glm::dvec3 firstDirection = firstDelta / firstLength;
    const double alongSpeed = std::max(
        0.0,
        glm::dot(request.initialVelocityMps, firstDirection)
    );
    const double brake = request.vehicle.maxBrakingAccelerationMps2;
    if (brake <= Epsilon)
        return alongSpeed > Epsilon;

    const double stoppingDistance = alongSpeed * alongSpeed / (2.0 * brake);
    const double availableDistance = std::clamp(
        stopProgress,
        0.0,
        sourceProgress.empty() ? 0.0 : sourceProgress.back()
    );
    return stoppingDistance > availableDistance + 1.0e-6;
}

double segmentSourceProgress(
    const glm::dvec3& position,
    const glm::dvec3& a,
    const glm::dvec3& b,
    double sourceStart,
    double sourceEnd
)
{
    const glm::dvec3 delta = b - a;
    const double length2 = glm::dot(delta, delta);
    if (length2 <= Epsilon)
        return sourceEnd;
    const double u = std::clamp(
        glm::dot(position - a, delta) / length2,
        0.0,
        1.0
    );
    return sourceStart + (sourceEnd - sourceStart) * u;
}

double smoothStep01(double value)
{
    const double t = std::clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

glm::dquat sampleOrientation(
    const world::navigation::TrajectoryGenerationRequest& request,
    const glm::dvec3& velocity,
    const glm::dvec3& segmentDirection,
    double sourceProgress,
    double totalSourceProgress
)
{
    glm::dvec3 forward = magnitude(velocity) > 0.25
        ? velocity
        : segmentDirection;
    if (magnitude(forward) <= Epsilon)
        forward = glm::dvec3(0.0, 0.0, -1.0);
    else
        forward = glm::normalize(forward);

    glm::dvec3 upHint(0.0, 1.0, 0.0);
    if (request.hasTerminalOrientation &&
        magnitude(request.terminalUp) > Epsilon)
    {
        upHint = glm::normalize(request.terminalUp);
    }

    glm::dquat orientation =
        world::navigation::orientationForForwardUp(forward, upHint);

    if (request.hasTerminalOrientation)
    {
        const glm::dquat terminal =
            world::navigation::orientationForForwardUp(
                request.terminalForward,
                request.terminalUp
            );
        const double blendDistance = std::max(
            0.0,
            request.terminalOrientationBlendDistanceMeters
        );
        double blend = 0.0;
        if (blendDistance > Epsilon)
        {
            const double remaining = std::max(
                0.0,
                totalSourceProgress - sourceProgress
            );
            blend = smoothStep01(1.0 - remaining / blendDistance);
        }
        else if (sourceProgress + Epsilon >= totalSourceProgress)
        {
            blend = 1.0;
        }
        if (glm::dot(orientation, terminal) < 0.0)
            orientation = -orientation;
        orientation = glm::normalize(glm::slerp(
            orientation,
            terminal,
            blend
        ));
    }

    return orientation;
}

bool validateSweptLeg(
    const world::navigation::TrajectoryGenerationRequest& request,
    const game::navigation::TrajectoryPredictionResult& prediction,
    const glm::dvec3& a,
    const glm::dvec3& b,
    double sourceStart,
    double sourceEnd,
    std::size_t& checkedSegments
)
{
    if (prediction.samples.size() < 2)
        return false;

    for (std::size_t i = 1; i < prediction.samples.size(); ++i)
    {
        const auto& previous = prediction.samples[i - 1].state.positionMeters;
        const auto& current = prediction.samples[i].state.positionMeters;
        const double previousProgress = segmentSourceProgress(
            previous,
            a,
            b,
            sourceStart,
            sourceEnd
        );
        const double currentProgress = segmentSourceProgress(
            current,
            a,
            b,
            sourceStart,
            sourceEnd
        );
        const double conservativeProgress = std::min(
            previousProgress,
            currentProgress
        );
        const bool legalTargetIngress =
            !request.terminalAllowedObstacleId.empty() &&
            conservativeProgress + 1.0e-7 >=
                request.terminalObstacleEntrySourceProgressMeters;

        ++checkedSegments;
        if (!world::navigation::segmentClearOfNavigationObstacles(
                previous,
                current,
                request.obstacles,
                request.vehicle.collisionRadiusMeters,
                request.vehicle.preferredClearanceMeters,
                legalTargetIngress
                    ? std::string_view(request.terminalAllowedObstacleId)
                    : std::string_view{}))
        {
            return false;
        }
    }
    return true;
}

void computeCurvatureDiagnostics(
    world::navigation::TrajectoryGenerationResult& result
)
{
    auto& samples = result.trajectory.samples;
    double maxCurvature = 0.0;
    double previousCurvature = 0.0;
    double variation = 0.0;
    bool havePrevious = false;

    for (std::size_t i = 1; i + 1 < samples.size(); ++i)
    {
        const glm::dvec3 first =
            samples[i].positionMeters - samples[i - 1].positionMeters;
        const glm::dvec3 second =
            samples[i + 1].positionMeters - samples[i].positionMeters;
        const double firstLength = magnitude(first);
        const double secondLength = magnitude(second);
        if (firstLength <= Epsilon || secondLength <= Epsilon)
            continue;

        const double cosine = std::clamp(
            glm::dot(first / firstLength, second / secondLength),
            -1.0,
            1.0
        );
        const double angle = std::acos(cosine);
        const double curvature = angle /
            std::max(Epsilon, 0.5 * (firstLength + secondLength));
        maxCurvature = std::max(maxCurvature, curvature);
        if (havePrevious)
            variation += std::abs(curvature - previousCurvature);
        previousCurvature = curvature;
        havePrevious = true;
    }

    result.diagnostics.maxCurvaturePerMeter = maxCurvature;
    result.diagnostics.curvatureVariation = variation;
}

void appendPerfLog(
    const world::navigation::TrajectoryGenerationRequest& request,
    const world::navigation::TrajectoryGenerationResult& result,
    double totalMs
)
{
    std::ofstream out(
        world::navigation::navigationPerfLogPath(),
        std::ios::app
    );
    if (!out)
        return;

    out << std::fixed << std::setprecision(4)
        << "[RuckigRoutePerf] total_ms=" << totalMs
        << " legs=" << result.diagnostics.ruckigLegAttempts
        << " ruckig_ok=" << result.diagnostics.ruckigLegSuccesses
        << " ruckig_ms=" << result.diagnostics.ruckigSolveMilliseconds
        << " collision_segments=" << result.diagnostics.collisionSegmentsChecked
        << " coarse_points=" << request.pathPointsMeters.size()
        << " obstacles=" << request.obstacles.size()
        << " samples=" << result.trajectory.samples.size()
        << " valid=" << (result.ready() ? 1 : 0)
        << '\n';
}

} // namespace

namespace game::navigation
{

world::navigation::TrajectoryGenerationResult RuckigRoutePlanner::plan(
    const world::navigation::TrajectoryGenerationRequest& request
)
{
    const auto totalStart = Clock::now();

    if (!validRequest(request))
        return failure(
            request,
            world::navigation::TrajectoryStatus::InvalidRequest,
            "invalid Ruckig route request"
        );

    const auto sourceProgress = sourceProgressTable(request.pathPointsMeters);
    if (sourceProgress.back() <= Epsilon)
        return failure(
            request,
            world::navigation::TrajectoryStatus::InvalidRequest,
            "Ruckig route has zero length"
        );

    if (violatesKnownStopDistance(request, sourceProgress))
        return failure(
            request,
            world::navigation::TrajectoryStatus::InitialStateInfeasible,
            "initial along-path speed cannot meet downstream constraints"
        );

    world::navigation::TrajectoryGenerationResult out;
    out.trajectory.systemId = request.systemId;
    out.trajectory.frameId = request.frameId;
    out.trajectory.startUniverseTimeSeconds = request.startUniverseTimeSeconds;
    out.trajectory.message = "Ruckig waypoint route trajectory";
    out.diagnostics.coarsePathLengthMeters = sourceProgress.back();

    const double acceleration = accelerationBudget(request.vehicle);
    if (acceleration <= Epsilon)
        return failure(
            request,
            world::navigation::TrajectoryStatus::InvalidRequest,
            "Ruckig route has no usable acceleration budget"
        );

    WorldKinematicState currentState;
    currentState.positionMeters = request.pathPointsMeters.front();
    currentState.velocityMps = request.initialVelocityMps;
    currentState.accelerationMps2 = glm::dvec3(0.0);
    glm::dvec3 currentProperAcceleration(0.0);

    double accumulatedPhysicalSeconds = 0.0;
    double accumulatedPathMeters = 0.0;
    std::size_t checkedCollisionSegments = 0;
    const double totalSourceProgress = sourceProgress.back();

    const glm::dvec3 firstDelta =
        request.pathPointsMeters[1] - request.pathPointsMeters[0];
    const glm::dvec3 firstDirection = magnitude(firstDelta) > Epsilon
        ? glm::normalize(firstDelta)
        : glm::dvec3(0.0, 0.0, -1.0);
    out.diagnostics.initialAlongPathSpeedMps =
        glm::dot(request.initialVelocityMps, firstDirection);
    const glm::dvec3 initialCross = request.initialVelocityMps -
        firstDirection * out.diagnostics.initialAlongPathSpeedMps;
    out.diagnostics.initialCrossTrackSpeedMps = magnitude(initialCross);
    out.diagnostics.pathCaptureRequired =
        out.diagnostics.initialCrossTrackSpeedMps > 0.25;

    for (std::size_t legIndex = 0;
         legIndex + 1 < request.pathPointsMeters.size();
         ++legIndex)
    {
        const glm::dvec3 legStart = request.pathPointsMeters[legIndex];
        const glm::dvec3 legEnd = request.pathPointsMeters[legIndex + 1];
        const glm::dvec3 legDelta = legEnd - legStart;
        const double legDistance = magnitude(legDelta);
        if (legDistance <= Epsilon)
            continue;

        const double speedLimit = segmentSpeedLimit(
            request,
            sourceProgress[legIndex],
            sourceProgress[legIndex + 1]
        );
        const double inheritedSpeed = magnitude(currentState.velocityMps);
        double duration = estimateStopToStopSeconds(
            legDistance,
            speedLimit,
            acceleration,
            inheritedSpeed
        );

        TrajectoryPredictionResult acceptedPrediction;
        bool accepted = false;
        constexpr int MaxDurationAttempts = 6;
        for (int attempt = 0; attempt < MaxDurationAttempts; ++attempt)
        {
            RuckigTrajectoryRequest ruckigRequest;
            ruckigRequest.systemId = request.systemId;
            ruckigRequest.startUniverseTimeSeconds = 0.0;
            ruckigRequest.initialState = currentState;
            ruckigRequest.initialProperAccelerationMps2 =
                currentProperAcceleration;
            ruckigRequest.motionEnvelope.maxProperAccelerationMps2 =
                acceleration;
            ruckigRequest.motionEnvelope.maxProperJerkMps3 =
                std::max(1.0, acceleration * 4.0);
            ruckigRequest.horizonSeconds = duration;
            ruckigRequest.sampleIntervalSeconds = std::min(0.10, duration);
            ruckigRequest.validationStepSeconds = std::min(0.02, duration);
            ruckigRequest.targetPositionMeters = legEnd;
            // Runtime baseline deliberately stops at coarse topology vertices.
            // This prevents a state-to-state polynomial from shaving an
            // obstacle corner. Through-waypoint blending can be added later as
            // a bounded local feature, not as a second global spline solver.
            ruckigRequest.targetVelocityMps = glm::dvec3(0.0);

            ++out.diagnostics.ruckigLegAttempts;
            const auto solveStart = Clock::now();
            auto candidate = RuckigTrajectorySolver::solve(ruckigRequest);
            const auto solveStop = Clock::now();
            out.diagnostics.ruckigSolveMilliseconds += elapsedMilliseconds(
                solveStart,
                solveStop
            );

            if (candidate.ok() && !candidate.prediction.samples.empty())
            {
                const double permittedPeakSpeed = std::max(
                    speedLimit,
                    inheritedSpeed
                ) + std::max(0.25, speedLimit * 0.01);
                if (candidate.prediction.diagnostics.maxSpeedMps <=
                    permittedPeakSpeed)
                {
                    acceptedPrediction = std::move(candidate.prediction);
                    accepted = true;
                    ++out.diagnostics.ruckigLegSuccesses;
                    break;
                }
            }

            duration *= 1.45;
        }

        if (!accepted)
        {
            auto failed = failure(
                request,
                world::navigation::TrajectoryStatus::NumericalFailure,
                "Ruckig could not solve a bounded coarse-route leg"
            );
            failed.diagnostics = out.diagnostics;
            appendPerfLog(
                request,
                failed,
                elapsedMilliseconds(totalStart, Clock::now())
            );
            return failed;
        }

        if (!validateSweptLeg(
                request,
                acceptedPrediction,
                legStart,
                legEnd,
                sourceProgress[legIndex],
                sourceProgress[legIndex + 1],
                checkedCollisionSegments))
        {
            auto failed = failure(
                request,
                world::navigation::TrajectoryStatus::NoSafePath,
                "Ruckig leg leaves the collision-free coarse corridor"
            );
            failed.diagnostics = out.diagnostics;
            failed.diagnostics.collisionSegmentsChecked =
                checkedCollisionSegments;
            appendPerfLog(
                request,
                failed,
                elapsedMilliseconds(totalStart, Clock::now())
            );
            return failed;
        }

        const glm::dvec3 segmentDirection = glm::normalize(legDelta);
        for (std::size_t sampleIndex = 0;
             sampleIndex < acceptedPrediction.samples.size();
             ++sampleIndex)
        {
            if (!out.trajectory.samples.empty() && sampleIndex == 0)
                continue;

            const auto& source = acceptedPrediction.samples[sampleIndex];
            world::navigation::TrajectorySample sample;
            sample.timeOffsetSeconds =
                accumulatedPhysicalSeconds + source.timeOffsetSeconds;
            sample.universeTimeSeconds =
                request.startUniverseTimeSeconds +
                sample.timeOffsetSeconds * request.universeTimeScale;
            sample.positionMeters = source.state.positionMeters;
            sample.velocityMps = source.state.velocityMps;
            sample.accelerationMps2 = source.state.accelerationMps2;
            sample.speedMps = magnitude(sample.velocityMps);
            sample.sourcePathProgressMeters = segmentSourceProgress(
                sample.positionMeters,
                legStart,
                legEnd,
                sourceProgress[legIndex],
                sourceProgress[legIndex + 1]
            );

            if (out.trajectory.samples.empty())
            {
                sample.pathProgressMeters = 0.0;
            }
            else
            {
                accumulatedPathMeters += magnitude(
                    sample.positionMeters -
                    out.trajectory.samples.back().positionMeters
                );
                sample.pathProgressMeters = accumulatedPathMeters;
            }

            sample.orientation = sampleOrientation(
                request,
                sample.velocityMps,
                segmentDirection,
                sample.sourcePathProgressMeters,
                totalSourceProgress
            );
            out.diagnostics.maxSpeedMps = std::max(
                out.diagnostics.maxSpeedMps,
                sample.speedMps
            );
            out.diagnostics.maxAccelerationMps2 = std::max(
                out.diagnostics.maxAccelerationMps2,
                magnitude(sample.accelerationMps2)
            );
            out.trajectory.samples.push_back(std::move(sample));
        }

        if (acceptedPrediction.samples.empty())
            continue;
        const auto& end = acceptedPrediction.samples.back();
        currentState = end.state;
        currentProperAcceleration = end.properAccelerationMps2;
        accumulatedPhysicalSeconds += end.timeOffsetSeconds;
    }

    if (out.trajectory.samples.size() < 2)
        return failure(
            request,
            world::navigation::TrajectoryStatus::NumericalFailure,
            "Ruckig route produced too few samples"
        );

    // The accepted product ends exactly at the authored/coarse terminal.
    auto& terminal = out.trajectory.samples.back();
    terminal.positionMeters = request.pathPointsMeters.back();
    terminal.velocityMps = glm::dvec3(0.0);
    terminal.speedMps = 0.0;
    terminal.sourcePathProgressMeters = totalSourceProgress;
    if (request.hasTerminalOrientation)
    {
        terminal.orientation = world::navigation::orientationForForwardUp(
            request.terminalForward,
            request.terminalUp
        );
    }

    out.trajectory.status = world::navigation::TrajectoryStatus::Ready;
    out.trajectory.durationSeconds =
        out.trajectory.samples.back().timeOffsetSeconds;
    out.trajectory.lengthMeters = accumulatedPathMeters;
    out.diagnostics.optimizedPathLengthMeters = accumulatedPathMeters;
    out.diagnostics.collisionSegmentsChecked = checkedCollisionSegments;

    computeCurvatureDiagnostics(out);

    // Compatibility fields remain populated until old diagnostics/tests are
    // renamed. They no longer mean spline candidates: one successful value is
    // one accepted Ruckig leg. Production code must use the Ruckig fields.
    out.diagnostics.smoothCandidatesEvaluated =
        out.diagnostics.ruckigLegAttempts;
    out.diagnostics.smoothSafeCandidates =
        out.diagnostics.ruckigLegSuccesses;
    out.diagnostics.selectedSmoothSupportLevel = 0;
    out.diagnostics.smoothingFellBackToPolyline = false;

    appendPerfLog(
        request,
        out,
        elapsedMilliseconds(totalStart, Clock::now())
    );
    return out;
}

} // namespace game::navigation

namespace world::navigation
{

TrajectoryGenerationResult TrajectoryGenerator::generate(
    const TrajectoryGenerationRequest& request
)
{
    // Compatibility facade only. The canonical runtime implementation lives in
    // game::navigation::RuckigRoutePlanner; the removed spline optimizer is not
    // part of the live route-to-trajectory path anymore.
    return game::navigation::RuckigRoutePlanner::plan(request);
}

} // namespace world::navigation
