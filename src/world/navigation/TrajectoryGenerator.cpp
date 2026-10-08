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
        !request.vehicle.valid() || !request.policy.valid() ||
        request.pathPointsMeters.size() < 2 ||
        !finite3(request.initialVelocityMps) ||
        !finite3(request.initialAccelerationMps2) ||
        (request.hasInitialOrientation &&
            (!finite3(request.initialForward) ||
             !finite3(request.initialUp))) ||
        (request.hasInitialAngularVelocity &&
            !finite3(request.initialAngularVelocityRadPerSecond)) ||
        (request.hasTerminalVelocity &&
            (!finite3(request.terminalVelocityMps) ||
             magnitude(request.terminalVelocityMps) >
                 request.vehicle.maxSpeedMps + 1.0e-6)) ||
        (request.hasRouteUpReference &&
            (!finite3(request.routeUpReference) ||
             magnitude(request.routeUpReference) <= Epsilon)) ||
        (request.hasTerminalAngularVelocity &&
            (!request.hasTerminalOrientation ||
             !finite3(request.terminalAngularVelocityRadPerSecond))))
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
    return std::max(request.policy.minimumSpeedMps, limit);
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
            request.policy.minimumAccelerationMps2,
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
            request.policy.cornerExpansionMinimumMeters,
            request.vehicle.collisionRadiusMeters *
                request.policy.cornerExpansionCollisionRadiusFactor
        );

        for (int expansionAttempt = 0;
             expansionAttempt < request.policy.cornerExpansionAttempts &&
             !rounded;
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

            if (angle <= request.policy.nearStraightAngleRad)
            {
                chosenEntry = corner;
                chosenExit = corner;
                chosenControl = corner;
                chosenCut = 0.0;
                chosenExpansion = expansionAttempt;
                rounded = true;
                break;
            }

            if (angle >=
                request.policy.maximumRoundableCornerAngleRad)
                continue;

            const double radiusForSpeed =
                authoredSpeed * authoredSpeed / lateralAcceleration;
            const double tangentForSpeed =
                radiusForSpeed * std::tan(angle * 0.5);

            const double maxCut =
                std::max(
                    1.0,
                    std::min(incomingLength, outgoingLength) *
                        request.policy.maximumCornerCutLegFraction
                );
            const double minCut =
                std::min(
                    maxCut,
                    std::max(
                        request.policy.minimumCornerCutMeters,
                        request.vehicle.collisionRadiusMeters *
                            request.policy.minimumCornerCutCollisionRadiusFactor
                    )
                );

            // Extra reserve keeps the execution curve from being the
            // mathematically tightest admissible turn.
            double cut = std::clamp(
                tangentForSpeed *
                    request.policy.cornerTangentReserveFactor,
                minCut,
                maxCut
            );

            for (int cutAttempt = 0;
                 cutAttempt < request.policy.cornerCutAttempts;
                 ++cutAttempt)
            {
                const glm::dvec3 entry = corner - incoming * cut;
                const glm::dvec3 exit = corner + outgoing * cut;

                const int previewSegments = std::clamp(
                    static_cast<int>(
                        std::ceil(
                            (2.0 * cut) /
                            request.policy.previewSampleSpacingMeters
                        )
                    ),
                    request.policy.previewMinimumSegments,
                    request.policy.previewMaximumSegments
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

                cut = std::max(
                    minCut,
                    cut * request.policy.cornerCutShrinkFactor
                );
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
                    std::min(chosenCut, incomingSourceLength *
                        request.policy.sourceProgressCutFraction),
                coarseProgress[i - 1] + 1.0e-6,
                coarseProgress[i] - 1.0e-6
            );
        const double exitProgress =
            std::clamp(
                coarseProgress[i] +
                    std::min(chosenCut, outgoingSourceLength *
                        request.policy.sourceProgressCutFraction),
                coarseProgress[i] + 1.0e-6,
                coarseProgress[i + 1] - 1.0e-6
            );

        const int curveSegments = std::clamp(
            static_cast<int>(
                std::ceil(
                    (2.0 * chosenCut) /
                    request.policy.curveSampleSpacingMeters
                )
            ),
            request.policy.curveMinimumSegments,
            request.policy.curveMaximumSegments
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

ExecutionGuide buildAuthoredExecutionGuide(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& sourceProgress
)
{
    ExecutionGuide guide;
    guide.points = request.pathPointsMeters;
    guide.sourceProgress = sourceProgress;
    guide.roundedCorners = 0;
    guide.expandedCorners = 0;
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
    double terminalSpeedMps,
    const world::navigation::TrajectoryGenerationPolicy& policy
)
{
    const double distance = std::max(0.0, distanceMeters);
    const double speed =
        std::max(policy.minimumSpeedMps, maxSpeedMps);
    const double acceleration =
        std::max(policy.minimumAccelerationMps2, accelerationMps2);
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
    return std::max(
        policy.minimumLegDurationSeconds,
        ideal * policy.legDurationScale +
            policy.legDurationPaddingSeconds
    );
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
        if (angle >=
            request.policy.maximumRoundableCornerAngleRad)
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
            request.policy.nominalBlendMinimumMeters,
            std::min(incomingLength, outgoingLength) *
                request.policy.nominalBlendLegFraction
        );
        const double minimumBlendDistance = std::min(
            nominalBlendDistance,
            std::max(
                request.policy.minimumBlendMeters,
                request.vehicle.collisionRadiusMeters *
                    request.policy.minimumBlendCollisionRadiusFactor
            )
        );

        double blendDistance = nominalBlendDistance;
        bool foundSafeBlend = false;
        for (int attempt = 0;
             attempt < request.policy.blendAttempts;
             ++attempt)
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
                    blendDistance *
                        request.policy.blendShrinkFactor
                );
        }

        if (!foundSafeBlend)
            continue;

        glm::dvec3 direction = incoming + outgoing;
        if (magnitude(direction) <= Epsilon)
            continue;
        direction = glm::normalize(direction);

        const double lateralAcceleration = std::max(
            request.policy.minimumAccelerationMps2,
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
        if (speed <=
            request.policy.minimumUsefulWaypointSpeedMps)
            continue;

        velocities[i] = direction * speed;
    }

    return velocities;
}

