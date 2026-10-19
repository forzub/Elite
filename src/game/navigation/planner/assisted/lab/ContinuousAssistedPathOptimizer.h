#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation::planner::assisted::lab
{

struct BoundaryState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 tangent {0.0, 0.0, -1.0};
    double speedMps = 0.0;
};

struct OptimizationWeights
{
    // Primary term: estimated traversal time after curvature speed limits and
    // longitudinal acceleration/braking have been applied.
    double travelTime = 1.0;

    // Quadratic penalty on route stretch above the geometric lower bound.
    // Kept under the historical field name so existing experimental callers
    // do not silently change ABI/source contracts.
    double pathLength = 0.35;

    // Dynamic reserve terms. These are dimensionless costs based on how much
    // of the ship's real control authority the candidate consumes. They do not
    // encode a preferred radius: the same radius can be cheap at low speed and
    // expensive at high speed.
    double lateralAccelerationEnergy = 0.12;
    double peakLateralUtilization = 0.10;
    double brakingDemand = 0.12;

    // Geometry costs are independent of speed. totalCurvature rejects
    // gratuitous left-right wriggles; curvatureConcentration rejects packing a
    // large heading change into a tiny hook. Both are dimensionless and scaled
    // from the ship hull, not from a magic preferred turn radius.
    double totalCurvature = 0.10;
    double curvatureConcentration = 0.18;

    // Turn-shape cost. This is the turn-angle-weighted coefficient of
    // variation of curvature inside each cubic transition. A constant-radius
    // turn is near zero; a flattened "sausage" with long shallow shoulders
    // and a tight nose is expensive. It does not prescribe any radius.
    double curvatureUniformity = 0.18;

    // Angular costs use the ship's real attitude-control authority.
    double angularVelocityDemand = 0.16;
    double angularAccelerationDemand = 0.20;

    // Pure geometric regularity remains a weak tie-breaker. It must not buy a
    // giant detour merely to make a curve numerically silk-smooth.
    double curvatureVariation = 0.02;
};

struct Request
{
    BoundaryState start;
    BoundaryState goal;

    double hullRadiusMeters = 0.0;

    // Maximum cruise speed, NOT a mandatory speed for every curve.
    double designSpeedMps = 0.0;

    double acceleratingMps2 = 0.0;
    double brakingMps2 = 0.0;
    double maxLateralAccelerationMps2 = 0.0;
    double maxAngularVelocityRadPerSecond = 0.0;
    double maxAngularAccelerationRadPerSecond2 = 0.0;

    // Optional true geometric/comfort floor. Unlike the previous optimizer,
    // maxSpeed^2/a_lat is NOT converted into a hard global turn radius.
    double minimumComfortRadiusMeters = 0.0;

    double protectedStartStraightMeters = 0.0;
    double protectedGoalStraightMeters = 0.0;

    std::vector<world::navigation::NavigationObstacle> obstacles;

    OptimizationWeights weights;

    std::size_t samplesPerBezier = 96;
    std::size_t maxPatternIterations = 500;
};

enum class OptimizedSegmentKind : unsigned char
{
    Line = 0,
    CubicBezier
};

struct OptimizedSegment
{
    OptimizedSegmentKind kind = OptimizedSegmentKind::Line;
    glm::dvec3 startMeters {0.0};
    glm::dvec3 control1Meters {0.0};
    glm::dvec3 control2Meters {0.0};
    glm::dvec3 endMeters {0.0};
};

// A bounded snapshot of a feasible candidate already evaluated by the
// optimizer. These are diagnostic measurements, not extra search work.
struct CandidateComparison
{
    std::size_t evaluationIndex = 0;
    double lengthMeters = 0.0;
    double estimatedTravelTimeSeconds = 0.0;
    double objective = 0.0;
    double minimumRadiusMeters = 0.0;
    double totalCurvatureRadians = 0.0;
};

// A distinct optimized search branch, not one tiny parameter perturbation.
// Infeasible branches are retained for diagnosis but never compared for ETA.
struct SeedBranchComparison
{
    std::size_t seedRank = 0;
    bool feasible = false;
    double lengthMeters = 0.0;
    double estimatedTravelTimeSeconds = 0.0;
    double objective = 0.0;
    double maxConstraintViolation = 0.0;
};

struct Diagnostics
{
    std::size_t objectiveEvaluations = 0;
    std::size_t acceptedSteps = 0;
    std::size_t seedCount = 0;

    // Counters cover all evaluated seeds and local-search candidates.
    std::size_t feasibleEvaluations = 0;
    std::size_t infeasibleEvaluations = 0;

    // Top 5 from the same feasible set, sorted by the named metric.
    // Candidate IDs refer to evaluation order, not independent routes.
    std::vector<CandidateComparison> shortestFeasible;
    std::vector<CandidateComparison> fastestFeasible;
    std::vector<CandidateComparison> lowestObjectiveFeasible;
    std::vector<SeedBranchComparison> searchBranchOutcomes;

    // Full route including protected start/goal straights.
    double pathLengthMeters = 0.0;
    double lowerBoundLengthMeters = 0.0;
    double stretchRatio = 0.0;
    double estimatedTravelTimeSeconds = 0.0;

    double maxCurvaturePerMeter = 0.0;
    double minimumRadiusMeters = 0.0;

    // Dynamic-load diagnostics are normalized to the authored vehicle
    // authority: 1.0 means the plan is using 100% of that authority.
    double maxLateralUtilization = 0.0;
    double lateralAccelerationEnergy = 0.0;
    double brakingDemandEnergy = 0.0;

    double totalCurvatureRadians = 0.0;
    double curvatureConcentrationEnergy = 0.0;

    // Dimensionless, turn-angle-weighted curvature variance. 0 means each
    // continuous turn is effectively constant-radius.
    double curvatureUniformityEnergy = 0.0;

    double maxAngularVelocityUtilization = 0.0;
    double maxAngularAccelerationUtilization = 0.0;
    double angularVelocityEnergy = 0.0;
    double angularAccelerationEnergy = 0.0;

    double curvatureVariationEnergy = 0.0;
    double objective = 0.0;
    double maxConstraintViolation = 0.0;
};

struct Result
{
    bool valid = false;
    std::string message;

    // Straight-biased free-flight topology:
    //   cubic transition -> optional straight -> cubic transition.
    // Boundary position/tangent are exact by construction.
    std::vector<OptimizedSegment> segments;

    // Compatibility/debug fields for the first/last cubic.
    glm::dvec3 joinMeters {0.0};
    glm::dvec3 joinTangent {1.0, 0.0, 0.0};
    glm::dvec3 firstControl1Meters {0.0};
    glm::dvec3 firstControl2Meters {0.0};
    glm::dvec3 secondControl1Meters {0.0};
    glm::dvec3 secondControl2Meters {0.0};

    std::vector<glm::dvec3> sampledCenterlineMeters;
    Diagnostics diagnostics;
};

class ContinuousAssistedPathOptimizer final
{
public:
    [[nodiscard]] static Result optimize(const Request& request);
};

} // namespace game::navigation::planner::assisted::lab
