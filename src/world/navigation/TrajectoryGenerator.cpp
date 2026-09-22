#include "src/world/navigation/TrajectoryGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/RuckigRoutePlanner.h"
#include "src/game/navigation/RuckigTrajectorySolver.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/world/navigation/NavigationOrientation.h"

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

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double n2 = glm::dot(value, value);
    if (!finite(n2) || n2 <= Epsilon)
        return fallback;
    return value / std::sqrt(n2);
}

double smoothStep01(double value) noexcept
{
    const double u = std::clamp(value, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}

world::navigation::TrajectoryGenerationResult failure(
    const world::navigation::TrajectoryGenerationRequest& request,
    world::navigation::TrajectoryStatus status,
    const std::string& message,
    const world::navigation::TrajectoryGenerationDiagnostics& diagnostics = {}
)
{
    world::navigation::TrajectoryGenerationResult out;
    out.trajectory.status = status;
    out.trajectory.systemId = request.systemId;
    out.trajectory.frameId = request.frameId;
    out.trajectory.startUniverseTimeSeconds = request.startUniverseTimeSeconds;
    out.trajectory.message = message;
    out.diagnostics = diagnostics;
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
        !finite3(request.initialVelocityMps) ||
        !finite3(request.initialAccelerationMps2) ||
        (request.hasTerminalVelocity &&
            (!finite3(request.terminalVelocityMps) ||
             magnitude(request.terminalVelocityMps) >
                 request.vehicle.maxSpeedMps + 1.0e-6)))
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

std::vector<double> sourceProgressTable(
    const std::vector<glm::dvec3>& points
)
{
    std::vector<double> out(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
        out[i] = out[i - 1] + magnitude(points[i] - points[i - 1]);
    return out;
}

double accelerationBudget(
    const world::navigation::NavigationVehicleProfile& vehicle
)
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

double pointSpeedLimit(
    const world::navigation::TrajectoryGenerationRequest& request,
    double sourceProgressMeters
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

        const double lo = std::min(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        const double hi = std::max(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        if (sourceProgressMeters >= lo - 1.0e-7 &&
            sourceProgressMeters <= hi + 1.0e-7)
        {
            limit = std::min(limit, range.maxSpeedMps);
        }
    }

    for (const auto& constraint : request.pointSpeedConstraints)
    {
        if (!finite(constraint.sourcePathProgressMeters) ||
            !finite(constraint.maxSpeedMps) || constraint.maxSpeedMps < 0.0)
        {
            continue;
        }
        if (std::abs(
                constraint.sourcePathProgressMeters - sourceProgressMeters
            ) <= 1.0e-5)
        {
            limit = std::min(limit, constraint.maxSpeedMps);
        }
    }
    return std::max(0.0, limit);
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

        const double lo = std::min(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        const double hi = std::max(
            range.sourcePathStartMeters,
            range.sourcePathEndMeters
        );
        const double overlapStart = std::max(lo, sourceStart + 1.0e-6);
        const double overlapEnd = std::min(hi, sourceEnd - 1.0e-6);
        if (overlapStart <= overlapEnd)
            limit = std::min(limit, range.maxSpeedMps);
    }
    return std::max(0.1, limit);
}


struct ExecutionGuide
{
    std::vector<glm::dvec3> points;
    std::vector<double> sourceProgress;
    std::size_t roundedCorners = 0;
    std::size_t expandedCorners = 0;
};

glm::dvec3 closestPointOnSegment(
    const glm::dvec3& point,
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    const glm::dvec3 ab = b - a;
    const double denom = glm::dot(ab, ab);
    if (denom <= Epsilon)
        return a;
    const double u = std::clamp(
        glm::dot(point - a, ab) / denom,
        0.0,
        1.0
    );
    return a + ab * u;
}

void appendGuidePoint(
    ExecutionGuide& guide,
    const glm::dvec3& point,
    double sourceProgress
)
{
    if (!guide.points.empty() &&
        magnitude(point - guide.points.back()) <= 1.0e-6)
    {
        guide.sourceProgress.back() = sourceProgress;
        return;
    }

    guide.points.push_back(point);
    guide.sourceProgress.push_back(sourceProgress);
}

bool guideCornerClear(
    const world::navigation::TrajectoryGenerationRequest& request,
    const glm::dvec3& previous,
    const glm::dvec3& entry,
    const glm::dvec3& exit,
    const glm::dvec3& next
)
{
    return
        world::navigation::segmentClearOfNavigationObstacles(
            previous,
            entry,
            request.obstacles,
            request.vehicle.collisionRadiusMeters,
            request.vehicle.preferredClearanceMeters) &&
        world::navigation::segmentClearOfNavigationObstacles(
            entry,
            exit,
            request.obstacles,
            request.vehicle.collisionRadiusMeters,
            request.vehicle.preferredClearanceMeters) &&
        world::navigation::segmentClearOfNavigationObstacles(
            exit,
            next,
            request.obstacles,
            request.vehicle.collisionRadiusMeters,
            request.vehicle.preferredClearanceMeters);
}

glm::dvec3 quadraticBezier(
    const glm::dvec3& a,
    const glm::dvec3& control,
    const glm::dvec3& b,
    double u
) noexcept
{
    const double v = 1.0 - u;
    return
        a * (v * v) +
        control * (2.0 * v * u) +
        b * (u * u);
}

bool sampledBezierClear(
    const world::navigation::TrajectoryGenerationRequest& request,
    const glm::dvec3& entry,
    const glm::dvec3& control,
    const glm::dvec3& exit,
    int segments
)
{
    glm::dvec3 previous = entry;
    for (int i = 1; i <= segments; ++i)
    {
        const double u =
            static_cast<double>(i) /
            static_cast<double>(segments);
        const glm::dvec3 current =
            quadraticBezier(entry, control, exit, u);

        if (!world::navigation::segmentClearOfNavigationObstacles(
                previous,
                current,
                request.obstacles,
                request.vehicle.collisionRadiusMeters,
                request.vehicle.preferredClearanceMeters))
        {
            return false;
        }

        previous = current;
    }
    return true;
}

ExecutionGuide buildExecutionGuide(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& coarseProgress
)
{
    ExecutionGuide guide;
    const auto& coarse = request.pathPointsMeters;
    guide.points.reserve(coarse.size() * 2);
    guide.sourceProgress.reserve(coarse.size() * 2);

    appendGuidePoint(guide, coarse.front(), coarseProgress.front());

    if (coarse.size() < 3)
    {
        appendGuidePoint(guide, coarse.back(), coarseProgress.back());
        return guide;
    }

    for (std::size_t i = 1; i + 1 < coarse.size(); ++i)
    {
        const glm::dvec3 previous = coarse[i - 1];
        const glm::dvec3 originalCorner = coarse[i];
        const glm::dvec3 next = coarse[i + 1];

        const double incomingSourceLength =
            coarseProgress[i] - coarseProgress[i - 1];
        const double outgoingSourceLength =
            coarseProgress[i + 1] - coarseProgress[i];

        const double authoredSpeed = std::min({
            pointSpeedLimit(request, coarseProgress[i]),
            segmentSpeedLimit(
                request,
                coarseProgress[i - 1],
                coarseProgress[i]),
            segmentSpeedLimit(
                request,
                coarseProgress[i],
                coarseProgress[i + 1])
        });

        const double lateralAcceleration = std::max(
            0.1,
            request.vehicle.maxLateralAccelerationMps2
        );

        bool rounded = false;
        glm::dvec3 chosenEntry(0.0);
        glm::dvec3 chosenExit(0.0);
        glm::dvec3 chosenControl = originalCorner;
        double chosenCut = 0.0;
        int chosenExpansion = 0;

        // The coarse Stage-1 vertex is topology, not a demand to fly through
        // one mathematical point with a bisector velocity. Build a local
        // entry/exit guide around it. If the physically useful turn does not
        // fit, move the local corner farther into free space instead of
        // creating a tight S-turn or forcing StopTurnGo.
        const glm::dvec3 chordClosest =
            closestPointOnSegment(originalCorner, previous, next);
        glm::dvec3 outward =
            originalCorner - chordClosest;
        if (magnitude(outward) <= 1.0e-6)
        {
            const glm::dvec3 midpoint = 0.5 * (previous + next);
            outward = originalCorner - midpoint;
        }
        outward = normalizedOr(outward, glm::dvec3(0.0));

        const double expansionStep = std::max(
            2.0,
            request.vehicle.collisionRadiusMeters * 0.25
        );

        for (int expansionAttempt = 0;
             expansionAttempt < 7 && !rounded;
             ++expansionAttempt)
        {
            const glm::dvec3 corner =
                originalCorner +
                outward * (expansionStep * expansionAttempt);

            const glm::dvec3 incomingDelta = corner - previous;
            const glm::dvec3 outgoingDelta = next - corner;
            const double incomingLength = magnitude(incomingDelta);
            const double outgoingLength = magnitude(outgoingDelta);
            if (incomingLength <= Epsilon || outgoingLength <= Epsilon)
                continue;

            const glm::dvec3 incoming = incomingDelta / incomingLength;
            const glm::dvec3 outgoing = outgoingDelta / outgoingLength;
            const double cosine = std::clamp(
                glm::dot(incoming, outgoing),
                -1.0,
                1.0
            );
            const double angle = std::acos(cosine);

            if (angle <= glm::radians(0.5))
            {
                chosenEntry = corner;
                chosenExit = corner;
                chosenControl = corner;
                chosenCut = 0.0;
                chosenExpansion = expansionAttempt;
                rounded = true;
                break;
            }

            if (angle >= glm::radians(150.0))
                continue;

            const double radiusForSpeed =
                authoredSpeed * authoredSpeed / lateralAcceleration;
            const double tangentForSpeed =
                radiusForSpeed * std::tan(angle * 0.5);

            const double maxCut =
                std::max(
                    1.0,
                    std::min(incomingLength, outgoingLength) * 0.42
                );
            const double minCut =
                std::min(
                    maxCut,
                    std::max(
                        2.0,
                        request.vehicle.collisionRadiusMeters * 0.30
                    )
                );

            // Extra reserve keeps the execution curve from being the
            // mathematically tightest admissible turn.
            double cut = std::clamp(
                tangentForSpeed * 1.35,
                minCut,
                maxCut
            );

            for (int cutAttempt = 0;
                 cutAttempt < 8;
                 ++cutAttempt)
            {
                const glm::dvec3 entry = corner - incoming * cut;
                const glm::dvec3 exit = corner + outgoing * cut;

                const int previewSegments = std::clamp(
                    static_cast<int>(
                        std::ceil((2.0 * cut) / 2.5)
                    ),
                    4,
                    24
                );

                if (guideCornerClear(
                        request,
                        previous,
                        entry,
                        exit,
                        next) &&
                    sampledBezierClear(
                        request,
                        entry,
                        corner,
                        exit,
                        previewSegments))
                {
                    chosenEntry = entry;
                    chosenExit = exit;
                    chosenControl = corner;
                    chosenCut = cut;
                    chosenExpansion = expansionAttempt;
                    rounded = true;
                    break;
                }

                if (cut <= minCut + 1.0e-6)
                    break;

                cut = std::max(minCut, cut * 0.78);
            }
        }

        if (!rounded)
        {
            appendGuidePoint(
                guide,
                originalCorner,
                coarseProgress[i]
            );
            continue;
        }

        if (chosenCut <= 1.0e-6)
        {
            appendGuidePoint(
                guide,
                chosenEntry,
                coarseProgress[i]
            );
            continue;
        }

        const double entryProgress =
            std::clamp(
                coarseProgress[i] -
                    std::min(chosenCut, incomingSourceLength * 0.45),
                coarseProgress[i - 1] + 1.0e-6,
                coarseProgress[i] - 1.0e-6
            );
        const double exitProgress =
            std::clamp(
                coarseProgress[i] +
                    std::min(chosenCut, outgoingSourceLength * 0.45),
                coarseProgress[i] + 1.0e-6,
                coarseProgress[i + 1] - 1.0e-6
            );

        const int curveSegments = std::clamp(
            static_cast<int>(
                std::ceil((2.0 * chosenCut) / 6.0)
            ),
            3,
            10
        );

        for (int sampleIndex = 0;
             sampleIndex <= curveSegments;
             ++sampleIndex)
        {
            const double u =
                static_cast<double>(sampleIndex) /
                static_cast<double>(curveSegments);

            const glm::dvec3 point =
                quadraticBezier(
                    chosenEntry,
                    chosenControl,
                    chosenExit,
                    u
                );
            const double progress =
                entryProgress +
                (exitProgress - entryProgress) * u;

            appendGuidePoint(guide, point, progress);
        }

        ++guide.roundedCorners;
        if (chosenExpansion > 0)
            ++guide.expandedCorners;
    }

    appendGuidePoint(guide, coarse.back(), coarseProgress.back());
    return guide;
}

bool violatesKnownStopDistance(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& sourceProgress
)
{
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
    const double distanceForAccelAndBrake = speed * speed / acceleration;

    double ideal = 0.0;
    if (distance <= distanceForAccelAndBrake)
        ideal = 2.0 * std::sqrt(distance / acceleration);
    else
        ideal = 2.0 * speed / acceleration +
            (distance - distanceForAccelAndBrake) / speed;

    ideal += (std::max(0.0, initialSpeedMps) +
              std::max(0.0, terminalSpeedMps)) /
        (2.0 * acceleration);
    return std::max(0.5, ideal * 1.20 + 0.25);
}

std::vector<glm::dvec3> buildWaypointVelocities(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& sourceProgress
)
{
    const std::size_t count = request.pathPointsMeters.size();
    std::vector<glm::dvec3> velocities(count, glm::dvec3(0.0));
    if (count < 3)
        return velocities;

    for (std::size_t i = 1; i + 1 < count; ++i)
    {
        const glm::dvec3 incomingDelta =
            request.pathPointsMeters[i] - request.pathPointsMeters[i - 1];
        const glm::dvec3 outgoingDelta =
            request.pathPointsMeters[i + 1] - request.pathPointsMeters[i];
        const double incomingLength = magnitude(incomingDelta);
        const double outgoingLength = magnitude(outgoingDelta);
        if (incomingLength <= Epsilon || outgoingLength <= Epsilon)
            continue;

        const glm::dvec3 incoming = incomingDelta / incomingLength;
        const glm::dvec3 outgoing = outgoingDelta / outgoingLength;
        const double cosine = std::clamp(
            glm::dot(incoming, outgoing),
            -1.0,
            1.0
        );
        const double angle = std::acos(cosine);
        if (angle >= glm::radians(150.0))
            continue;

        const double pointLimit = pointSpeedLimit(
            request,
            sourceProgress[i]
        );
        if (pointLimit <= Epsilon)
            continue;

        const double incomingLimit = segmentSpeedLimit(
            request,
            sourceProgress[i - 1],
            sourceProgress[i]
        );
        const double outgoingLimit = segmentSpeedLimit(
            request,
            sourceProgress[i],
            sourceProgress[i + 1]
        );

        // The coarse polyline is a geometric intent, not a command to perform
        // StopTurnGo at every topology vertex. Search for the widest local
        // corner chord that is still inside the proved free space. The old
        // implementation tested exactly one 25%-of-leg chord; if that single
        // chord was blocked it forced a zero-speed waypoint even when a tighter
        // continuous turn was perfectly safe.
        const double nominalBlendDistance = std::max(
            1.0,
            std::min(incomingLength, outgoingLength) * 0.25
        );
        const double minimumBlendDistance = std::min(
            nominalBlendDistance,
            std::max(
                0.75,
                request.vehicle.collisionRadiusMeters * 0.25
            )
        );

        double blendDistance = nominalBlendDistance;
        bool foundSafeBlend = false;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            blendDistance = std::max(
                minimumBlendDistance,
                blendDistance
            );

            const glm::dvec3 entry =
                request.pathPointsMeters[i] -
                incoming * blendDistance;
            const glm::dvec3 exit =
                request.pathPointsMeters[i] +
                outgoing * blendDistance;

            if (world::navigation::segmentClearOfNavigationObstacles(
                    entry,
                    exit,
                    request.obstacles,
                    request.vehicle.collisionRadiusMeters,
                    request.vehicle.preferredClearanceMeters))
            {
                foundSafeBlend = true;
                break;
            }

            if (blendDistance <= minimumBlendDistance + 1.0e-6)
                break;

            blendDistance =
                std::max(
                    minimumBlendDistance,
                    blendDistance * 0.70
                );
        }

        if (!foundSafeBlend)
            continue;

        glm::dvec3 direction = incoming + outgoing;
        if (magnitude(direction) <= Epsilon)
            continue;
        direction = glm::normalize(direction);

        const double lateralAcceleration = std::max(
            0.1,
            request.vehicle.maxLateralAccelerationMps2
        );

        // IMPORTANT: this function now also receives densely sampled points
        // from the rounded execution guide. The old speed heuristic used
        // blendDistance, which scales with local segment length. Densifying a
        // perfectly identical curve therefore made the allowed speed smaller
        // purely because we added more samples. That is physically wrong.
        //
        // Use the circumcircle curvature of the three geometric samples
        // instead. For points A-B-C:
        //   kappa = 2*|AB x BC| / (|AB| |BC| |AC|)
        // and the lateral-acceleration speed limit is:
        //   v = sqrt(a_lat / kappa)
        //
        // This makes the limit invariant to guide sampling density.
        const glm::dvec3 acrossDelta =
            request.pathPointsMeters[i + 1] -
            request.pathPointsMeters[i - 1];
        const double acrossLength = magnitude(acrossDelta);
        const double crossMagnitude =
            magnitude(glm::cross(incomingDelta, outgoingDelta));

        double curvature = 0.0;
        const double curvatureDenominator =
            incomingLength * outgoingLength * acrossLength;
        if (curvatureDenominator > Epsilon)
        {
            curvature =
                2.0 * crossMagnitude /
                curvatureDenominator;
        }

        const double turnSpeed =
            curvature > 1.0e-9
                ? std::sqrt(lateralAcceleration / curvature)
                : request.vehicle.maxSpeedMps;

        const double speed = std::max(
            0.0,
            std::min({
                pointLimit,
                incomingLimit,
                outgoingLimit,
                turnSpeed
            })
        );
        if (speed <= 0.5)
            continue;

        velocities[i] = direction * speed;
    }

    return velocities;
}

bool nonZeroVelocity(const glm::dvec3& value) noexcept
{
    return glm::dot(value, value) > 0.25;
}

glm::dquat sampleOrientation(
    const world::navigation::TrajectoryGenerationRequest& request,
    const glm::dvec3& velocity,
    const glm::dvec3& fallbackForward,
    double sourceProgress,
    double totalSourceProgress
)
{
    const glm::dvec3 forward = normalizedOr(
        velocity,
        normalizedOr(fallbackForward, glm::dvec3(0.0, 0.0, -1.0))
    );

    glm::dvec3 upHint(0.0, 1.0, 0.0);
    if (request.hasTerminalOrientation &&
        magnitude(request.terminalUp) > Epsilon)
    {
        upHint = glm::normalize(request.terminalUp);
    }

    glm::dquat orientation =
        world::navigation::orientationForForwardUp(forward, upHint);

    if (!request.hasTerminalOrientation)
        return orientation;

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

    glm::dquat target = terminal;
    if (glm::dot(orientation, target) < 0.0)
        target = -target;
    return glm::normalize(glm::slerp(orientation, target, blend));
}

bool validateSweptPrediction(
    const world::navigation::TrajectoryGenerationRequest& request,
    const game::navigation::TrajectoryPredictionResult& prediction,
    double sourceStart,
    double sourceEnd,
    std::size_t& checkedSegments
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
        const double progress0 = sourceStart +
            (sourceEnd - sourceStart) * u0;
        const double progress1 = sourceStart +
            (sourceEnd - sourceStart) * u1;
        const bool legalTargetIngress =
            !request.terminalAllowedObstacleId.empty() &&
            std::min(progress0, progress1) + 1.0e-7 >=
                request.terminalObstacleEntrySourceProgressMeters;

        ++checkedSegments;
        if (!world::navigation::segmentClearOfNavigationObstacles(
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

struct LegSolve
{
    bool ready = false;
    game::navigation::TrajectoryPredictionResult prediction;
};

LegSolve solveLeg(
    const world::navigation::TrajectoryGenerationRequest& request,
    const game::navigation::WorldKinematicState& initialState,
    const glm::dvec3& initialProperAcceleration,
    const glm::dvec3& targetPosition,
    const glm::dvec3& targetVelocity,
    double speedLimit,
    double acceleration,
    world::navigation::TrajectoryGenerationDiagnostics& diagnostics
)
{
    LegSolve out;
    const double distance = magnitude(targetPosition - initialState.positionMeters);
    const double inheritedSpeed = magnitude(initialState.velocityMps);
    const double terminalSpeed = magnitude(targetVelocity);
    double duration = estimateLegDuration(
        distance,
        speedLimit,
        acceleration,
        inheritedSpeed,
        terminalSpeed
    );

    const double speedForSampling = std::max({
        1.0,
        speedLimit,
        inheritedSpeed,
        terminalSpeed
    });
    const double desiredCollisionChord = std::clamp(
        std::max(1.0, request.vehicle.collisionRadiusMeters * 0.25),
        1.0,
        5.0
    );
    const double sampleInterval = std::clamp(
        desiredCollisionChord / speedForSampling,
        0.02,
        0.05
    );

    constexpr int MaxDurationAttempts = 8;
    for (int attempt = 0; attempt < MaxDurationAttempts; ++attempt)
    {
        game::navigation::RuckigTrajectoryRequest ruckigRequest;
        ruckigRequest.systemId = request.systemId;
        ruckigRequest.startUniverseTimeSeconds = 0.0;
        ruckigRequest.initialState = initialState;
        ruckigRequest.initialProperAccelerationMps2 = initialProperAcceleration;
        ruckigRequest.motionEnvelope.maxProperAccelerationMps2 = acceleration;
        ruckigRequest.motionEnvelope.maxProperJerkMps3 =
            std::max(1.0, acceleration * 4.0);
        ruckigRequest.horizonSeconds = duration;
        ruckigRequest.sampleIntervalSeconds = std::min(
            sampleInterval,
            duration
        );
        ruckigRequest.validationStepSeconds = std::min(0.02, duration);
        ruckigRequest.targetPositionMeters = targetPosition;
        ruckigRequest.targetVelocityMps = targetVelocity;

        ++diagnostics.ruckigLegAttempts;
        auto candidate = game::navigation::RuckigTrajectorySolver::solve(
            ruckigRequest
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
                ++diagnostics.ruckigLegSuccesses;
                out.ready = true;
                out.prediction = std::move(candidate.prediction);
                return out;
            }
        }

        duration *= 1.45;
    }

    return out;
}

void computeCurvatureDiagnostics(
    world::navigation::TrajectoryGenerationResult& result
)
{
    const auto& samples = result.trajectory.samples;
    double maximum = 0.0;
    double previous = 0.0;
    double variation = 0.0;
    bool havePrevious = false;

    for (std::size_t i = 1; i + 1 < samples.size(); ++i)
    {
        const glm::dvec3 a =
            samples[i].positionMeters - samples[i - 1].positionMeters;
        const glm::dvec3 b =
            samples[i + 1].positionMeters - samples[i].positionMeters;
        const double la = magnitude(a);
        const double lb = magnitude(b);
        if (la <= Epsilon || lb <= Epsilon)
            continue;

        const double angle = std::acos(std::clamp(
            glm::dot(a / la, b / lb),
            -1.0,
            1.0
        ));
        const double curvature = angle /
            std::max(Epsilon, 0.5 * (la + lb));
        maximum = std::max(maximum, curvature);
        if (havePrevious)
            variation += std::abs(curvature - previous);
        previous = curvature;
        havePrevious = true;
    }

    result.diagnostics.maxCurvaturePerMeter = maximum;
    result.diagnostics.curvatureVariation = variation;
}

struct GuideMetricSample
{
    glm::dvec3 position {0.0};
    glm::dvec3 tangent {1.0, 0.0, 0.0};
    double sourceProgressMeters = 0.0;
};

std::vector<double> guideArcTable(
    const ExecutionGuide& guide
)
{
    std::vector<double> arc(
        guide.points.size(),
        0.0
    );
    for (std::size_t i = 1; i < guide.points.size(); ++i)
    {
        arc[i] =
            arc[i - 1] +
            magnitude(
                guide.points[i] -
                guide.points[i - 1]
            );
    }
    return arc;
}

GuideMetricSample sampleGuide(
    const ExecutionGuide& guide,
    const std::vector<double>& arc,
    double progressMeters
)
{
    GuideMetricSample out;
    if (guide.points.empty())
        return out;
    if (guide.points.size() == 1 || arc.size() != guide.points.size())
    {
        out.position = guide.points.front();
        return out;
    }

    const double s = std::clamp(
        progressMeters,
        0.0,
        arc.back()
    );

    const auto upper = std::upper_bound(
        arc.begin(),
        arc.end(),
        s
    );
    std::size_t index =
        upper == arc.begin()
            ? 1
            : static_cast<std::size_t>(
                std::distance(arc.begin(), upper)
              );
    index = std::clamp<std::size_t>(
        index,
        1,
        guide.points.size() - 1
    );

    const double segmentStart = arc[index - 1];
    const double segmentEnd = arc[index];
    const double segmentLength =
        std::max(Epsilon, segmentEnd - segmentStart);
    const double u = std::clamp(
        (s - segmentStart) / segmentLength,
        0.0,
        1.0
    );

    out.position =
        guide.points[index - 1] * (1.0 - u) +
        guide.points[index] * u;
    out.tangent = normalizedOr(
        guide.points[index] -
            guide.points[index - 1],
        glm::dvec3(1.0, 0.0, 0.0)
    );
    out.sourceProgressMeters =
        guide.sourceProgress[index - 1] * (1.0 - u) +
        guide.sourceProgress[index] * u;
    return out;
}

glm::dvec3 guideCurvatureVector(
    const ExecutionGuide& guide,
    const std::vector<double>& arc,
    double progressMeters
)
{
    if (arc.empty() || arc.back() <= Epsilon)
        return glm::dvec3(0.0);

    const double probe = std::clamp(
        arc.back() * 0.0025,
        0.20,
        1.00
    );
    const double s0 =
        std::max(0.0, progressMeters - probe);
    const double s1 =
        std::min(arc.back(), progressMeters + probe);
    if (s1 - s0 <= Epsilon)
        return glm::dvec3(0.0);

    const glm::dvec3 t0 =
        sampleGuide(guide, arc, s0).tangent;
    const glm::dvec3 t1 =
        sampleGuide(guide, arc, s1).tangent;
    return (t1 - t0) / (s1 - s0);
}

double globalGuideSpeedLimit(
    const world::navigation::TrajectoryGenerationRequest& request,
    const ExecutionGuide& guide,
    const std::vector<double>& arc
)
{
    double limit = request.vehicle.maxSpeedMps;

    for (const auto& range : request.speedLimitRanges)
    {
        if (finite(range.maxSpeedMps) &&
            range.maxSpeedMps > 0.0)
        {
            limit = std::min(limit, range.maxSpeedMps);
        }
    }

    const double terminalSourceProgress =
        guide.sourceProgress.empty()
            ? std::numeric_limits<double>::infinity()
            : guide.sourceProgress.back();

    for (const auto& point : request.pointSpeedConstraints)
    {
        if (!finite(point.maxSpeedMps) ||
            point.maxSpeedMps <= 0.0)
        {
            continue;
        }

        const bool exactMovingTerminal =
            request.hasTerminalVelocity &&
            finite(point.sourcePathProgressMeters) &&
            std::abs(
                point.sourcePathProgressMeters -
                terminalSourceProgress
            ) <= 1.0e-5;

        // A terminal speed is a boundary state, not a cruise-speed command.
        // Ruckig already receives it as targetSpeedMps. Applying it here as a
        // global maximum would needlessly hold the whole route at finish speed.
        if (exactMovingTerminal)
            continue;

        limit = std::min(limit, point.maxSpeedMps);
    }

    const double lateralAcceleration = std::max(
        0.1,
        request.vehicle.maxLateralAccelerationMps2
    );

    for (std::size_t i = 1;
         i + 1 < guide.points.size();
         ++i)
    {
        const glm::dvec3 a =
            guide.points[i] - guide.points[i - 1];
        const glm::dvec3 b =
            guide.points[i + 1] - guide.points[i];
        const glm::dvec3 across =
            guide.points[i + 1] - guide.points[i - 1];

        const double la = magnitude(a);
        const double lb = magnitude(b);
        const double lc = magnitude(across);
        const double denom = la * lb * lc;
        if (denom <= Epsilon)
            continue;

        const double curvature =
            2.0 * magnitude(glm::cross(a, b)) /
            denom;
        if (curvature > 1.0e-9)
        {
            limit = std::min(
                limit,
                std::sqrt(
                    lateralAcceleration / curvature
                )
            );
        }
    }

    return std::max(0.1, limit);
}

world::navigation::TrajectoryGenerationResult
buildPathProgressTrajectory(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& coarseSourceProgress,
    const ExecutionGuide& guide,
    double totalStartMilliseconds = 0.0
)
{
    (void)totalStartMilliseconds;

    if (guide.points.size() < 2)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::NumericalFailure,
            "execution guide has too few points"
        );
    }

    const std::vector<double> arc =
        guideArcTable(guide);
    if (arc.back() <= Epsilon)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::NumericalFailure,
            "execution guide has zero length"
        );
    }

    const GuideMetricSample first =
        sampleGuide(guide, arc, 0.0);
    const GuideMetricSample last =
        sampleGuide(guide, arc, arc.back());

    const double initialAlongSpeed =
        glm::dot(
            request.initialVelocityMps,
            first.tangent
        );
    const glm::dvec3 initialCrossVelocity =
        request.initialVelocityMps -
        first.tangent * initialAlongSpeed;

    const double terminalAlongSpeed =
        request.hasTerminalVelocity
            ? std::max(
                0.0,
                glm::dot(
                    request.terminalVelocityMps,
                    last.tangent
                )
              )
            : 0.0;

    const double pathSpeedLimit =
        globalGuideSpeedLimit(
            request,
            guide,
            arc
        );

    game::navigation::RuckigProgressRequest progressRequest;
    progressRequest.startProgressMeters = 0.0;
    progressRequest.startSpeedMps =
        std::max(0.0, initialAlongSpeed);
    progressRequest.startAccelerationMps2 =
        glm::dot(
            request.initialAccelerationMps2,
            first.tangent
        );
    progressRequest.targetProgressMeters = arc.back();
    progressRequest.targetSpeedMps = terminalAlongSpeed;
    progressRequest.targetAccelerationMps2 = 0.0;
    progressRequest.maxSpeedMps = std::max({
        pathSpeedLimit,
        progressRequest.startSpeedMps,
        progressRequest.targetSpeedMps
    });
    progressRequest.maxAccelerationMps2 = std::max(
        0.1,
        std::min(
            request.vehicle.maxForwardAccelerationMps2,
            request.vehicle.maxBrakingAccelerationMps2
        )
    );
    progressRequest.maxJerkMps3 = std::max(
        1.0,
        progressRequest.maxAccelerationMps2 * 4.0
    );
    progressRequest.sampleIntervalSeconds = 0.02;

    const auto progress =
        game::navigation::RuckigTrajectorySolver::solveProgress(
            progressRequest
        );

    if (!progress.ready)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::NumericalFailure,
            "scalar Ruckig path progress failed: " +
                progress.message
        );
    }

    world::navigation::TrajectoryGenerationResult out;
    out.trajectory.status =
        world::navigation::TrajectoryStatus::Ready;
    out.trajectory.systemId = request.systemId;
    out.trajectory.frameId = request.frameId;
    out.trajectory.startUniverseTimeSeconds =
        request.startUniverseTimeSeconds;
    out.trajectory.message =
        "Ruckig scalar path-progress trajectory";
    out.trajectory.durationSeconds =
        progress.durationSeconds;
    out.trajectory.lengthMeters =
        arc.back();

    out.executionGuidePointsMeters = guide.points;
    out.diagnostics.coarsePathLengthMeters =
        coarseSourceProgress.empty()
            ? 0.0
            : coarseSourceProgress.back();
    out.diagnostics.optimizedPathLengthMeters =
        arc.back();
    out.diagnostics.executionGuidePoints =
        guide.points.size();
    out.diagnostics.roundedGuideCorners =
        guide.roundedCorners;
    out.diagnostics.expandedGuideCorners =
        guide.expandedCorners;
    out.diagnostics.ruckigLegAttempts = 1;
    out.diagnostics.ruckigLegSuccesses = 1;
    out.diagnostics.initialAlongPathSpeedMps =
        initialAlongSpeed;
    out.diagnostics.initialCrossTrackSpeedMps =
        magnitude(initialCrossVelocity);
    out.diagnostics.pathCaptureRequired =
        magnitude(initialCrossVelocity) > 0.25;

    out.trajectory.samples.reserve(
        progress.samples.size()
    );

    for (std::size_t i = 0;
         i < progress.samples.size();
         ++i)
    {
        const auto& progressSample =
            progress.samples[i];
        const GuideMetricSample spatial =
            sampleGuide(
                guide,
                arc,
                progressSample.progressMeters
            );
        const glm::dvec3 curvature =
            guideCurvatureVector(
                guide,
                arc,
                progressSample.progressMeters
            );

        world::navigation::TrajectorySample sample;
        sample.timeOffsetSeconds =
            progressSample.timeOffsetSeconds;
        sample.universeTimeSeconds =
            request.startUniverseTimeSeconds +
            sample.timeOffsetSeconds *
                request.universeTimeScale;
        sample.pathProgressMeters =
            progressSample.progressMeters;
        sample.sourcePathProgressMeters =
            spatial.sourceProgressMeters;
        sample.positionMeters = spatial.position;
        sample.speedMps = progressSample.speedMps;
        sample.velocityMps =
            spatial.tangent *
            progressSample.speedMps;
        sample.accelerationMps2 =
            spatial.tangent *
                progressSample.accelerationMps2 +
            curvature *
                (progressSample.speedMps *
                 progressSample.speedMps);
        sample.orientation = sampleOrientation(
            request,
            sample.velocityMps,
            spatial.tangent,
            sample.sourcePathProgressMeters,
            coarseSourceProgress.empty()
                ? 0.0
                : coarseSourceProgress.back()
        );

        out.diagnostics.maxSpeedMps = std::max(
            out.diagnostics.maxSpeedMps,
            sample.speedMps
        );
        out.diagnostics.maxAccelerationMps2 =
            std::max(
                out.diagnostics.maxAccelerationMps2,
                magnitude(sample.accelerationMps2)
            );

        out.trajectory.samples.push_back(
            std::move(sample)
        );
    }

    if (out.trajectory.samples.size() < 2)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::NumericalFailure,
            "scalar path progress produced too few mapped samples"
        );
    }

    // Preserve the exact authored initial state; the path-progress solve owns
    // subsequent motion, not the input seed itself.
    out.trajectory.samples.front().positionMeters =
        request.pathPointsMeters.front();
    out.trajectory.samples.front().velocityMps =
        request.initialVelocityMps;
    out.trajectory.samples.front().accelerationMps2 =
        request.initialAccelerationMps2;
    out.trajectory.samples.front().speedMps =
        magnitude(request.initialVelocityMps);

    auto& terminal =
        out.trajectory.samples.back();
    terminal.positionMeters =
        request.pathPointsMeters.back();
    if (request.hasTerminalVelocity)
    {
        terminal.velocityMps =
            request.terminalVelocityMps;
        terminal.speedMps =
            magnitude(request.terminalVelocityMps);
    }
    terminal.sourcePathProgressMeters =
        coarseSourceProgress.empty()
            ? terminal.sourcePathProgressMeters
            : coarseSourceProgress.back();

    if (request.hasTerminalOrientation)
    {
        terminal.orientation =
            world::navigation::orientationForForwardUp(
                request.terminalForward,
                request.terminalUp
            );
    }

    std::size_t collisionSegments = 0;
    for (std::size_t i = 1;
         i < out.trajectory.samples.size();
         ++i)
    {
        const auto& previous =
            out.trajectory.samples[i - 1];
        const auto& current =
            out.trajectory.samples[i];
        const bool legalTargetIngress =
            !request.terminalAllowedObstacleId.empty() &&
            std::min(
                previous.sourcePathProgressMeters,
                current.sourcePathProgressMeters
            ) + 1.0e-7 >=
                request.terminalObstacleEntrySourceProgressMeters;

        ++collisionSegments;
        if (!world::navigation::segmentClearOfNavigationObstacles(
                previous.positionMeters,
                current.positionMeters,
                request.obstacles,
                request.vehicle.collisionRadiusMeters,
                request.vehicle.preferredClearanceMeters,
                legalTargetIngress
                    ? std::string_view(
                        request.terminalAllowedObstacleId
                      )
                    : std::string_view{}))
        {
            return failure(
                request,
                world::navigation::TrajectoryStatus::NoSafePath,
                "scalar path-progress trajectory leaves collision-free guide"
            );
        }
    }
    out.diagnostics.collisionSegmentsChecked =
        collisionSegments;

    computeCurvatureDiagnostics(out);
    out.diagnostics.smoothCandidatesEvaluated = 1;
    out.diagnostics.smoothSafeCandidates = 1;
    out.diagnostics.selectedSmoothSupportLevel = 0;
    out.diagnostics.smoothingFellBackToPolyline = false;

    return out;
}