bool nonZeroVelocity(
    const glm::dvec3& value,
    double thresholdMps
) noexcept
{
    const double threshold =
        std::max(0.0, thresholdMps);
    return
        glm::dot(value, value) >
        threshold * threshold;
}

glm::dquat sampleOrientation(
    const world::navigation::TrajectoryGenerationRequest& request,
    const glm::dvec3& velocity,
    const glm::dvec3& fallbackForward,
    double sourceProgress,
    double totalSourceProgress,
    double sampleTimeOffsetSeconds,
    double terminalTimeOffsetSeconds
)
{
    const glm::dvec3 forward = normalizedOr(
        velocity,
        normalizedOr(fallbackForward, glm::dvec3(0.0, 0.0, -1.0))
    );

    glm::dvec3 upHint(0.0, 1.0, 0.0);
    if (request.hasRouteUpReference)
    {
        upHint = glm::normalize(request.routeUpReference);
    }
    else if (request.hasTerminalOrientation &&
        magnitude(request.terminalUp) > Epsilon)
    {
        upHint = glm::normalize(request.terminalUp);
    }

    glm::dquat orientation =
        world::navigation::orientationForForwardUp(forward, upHint);

    if (!request.hasTerminalOrientation)
        return orientation;

    glm::dquat terminal =
        world::navigation::orientationForForwardUp(
            request.terminalForward,
            request.terminalUp
        );

    // A rotating capture target owns a time-varying terminal attitude. Work
    // backwards from the exact final pose using its authored angular velocity
    // so the last trajectory samples already carry the target spin instead of
    // forcing AcceptedManeuverProgramBuilder to invent an instantaneous omega
    // jump at the final sample.
    if (request.hasTerminalAngularVelocity &&
        finite(sampleTimeOffsetSeconds) &&
        finite(terminalTimeOffsetSeconds) &&
        terminalTimeOffsetSeconds >= sampleTimeOffsetSeconds)
    {
        const double omega =
            magnitude(request.terminalAngularVelocityRadPerSecond);
        const double remainingSeconds =
            terminalTimeOffsetSeconds - sampleTimeOffsetSeconds;
        if (omega > Epsilon && remainingSeconds > Epsilon)
        {
            const glm::dvec3 axis =
                request.terminalAngularVelocityRadPerSecond / omega;
            terminal = glm::normalize(
                glm::angleAxis(
                    -omega * remainingSeconds,
                    axis
                ) * terminal
            );
        }
    }
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


glm::dvec3 angularVelocityBetweenOrientations(
    const glm::dquat& from,
    const glm::dquat& to,
    double durationSeconds
) noexcept
{
    if (!(durationSeconds > Epsilon))
        return glm::dvec3(0.0);

    glm::dquat delta = glm::normalize(
        to * glm::conjugate(from)
    );
    if (delta.w < 0.0)
        delta = -delta;

    const double w = std::clamp(delta.w, -1.0, 1.0);
    const double angle = 2.0 * std::acos(w);
    const double sinHalf =
        std::sqrt(std::max(0.0, 1.0 - w * w));
    if (!(angle > Epsilon) || !(sinHalf > Epsilon))
        return glm::dvec3(0.0);

    return glm::dvec3(
        delta.x / sinHalf,
        delta.y / sinHalf,
        delta.z / sinHalf
    ) * (angle / durationSeconds);
}

bool compileBoundedAngularKinematics(
    const world::navigation::TrajectoryGenerationRequest& request,
    world::navigation::Trajectory& trajectory,
    std::string* failureReason = nullptr
)
{
    const auto angularFail = [&](std::string reason)
    {
        if (failureReason)
            *failureReason = std::move(reason);
        return false;
    };
    // Legacy/advisory callers that do not supply the real hull attitude keep
    // the existing geometric orientation product. Automatic execution supplies
    // an initial body state and therefore receives a physical angular program.
    if (!request.hasInitialOrientation)
        return true;

    if (trajectory.samples.size() < 2)
        return angularFail("too-few-angular-samples");

    const double maxRate =
        request.vehicle.maxAngularVelocityRadPerSecond;
    const double maxAlpha =
        request.vehicle.maxAngularAccelerationRadPerSecond2;
    if (!(maxRate > Epsilon) || !(maxAlpha > Epsilon))
        return angularFail("invalid-angular-capability");

    std::vector<glm::dquat> desired;
    desired.reserve(trajectory.samples.size());
    for (const auto& sample : trajectory.samples)
        desired.push_back(glm::normalize(sample.orientation));

    glm::dquat current = world::navigation::orientationForForwardUp(
        request.initialForward,
        request.initialUp
    );
    glm::dvec3 omega =
        request.hasInitialAngularVelocity
            ? request.initialAngularVelocityRadPerSecond
            : glm::dvec3(0.0);

    if (!finite3(omega) ||
        magnitude(omega) > maxRate + 1.0e-6)
    {
        return angularFail(
            "initial-omega-outside-capability omega=" +
            std::to_string(magnitude(omega)) +
            " max=" + std::to_string(maxRate)
        );
    }

    trajectory.samples.front().orientation = current;
    trajectory.samples.front().angularVelocityRadPerSecond = omega;

    for (std::size_t i = 1; i < trajectory.samples.size(); ++i)
    {
        const double dt =
            trajectory.samples[i].timeOffsetSeconds -
            trajectory.samples[i - 1].timeOffsetSeconds;
        if (!(dt > Epsilon) || !finite(dt))
            return angularFail(
                "invalid-angular-step sample=" + std::to_string(i) +
                " dt=" + std::to_string(dt)
            );

        glm::dquat target = desired[i];
        if (glm::dot(current, target) < 0.0)
            target = -target;

        const glm::dvec3 errorVector =
            angularVelocityBetweenOrientations(
                current,
                target,
                1.0
            );
        const double errorAngle = magnitude(errorVector);

        glm::dvec3 feedForward =
            angularVelocityBetweenOrientations(
                desired[i - 1],
                desired[i],
                dt
            );

        glm::dvec3 correction(0.0);
        if (errorAngle > Epsilon)
        {
            const double correctionSpeed = std::min(
                maxRate,
                std::sqrt(
                    std::max(
                        0.0,
                        2.0 * maxAlpha * errorAngle
                    )
                )
            );
            correction =
                (errorVector / errorAngle) * correctionSpeed;
        }

        glm::dvec3 targetOmega = feedForward + correction;

        const double targetSpeed = magnitude(targetOmega);
        if (targetSpeed > maxRate && targetSpeed > Epsilon)
            targetOmega *= maxRate / targetSpeed;

        if (request.hasTerminalAngularVelocity)
        {
            const glm::dvec3 terminalOmega =
                request.terminalAngularVelocityRadPerSecond;
            const double terminalTime =
                trajectory.samples.back().timeOffsetSeconds;
            const double previousTime =
                trajectory.samples[i - 1].timeOffsetSeconds;
            const double currentTime =
                trajectory.samples[i].timeOffsetSeconds;
            const double remainingBefore = std::max(
                0.0,
                terminalTime - previousTime
            );
            const double remainingAfter = std::max(
                0.0,
                terminalTime - currentTime
            );

            // Terminal omega is a boundary condition, not something to snap
            // onto in the final sample. At every step the current state must
            // remain inside the backwards-reachable alpha cone:
            //
            //   |omega(t) - omega_terminal| <= alpha_max * (T - t)
            //
            // The old implementation only replaced targetOmega on the final
            // sample, so a perfectly feasible program could arrive there with
            // too much angular speed to remove in one dt and reject itself.
            const double terminalOmegaErrorBefore =
                magnitude(omega - terminalOmega);
            const double terminalOmegaReachBefore =
                maxAlpha * remainingBefore;
            if (terminalOmegaErrorBefore >
                terminalOmegaReachBefore + 1.0e-6)
            {
                return angularFail(
                    "terminal-omega-unreachable-before-sample=" +
                    std::to_string(i) +
                    " error=" +
                    std::to_string(terminalOmegaErrorBefore) +
                    " reachable=" +
                    std::to_string(terminalOmegaReachBefore)
                );
            }

            glm::dvec3 terminalDelta =
                targetOmega - terminalOmega;
            const double terminalDeltaMagnitude =
                magnitude(terminalDelta);
            const double maxTerminalDelta =
                maxAlpha * remainingAfter;
            if (terminalDeltaMagnitude > maxTerminalDelta &&
                terminalDeltaMagnitude > Epsilon)
            {
                terminalDelta *=
                    maxTerminalDelta /
                    terminalDeltaMagnitude;
                targetOmega =
                    terminalOmega + terminalDelta;
            }
        }

        glm::dvec3 deltaOmega = targetOmega - omega;
        const double deltaMagnitude = magnitude(deltaOmega);
        const double maxDelta = maxAlpha * dt;
        if (deltaMagnitude > maxDelta && deltaMagnitude > Epsilon)
            deltaOmega *= maxDelta / deltaMagnitude;

        const glm::dvec3 nextOmega = omega + deltaOmega;
        const glm::dvec3 averageOmega =
            0.5 * (omega + nextOmega);
        const double averageSpeed = magnitude(averageOmega);

        if (averageSpeed > Epsilon)
        {
            double stepAngle = averageSpeed * dt;
            const glm::dvec3 stepAxis =
                averageOmega / averageSpeed;

            if (errorAngle > Epsilon)
            {
                const glm::dvec3 errorAxis =
                    errorVector / errorAngle;
                if (glm::dot(stepAxis, errorAxis) > 0.999 &&
                    stepAngle > errorAngle)
                {
                    stepAngle = errorAngle;
                }
            }

            current = glm::normalize(
                glm::angleAxis(stepAngle, stepAxis) * current
            );
        }

        omega = nextOmega;
        trajectory.samples[i].orientation = current;
        trajectory.samples[i].angularVelocityRadPerSecond = omega;
    }

    if (request.hasTerminalOrientation)
    {
        const glm::dquat terminal =
            world::navigation::orientationForForwardUp(
                request.terminalForward,
                request.terminalUp
            );
        const double terminalAngleError =
            magnitude(
                angularVelocityBetweenOrientations(
                    current,
                    terminal,
                    1.0
                )
            );
        if (terminalAngleError > 0.08726646259971647)
            return angularFail(
                "terminal-orientation-error-deg=" +
                std::to_string(glm::degrees(terminalAngleError))
            );
    }

    if (request.hasTerminalAngularVelocity)
    {
        const double terminalOmegaError =
            magnitude(
                omega -
                request.terminalAngularVelocityRadPerSecond
            );
        if (terminalOmegaError > 0.05)
        {
            return angularFail(
                "terminal-omega-error=" +
                std::to_string(terminalOmegaError)
            );
        }
    }

    trajectory.angularKinematicsAuthored = true;
    return true;
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
        terminalSpeed,
        request.policy
    );

    const double speedForSampling = std::max({
        request.policy.minimumSamplingSpeedMps,
        speedLimit,
        inheritedSpeed,
        terminalSpeed
    });
    const double desiredCollisionChord = std::clamp(
        std::max(
            request.policy.collisionChordMinimumMeters,
            request.vehicle.collisionRadiusMeters *
                request.policy.collisionChordRadiusFactor
        ),
        request.policy.collisionChordMinimumMeters,
        request.policy.collisionChordMaximumMeters
    );
    const double sampleInterval = std::clamp(
        desiredCollisionChord / speedForSampling,
        request.policy.legSampleIntervalMinimumSeconds,
        request.policy.legSampleIntervalMaximumSeconds
    );

    for (int attempt = 0;
         attempt < request.policy.ruckigDurationAttempts;
         ++attempt)
    {
        game::navigation::RuckigTrajectoryRequest ruckigRequest;
        ruckigRequest.systemId = request.systemId;
        ruckigRequest.startUniverseTimeSeconds = 0.0;
        ruckigRequest.initialState = initialState;
        ruckigRequest.initialProperAccelerationMps2 = initialProperAcceleration;
        ruckigRequest.motionEnvelope.maxProperAccelerationMps2 = acceleration;
        ruckigRequest.motionEnvelope.maxProperJerkMps3 =
            std::max(
                request.policy.jerkMinimumMps3,
                acceleration *
                    request.policy.jerkAccelerationMultiplier
            );
        ruckigRequest.horizonSeconds = duration;
        ruckigRequest.sampleIntervalSeconds = std::min(
            sampleInterval,
            duration
        );
        ruckigRequest.validationStepSeconds = std::min(
            request.policy.validationStepSeconds,
            duration
        );
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
            ) + std::max(
                request.policy.peakSpeedToleranceMps,
                speedLimit *
                    request.policy.peakSpeedToleranceFraction
            );
            if (candidate.prediction.diagnostics.maxSpeedMps <=
                permittedPeakSpeed)
            {
                ++diagnostics.ruckigLegSuccesses;
                out.ready = true;
                out.prediction = std::move(candidate.prediction);
                return out;
            }
        }

        duration *= request.policy.durationRetryFactor;
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

