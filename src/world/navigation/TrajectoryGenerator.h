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

    // Coarse topology only. Runtime trajectory generation must not search a
    // second global path here. A local execution guide may round these corners,
    // but dense guide samples are geometry p(s), not independent Ruckig target
    // states. Multi-point routes use scalar Ruckig progress s(t).
    std::vector<glm::dvec3> pathPointsMeters;
    std::vector<NavigationObstacle> obstacles;
    NavigationVehicleProfile vehicle;

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