struct RouteAttempt
{
    world::navigation::TrajectoryGenerationResult result;
    bool ready = false;
    std::size_t failedLeg = 0;
    world::navigation::TrajectoryStatus failureStatus =
        world::navigation::TrajectoryStatus::NumericalFailure;
    std::string failureMessage;
};

RouteAttempt buildRouteAttempt(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& sourceProgress,
    const std::vector<glm::dvec3>& waypointVelocities,
    world::navigation::TrajectoryGenerationDiagnostics& cumulativeDiagnostics
)
{
    RouteAttempt attempt;
    auto& out = attempt.result;
    out.trajectory.systemId = request.systemId;
    out.trajectory.frameId = request.frameId;
    out.trajectory.startUniverseTimeSeconds = request.startUniverseTimeSeconds;
    out.trajectory.message = "Ruckig waypoint route trajectory";
    out.diagnostics.coarsePathLengthMeters = sourceProgress.back();

    const double acceleration = accelerationBudget(request.vehicle);
    game::navigation::WorldKinematicState currentState;
    currentState.positionMeters = request.pathPointsMeters.front();
    currentState.velocityMps = request.initialVelocityMps;
    currentState.accelerationMps2 = request.initialAccelerationMps2;
    glm::dvec3 currentProperAcceleration =
        request.initialAccelerationMps2;

    double accumulatedPhysicalSeconds = 0.0;
    double accumulatedPathMeters = 0.0;
    const double totalSourceProgress = sourceProgress.back();

    const glm::dvec3 firstDirection = normalizedOr(
        request.pathPointsMeters[1] - request.pathPointsMeters[0],
        glm::dvec3(0.0, 0.0, -1.0)
    );
    out.diagnostics.initialAlongPathSpeedMps =
        glm::dot(request.initialVelocityMps, firstDirection);
    const glm::dvec3 crossTrack = request.initialVelocityMps -
        firstDirection * out.diagnostics.initialAlongPathSpeedMps;
    out.diagnostics.initialCrossTrackSpeedMps = magnitude(crossTrack);
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
        const glm::dvec3 targetVelocity =
            waypointVelocities[legIndex + 1];

        auto solved = solveLeg(
            request,
            currentState,
            currentProperAcceleration,
            legEnd,
            targetVelocity,
            speedLimit,
            acceleration,
            cumulativeDiagnostics
        );
        if (!solved.ready)
        {
            attempt.failedLeg = legIndex;
            attempt.failureStatus =
                world::navigation::TrajectoryStatus::NumericalFailure;
            attempt.failureMessage =
                "Ruckig could not solve a bounded coarse-route leg";
            return attempt;
        }

        std::size_t collisionSegments = 0;
        if (!validateSweptPrediction(
                request,
                solved.prediction,
                sourceProgress[legIndex],
                sourceProgress[legIndex + 1],
                collisionSegments))
        {
            cumulativeDiagnostics.collisionSegmentsChecked +=
                collisionSegments;
            attempt.failedLeg = legIndex;
            attempt.failureStatus =
                world::navigation::TrajectoryStatus::NoSafePath;
            attempt.failureMessage =
                "Ruckig leg leaves the collision-free coarse corridor";
            return attempt;
        }
        cumulativeDiagnostics.collisionSegmentsChecked += collisionSegments;

        const double legDuration = std::max(
            Epsilon,
            solved.prediction.samples.back().timeOffsetSeconds
        );
        const glm::dvec3 fallbackForward = normalizedOr(
            legDelta,
            firstDirection
        );
        for (std::size_t sampleIndex = 0;
             sampleIndex < solved.prediction.samples.size();
             ++sampleIndex)
        {
            if (!out.trajectory.samples.empty() && sampleIndex == 0)
                continue;

            const auto& source = solved.prediction.samples[sampleIndex];
            const double u = std::clamp(
                source.timeOffsetSeconds / legDuration,
                0.0,
                1.0
            );

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
            sample.sourcePathProgressMeters =
                sourceProgress[legIndex] +
                (sourceProgress[legIndex + 1] - sourceProgress[legIndex]) * u;

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
                fallbackForward,
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

        const auto& end = solved.prediction.samples.back();
        currentState = end.state;
        currentProperAcceleration = end.properAccelerationMps2;
        accumulatedPhysicalSeconds += end.timeOffsetSeconds;
    }

    if (out.trajectory.samples.size() < 2)
    {
        attempt.failureStatus =
            world::navigation::TrajectoryStatus::NumericalFailure;
        attempt.failureMessage = "Ruckig route produced too few samples";
        return attempt;
    }

    auto& terminal = out.trajectory.samples.back();
    terminal.positionMeters = request.pathPointsMeters.back();
    terminal.velocityMps = waypointVelocities.back();
    terminal.speedMps = magnitude(terminal.velocityMps);
    terminal.sourcePathProgressMeters = totalSourceProgress;
    if (request.hasTerminalOrientation)
    {
        terminal.orientation = world::navigation::orientationForForwardUp(
            request.terminalForward,
            request.terminalUp
        );
    }

    out.trajectory.status = world::navigation::TrajectoryStatus::Ready;
    out.trajectory.durationSeconds = terminal.timeOffsetSeconds;
    out.trajectory.lengthMeters = accumulatedPathMeters;
    out.diagnostics.optimizedPathLengthMeters = accumulatedPathMeters;

    out.diagnostics.ruckigLegAttempts =
        cumulativeDiagnostics.ruckigLegAttempts;
    out.diagnostics.ruckigLegSuccesses =
        cumulativeDiagnostics.ruckigLegSuccesses;
    out.diagnostics.ruckigSolveMilliseconds =
        cumulativeDiagnostics.ruckigSolveMilliseconds;
    out.diagnostics.collisionSegmentsChecked =
        cumulativeDiagnostics.collisionSegmentsChecked;

    computeCurvatureDiagnostics(out);

    out.diagnostics.smoothCandidatesEvaluated =
        out.diagnostics.ruckigLegAttempts;
    out.diagnostics.smoothSafeCandidates =
        out.diagnostics.ruckigLegSuccesses;
    out.diagnostics.selectedSmoothSupportLevel = 0;
    out.diagnostics.smoothingFellBackToPolyline = false;

    attempt.ready = true;
    return attempt;
}