glm::dvec3 guideVertexCurvatureVector(
    const ExecutionGuide& guide,
    std::size_t index
)
{
    if (guide.points.size() < 3 ||
        index == 0 ||
        index + 1 >= guide.points.size())
    {
        return glm::dvec3(0.0);
    }

    const glm::dvec3 incomingDelta =
        guide.points[index] -
        guide.points[index - 1];
    const glm::dvec3 outgoingDelta =
        guide.points[index + 1] -
        guide.points[index];
    const double incomingLength =
        magnitude(incomingDelta);
    const double outgoingLength =
        magnitude(outgoingDelta);
    const double acrossLength = magnitude(
        guide.points[index + 1] -
        guide.points[index - 1]
    );
    const double denominator =
        incomingLength * outgoingLength * acrossLength;
    if (denominator <= Epsilon)
        return glm::dvec3(0.0);

    const double curvature =
        2.0 * magnitude(
            glm::cross(incomingDelta, outgoingDelta)
        ) / denominator;
    if (curvature <= 1.0e-12)
        return glm::dvec3(0.0);

    const glm::dvec3 incoming =
        incomingDelta / incomingLength;
    const glm::dvec3 outgoing =
        outgoingDelta / outgoingLength;
    const glm::dvec3 tangentDelta =
        outgoing - incoming;
    return normalizedOr(
        tangentDelta,
        glm::dvec3(0.0)
    ) * curvature;
}

