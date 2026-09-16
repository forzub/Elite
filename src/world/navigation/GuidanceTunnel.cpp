#include "src/world/navigation/GuidanceTunnel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/RuckigTrajectorySolver.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/world/navigation/NavigationOrientation.h"

namespace world::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite3(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double magnitude(const glm::dvec3& value) noexcept
{
    return std::sqrt(glm::dot(value, value));
}

glm::dvec3 normalizedVectorOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double n2 = glm::dot(value, value);
    if (!finite(n2) || n2 <= Epsilon)
        return fallback;
    return value / std::sqrt(n2);
}

glm::dquat normalizedQuatOr(
    const glm::dquat& value,
    const glm::dquat& fallback
) noexcept
{
    const double n2 = glm::dot(value, value);
    if (!finite(n2) || n2 <= Epsilon)
        return fallback;
    return glm::normalize(value);
}

double quaternionAngularDistance(
    const glm::dquat& a,
    const glm::dquat& b
) noexcept
{
    const glm::dquat qa = normalizedQuatOr(
        a,
        glm::dquat(1.0, 0.0, 0.0, 0.0)
    );
    const glm::dquat qb = normalizedQuatOr(b, qa);
    const double qdot = std::clamp(std::abs(glm::dot(qa, qb)), 0.0, 1.0);
    return 2.0 * std::acos(qdot);
}

double smoothStep01(double value) noexcept
{
    const double u = std::clamp(value, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}

struct TrajectoryPoint
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    double sourceProgressMeters = 0.0;
    double speedMps = 0.0;
};

TrajectoryPoint sampleTrajectoryAtProgress(
    const Trajectory& trajectory,
    double progressMeters
)
{
    TrajectoryPoint out;
    if (trajectory.samples.empty())
        return out;

    const double clamped = std::clamp(
        progressMeters,
        trajectory.samples.front().pathProgressMeters,
        trajectory.samples.back().pathProgressMeters
    );
    std::size_t hi = 1;
    while (hi < trajectory.samples.size() &&
           trajectory.samples[hi].pathProgressMeters < clamped)
    {
        ++hi;
    }

    if (hi >= trajectory.samples.size())
    {
        const auto& last = trajectory.samples.back();
        out.positionMeters = last.positionMeters;
        out.velocityMps = last.velocityMps;
        out.orientation = last.orientation;
        out.sourceProgressMeters = last.sourcePathProgressMeters;
        out.speedMps = last.speedMps;
        return out;
    }

    const auto& a = trajectory.samples[hi - 1];
    const auto& b = trajectory.samples[hi];
    const double span = std::max(
        Epsilon,
        b.pathProgressMeters - a.pathProgressMeters
    );
    const double u = std::clamp(
        (clamped - a.pathProgressMeters) / span,
        0.0,
        1.0
    );

    out.positionMeters = a.positionMeters +
        (b.positionMeters - a.positionMeters) * u;
    out.velocityMps = a.velocityMps +
        (b.velocityMps - a.velocityMps) * u;
    out.sourceProgressMeters = a.sourcePathProgressMeters +
        (b.sourcePathProgressMeters - a.sourcePathProgressMeters) * u;
    out.speedMps = a.speedMps + (b.speedMps - a.speedMps) * u;

    glm::dquat qa = normalizedQuatOr(
        a.orientation,
        glm::dquat(1.0, 0.0, 0.0, 0.0)
    );
    glm::dquat qb = normalizedQuatOr(b.orientation, qa);
    if (glm::dot(qa, qb) < 0.0)
        qb = -qb;
    out.orientation = glm::normalize(glm::slerp(qa, qb, u));
    return out;
}

