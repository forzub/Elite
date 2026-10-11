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
    double pathLength = 1.0;

    // Integral of squared lateral acceleration surrogate:
    // integral((v_design^2 * curvature)^2 ds).
    double lateralAccelerationEnergy = 0.02;

    // Integral of squared curvature derivative along arc length.
    double curvatureVariation = 5.0e7;
};

struct Request
{
    BoundaryState start;
    BoundaryState goal;

    double hullRadiusMeters = 0.0;
    double designSpeedMps = 0.0;
    double maxLateralAccelerationMps2 = 0.0;

    // Additional comfort floor. Effective max curvature is the tighter of
    // this radius and the dynamic radius v^2/a_lat.
    double minimumComfortRadiusMeters = 0.0;

    double protectedStartStraightMeters = 0.0;
    double protectedGoalStraightMeters = 0.0;

    std::vector<world::navigation::NavigationObstacle> obstacles;

    OptimizationWeights weights;

    std::size_t samplesPerBezier = 96;
    std::size_t maxPatternIterations = 500;
};

struct Diagnostics
{
    std::size_t objectiveEvaluations = 0;
    std::size_t acceptedSteps = 0;

    double pathLengthMeters = 0.0;
    double maxCurvaturePerMeter = 0.0;
    double minimumRadiusMeters = 0.0;
    double lateralAccelerationEnergy = 0.0;
    double curvatureVariationEnergy = 0.0;
    double objective = 0.0;
    double maxConstraintViolation = 0.0;
};

struct Result
{
    bool valid = false;
    std::string message;

    // The laboratory parameterization is two C1-connected cubic Beziers.
    // Boundary position and tangent are exact by construction.
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
