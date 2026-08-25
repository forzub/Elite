#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{

/*
    Docking-specific composition around the shared geometric path planner.

    The generic planner owns only collision-free geometry from start to the
    authored approach point. Docking semantics add the final straight ingress.
    Current velocity is intentionally absent; kinematic feasibility belongs to
    the later TrajectoryGenerator stage.
*/
struct DockingPathRequest
{
    glm::dvec3 startPositionMeters {0.0};
    glm::dvec3 dockCenterMeters {0.0};
    glm::dvec3 dockOutward {0.0, 0.0, 1.0};

    double vehicleSafetyRadiusMeters = 0.0;
    // Alignment starts farther out on the authored docking axis.  Transit is
    // planned to this point so trajectory smoothing can happen before the
    // locked approach/ingress line instead of rounding the mouth of the dock.
    double alignmentStandoffMeters = 650.0;
    double approachStandoffMeters = 300.0;
    double terminalDepthMeters = 20.0;

    std::string targetObstacleId;
    std::vector<world::navigation::NavigationObstacle> obstacles;
};

struct DockingPathPlan
{
    bool valid = false;
    bool obstacleDetourUsed = false;
    std::string message;

    glm::dvec3 startPointMeters {0.0};
    glm::dvec3 alignmentPointMeters {0.0};
    glm::dvec3 approachPointMeters {0.0};
    glm::dvec3 terminalPointMeters {0.0};
    double alignmentSourceProgressMeters = 0.0;
    double approachSourceProgressMeters = 0.0;
    double terminalSourceProgressMeters = 0.0;
    std::vector<glm::dvec3> pointsMeters;
};

class DockingPathPlanner
{
public:
    static DockingPathPlan plan(const DockingPathRequest& request);
};

} // namespace game::navigation