double nearestTrajectoryProgress(
    const Trajectory& trajectory,
    const glm::dvec3& pointMeters
)
{
    double bestProgress = trajectory.samples.front().pathProgressMeters;
    double bestDistance2 = std::numeric_limits<double>::infinity();

    for (std::size_t i = 1; i < trajectory.samples.size(); ++i)
    {
        const auto& a = trajectory.samples[i - 1];
        const auto& b = trajectory.samples[i];
        const glm::dvec3 segment = b.positionMeters - a.positionMeters;
        const double length2 = glm::dot(segment, segment);
        const double u = length2 > Epsilon
            ? std::clamp(
                glm::dot(pointMeters - a.positionMeters, segment) / length2,
                0.0,
                1.0
              )
            : 0.0;
        const glm::dvec3 projected = a.positionMeters + segment * u;
        const glm::dvec3 delta = pointMeters - projected;
        const double distance2 = glm::dot(delta, delta);
        if (distance2 >= bestDistance2)
            continue;

        bestDistance2 = distance2;
        bestProgress = a.pathProgressMeters +
            (b.pathProgressMeters - a.pathProgressMeters) * u;
    }
    return bestProgress;
}

NavigationVehicleProfile effectiveVehicle(
    const GuidanceTunnelRequest& request
)
{
    if (request.vehicle.valid())
        return request.vehicle;

    NavigationVehicleProfile out = request.vehicle;
    double observedSpeed = 0.0;
    for (const auto& sample : request.trajectory->samples)
        observedSpeed = std::max(observedSpeed, sample.speedMps);

    out.collisionRadiusMeters = std::max(0.0, out.collisionRadiusMeters);
    out.preferredClearanceMeters = std::max(0.0, out.preferredClearanceMeters);
    out.maxSpeedMps = std::max(150.0, observedSpeed);
    out.maxForwardAccelerationMps2 = 20.0;
    out.maxBrakingAccelerationMps2 = 20.0;
    out.maxLateralAccelerationMps2 = 10.0;
    out.maxAngularVelocityRadPerSecond = std::max(
        1.0,
        out.maxAngularVelocityRadPerSecond
    );
    out.maxAngularAccelerationRadPerSecond2 = std::max(
        1.0,
        out.maxAngularAccelerationRadPerSecond2
    );
    return out;
}

double accelerationBudget(const NavigationVehicleProfile& vehicle)
{
    double result = std::numeric_limits<double>::infinity();
    for (const double value : {
            vehicle.maxForwardAccelerationMps2,
            vehicle.maxBrakingAccelerationMps2,
            vehicle.maxLateralAccelerationMps2})
    {
        if (finite(value) && value > Epsilon)
            result = std::min(result, value);
    }
    return finite(result) ? result : 0.0;
}

double estimateLegDuration(
    double distanceMeters,
    double maxSpeedMps,
    double accelerationMps2,
    double initialSpeedMps,
    double terminalSpeedMps
)
{
    const double distance = std::max(0.0, distanceMeters);
    const double speed = std::max(0.1, maxSpeedMps);
    const double acceleration = std::max(0.1, accelerationMps2);
    const double triangularDistance = speed * speed / acceleration;

    double ideal = 0.0;
    if (distance <= triangularDistance)
        ideal = 2.0 * std::sqrt(distance / acceleration);
    else
        ideal = 2.0 * speed / acceleration +
            (distance - triangularDistance) / speed;

    ideal += (std::max(0.0, initialSpeedMps) +
              std::max(0.0, terminalSpeedMps)) /
        (2.0 * acceleration);
    return std::max(0.5, ideal * 1.25 + 0.25);
}

