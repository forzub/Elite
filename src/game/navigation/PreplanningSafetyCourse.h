#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{

struct PreplanningSafetyCourseRequest
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 hullForward {0.0, 0.0, -1.0};
    glm::dvec3 preferredDirection {0.0, 0.0, -1.0};

    double hullRadiusMeters = 0.0;
    double additionalClearanceMeters = 0.0;

    // Physical authority available while acquiring a new inertial course.
    double courseAccelerationMps2 = 0.0;
    double brakingAccelerationMps2 = 0.0;
    double maxAngularVelocityRadPerSecond = 0.0;
    bool newtonian = false;

    std::vector<world::navigation::NavigationObstacle> obstacles;
};

struct PreplanningSafetyCourseResult
{
    bool valid = false;
    bool currentCourseSafe = false;
    bool safeCourseFound = false;
    bool stopFallbackClear = false;
    bool physicallyRecoverable = false;

    glm::dvec3 currentCourse {0.0, 0.0, -1.0};
    glm::dvec3 safeCourse {0.0, 0.0, -1.0};

    double speedMps = 0.0;
    double sceneRayMeters = 0.0;
    double turnAngleRad = 0.0;
    double turnRadiusMeters = 0.0;
    double stopDistanceMeters = 0.0;

    std::string message;
};

// Fast, deterministic pre-planning guard.
//
// It does NOT build the requested route. It answers a smaller question:
// "Can the craft safely continue/launch through the supplied static planning
// scene? At speed this means the present inertial course; at rest it means the
// hull-forward launch ray. If blocked, is there a physically reachable free
// course, or at least a collision-free stop?"
//
// This layer exists so route-planner failure can never mean "continue flying
// blindly through the obstacle that just caused the failure".
class PreplanningSafetyCourse final
{
public:
    [[nodiscard]] static PreplanningSafetyCourseResult evaluate(
        const PreplanningSafetyCourseRequest& request
    );
};

} // namespace game::navigation
