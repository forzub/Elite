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

    std::vector<glm::dvec3> pathPointsMeters;
    std::vector<NavigationObstacle> obstacles;
    NavigationVehicleProfile vehicle;

    // Velocity relative to the planning frame. Stage 5A preserves the
    // along-path component and reports the cross-track component explicitly;
    // path-capture control belongs to the future follower.
    glm::dvec3 initialVelocityMps {0.0};

    // Stage 5C builds one globally smooth cubic B-spline. Candidate 0 is
    // intentionally broad; higher support levels pull it closer to the coarse
    // route only when geometry blocks the smoother candidate.
    std::size_t maxSmoothSupportLevel = 5;
    double sampleSpacingMeters = 8.0;
    double maxCurveChordErrorMeters = 0.05;

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
};

struct TrajectoryGenerationDiagnostics
{
    std::size_t smoothCandidatesEvaluated = 0;
    std::size_t smoothSafeCandidates = 0;
    std::size_t selectedSmoothSupportLevel = 0;
    double coarsePathLengthMeters = 0.0;
    double optimizedPathLengthMeters = 0.0;
    double maxCurvaturePerMeter = 0.0;
    double curvatureVariation = 0.0;
    bool smoothingFellBackToPolyline = false;

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

    bool ready() const noexcept
    {
        return trajectory.ready();
    }
};

class TrajectoryGenerator
{
public:
    static TrajectoryGenerationResult generate(
        const TrajectoryGenerationRequest& request
    );
};

} // namespace world::navigation