struct CurvePoint
{
    glm::dvec3 positionMeters {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    double sourceProgressMeters = 0.0;
    double speedMps = 0.0;
};

void appendCurvePoint(
    std::vector<CurvePoint>& points,
    CurvePoint point
)
{
    if (!points.empty() &&
        magnitude(points.back().positionMeters - point.positionMeters) <= 1.0e-7)
    {
        points.back().sourceProgressMeters = std::max(
            points.back().sourceProgressMeters,
            point.sourceProgressMeters
        );
        points.back().speedMps = point.speedMps;
        points.back().orientation = point.orientation;
        return;
    }
    points.push_back(std::move(point));
}

bool predictionClearOfObstacles(
    const GuidanceTunnelRequest& request,
    const game::navigation::TrajectoryPredictionResult& prediction,
    double sourceStart,
    double sourceEnd
)
{
    if (prediction.samples.size() < 2)
        return false;

    const double totalTime = std::max(
        Epsilon,
        prediction.samples.back().timeOffsetSeconds
    );
    for (std::size_t i = 1; i < prediction.samples.size(); ++i)
    {
        const auto& previous = prediction.samples[i - 1];
        const auto& current = prediction.samples[i];
        const double u0 = std::clamp(
            previous.timeOffsetSeconds / totalTime,
            0.0,
            1.0
        );
        const double u1 = std::clamp(
            current.timeOffsetSeconds / totalTime,
            0.0,
            1.0
        );
        const double progress0 = sourceStart + (sourceEnd - sourceStart) * u0;
        const double progress1 = sourceStart + (sourceEnd - sourceStart) * u1;
        const bool legalTargetIngress =
            !request.terminalAllowedObstacleId.empty() &&
            std::min(progress0, progress1) + 1.0e-7 >=
                request.terminalObstacleEntrySourceProgressMeters;

        if (!segmentClearOfNavigationObstacles(
                previous.state.positionMeters,
                current.state.positionMeters,
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

bool solveRuckigLeg(
    const GuidanceTunnelRequest& request,
    const NavigationVehicleProfile& vehicle,
    const game::navigation::WorldKinematicState& initialState,
    const glm::dvec3& targetPositionMeters,
    const glm::dvec3& targetVelocityMps,
    double sourceStart,
    double sourceEnd,
    std::vector<CurvePoint>& curve,
    game::navigation::WorldKinematicState& endState
)
{
    const double distance = magnitude(
        targetPositionMeters - initialState.positionMeters
    );
    if (distance <= Epsilon)
    {
        endState = initialState;
        endState.positionMeters = targetPositionMeters;
        endState.velocityMps = targetVelocityMps;
        return true;
    }

    const double acceleration = accelerationBudget(vehicle);
    if (acceleration <= Epsilon)
        return false;

    const double initialSpeed = magnitude(initialState.velocityMps);
    const double terminalSpeed = magnitude(targetVelocityMps);
    double duration = estimateLegDuration(
        distance,
        vehicle.maxSpeedMps,
        acceleration,
        initialSpeed,
        terminalSpeed
    );

    const double speedForSampling = std::max({
        1.0,
        vehicle.maxSpeedMps,
        initialSpeed,
        terminalSpeed
    });
    const double desiredCollisionChord = std::clamp(
        std::max(1.0, vehicle.collisionRadiusMeters * 0.25),
        1.0,
        5.0
    );
    const double sampleInterval = std::clamp(
        desiredCollisionChord / speedForSampling,
        0.02,
        0.05
    );

    constexpr int MaxAttempts = 8;
    for (int attempt = 0; attempt < MaxAttempts; ++attempt)
    {
        game::navigation::RuckigTrajectoryRequest ruckigRequest;
        ruckigRequest.systemId = request.trajectory->systemId;
        ruckigRequest.startUniverseTimeSeconds = 0.0;
        ruckigRequest.initialState = initialState;
        ruckigRequest.initialProperAccelerationMps2 = glm::dvec3(0.0);
        ruckigRequest.motionEnvelope.maxProperAccelerationMps2 = acceleration;
        ruckigRequest.motionEnvelope.maxProperJerkMps3 =
            std::max(1.0, acceleration * 4.0);
        ruckigRequest.horizonSeconds = duration;
        ruckigRequest.sampleIntervalSeconds = std::min(
            sampleInterval,
            duration
        );
        ruckigRequest.validationStepSeconds = std::min(0.02, duration);
        ruckigRequest.targetPositionMeters = targetPositionMeters;
        ruckigRequest.targetVelocityMps = targetVelocityMps;

        auto solved = game::navigation::RuckigTrajectorySolver::solve(
            ruckigRequest
        );
        if (!solved.ok() || solved.prediction.samples.size() < 2)
        {
            duration *= 1.45;
            continue;
        }

        const double permittedPeakSpeed = std::max(
            vehicle.maxSpeedMps,
            initialSpeed
        ) + std::max(0.25, vehicle.maxSpeedMps * 0.01);
        if (solved.prediction.diagnostics.maxSpeedMps > permittedPeakSpeed)
        {
            duration *= 1.45;
            continue;
        }

        if (!predictionClearOfObstacles(
                request,
                solved.prediction,
                sourceStart,
                sourceEnd))
        {
            duration *= 1.25;
            continue;
        }

        const double totalTime = std::max(
            Epsilon,
            solved.prediction.samples.back().timeOffsetSeconds
        );
        const glm::dvec3 fallbackForward = normalizedVectorOr(
            targetPositionMeters - initialState.positionMeters,
            glm::dvec3(0.0, 0.0, -1.0)
        );
        const glm::dvec3 upHint = normalizedVectorOr(
            request.currentOrientation * glm::dvec3(0.0, 1.0, 0.0),
            glm::dvec3(0.0, 1.0, 0.0)
        );

        for (std::size_t i = 0; i < solved.prediction.samples.size(); ++i)
        {
            if (!curve.empty() && i == 0)
                continue;

            const auto& sample = solved.prediction.samples[i];
            const double u = std::clamp(
                sample.timeOffsetSeconds / totalTime,
                0.0,
                1.0
            );
            const glm::dvec3 forward = normalizedVectorOr(
                sample.state.velocityMps,
                fallbackForward
            );
            appendCurvePoint(curve, {
                sample.state.positionMeters,
                orientationForForwardUp(forward, upHint),
                sourceStart + (sourceEnd - sourceStart) * u,
                magnitude(sample.state.velocityMps)
            });
        }

        endState = solved.prediction.samples.back().state;
        return true;
    }

    return false;
}

struct DynamicCurve
{
    std::vector<CurvePoint> points;
    double passedTrajectoryProgressMeters = 0.0;
    bool reconnectSolveBounded = false;
    bool reconnectFullSolveFallback = false;
    double reconnectSolveStartProgressMeters = 0.0;
    double reconnectSolveEndProgressMeters = 0.0;
    double reconnectLookaheadMeters = 0.0;
    glm::dquat terminalOrientation {1.0, 0.0, 0.0, 0.0};
};

DynamicCurve buildTrajectoryBackboneCurve(
    const GuidanceTunnelRequest& request
)
{
    DynamicCurve out;
    const Trajectory& trajectory = *request.trajectory;
    if (trajectory.samples.size() < 2)
        return out;

    const double startProgress = trajectory.samples.front().pathProgressMeters;
    out.passedTrajectoryProgressMeters = startProgress;
    out.reconnectSolveStartProgressMeters = startProgress;
    out.reconnectSolveEndProgressMeters =
        trajectory.samples.back().pathProgressMeters;
    out.reconnectLookaheadMeters = std::max(
        0.0,
        out.reconnectSolveEndProgressMeters - startProgress
    );
    out.terminalOrientation = trajectory.samples.back().orientation;

    for (const auto& sample : trajectory.samples)
    {
        appendCurvePoint(out.points, {
            sample.positionMeters,
            sample.orientation,
            sample.sourcePathProgressMeters,
            sample.speedMps
        });
    }
    return out;
}

bool terminalStable(
    const GuidanceTunnelRequest& request,
    const TrajectoryPoint& acceptedTerminal
)
{
    return magnitude(
            request.terminalPositionMeters - acceptedTerminal.positionMeters
        ) <= request.reconnectMaxTerminalPositionDriftMeters &&
        quaternionAngularDistance(
            request.terminalOrientation,
            acceptedTerminal.orientation
        ) <= request.reconnectMaxTerminalAngleDriftRadians;
}

DynamicCurve buildFullRuckigReconnect(
    const GuidanceTunnelRequest& request,
    double startProgress,
    bool fullFallbackDiagnostic
)
{
    DynamicCurve out;
    const Trajectory& trajectory = *request.trajectory;
    const NavigationVehicleProfile vehicle = effectiveVehicle(request);
    const double endProgress = trajectory.samples.back().pathProgressMeters;
    const TrajectoryPoint startSample = sampleTrajectoryAtProgress(
        trajectory,
        startProgress
    );

    out.passedTrajectoryProgressMeters = startProgress;
    out.reconnectSolveStartProgressMeters = startProgress;
    out.reconnectSolveEndProgressMeters = endProgress;
    out.reconnectLookaheadMeters = std::max(0.0, endProgress - startProgress);
    out.reconnectFullSolveFallback = fullFallbackDiagnostic;
    out.terminalOrientation = normalizedQuatOr(
        request.terminalOrientation,
        trajectory.samples.back().orientation
    );

    game::navigation::WorldKinematicState state;
    state.positionMeters = request.currentPositionMeters;
    state.velocityMps = request.currentVelocityMps;
    state.accelerationMps2 = glm::dvec3(0.0);

    const double totalSource = trajectory.samples.back().sourcePathProgressMeters;
    const double supportSpacing = std::max(
        100.0,
        request.gateSpacingMeters * 4.0
    );
    const double terminalDistance = magnitude(
        request.terminalPositionMeters - request.currentPositionMeters
    );
    const double requestedAlignment = std::max(
        request.gateSpacingMeters * 2.0,
        request.terminalAlignmentDistanceMeters
    );
    const double alignmentDistance = terminalDistance > Epsilon
        ? std::min(requestedAlignment, terminalDistance * 0.45)
        : 0.0;
    const glm::dvec3 terminalForward = normalizedVectorOr(
        out.terminalOrientation * glm::dvec3(0.0, 0.0, -1.0),
        normalizedVectorOr(
            request.terminalPositionMeters - request.currentPositionMeters,
            glm::dvec3(0.0, 0.0, -1.0)
        )
    );
    const glm::dvec3 alignmentPoint =
        request.terminalPositionMeters - terminalForward * alignmentDistance;

    struct Waypoint
    {
        glm::dvec3 positionMeters {0.0};
        double sourceProgressMeters = 0.0;
    };
    std::vector<Waypoint> waypoints;

    for (double progress = startProgress + supportSpacing;
         progress < endProgress - supportSpacing * 0.5;
         progress += supportSpacing)
    {
        const TrajectoryPoint sample = sampleTrajectoryAtProgress(
            trajectory,
            progress
        );
        if (alignmentDistance > Epsilon &&
            magnitude(sample.positionMeters - request.terminalPositionMeters) <=
                alignmentDistance * 1.20)
        {
            break;
        }
        waypoints.push_back({
            sample.positionMeters,
            sample.sourceProgressMeters
        });
    }

    if (alignmentDistance > request.gateSpacingMeters * 0.5 &&
        magnitude(alignmentPoint - state.positionMeters) >
            request.gateSpacingMeters * 0.5)
    {
        double alignmentSource = std::max(
            startSample.sourceProgressMeters,
            totalSource - alignmentDistance
        );
        if (finite(request.terminalObstacleEntrySourceProgressMeters))
        {
            alignmentSource = std::max(
                alignmentSource,
                request.terminalObstacleEntrySourceProgressMeters
            );
        }
        waypoints.push_back({alignmentPoint, alignmentSource});
    }

    waypoints.push_back({request.terminalPositionMeters, totalSource});

    double source = startSample.sourceProgressMeters;
    for (std::size_t i = 0; i < waypoints.size(); ++i)
    {
        const bool final = i + 1 == waypoints.size();
        game::navigation::WorldKinematicState endState;
        const glm::dvec3 targetVelocity = glm::dvec3(0.0);
        if (!solveRuckigLeg(
                request,
                vehicle,
                state,
                waypoints[i].positionMeters,
                targetVelocity,
                source,
                waypoints[i].sourceProgressMeters,
                out.points,
                endState))
        {
            out.points.clear();
            return out;
        }
        state = endState;
        source = waypoints[i].sourceProgressMeters;
        if (!final)
            state.velocityMps = glm::dvec3(0.0);
    }

    return out;
}

DynamicCurve buildDynamicCurve(const GuidanceTunnelRequest& request)
{
    DynamicCurve out;
    const Trajectory& trajectory = *request.trajectory;
    const NavigationVehicleProfile vehicle = effectiveVehicle(request);
    const double startProgress = nearestTrajectoryProgress(
        trajectory,
        request.currentPositionMeters
    );
    const double endProgress = trajectory.samples.back().pathProgressMeters;
    const TrajectoryPoint startSample = sampleTrajectoryAtProgress(
        trajectory,
        startProgress
    );
    const TrajectoryPoint endSample = sampleTrajectoryAtProgress(
        trajectory,
        endProgress
    );
    const bool stableTerminal = terminalStable(request, endSample);

    const double speedMps = magnitude(request.currentVelocityMps);
    const double brakingDistanceMeters =
        vehicle.maxBrakingAccelerationMps2 > Epsilon
            ? speedMps * speedMps /
                (2.0 * vehicle.maxBrakingAccelerationMps2)
            : 0.0;
    const double latencyDistanceMeters =
        speedMps * request.reconnectPlanningLatencySeconds;

    const glm::dvec3 travelForward = normalizedVectorOr(
        request.currentVelocityMps,
        normalizedVectorOr(
            request.currentOrientation * glm::dvec3(0.0, 0.0, -1.0),
            glm::dvec3(0.0, 0.0, -1.0)
        )
    );
    const double probeProgress = std::min(
        endProgress,
        startProgress + std::max(
            request.startCaptureDistanceMeters,
            request.gateSpacingMeters * 6.0
        )
    );
    const TrajectoryPoint probeSample = sampleTrajectoryAtProgress(
        trajectory,
        probeProgress
    );
    const glm::dvec3 routeDirection = normalizedVectorOr(
        probeSample.positionMeters - startSample.positionMeters,
        travelForward
    );
    const double courseChangeRadians = std::acos(std::clamp(
        glm::dot(travelForward, routeDirection),
        -1.0,
        1.0
    ));
    const double turnDistanceMeters =
        std::max(0.0, request.minimumTurnRadiusMeters) *
        courseChangeRadians;
    const double safetyMarginMeters = std::max({
        request.reconnectSafetyMarginMeters,
        request.startCaptureDistanceMeters,
        request.gateSpacingMeters * 4.0,
        vehicle.collisionRadiusMeters * 4.0
    });
    const double lookaheadMeters = std::max(
        request.gateSpacingMeters * 8.0,
        latencyDistanceMeters +
            brakingDistanceMeters +
            turnDistanceMeters +
            safetyMarginMeters
    );
    const double localEndProgress = std::min(
        endProgress,
        startProgress + lookaheadMeters
    );
    const bool canBound =
        stableTerminal &&
        localEndProgress + std::max(1.0, request.gateSpacingMeters * 2.0) <
            endProgress;

    bool boundedAttempted = false;
    if (canBound)
    {
        boundedAttempted = true;
        const TrajectoryPoint rejoin = sampleTrajectoryAtProgress(
            trajectory,
            localEndProgress
        );

        game::navigation::WorldKinematicState state;
        state.positionMeters = request.currentPositionMeters;
        state.velocityMps = request.currentVelocityMps;
        state.accelerationMps2 = glm::dvec3(0.0);
        game::navigation::WorldKinematicState endState;

        out.passedTrajectoryProgressMeters = startProgress;
        out.reconnectSolveStartProgressMeters = startProgress;
        out.reconnectSolveEndProgressMeters = localEndProgress;
        out.reconnectLookaheadMeters =
            localEndProgress - startProgress;
        out.terminalOrientation = trajectory.samples.back().orientation;

        if (solveRuckigLeg(
                request,
                vehicle,
                state,
                rejoin.positionMeters,
                rejoin.velocityMps,
                startSample.sourceProgressMeters,
                rejoin.sourceProgressMeters,
                out.points,
                endState))
        {
            for (const auto& sample : trajectory.samples)
            {
                if (sample.pathProgressMeters <= localEndProgress + Epsilon)
                    continue;
                appendCurvePoint(out.points, {
                    sample.positionMeters,
                    sample.orientation,
                    sample.sourcePathProgressMeters,
                    sample.speedMps
                });
            }

            if (out.points.size() >= 2)
            {
                out.reconnectSolveBounded = true;
                return out;
            }
        }
    }

    return buildFullRuckigReconnect(
        request,
        startProgress,
        boundedAttempted || !stableTerminal
    );
}

std::vector<double> curveProgress(const std::vector<CurvePoint>& points)
{
    std::vector<double> progress(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        progress[i] = progress[i - 1] + magnitude(
            points[i].positionMeters - points[i - 1].positionMeters
        );
    }
    return progress;
}

CurvePoint sampleCurveAtDistance(
    const std::vector<CurvePoint>& points,
    const std::vector<double>& progress,
    double distanceMeters
)
{
    if (points.size() == 1)
        return points.front();

    const double clamped = std::clamp(
        distanceMeters,
        0.0,
        progress.back()
    );
    std::size_t hi = 1;
    while (hi < progress.size() && progress[hi] < clamped)
        ++hi;
    if (hi >= progress.size())
        return points.back();

    const double span = std::max(
        Epsilon,
        progress[hi] - progress[hi - 1]
    );
    const double u = std::clamp(
        (clamped - progress[hi - 1]) / span,
        0.0,
        1.0
    );

    CurvePoint out;
    out.positionMeters = points[hi - 1].positionMeters +
        (points[hi].positionMeters - points[hi - 1].positionMeters) * u;
    out.sourceProgressMeters = points[hi - 1].sourceProgressMeters +
        (points[hi].sourceProgressMeters -
         points[hi - 1].sourceProgressMeters) * u;
    out.speedMps = points[hi - 1].speedMps +
        (points[hi].speedMps - points[hi - 1].speedMps) * u;

    glm::dquat qa = points[hi - 1].orientation;
    glm::dquat qb = points[hi].orientation;
    if (glm::dot(qa, qb) < 0.0)
        qb = -qb;
    out.orientation = glm::normalize(glm::slerp(qa, qb, u));
    return out;
}

double computeMaxCurvature(const std::vector<GuidanceTunnelGate>& gates)
{
    double maximum = 0.0;
    for (std::size_t i = 1; i + 1 < gates.size(); ++i)
    {
        const glm::dvec3 a =
            gates[i].positionMeters - gates[i - 1].positionMeters;
        const glm::dvec3 b =
            gates[i + 1].positionMeters - gates[i].positionMeters;
        const double la = magnitude(a);
        const double lb = magnitude(b);
        if (la <= Epsilon || lb <= Epsilon)
            continue;

        const double angle = std::acos(std::clamp(
            glm::dot(a / la, b / lb),
            -1.0,
            1.0
        ));
        maximum = std::max(
            maximum,
            angle / std::max(Epsilon, 0.5 * (la + lb))
        );
    }
    return maximum;
}

bool validRequest(const GuidanceTunnelRequest& request)
{
    return request.trajectory && request.trajectory->ready() &&
        finite3(request.currentPositionMeters) &&
        finite3(request.currentVelocityMps) &&
        finite3(request.terminalPositionMeters) &&
        finite(request.gateSpacingMeters) &&
        request.gateSpacingMeters > 0.0 &&
        finite(request.gateWidthMeters) &&
        request.gateWidthMeters > 0.0 &&
        finite(request.gateHeightMeters) &&
        request.gateHeightMeters > 0.0 &&
        finite(request.lateralToleranceMeters) &&
        request.lateralToleranceMeters >= 0.0 &&
        finite(request.verticalToleranceMeters) &&
        request.verticalToleranceMeters >= 0.0 &&
        finite(request.minimumTurnRadiusMeters) &&
        request.minimumTurnRadiusMeters >= 0.0 &&
        finite(request.reconnectPlanningLatencySeconds) &&
        request.reconnectPlanningLatencySeconds >= 0.0 &&
        finite(request.reconnectSafetyMarginMeters) &&
        request.reconnectSafetyMarginMeters >= 0.0;
}

} // namespace

GuidanceTunnel GuidanceTunnelBuilder::build(
    const GuidanceTunnelRequest& request
)
{
    GuidanceTunnel out;
    if (!validRequest(request))
        return out;

    const DynamicCurve dynamic =
        request.buildMode == GuidanceTunnelBuildMode::TrajectoryBackbone
            ? buildTrajectoryBackboneCurve(request)
            : buildDynamicCurve(request);
    if (dynamic.points.size() < 2)
        return out;

    const auto progress = curveProgress(dynamic.points);
    const double total = progress.back();
    if (!finite(total) || total <= Epsilon)
        return out;

    const double spacing = std::max(1.0, request.gateSpacingMeters);
    const std::size_t fullSteps = static_cast<std::size_t>(
        std::floor(total / spacing)
    );
    out.gates.reserve(fullSteps + 2);

    auto appendGate = [&](double distance)
    {
        const CurvePoint point = sampleCurveAtDistance(
            dynamic.points,
            progress,
            distance
        );
        GuidanceTunnelGate gate;
        gate.distanceAlongTunnelMeters = distance;
        gate.sourceTrajectoryProgressMeters = point.sourceProgressMeters;
        gate.positionMeters = point.positionMeters;
        gate.orientation = point.orientation;
        gate.widthMeters = request.gateWidthMeters;
        gate.heightMeters = request.gateHeightMeters;
        gate.lateralToleranceMeters = request.lateralToleranceMeters;
        gate.verticalToleranceMeters = request.verticalToleranceMeters;
        gate.recommendedSpeedMps = point.speedMps;
        out.gates.push_back(std::move(gate));
    };

    appendGate(0.0);
    for (std::size_t step = 1; step <= fullSteps; ++step)
    {
        const double distance = spacing * static_cast<double>(step);
        if (distance >= total - 1.0e-7)
            break;
        appendGate(distance);
    }
    appendGate(total);

    if (out.gates.size() < 2)
        return GuidanceTunnel{};

    if (request.buildMode == GuidanceTunnelBuildMode::ReconnectCurrentPose)
    {
        const glm::dquat startOrientation = normalizedQuatOr(
            request.currentOrientation,
            out.gates.front().orientation
        );
        const glm::dquat terminalOrientation = normalizedQuatOr(
            dynamic.terminalOrientation,
            out.gates.back().orientation
        );

        out.gates.front().positionMeters = request.currentPositionMeters;
        out.gates.front().orientation = startOrientation;
        if (dynamic.reconnectSolveBounded)
        {
            out.gates.back().positionMeters =
                request.trajectory->samples.back().positionMeters;
        }
        else
        {
            out.gates.back().positionMeters = request.terminalPositionMeters;
        }
        out.gates.back().orientation = terminalOrientation;

        for (std::size_t i = 1; i + 1 < out.gates.size(); ++i)
        {
            const glm::dvec3 tangent = normalizedVectorOr(
                out.gates[i + 1].positionMeters -
                    out.gates[i - 1].positionMeters,
                glm::dvec3(0.0, 0.0, -1.0)
            );
            glm::dquat referenceA = startOrientation;
            glm::dquat referenceB = terminalOrientation;
            if (glm::dot(referenceA, referenceB) < 0.0)
                referenceB = -referenceB;
            const double u = smoothStep01(
                out.gates[i].distanceAlongTunnelMeters / total
            );
            const glm::dquat reference = glm::normalize(glm::slerp(
                referenceA,
                referenceB,
                u
            ));
            const glm::dvec3 upHint = normalizedVectorOr(
                reference * glm::dvec3(0.0, 1.0, 0.0),
                glm::dvec3(0.0, 1.0, 0.0)
            );
            out.gates[i].orientation = orientationForForwardUp(
                tangent,
                upHint
            );
        }
    }

    out.systemId = request.trajectory->systemId;
    out.frameId = request.trajectory->frameId;
    out.passedTrajectoryProgressMeters =
        dynamic.passedTrajectoryProgressMeters;
    out.reconnectSolveBounded = dynamic.reconnectSolveBounded;
    out.reconnectFullSolveFallback = dynamic.reconnectFullSolveFallback;
    out.reconnectSolveStartProgressMeters =
        dynamic.reconnectSolveStartProgressMeters;
    out.reconnectSolveEndProgressMeters =
        dynamic.reconnectSolveEndProgressMeters;
    out.reconnectLookaheadMeters = dynamic.reconnectLookaheadMeters;
    out.maxCurvaturePerMeter = computeMaxCurvature(out.gates);
    out.minimumTurnRadiusMeters = out.maxCurvaturePerMeter > Epsilon
        ? 1.0 / out.maxCurvaturePerMeter
        : 0.0;
    out.valid = true;
    return out;
}

} // namespace world::navigation