glm::dvec3 guideCurvatureVector(
    const world::navigation::TrajectoryGenerationRequest& request,
    const ExecutionGuide& guide,
    const std::vector<double>& arc,
    double progressMeters
)
{
    (void)request;
    if (guide.points.size() < 3 ||
        arc.size() != guide.points.size() ||
        arc.back() <= Epsilon)
    {
        return glm::dvec3(0.0);
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
    std::size_t right =
        upper == arc.end()
            ? arc.size() - 1
            : static_cast<std::size_t>(
                std::distance(arc.begin(), upper)
              );
    right = std::clamp<std::size_t>(
        right,
        1,
        arc.size() - 1
    );
    const std::size_t left = right - 1;

    const double span =
        std::max(Epsilon, arc[right] - arc[left]);
    const double u = std::clamp(
        (s - arc[left]) / span,
        0.0,
        1.0
    );

    // The execution guide is a sampled smooth corner. Treating each sampled
    // chord as a piecewise-constant tangent created a mathematical impulse at
    // every sample boundary: a 1 m finite-difference probe could report
    // 5-10x the physical curvature that the speed planner used. Interpolate
    // the circumcircle curvature of adjacent guide vertices instead, so the
    // feed-forward acceleration and the speed limit describe the same curve.
    //
    // Curvature is, by definition, normal to the instantaneous tangent.
    // Interpolating two vertex normals can introduce a small tangential
    // component on a chord. If left in place, that fake component is added to
    // the real longitudinal acceleration and can make an otherwise feasible
    // sample exceed the vehicle envelope.
    const glm::dvec3 tangent =
        sampleGuide(guide, arc, s).tangent;
    glm::dvec3 curvature =
        guideVertexCurvatureVector(guide, left) * (1.0 - u) +
        guideVertexCurvatureVector(guide, right) * u;
    curvature -= tangent * glm::dot(curvature, tangent);
    return curvature;
}

struct KeyframedProgressSample
{
    double timeOffsetSeconds = 0.0;
    double progressMeters = 0.0;
    double speedMps = 0.0;
    double accelerationMps2 = 0.0;
};

struct KeyframedProgressResult
{
    bool ready = false;
    std::string message;
    double durationSeconds = 0.0;
    std::vector<KeyframedProgressSample> samples;
};

KeyframedProgressResult keyframedGuideProgress(
    const world::navigation::TrajectoryGenerationRequest& request,
    const ExecutionGuide& guide,
    const std::vector<double>& arc,
    double initialSpeed,
    double terminalSpeed,
    double speedScale
)
{
    KeyframedProgressResult out;
    const std::size_t count = guide.points.size();
    if (count < 2 || arc.size() != count)
        return out;

    // SINGLE GLOBAL SPEED POLICY:
    // cruise = 0.8 * physical max speed.
    //
    // speedScale is NOT allowed to reduce cruise globally. It is retained only
    // as a local terminal-attitude time scale applied inside the terminal
    // orientation blend below.
    const double cruise =
        request.vehicle.maxSpeedMps * 0.8;
    const double terminalAngularScale =
        std::clamp(speedScale, 0.01, 1.0);
    const double accelerating =
        request.vehicle.maxForwardAccelerationMps2;
    const double braking =
        request.vehicle.maxBrakingAccelerationMps2;
    const double lateral =
        request.vehicle.maxLateralAccelerationMps2;
    std::vector<double> limits(count, cruise);

    // A restriction belongs to its local route station. A slow docking arc
    // must never set the cruise speed of a straight kilometre away.
    for (const auto& range : request.speedLimitRanges)
    {
        if (!finite(range.sourcePathStartMeters) ||
            !finite(range.sourcePathEndMeters) ||
            !finite(range.maxSpeedMps) || range.maxSpeedMps <= 0.0)
            continue;
        const double lo = std::min(range.sourcePathStartMeters,
                                   range.sourcePathEndMeters);
        const double hi = std::max(range.sourcePathStartMeters,
                                   range.sourcePathEndMeters);
        const auto first = std::lower_bound(guide.sourceProgress.begin(),
                                            guide.sourceProgress.end(), lo);
        const auto last = std::upper_bound(first,
            guide.sourceProgress.end(), hi);
        for (auto it = first; it != last; ++it)
        {
            const auto index = static_cast<std::size_t>(
                it - guide.sourceProgress.begin());
            limits[index] = std::min(limits[index], range.maxSpeedMps);
        }
    }
    for (const auto& constraint : request.pointSpeedConstraints)
    {
        if (!finite(constraint.sourcePathProgressMeters) ||
            !finite(constraint.maxSpeedMps) ||
            constraint.maxSpeedMps < 0.0)
            continue;
        const auto after = std::lower_bound(
            guide.sourceProgress.begin(), guide.sourceProgress.end(),
            constraint.sourcePathProgressMeters);
        std::size_t nearest = static_cast<std::size_t>(
            after - guide.sourceProgress.begin());
        if (nearest == count)
            nearest = count - 1;
        else if (nearest > 0 &&
                 std::abs(guide.sourceProgress[nearest - 1] -
                          constraint.sourcePathProgressMeters) <
                 std::abs(guide.sourceProgress[nearest] -
                          constraint.sourcePathProgressMeters))
            --nearest;
        limits[nearest] = std::min(limits[nearest],
                                   constraint.maxSpeedMps);
    }

    // If terminal hull attitude needs extra time, buy that time ONLY inside
    // the authored terminal-orientation blend. Never reduce cruise on remote
    // straights or unrelated turns.
    const double terminalBlendDistance =
        request.hasTerminalOrientation
            ? std::max(
                0.0,
                request.terminalOrientationBlendDistanceMeters
              )
            : 0.0;
    const double terminalBlendStart =
        !guide.sourceProgress.empty()
            ? std::max(
                0.0,
                guide.sourceProgress.back() -
                    terminalBlendDistance
              )
            : 0.0;
    if (terminalAngularScale < 0.999 &&
        terminalBlendDistance > Epsilon)
    {
        const double terminalCruise =
            cruise * terminalAngularScale;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (guide.sourceProgress[i] + Epsilon >=
                terminalBlendStart)
            {
                limits[i] =
                    std::min(limits[i], terminalCruise);
            }
        }
    }
    for (std::size_t i = 1; i + 1 < count; ++i)
    {
        const double curvature = magnitude(
            guideCurvatureVector(
                request,
                guide,
                arc,
                arc[i]
            )
        );
        if (curvature > 1.0e-9)
        {
            limits[i] = std::min(
                limits[i],
                std::sqrt(lateral / curvature)
            );
        }
    }

    // Curvature is interpolated along each guide edge, then projected normal
    // to that edge's constant tangent. At an exact guide vertex
    // guideCurvatureVector() deliberately changes to the outgoing edge
    // (upper_bound), so sampling curvature only at arc[i] is NOT a safe bound
    // for the preceding edge.
    //
    // For one edge, project both endpoint vertex-curvature vectors onto that
    // edge's normal plane. The curvature vector inside the edge is their
    // linear interpolation. Its norm is convex, so the maximum over the edge
    // is attained at one of those projected endpoints. This gives an exact
    // speed ceiling for the same curvature model used later to construct
    // feed-forward acceleration.
    std::vector<double> edgeLimits(count - 1, cruise);
    for (std::size_t i = 0; i + 1 < count; ++i)
    {
        const glm::dvec3 edgeTangent = normalizedOr(
            guide.points[i + 1] - guide.points[i],
            glm::dvec3(1.0, 0.0, 0.0)
        );

        auto edgeNormalCurvature =
            [&](std::size_t vertexIndex)
            {
                glm::dvec3 curvature =
                    guideVertexCurvatureVector(
                        guide,
                        vertexIndex
                    );
                curvature -=
                    edgeTangent *
                    glm::dot(curvature, edgeTangent);
                return magnitude(curvature);
            };

        const double edgeCurvature = std::max(
            edgeNormalCurvature(i),
            edgeNormalCurvature(i + 1)
        );
        const double edgeCurvatureSpeed =
            edgeCurvature > 1.0e-9
                ? std::sqrt(lateral / edgeCurvature)
                : cruise;

        double localCruise = cruise;
        if (terminalAngularScale < 0.999 &&
            terminalBlendDistance > Epsilon &&
            guide.sourceProgress[i + 1] + Epsilon >=
                terminalBlendStart)
        {
            localCruise =
                cruise * terminalAngularScale;
        }

        edgeLimits[i] = std::min(
            {
                localCruise,
                segmentSpeedLimit(
                    request,
                    guide.sourceProgress[i],
                    guide.sourceProgress[i + 1]
                ),
                edgeCurvatureSpeed
            }
        );
        // A low speed at the *end* of a long straight is a braking target,
        // never a cruise cap over the entire straight.
        if (arc[i + 1] - arc[i] <= 30.0)
        {
            // Point limits are boundary conditions. In particular, a zero
            // speed at HOLD means "arrive here stopped"; it is NOT a zero
            // cruise-speed limit over the whole adjacent edge. Applying zero
            // to edgeLimits makes the profile stop early and then attempt to
            // coast the remaining distance at v=0.
            if (limits[i] > Epsilon)
                edgeLimits[i] =
                    std::min(edgeLimits[i], limits[i]);
            if (limits[i + 1] > Epsilon)
                edgeLimits[i] =
                    std::min(edgeLimits[i], limits[i + 1]);
        }
    }

    std::vector<double> speeds = limits;
    speeds.front() = std::max(0.0, initialSpeed);
    speeds.back() = std::max(0.0, terminalSpeed);
    // Initial speed is measured state, not a route constraint. It may already
    // exceed the route/vehicle limit; that means brake, not reject the route.
    // Terminal speed remains an authored boundary and must be feasible.
    if (speeds.back() > request.vehicle.maxSpeedMps + Epsilon)
        return out;
    for (std::size_t i = count - 1; i > 0; --i)
    {
        const double distance = arc[i] - arc[i - 1];
        if (distance <= Epsilon)
            return out;
        const double reachable = std::sqrt(
            speeds[i] * speeds[i] + 2.0 * braking * distance);
        if (i > 1)
            speeds[i - 1] = std::min(speeds[i - 1], reachable);
        // i==1 reaches the measured initial state. Never rewrite or reject
        // it: an excessive actual speed is handled by the forward braking
        // repair pass below.
    }
    for (std::size_t i = 1; i < count; ++i)
    {
        const double distance = arc[i] - arc[i - 1];

        // Upper reachable speed when accelerating.
        const double maxReachable = std::sqrt(
            speeds[i - 1] * speeds[i - 1] +
            2.0 * accelerating * distance);

        // Lower reachable speed when braking at full available authority.
        // If this is still above the authored route limit, the craft is
        // temporarily overspeed but remains on the route and keeps braking.
        const double minReachable = std::sqrt(
            std::max(
                0.0,
                speeds[i - 1] * speeds[i - 1] -
                2.0 * braking * distance
            )
        );

        if (speeds[i] > maxReachable)
            speeds[i] = maxReachable;
        if (speeds[i] < minReachable)
            speeds[i] = minReachable;
    }

    out.samples.push_back({0.0, 0.0, speeds.front(), 0.0});
    const double interval = request.policy.progressSampleIntervalSeconds;
    double clock = 0.0;
    // The extra peak/cruise/brake keyframes are produced analytically on each
    // long straight. Speeds are interpolated by v²(s): this makes physical
    // acceleration constant on a keyframe interval, including a stop at HOLD.
    for (std::size_t i = 0; i + 1 < count; ++i)
    {
        const double distance = arc[i + 1] - arc[i];
        const double v0 = speeds[i], v1 = speeds[i + 1];
        const double segmentCeiling = edgeLimits[i];

        const auto leg = [&](double length, double from, double acceleration)
        {
            if (length <= 1.0e-8)
                return;
            const double to = std::sqrt(std::max(0.0,
                from * from + 2.0 * acceleration * length));
            const double denominator =
                std::abs(acceleration) > Epsilon
                    ? from + to
                    : from;

            // Never turn an infeasible zero-speed coast into an unbounded
            // sample loop. A stop is a boundary at a route station; movement
            // over non-zero distance requires either non-zero speed or
            // acceleration.
            if (!(denominator > Epsilon) ||
                !finite(denominator))
            {
                return;
            }

            const double duration =
                std::abs(acceleration) > Epsilon
                    ? 2.0 * length / denominator
                    : length / denominator;
            if (!(duration >= 0.0) || !finite(duration))
                return;

            const double startClock = clock;
            const double startProgress = out.samples.back().progressMeters;
            while (clock + interval < startClock + duration - 1.0e-8)
            {
                clock += interval;
                const double t = clock - startClock;
                out.samples.push_back({clock,
                    startProgress + from * t +
                        0.5 * acceleration * t * t,
                    from + acceleration * t, acceleration});
            }
            clock = startClock + duration;
            out.samples.push_back({clock, startProgress + length,
                to, acceleration});
        };

        double remainingDistance = distance;
        double currentSpeed = v0;

        // If the measured state is already over this segment's ceiling,
        // braking starts immediately. Do not coast above the route limit just
        // because the old peak-speed construction could postpone braking.
        if (currentSpeed > segmentCeiling + Epsilon &&
            braking > Epsilon)
        {
            const double distanceToCeiling =
                (currentSpeed * currentSpeed -
                 segmentCeiling * segmentCeiling) /
                (2.0 * braking);
            const double brakeNow =
                std::min(remainingDistance, distanceToCeiling);
            leg(brakeNow, currentSpeed, -braking);
            currentSpeed = std::sqrt(
                std::max(
                    0.0,
                    currentSpeed * currentSpeed -
                    2.0 * braking * brakeNow
                )
            );
            remainingDistance -= brakeNow;
        }

        if (remainingDistance > 1.0e-8)
        {
            const double vmax =
                std::max({currentSpeed, v1, segmentCeiling});
            const double accelerateDistance =
                std::max(
                    0.0,
                    (vmax * vmax - currentSpeed * currentSpeed) /
                        (2.0 * accelerating)
                );
            const double brakeDistance =
                std::max(
                    0.0,
                    (vmax * vmax - v1 * v1) /
                        (2.0 * braking)
                );

            double peak = vmax;
            if (accelerateDistance + brakeDistance > remainingDistance)
            {
                peak = std::sqrt(std::max(0.0,
                    (2.0 * accelerating * braking * remainingDistance +
                     braking * currentSpeed * currentSpeed +
                     accelerating * v1 * v1) /
                    (accelerating + braking)));
            }

            const double upDistance =
                std::max(
                    0.0,
                    (peak * peak - currentSpeed * currentSpeed) /
                        (2.0 * accelerating)
                );
            const double downDistance =
                std::max(
                    0.0,
                    (peak * peak - v1 * v1) /
                        (2.0 * braking)
                );
            const double coastDistance =
                std::max(
                    0.0,
                    remainingDistance - upDistance - downDistance
                );

            leg(upDistance, currentSpeed, accelerating);
            leg(coastDistance, peak, 0.0);
            leg(downDistance, peak, -braking);
        }

        out.samples.back().progressMeters = arc[i + 1];
        out.samples.back().speedMps = v1;
    }
    // The initial speed is a boundary condition, not a command to hold
    // zero acceleration. If the first non-zero-length leg accelerates or
    // brakes, the t=0 sample must carry that physical acceleration too.
    // Otherwise a spatial follower parked exactly at s=0 sees v=0,a=0 and
    // has no command that can start motion.
    if (out.samples.size() >= 2)
        out.samples.front().accelerationMps2 =
            out.samples[1].accelerationMps2;

    out.durationSeconds = clock;
    out.ready = out.samples.size() >= 2 && finite(clock);
    if (!out.ready)
        out.message = "keyframed speed profile infeasible";
    return out;
}

