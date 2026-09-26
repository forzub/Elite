#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationVehicleProfile.h"
#include "src/world/navigation/Trajectory.h"

namespace world::navigation
{

struct TrajectoryGenerationPolicy
{
    // Numeric/solver floors. These are explicit because they change accepted
    // motion rather than being implementation-only epsilons.
    double minimumAccelerationMps2 = 0.1;
    double minimumSpeedMps = 0.1;

    // Local corner-guide construction.
    double nearStraightAngleRad = 0.008726646259971648; // 0.5 deg
    double maximumRoundableCornerAngleRad = 2.6179938779914944; // 150 deg
    double cornerExpansionMinimumMeters = 2.0;
    double cornerExpansionCollisionRadiusFactor = 0.25;
    int cornerExpansionAttempts = 7;
    double maximumCornerCutLegFraction = 0.42;
    double minimumCornerCutMeters = 2.0;
    double minimumCornerCutCollisionRadiusFactor = 0.30;
    double cornerTangentReserveFactor = 1.35;
    int cornerCutAttempts = 8;
    double cornerCutShrinkFactor = 0.78;
    double previewSampleSpacingMeters = 2.5;
    int previewMinimumSegments = 4;
    int previewMaximumSegments = 24;
    double sourceProgressCutFraction = 0.45;
    double curveSampleSpacingMeters = 6.0;
    int curveMinimumSegments = 3;
    int curveMaximumSegments = 10;

    // Legacy single-leg through-corner candidate policy.
    double nominalBlendLegFraction = 0.25;
    double nominalBlendMinimumMeters = 1.0;
    double minimumBlendMeters = 0.75;
    double minimumBlendCollisionRadiusFactor = 0.25;
    int blendAttempts = 10;
    double blendShrinkFactor = 0.70;
    double minimumUsefulWaypointSpeedMps = 0.5;

    // Ruckig state-to-state solve policy.
    double minimumLegDurationSeconds = 0.5;
    double legDurationScale = 1.20;
    double legDurationPaddingSeconds = 0.25;
    int ruckigDurationAttempts = 8;
    double jerkMinimumMps3 = 1.0;
    double jerkAccelerationMultiplier = 4.0;
    double validationStepSeconds = 0.02;
    double minimumSamplingSpeedMps = 1.0;
    double collisionChordMinimumMeters = 1.0;
    double collisionChordRadiusFactor = 0.25;
    double collisionChordMaximumMeters = 5.0;
    double legSampleIntervalMinimumSeconds = 0.02;
    double legSampleIntervalMaximumSeconds = 0.05;
    double peakSpeedToleranceMps = 0.25;
    double peakSpeedToleranceFraction = 0.01;
    double durationRetryFactor = 1.45;

    // Scalar path-progress timing / curvature sampling.
    double curvatureProbeFraction = 0.0025;
    double curvatureProbeMinimumMeters = 0.20;
    double curvatureProbeMaximumMeters = 1.00;
    double progressSampleIntervalSeconds = 0.02;
    double pathCaptureSpeedThresholdMps = 0.25;

    // State-to-state fallback relaxation.
    std::size_t restartAttemptsPerGuidePoint = 8;
    std::size_t restartBaseAttempts = 4;
    double waypointRelaxThresholdMps = 1.25;
    double waypointRelaxFactor = 0.70;

