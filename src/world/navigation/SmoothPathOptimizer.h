#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationVehicleProfile.h"
#include "src/world/navigation/NavigationPerfLog.h"

namespace world::navigation
{

/*
    RETIRED NAV-RUCKIG-1 compatibility types.

    The production custom spline implementation has been removed. Runtime route
    motion is owned by game::navigation::RuckigRoutePlanner and rolling local
    motion by RuckigTrajectorySolver. This API remains temporarily so stale
    source/tests fail closed instead of breaking the migration in one commit.
*/
struct SmoothPathPoint
{
    glm::dvec3 positionMeters {0.0};
    double sourceProgressMeters = 0.0;
};

struct SmoothPathRequest
{
    std::vector<glm::dvec3> pathPointsMeters;
    std::vector<double> sourceProgressMeters;
    std::vector<NavigationObstacle> obstacles;
    NavigationVehicleProfile vehicle;

    // Legacy fields. Production code must not use these to request motion.
    double maxSampleSpacingMeters = 8.0;
    double maxChordErrorMeters = 0.05;
    std::size_t maxSupportLevel = 5;
    double maxCurvaturePerMeter = 0.0;
    bool allowPolylineFallback = true;

    std::string terminalAllowedObstacleId;
    double terminalObstacleEntrySourceProgressMeters =
        std::numeric_limits<double>::infinity();
};

struct SmoothPathDiagnostics
{
    std::size_t candidatesEvaluated = 0;
    std::size_t safeCandidates = 0;
    std::size_t selectedSupportLevel = 0;
    double coarseLengthMeters = 0.0;
    double optimizedLengthMeters = 0.0;
    double maxCurvaturePerMeter = 0.0;
    double curvatureVariation = 0.0;
    bool fellBackToPolyline = false;
};

struct SmoothPathResult
{
    bool valid = false;
    std::string message;
    std::vector<SmoothPathPoint> points;
    SmoothPathDiagnostics diagnostics;
};

class SmoothPathOptimizer
{
public:
    /*
        Production builds always return invalid/retired. Only the legacy
        navigation test target defines ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT,
        which exposes a minimal non-smoothing polyline shim while old tests are
        being migrated. No runtime target may define that macro.
    */
    static SmoothPathResult optimize(const SmoothPathRequest& request);
};

} // namespace world::navigation
