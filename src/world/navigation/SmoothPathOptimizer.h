#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace world::navigation
{

struct SmoothPathPoint
{
    glm::dvec3 positionMeters {0.0};
    // Monotone progress along the coarse GeometricPath.  It lets downstream
    // docking/speed constraints survive even though the optimized curve is free
    // to move well away from the original polyline.
    double sourceProgressMeters = 0.0;
};

struct SmoothPathRequest
{
    std::vector<glm::dvec3> pathPointsMeters;
    // Optional monotone semantic progress corresponding one-to-one with the
    // coarse controls. Guidance can therefore rebuild a new spatial curve
    // while retaining progress on the immutable route for speed/ingress data.
    std::vector<double> sourceProgressMeters;
    std::vector<NavigationObstacle> obstacles;
    NavigationVehicleProfile vehicle;

    // Geometry sampling bounds.  The optimizer creates a cubic B-spline and
    // adaptively tessellates it until both chord length and sagitta are small.
    double maxSampleSpacingMeters = 8.0;
    double maxChordErrorMeters = 0.05;

    // Candidate 0 is the broadest global spline over the coarse route. Higher
    // support levels add controls along the same topological route only when a
    // broad candidate cuts an obstacle. This deliberately prefers smoothness
    // over shortest distance/local corner rounding.
    std::size_t maxSupportLevel = 5;

    // Optional hard curvature contract. Guidance corridors use this to reject
    // a geometrically safe curve that still asks the pilot for an implausibly
    // tight turn. Zero means "no hard curvature bound".
    double maxCurvaturePerMeter = 0.0;

    // Trajectory generation may retain a known-safe coarse route as a last
    // resort. Manual guidance must never turn a failed smooth solution into a
    // kinked HUD tunnel, so it disables this fallback.
    bool allowPolylineFallback = true;

    // Docking is allowed to enter the target obstacle only after the authored
    // ingress progress.  All other geometry remains solid for every candidate.
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
    static SmoothPathResult optimize(const SmoothPathRequest& request);
};

} // namespace world::navigation