    [[nodiscard]] bool valid() const noexcept
    {
        const auto positive = [](double v) noexcept
        {
            return std::isfinite(v) && v > 0.0;
        };
        const auto nonNegative = [](double v) noexcept
        {
            return std::isfinite(v) && v >= 0.0;
        };

        return
            positive(minimumAccelerationMps2) &&
            positive(minimumSpeedMps) &&
            nonNegative(nearStraightAngleRad) &&
            positive(maximumRoundableCornerAngleRad) &&
            maximumRoundableCornerAngleRad > nearStraightAngleRad &&
            nonNegative(cornerExpansionMinimumMeters) &&
            nonNegative(cornerExpansionCollisionRadiusFactor) &&
            cornerExpansionAttempts > 0 &&
            positive(maximumCornerCutLegFraction) &&
            nonNegative(minimumCornerCutMeters) &&
            nonNegative(minimumCornerCutCollisionRadiusFactor) &&
            positive(cornerTangentReserveFactor) &&
            cornerCutAttempts > 0 &&
            cornerCutShrinkFactor > 0.0 && cornerCutShrinkFactor < 1.0 &&
            positive(previewSampleSpacingMeters) &&
            previewMinimumSegments >= 1 &&
            previewMaximumSegments >= previewMinimumSegments &&
            sourceProgressCutFraction > 0.0 &&
            sourceProgressCutFraction < 0.5 &&
            positive(curveSampleSpacingMeters) &&
            curveMinimumSegments >= 1 &&
            curveMaximumSegments >= curveMinimumSegments &&
            nominalBlendLegFraction > 0.0 &&
            nominalBlendLegFraction < 0.5 &&
            nonNegative(nominalBlendMinimumMeters) &&
            nonNegative(minimumBlendMeters) &&
            nonNegative(minimumBlendCollisionRadiusFactor) &&
            blendAttempts > 0 &&
            blendShrinkFactor > 0.0 && blendShrinkFactor < 1.0 &&
            nonNegative(minimumUsefulWaypointSpeedMps) &&
            positive(minimumLegDurationSeconds) &&
            positive(legDurationScale) &&
            nonNegative(legDurationPaddingSeconds) &&
            ruckigDurationAttempts > 0 &&
            nonNegative(jerkMinimumMps3) &&
            nonNegative(jerkAccelerationMultiplier) &&
            positive(validationStepSeconds) &&
            positive(minimumSamplingSpeedMps) &&
            nonNegative(collisionChordMinimumMeters) &&
            nonNegative(collisionChordRadiusFactor) &&
            collisionChordMaximumMeters >= collisionChordMinimumMeters &&
            positive(legSampleIntervalMinimumSeconds) &&
            legSampleIntervalMaximumSeconds >=
                legSampleIntervalMinimumSeconds &&
            nonNegative(peakSpeedToleranceMps) &&
            nonNegative(peakSpeedToleranceFraction) &&
            durationRetryFactor > 1.0 &&
            nonNegative(curvatureProbeFraction) &&
            positive(curvatureProbeMinimumMeters) &&
            curvatureProbeMaximumMeters >= curvatureProbeMinimumMeters &&
            positive(progressSampleIntervalSeconds) &&
            nonNegative(pathCaptureSpeedThresholdMps) &&
            restartAttemptsPerGuidePoint > 0 &&
            waypointRelaxThresholdMps >= 0.0 &&
            waypointRelaxFactor > 0.0 && waypointRelaxFactor < 1.0;
    }
};

struct TrajectoryPointSpeedConstraint
{
    // Progress along the original GeometricPath polyline.
    double sourcePathProgressMeters = 0.0;
    double maxSpeedMps = 0.0;
};

struct TrajectorySpeedLimitRange
{
    double sourcePathStartMeters = 0.0;
    double sourcePathEndMeters = 0.0;
    double maxSpeedMps = 0.0;
};

struct TrajectoryGenerationRequest
{
    int systemId = -1;
    std::string frameId;
    double startUniverseTimeSeconds = 0.0;
    // Maps physical/gameplay execution seconds to the universe ephemeris
    // clock. Local acceleration and braking always use timeOffsetSeconds.
    double universeTimeScale = 1.0;

    // Coarse topology only. Runtime trajectory generation must not search a
    // second global path here. A local execution guide may round these corners,
    // but dense guide samples are geometry p(s), not independent Ruckig target
    // states. Multi-point routes use scalar Ruckig progress s(t).
    std::vector<glm::dvec3> pathPointsMeters;
    std::vector<NavigationObstacle> obstacles;
    NavigationVehicleProfile vehicle;
    TrajectoryGenerationPolicy policy {};

