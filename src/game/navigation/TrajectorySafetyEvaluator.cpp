#include "src/game/navigation/TrajectorySafetyEvaluator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation
{
namespace
{
constexpr double TimeEpsilon = 1.0e-9;
constexpr double DistanceEpsilon = 1.0e-9;


glm::dvec3 lerpPosition(
    const TrajectoryPredictionSample& a,
    const TrajectoryPredictionSample& b,
    double t
)
{
    const double span = b.universeTimeSeconds - a.universeTimeSeconds;
    if (span <= TimeEpsilon)
        return a.state.positionMeters;

    const double u = std::clamp(
        (t - a.universeTimeSeconds) / span,
        0.0,
        1.0
    );
    return a.state.positionMeters +
        (b.state.positionMeters - a.state.positionMeters) * u;
}

glm::dvec3 obstaclePositionAt(
    const NavigationObstacleState& obstacle,
    double universeTimeSeconds
)
{
    const double dt =
        universeTimeSeconds - obstacle.epochUniverseTimeSeconds;
    return obstacle.geometry.centerMeters +
        obstacle.velocityMps * dt +
        0.5 * obstacle.accelerationMps2 * dt * dt;
}

double obstacleUncertaintyAt(
    const NavigationObstacleState& obstacle,
    double universeTimeSeconds
)
{
    const double dt = std::abs(
        universeTimeSeconds - obstacle.epochUniverseTimeSeconds
    );
    return std::max(0.0, obstacle.positionUncertaintyMeters) +
        std::max(0.0, obstacle.velocityUncertaintyMps) * dt;
}

struct ClosestApproach
{
    double time = 0.0;
    double distance = std::numeric_limits<double>::infinity();
    glm::dvec3 aPosition {0.0};
    glm::dvec3 bPosition {0.0};
};

ClosestApproach closestLinearApproach(
    double t0,
    double t1,
    const glm::dvec3& a0,
    const glm::dvec3& a1,
    const glm::dvec3& b0,
    const glm::dvec3& b1
)
{
    ClosestApproach out;
    out.time = t0;
    out.aPosition = a0;
    out.bPosition = b0;

    const glm::dvec3 r0 = a0 - b0;
    const glm::dvec3 relativeDelta = (a1 - a0) - (b1 - b0);
    const double denom = glm::dot(relativeDelta, relativeDelta);

    double u = 0.0;
    if (denom > DistanceEpsilon)
    {
        u = std::clamp(
            -glm::dot(r0, relativeDelta) / denom,
            0.0,
            1.0
        );
    }

    out.time = t0 + (t1 - t0) * u;
    out.aPosition = a0 + (a1 - a0) * u;
    out.bPosition = b0 + (b1 - b0) * u;
    out.distance = glm::length(out.aPosition - out.bPosition);
    return out;
}

bool trafficPositionAt(
    const KnownTrafficIntent& traffic,
    double universeTimeSeconds,
    glm::dvec3& outPosition
)
{
    if (traffic.samples.empty())
        return false;

    const double uncertainty = std::max(0.0, traffic.timingUncertaintySeconds);
    const double firstTime = traffic.samples.front().universeTimeSeconds;
    const double lastTime = traffic.samples.back().universeTimeSeconds;

    // Timing uncertainty extends the published time window only by the amount
    // explicitly declared. A scheduled vessel is never frozen at its last
    // sample forever.
    if (universeTimeSeconds < firstTime - uncertainty ||
        universeTimeSeconds > lastTime + uncertainty)
    {
        return false;
    }

    if (traffic.samples.size() == 1)
    {
        const auto& sample = traffic.samples.front();
        outPosition = sample.positionMeters +
            sample.velocityMps *
                (universeTimeSeconds - sample.universeTimeSeconds);
        return true;
    }

    if (universeTimeSeconds <= firstTime)
    {
        const auto& sample = traffic.samples.front();
        outPosition = sample.positionMeters +
            sample.velocityMps *
                (universeTimeSeconds - sample.universeTimeSeconds);
        return true;
    }

    if (universeTimeSeconds >= lastTime)
    {
        const auto& sample = traffic.samples.back();
        outPosition = sample.positionMeters +
            sample.velocityMps *
                (universeTimeSeconds - sample.universeTimeSeconds);
        return true;
    }

    for (std::size_t i = 1; i < traffic.samples.size(); ++i)
    {
        const auto& a = traffic.samples[i - 1];
        const auto& b = traffic.samples[i];
        if (universeTimeSeconds > b.universeTimeSeconds)
            continue;

        const double span = b.universeTimeSeconds - a.universeTimeSeconds;
        if (span <= TimeEpsilon)
        {
            outPosition = b.positionMeters;
            return true;
        }

        const double u = std::clamp(
            (universeTimeSeconds - a.universeTimeSeconds) / span,
            0.0,
            1.0
        );
        outPosition = a.positionMeters +
            (b.positionMeters - a.positionMeters) * u;
        return true;
    }

    return false;
}

void recordConflict(
    TrajectorySafetyReport& report,
    TrajectoryConflictKind kind,
    const std::string& id,
    const ClosestApproach& closest,
    double requiredSeparation
)
{
    report.minimumSeparationMeters = std::min(
        report.minimumSeparationMeters,
        closest.distance
    );

    if (closest.distance >= requiredSeparation)
        return;

    report.safe = false;

    TrajectoryConflict conflict;
    conflict.kind = kind;
    conflict.objectId = id;
    conflict.universeTimeSeconds = closest.time;
    conflict.separationMeters = closest.distance;
    conflict.requiredSeparationMeters = requiredSeparation;
    conflict.shipPositionMeters = closest.aPosition;
    conflict.hazardPositionMeters = closest.bPosition;
    report.conflicts.push_back(std::move(conflict));
}

} // namespace

TrajectorySafetyReport TrajectorySafetyEvaluator::evaluate(
    const TrajectoryPredictionResult& trajectory,
    const NavigationPlanningSnapshot& environment,
    double shipSafetyRadiusMeters
)
{
    TrajectorySafetyReport report;
    report.minimumSeparationMeters =
        std::numeric_limits<double>::infinity();

    if (!trajectory.ok() || trajectory.samples.empty())
    {
        report.safe = false;
        report.minimumSeparationMeters = 0.0;
        return report;
    }

    if (trajectory.samples.size() == 1)
    {
        report.minimumSeparationMeters =
            std::numeric_limits<double>::infinity();
        return report;
    }

    const double shipRadius = std::max(0.0, shipSafetyRadiusMeters);

    for (std::size_t i = 1; i < trajectory.samples.size(); ++i)
    {
        const auto& shipA = trajectory.samples[i - 1];
        const auto& shipB = trajectory.samples[i];
        const double t0 = shipA.universeTimeSeconds;
        const double t1 = shipB.universeTimeSeconds;
        if (t1 <= t0 + TimeEpsilon)
            continue;

        for (const NavigationObstacleState& obstacle : environment.obstacles)
        {
            if (obstacle.systemId >= 0 &&
                trajectory.systemId >= 0 &&
                obstacle.systemId != trajectory.systemId)
            {
                continue;
            }

            const double overlapStart =
                obstacle.validFromUniverseTimeSeconds > 0.0
                    ? std::max(t0, obstacle.validFromUniverseTimeSeconds)
                    : t0;
            const double overlapEnd =
                obstacle.validUntilUniverseTimeSeconds >
                        obstacle.validFromUniverseTimeSeconds
                    ? std::min(t1, obstacle.validUntilUniverseTimeSeconds)
                    : t1;

            if (overlapEnd < overlapStart - TimeEpsilon)
                continue;

            const glm::dvec3 ship0 = lerpPosition(shipA, shipB, overlapStart);
            const glm::dvec3 ship1 = lerpPosition(shipA, shipB, overlapEnd);
            const glm::dvec3 hazard0 =
                obstaclePositionAt(obstacle, overlapStart);
            const glm::dvec3 hazard1 =
                obstaclePositionAt(obstacle, overlapEnd);

            const ClosestApproach closest = closestLinearApproach(
                overlapStart,
                overlapEnd,
                ship0,
                ship1,
                hazard0,
                hazard1
            );

            const double required =
                shipRadius +
                std::max(0.0, obstacle.geometry.conservativeRadiusMeters()) +
                std::max(0.0, obstacle.geometry.requiredClearanceMeters) +
                obstacleUncertaintyAt(obstacle, closest.time);

            recordConflict(
                report,
                TrajectoryConflictKind::Obstacle,
                obstacle.geometry.id,
                closest,
                required
            );
        }

        for (const RestrictedNavigationVolume& volume :
             environment.restrictedVolumes)
        {
            if (volume.systemId >= 0 &&
                trajectory.systemId >= 0 &&
                volume.systemId != trajectory.systemId)
            {
                continue;
            }

            const double overlapStart =
                volume.validFromUniverseTimeSeconds > 0.0
                    ? std::max(t0, volume.validFromUniverseTimeSeconds)
                    : t0;
            const double overlapEnd =
                volume.validUntilUniverseTimeSeconds >
                        volume.validFromUniverseTimeSeconds
                    ? std::min(t1, volume.validUntilUniverseTimeSeconds)
                    : t1;
            if (overlapEnd < overlapStart - TimeEpsilon)
                continue;

            const glm::dvec3 ship0 = lerpPosition(shipA, shipB, overlapStart);
            const glm::dvec3 ship1 = lerpPosition(shipA, shipB, overlapEnd);
            const ClosestApproach closest = closestLinearApproach(
                overlapStart,
                overlapEnd,
                ship0,
                ship1,
                volume.centerMeters,
                volume.centerMeters
            );

            recordConflict(
                report,
                TrajectoryConflictKind::RestrictedVolume,
                volume.id,
                closest,
                shipRadius +
                    std::max(0.0, volume.radiusMeters) +
                    std::max(0.0, volume.clearanceMeters)
            );
        }

        for (const KnownTrafficIntent& traffic : environment.scheduledTraffic)
        {
            if (traffic.systemId >= 0 &&
                trajectory.systemId >= 0 &&
                traffic.systemId != trajectory.systemId)
            {
                continue;
            }

            if (traffic.samples.empty())
                continue;

            const double uncertainty =
                std::max(0.0, traffic.timingUncertaintySeconds);
            const double trafficStart =
                traffic.samples.front().universeTimeSeconds - uncertainty;
            const double trafficEnd =
                traffic.samples.back().universeTimeSeconds + uncertainty;

            const double overlapStart = std::max(t0, trafficStart);
            const double overlapEnd = std::min(t1, trafficEnd);
            if (overlapEnd < overlapStart - TimeEpsilon)
                continue;

            // Split the ship segment at every published traffic sample time.
            // A scheduled vessel may change velocity/direction at those knots;
            // treating the entire overlap as one chord can miss a conflict near
            // an intermediate sample even though both endpoint positions look
            // clear.
            std::vector<double> trafficBreaks;
            trafficBreaks.reserve(traffic.samples.size() + 2);
            trafficBreaks.push_back(overlapStart);
            for (const auto& sample : traffic.samples)
            {
                if (sample.universeTimeSeconds > overlapStart + TimeEpsilon &&
                    sample.universeTimeSeconds < overlapEnd - TimeEpsilon)
                {
                    trafficBreaks.push_back(sample.universeTimeSeconds);
                }
            }
            trafficBreaks.push_back(overlapEnd);
            std::sort(trafficBreaks.begin(), trafficBreaks.end());
            trafficBreaks.erase(
                std::unique(
                    trafficBreaks.begin(),
                    trafficBreaks.end(),
                    [](double a, double b)
                    {
                        return std::abs(a - b) <= TimeEpsilon;
                    }
                ),
                trafficBreaks.end()
            );

            const double required =
                shipRadius +
                std::max(0.0, traffic.physicalRadiusMeters) +
                std::max(0.0, traffic.requiredSeparationMeters) +
                std::max(0.0, traffic.positionUncertaintyMeters);

            for (std::size_t j = 1; j < trafficBreaks.size(); ++j)
            {
                const double segmentStart = trafficBreaks[j - 1];
                const double segmentEnd = trafficBreaks[j];

                glm::dvec3 traffic0 {0.0};
                glm::dvec3 traffic1 {0.0};
                if (!trafficPositionAt(traffic, segmentStart, traffic0) ||
                    !trafficPositionAt(traffic, segmentEnd, traffic1))
                {
                    continue;
                }

                const ClosestApproach closest = closestLinearApproach(
                    segmentStart,
                    segmentEnd,
                    lerpPosition(shipA, shipB, segmentStart),
                    lerpPosition(shipA, shipB, segmentEnd),
                    traffic0,
                    traffic1
                );

                recordConflict(
                    report,
                    TrajectoryConflictKind::ScheduledTraffic,
                    traffic.id,
                    closest,
                    required
                );
            }
        }
    }

    if (!std::isfinite(report.minimumSeparationMeters))
        report.minimumSeparationMeters = 0.0;

    std::sort(
        report.conflicts.begin(),
        report.conflicts.end(),
        [](const TrajectoryConflict& a, const TrajectoryConflict& b)
        {
            return a.universeTimeSeconds < b.universeTimeSeconds;
        }
    );

    return report;
}

} // namespace game::navigation