world::navigation::TrajectoryGenerationResult
buildPathProgressTrajectory(
    const world::navigation::TrajectoryGenerationRequest& request,
    const std::vector<double>& coarseSourceProgress,
    const ExecutionGuide& guide,
    double speedScale
)
{

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

    const auto progress = keyframedGuideProgress(
        request, guide, arc, std::max(0.0, initialAlongSpeed),
        terminalAlongSpeed, speedScale);

    if (!progress.ready)
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::NumericalFailure,
            "keyframed path progress failed: " +
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
        speedScale < 0.999
            ? "keyframed path-progress trajectory; terminal-angular-speed-relaxed"
            : "keyframed path-progress trajectory";
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
    // This branch uses route-local keyframes, not the legacy one-scalar
    // Ruckig solve. Keep solver diagnostics truthful for runtime analysis.
    out.diagnostics.ruckigLegAttempts = 0;
    out.diagnostics.ruckigLegSuccesses = 0;
    out.diagnostics.initialAlongPathSpeedMps =
        initialAlongSpeed;
    out.diagnostics.initialCrossTrackSpeedMps =
        magnitude(initialCrossVelocity);
    out.diagnostics.pathCaptureRequired =
        magnitude(initialCrossVelocity) >
            request.policy.pathCaptureSpeedThresholdMps;

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
                request,
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
                : coarseSourceProgress.back(),
            sample.timeOffsetSeconds,
            progress.durationSeconds
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

    // A mapped path-progress sample must remain inside the same physical
    // directional envelope used to build its speed profile. This is the
    // Planner-side proof that prevents a mathematically valid curve from
    // asking the live ship for, e.g., 100+ m/s^2 of lateral acceleration.
    for (std::size_t sampleIndex = 0;
         sampleIndex < out.trajectory.samples.size();
         ++sampleIndex)
    {
        const auto& sample = out.trajectory.samples[sampleIndex];
        const glm::dvec3 forward = normalizedOr(
            sample.velocityMps,
            sample.orientation *
                glm::dvec3(0.0, 0.0, -1.0)
        );
        const double along =
            glm::dot(sample.accelerationMps2, forward);
        const glm::dvec3 lateralAcceleration =
            sample.accelerationMps2 - forward * along;
        const double lateralMagnitude =
            magnitude(lateralAcceleration);

        // This is a numerical feasibility check over sampled/interpolated
        // geometry, not a hardware calibration bench. Comparing against the
        // physical envelope with a 1e-5 m/s^2 absolute epsilon makes harmless
        // interpolation noise reject an otherwise identical executable path.
        //
        // Use a small engineering tolerance: 0.2% of each installed limit,
        // with only a tiny absolute floor for near-zero authorities. Material
        // envelope violations still fail and may trigger the slower-profile
        // retry below.
        const auto envelopeTolerance =
            [](double limit)
            {
                return std::max(
                    1.0e-4,
                    std::abs(limit) * 2.0e-3
                );
            };

        const double forwardTolerance =
            envelopeTolerance(
                request.vehicle.maxForwardAccelerationMps2
            );
        const double brakeTolerance =
            envelopeTolerance(
                request.vehicle.maxBrakingAccelerationMps2
            );
        const double lateralTolerance =
            envelopeTolerance(
                request.vehicle.maxLateralAccelerationMps2
            );

        if (along >
                request.vehicle.maxForwardAccelerationMps2 +
                    forwardTolerance ||
            along <
                -request.vehicle.maxBrakingAccelerationMps2 -
                    brakeTolerance ||
            lateralMagnitude >
                request.vehicle.maxLateralAccelerationMps2 +
                    lateralTolerance)
        {
            return failure(
                request,
                world::navigation::TrajectoryStatus::NumericalFailure,
                "path-progress acceleration exceeds vehicle envelope"
                " sample=" + std::to_string(sampleIndex) +
                " along=" + std::to_string(along) +
                " forward_limit=" +
                    std::to_string(
                        request.vehicle.maxForwardAccelerationMps2
                    ) +
                " forward_tol=" +
                    std::to_string(forwardTolerance) +
                " brake_limit=" +
                    std::to_string(
                        request.vehicle.maxBrakingAccelerationMps2
                    ) +
                " brake_tol=" +
                    std::to_string(brakeTolerance) +
                " lateral=" + std::to_string(lateralMagnitude) +
                " lateral_limit=" +
                    std::to_string(
                        request.vehicle.maxLateralAccelerationMps2
                    ) +
                " lateral_tol=" +
                    std::to_string(lateralTolerance) +
                " speed=" + std::to_string(sample.speedMps)
            );
        }
    }

    // Preserve the exact authored initial position and measured velocity.
    // Acceleration is different: this trajectory sample is an executable
    // feed-forward command, so it must keep the acceleration of the first
    // physical path-progress leg. Overwriting it with the measured/seed
    // initial acceleration (commonly zero at a stopped start) creates the
    // dead state v=0,a=0 and the follower has no command that can launch.
    out.trajectory.samples.front().positionMeters =
        request.pathPointsMeters.front();
    out.trajectory.samples.front().velocityMps =
        request.initialVelocityMps;
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

    std::string angularFailureReason;
    if (!compileBoundedAngularKinematics(
            request,
            out.trajectory,
            &angularFailureReason))
    {
        return failure(
            request,
            world::navigation::TrajectoryStatus::InitialStateInfeasible,
            "angular trajectory cannot reach requested terminal state: " +
                angularFailureReason
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
        out.diagnostics.initialCrossTrackSpeedMps >
        request.policy.pathCaptureSpeedThresholdMps;

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
                totalSourceProgress,
                sample.timeOffsetSeconds,
                0.0
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

    // Single-leg routes learn their total duration only after the Ruckig solve.
    // Re-evaluate the terminal-attitude blend here so rotating terminal targets
    // receive the same time-varying boundary condition as scalar path routes.
    if (request.hasTerminalOrientation &&
        request.hasTerminalAngularVelocity)
    {
        for (std::size_t i = 0;
             i < out.trajectory.samples.size();
             ++i)
        {
            auto& sample = out.trajectory.samples[i];
            glm::dvec3 fallbackForward(0.0, 0.0, -1.0);
            if (glm::length(sample.velocityMps) > Epsilon)
            {
                fallbackForward = sample.velocityMps;
            }
            else if (i + 1 < out.trajectory.samples.size())
            {
                fallbackForward =
                    out.trajectory.samples[i + 1].positionMeters -
                    sample.positionMeters;
            }
            else if (i > 0)
            {
                fallbackForward =
                    sample.positionMeters -
                    out.trajectory.samples[i - 1].positionMeters;
            }

            sample.orientation = sampleOrientation(
                request,
                sample.velocityMps,
                fallbackForward,
                sample.sourcePathProgressMeters,
                totalSourceProgress,
                sample.timeOffsetSeconds,
                out.trajectory.durationSeconds
            );
        }

        // Preserve the exact final pose after the time-varying blend.
        out.trajectory.samples.back().orientation =
            world::navigation::orientationForForwardUp(
                request.terminalForward,
                request.terminalUp
            );
    }

    std::string angularFailureReason;
    if (!compileBoundedAngularKinematics(
            request,
            out.trajectory,
            &angularFailureReason))
    {
        attempt.failureStatus =
            world::navigation::TrajectoryStatus::InitialStateInfeasible;
        attempt.failureMessage =
            "angular trajectory cannot reach requested terminal state: " +
            angularFailureReason;
        return attempt;
    }

    out.diagnostics.ruckigLegAttempts =
        cumulativeDiagnostics.ruckigLegAttempts;
    out.diagnostics.ruckigLegSuccesses =
        cumulativeDiagnostics.ruckigLegSuccesses;
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
        request.pathGeometryAlreadyAuthored
            ? buildAuthoredExecutionGuide(
                  request,
                  coarseSourceProgress
              )
            : buildExecutionGuide(
                  request,
                  coarseSourceProgress
              );

    // A dense 3-D state-to-state waypoint solver can create stop-like
    // slowdowns and lateral bows. Curved retained routes instead fix geometry
    // first, then assign local speed/acceleration keyframes to path progress.
    // The 3-D Ruckig leg solver remains useful for a true single leg.
    if (request.pathPointsMeters.size() > 2)
    {
        // Translation and attitude share one clock only near the terminal
        // attitude blend. If terminal pose/omega needs extra time, slow ONLY
        // that local blend. Remote straights and unrelated turns keep their
        // own physical/local speed limits.
        constexpr double TerminalAngularTimeScales[] = {
            1.00,
            0.80,
            0.64,
            0.50,
            0.40,
            0.32,
            0.25,
            0.20,
            0.16,
            0.125,
            0.10,
            0.08,
            0.06,
            0.05
        };

        world::navigation::TrajectoryGenerationResult result;
        for (const double terminalAngularScale :
             TerminalAngularTimeScales)
        {
            result = buildPathProgressTrajectory(
                request,
                coarseSourceProgress,
                guide,
                terminalAngularScale
            );
            result.executionGuidePointsMeters =
                guide.points;
            result.diagnostics.executionGuidePoints =
                guide.points.size();
            result.diagnostics.roundedGuideCorners =
                guide.roundedCorners;
            result.diagnostics.expandedGuideCorners =
                guide.expandedCorners;

            if (result.ready())
                return result;

            const bool angularNeedsMoreTime =
                result.trajectory.message.rfind(
                    "angular trajectory cannot reach requested terminal state",
                    0
                ) == 0;

            // Translational envelope violations are local geometry/profile
            // defects. Do not hide them by globally slowing the route.
            if (!angularNeedsMoreTime)
                return result;
        }

        result.trajectory.message =
            "terminal attitude remains infeasible after local time relaxation: " +
            result.trajectory.message;
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
        guide.points.size() *
            request.policy.restartAttemptsPerGuidePoint +
        request.policy.restartBaseAttempts;

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

            return attempt.result;
        }

        bool relaxed = false;
        const std::size_t leg = attempt.failedLeg;

        const auto relaxWaypoint = [&](std::size_t index)
        {
            if (index == 0 ||
                index + 1 >= waypointVelocities.size() ||
                !nonZeroVelocity(
                    waypointVelocities[index],
                    request.policy.minimumUsefulWaypointSpeedMps
                ))
            {
                return false;
            }

            const double speed =
                magnitude(waypointVelocities[index]);

            // A collision or numerical failure at one candidate through-speed
            // does not prove that the corner requires a stop. Reduce the
            // through-speed first and retry the exact swept validation. Only
            // collapse to zero when the remaining speed is already negligible.
            if (speed > request.policy.waypointRelaxThresholdMps)
            {
                waypointVelocities[index] *=
                    request.policy.waypointRelaxFactor;
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