std::size_t countBlendedWaypoints(
    const std::vector<glm::dvec3>& velocities
)
{
    std::size_t count = 0;
    for (std::size_t i = 1; i + 1 < velocities.size(); ++i)
    {
        if (nonZeroVelocity(velocities[i]))
            ++count;
    }
    return count;
}

} // namespace

namespace game::navigation
{

world::navigation::TrajectoryGenerationResult RuckigRoutePlanner::plan(
    const world::navigation::TrajectoryGenerationRequest& request
)
{
    if (!validRequest(request))
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::InvalidRequest,
            "invalid Ruckig route request"
        );
    }

    const auto coarseSourceProgress =
        sourceProgressTable(request.pathPointsMeters);
    if (coarseSourceProgress.back() <= Epsilon)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::InvalidRequest,
            "Ruckig route has zero length"
        );
    }

    if (violatesKnownStopDistance(request, coarseSourceProgress))
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::InitialStateInfeasible,
            "initial along-path speed cannot meet downstream constraints"
        );
    }

    if (accelerationBudget(request.vehicle) <= Epsilon)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::InvalidRequest,
            "Ruckig route has no usable acceleration budget"
        );
    }

    const ExecutionGuide guide =
        buildExecutionGuide(request, coarseSourceProgress);

    // Ruckig is a state-to-state online trajectory generator. Do not abuse
    // the 3-D solver as a dense waypoint/path interpolator: close spatial
    // guide samples become separate target states and can create stop-like
    // slowdowns and large lateral polynomial bows. For a curved retained
    // route, geometry is fixed first and Ruckig owns only scalar path progress
    // s(t). The legacy 3-D state-to-state leg solver remains useful for the
    // true single-leg case.
    if (request.pathPointsMeters.size() > 2)
    {
        auto result = buildPathProgressTrajectory(
            request,
            coarseSourceProgress,
            guide
        );
        result.executionGuidePointsMeters =
            guide.points;
        result.diagnostics.executionGuidePoints =
            guide.points.size();
        result.diagnostics.roundedGuideCorners =
            guide.roundedCorners;
        result.diagnostics.expandedGuideCorners =
            guide.expandedCorners;

        return result;
    }

    world::navigation::TrajectoryGenerationRequest executionRequest =
        request;
    executionRequest.pathPointsMeters = guide.points;

    std::vector<glm::dvec3> waypointVelocities =
        buildWaypointVelocities(
            executionRequest,
            guide.sourceProgress
        );

    // Historical behavior stopped at every route terminal. Preserve that
    // default, but allow an explicitly authored fly-through terminal velocity
    // for tests, formation legs and ordinary moving route continuation.
    waypointVelocities.back() =
        request.hasTerminalVelocity
            ? request.terminalVelocityMps
            : glm::dvec3(0.0);

    world::navigation::TrajectoryGenerationDiagnostics cumulativeDiagnostics;
    // Preserve continuous corner motion when possible: waypoint through-speed
    // is relaxed progressively before the final StopTurnGo fallback.
    const std::size_t maxRestarts =
        guide.points.size() * 8 + 4;

    for (std::size_t restart = 0; restart < maxRestarts; ++restart)
    {
        auto attempt = buildRouteAttempt(
            executionRequest,
            guide.sourceProgress,
            waypointVelocities,
            cumulativeDiagnostics
        );

        if (attempt.ready)
        {
            attempt.result.executionGuidePointsMeters =
                guide.points;
            attempt.result.diagnostics.executionGuidePoints =
                guide.points.size();
            attempt.result.diagnostics.roundedGuideCorners =
                guide.roundedCorners;
            attempt.result.diagnostics.expandedGuideCorners =
                guide.expandedCorners;

            const std::size_t blended = countBlendedWaypoints(
                waypointVelocities
            );
            return attempt.result;
        }

        bool relaxed = false;
        const std::size_t leg = attempt.failedLeg;

        const auto relaxWaypoint = [&](std::size_t index)
        {
            if (index == 0 ||
                index + 1 >= waypointVelocities.size() ||
                !nonZeroVelocity(waypointVelocities[index]))
            {
                return false;
            }

            const double speed =
                magnitude(waypointVelocities[index]);

            // A collision or numerical failure at one candidate through-speed
            // does not prove that the corner requires a stop. Reduce the
            // through-speed first and retry the exact swept validation. Only
            // collapse to zero when the remaining speed is already negligible.
            if (speed > 1.25)
            {
                waypointVelocities[index] *= 0.70;
            }
            else
            {
                waypointVelocities[index] = glm::dvec3(0.0);
            }
            return true;
        };

        if (leg < waypointVelocities.size())
            relaxed = relaxWaypoint(leg) || relaxed;
        if (leg + 1 < waypointVelocities.size())
            relaxed = relaxWaypoint(leg + 1) || relaxed;

        if (relaxed)
            continue;

        auto failed = failure(
            request,
            attempt.failureStatus,
            attempt.failureMessage,
            cumulativeDiagnostics
        );
        failed.executionGuidePointsMeters = guide.points;
        return failed;
    }

    auto failed = failure(
        request,
        world::navigation::TrajectoryStatus::NumericalFailure,
        "Ruckig waypoint relaxation did not converge",
        cumulativeDiagnostics
    );
    failed.executionGuidePointsMeters = guide.points;
    return failed;
}

} // namespace game::navigation

namespace world::navigation
{

TrajectoryGenerationResult TrajectoryGenerator::generate(
    const TrajectoryGenerationRequest& request
)
{
    return game::navigation::RuckigRoutePlanner::plan(request);
}

} // namespace world::navigation