    // Initial kinematic state relative to the planning frame.
    glm::dvec3 initialVelocityMps {0.0};
    glm::dvec3 initialAccelerationMps2 {0.0};

    // Optional exact terminal inertial velocity. This is distinct from
    // pointSpeedConstraints, which are upper bounds along the retained route.
    // A moving fly-through finish must not be represented as a fake stop.
    bool hasTerminalVelocity = false;
    glm::dvec3 terminalVelocityMps {0.0};

    std::vector<TrajectoryPointSpeedConstraint> pointSpeedConstraints;
    std::vector<TrajectorySpeedLimitRange> speedLimitRanges;

    // Docking may legally enter the target module only after the authored
    // ingress point. Other obstacles are always solid.
    std::string terminalAllowedObstacleId;
    double terminalObstacleEntrySourceProgressMeters =
        std::numeric_limits<double>::infinity();

    bool hasTerminalOrientation = false;
    glm::dvec3 terminalForward {0.0, 0.0, -1.0};
    glm::dvec3 terminalUp {0.0, 1.0, 0.0};
    double terminalOrientationBlendDistanceMeters = 0.0;

    // Optional terminal angular velocity in the same navigation frame.
    // A rotating capture target is a moving attitude boundary condition, not
    // a static final quaternion followed by an instantaneous omega jump.
    bool hasTerminalAngularVelocity = false;
    glm::dvec3 terminalAngularVelocityRadPerSecond {0.0};
};

struct TrajectoryGenerationDiagnostics
{
    // Canonical runtime backend diagnostics.
    std::size_t ruckigLegAttempts = 0;
    std::size_t ruckigLegSuccesses = 0;
    std::size_t collisionSegmentsChecked = 0;

    std::size_t executionGuidePoints = 0;
    std::size_t roundedGuideCorners = 0;
    std::size_t expandedGuideCorners = 0;

    // Deprecated names retained temporarily for old logging/tests. The Ruckig
    // planner maps attempts/successes into these fields so old diagnostics do
    // not break while production code migrates to the fields above.
    std::size_t smoothCandidatesEvaluated = 0;
    std::size_t smoothSafeCandidates = 0;
    std::size_t selectedSmoothSupportLevel = 0;
    bool smoothingFellBackToPolyline = false;

    double coarsePathLengthMeters = 0.0;
    double optimizedPathLengthMeters = 0.0;
    double maxCurvaturePerMeter = 0.0;
    double curvatureVariation = 0.0;

    double initialAlongPathSpeedMps = 0.0;
    double initialCrossTrackSpeedMps = 0.0;
    bool pathCaptureRequired = false;

    double maxSpeedMps = 0.0;
    double maxAccelerationMps2 = 0.0;
    double maxAngularVelocityRadPerSecond = 0.0;
};

struct TrajectoryGenerationResult
{
    Trajectory trajectory;
    TrajectoryGenerationDiagnostics diagnostics;

    // The collision-checked execution guide actually handed to the canonical
    // Ruckig route backend. It may contain local corner entry/exit points that
    // round a coarse geometric-polyline vertex. The retained Stage-1 route is
    // never mutated.
    std::vector<glm::dvec3> executionGuidePointsMeters;

    bool ready() const noexcept
    {
        return trajectory.ready();
    }
};

/*
    Compatibility facade for the runtime route-to-trajectory service.

    The canonical implementation is game::navigation::RuckigRoutePlanner.
    Keeping this facade lets old callers migrate without preserving the removed
    B-spline/SmoothPathOptimizer runtime implementation.
*/
class TrajectoryGenerator
{
public:
    static TrajectoryGenerationResult generate(
        const TrajectoryGenerationRequest& request
    );
};

} // namespace world::navigation
