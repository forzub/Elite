#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/game/navigation/PreplanningSafetyCourse.h"
#include "src/game/navigation/DockingRouteEntryPolicy.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

world::navigation::NavigationObstacle sphere(
    const char* id,
    const glm::dvec3& center,
    double radius
)
{
    world::navigation::NavigationObstacle out;
    out.id = id;
    out.shape =
        world::navigation::NavigationObstacleShape::Sphere;
    out.centerMeters = center;
    out.radiusMeters = radius;
    return out;
}

// The safety selector owns the acquired FIRST TANGENT. Verify the ordinary
// route backend consumes that direction at rest and at nonzero speed, rather
// than reconstructing a path from the discarded, obstacle-facing ray.
void requireSelectedEscapeIsRouteStart(
    const glm::dvec3& selectedCourse,
    double initialSpeedMps
)
{
    using namespace game::navigation::planner;
    RoutePlanRequest route;
    route.startMeters = {0.0, 0.0, 0.0};
    route.goalMeters = selectedCourse * 10000.0;
    route.terminalOutward = -selectedCourse;
    route.agentRadiusMeters = 10.0;
    route.maxSpeedMps = 150.0;
    route.acceleratingMps2 = 25.0;
    route.brakingMps2 = 25.0;
    route.lateralMps2 = 25.0;
    route.roundTurns = false;
    route.hasInitialForward = true;
    route.initialForward = selectedCourse;
    route.initialSpeedMps = initialSpeedMps;
    route.initialForwardLeadMeters = 1500.0;

    const auto plan = RoutePlanner::plan(route);
    require(plan.valid(), "escape-direction route was not constructed");
    require(!plan.routeCurves.empty(),
            "escape-direction route has no first segment");
    require(
        glm::dot(
            glm::normalize(plan.routeCurves.front().startForward),
            selectedCourse
        ) > 0.999,
        "first route tangent returned to the unsafe pre-escape course"
    );
}
}

int main()
{
    try
    {
        using game::navigation::PreplanningSafetyCourse;
        using game::navigation::PreplanningSafetyCourseRequest;

        {
            PreplanningSafetyCourseRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {0.0, 0.0, 0.0};
            request.hullForward = {1.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 10.0;
            request.additionalClearanceMeters = 20.0;
            request.courseAccelerationMps2 = 20.0;
            request.brakingAccelerationMps2 = 20.0;
            request.maxAngularVelocityRadPerSecond = 1.0;
            request.obstacles.push_back(
                sphere("dock-ahead", {900.0, 0.0, 0.0}, 250.0)
            );

            const auto result =
                PreplanningSafetyCourse::evaluate(request);

            require(
                result.valid,
                "stationary blocked-launch request invalid"
            );
            require(
                glm::dot(
                    result.currentCourse,
                    glm::dvec3(1.0, 0.0, 0.0)
                ) > 0.999,
                "stationary safety probe did not use hull-forward launch ray"
            );
            require(
                !result.currentCourseSafe,
                "stationary hull-forward obstacle was ignored"
            );
            require(
                result.safeCourseFound,
                "stationary blocked launch did not find a free course"
            );
            require(
                glm::dot(
                    result.safeCourse,
                    glm::dvec3(1.0, 0.0, 0.0)
                ) < 0.999,
                "stationary escape course still points into the obstacle"
            );
            require(
                result.speedMps <= 0.5,
                "stationary safety probe invented translational speed"
            );
            const glm::dvec3 stoppedEntry =
                game::navigation::DockingRouteEntryPolicy::initialCourse(
                    request.velocityMps, request.hullForward
                );
            require(
                glm::dot(stoppedEntry, result.currentCourse) > 0.999999,
                "stopped route entry did not start on the actual hull ray"
            );
            require(
                glm::dot(stoppedEntry, result.safeCourse) < 0.999,
                "stopped safety escape did not require a new route tangent"
            );
            requireSelectedEscapeIsRouteStart(
                result.safeCourse, result.speedMps
            );
        }

        {
            PreplanningSafetyCourseRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {100.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 10.0;
            request.additionalClearanceMeters = 20.0;
            request.courseAccelerationMps2 = 20.0;
            request.brakingAccelerationMps2 = 20.0;
            request.maxAngularVelocityRadPerSecond = 1.0;
            request.obstacles.push_back(
                sphere("off-course", {0.0, 1000.0, 0.0}, 100.0)
            );

            const auto result =
                PreplanningSafetyCourse::evaluate(request);

            require(result.valid, "clear-course request invalid");
            require(
                result.currentCourseSafe,
                "clear current course was rejected"
            );
            require(
                result.safeCourseFound,
                "clear current course was not preserved"
            );
        }

        {
            PreplanningSafetyCourseRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {100.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 10.0;
            request.additionalClearanceMeters = 20.0;
            request.courseAccelerationMps2 = 25.0;
            request.brakingAccelerationMps2 = 25.0;
            request.maxAngularVelocityRadPerSecond = 1.0;
            request.obstacles.push_back(
                sphere("ahead", {1800.0, 0.0, 0.0}, 250.0)
            );

            const auto result =
                PreplanningSafetyCourse::evaluate(request);

            require(result.valid, "blocked-course request invalid");
            require(
                !result.currentCourseSafe,
                "blocked ballistic course was accepted"
            );
            require(
                result.safeCourseFound,
                "blocked course did not produce an escape course"
            );
            require(
                glm::dot(
                    result.safeCourse,
                    glm::dvec3(1.0, 0.0, 0.0)
                ) < 0.999,
                "escape course did not change inertial direction"
            );
            require(
                result.turnRadiusMeters > 0.0,
                "escape course omitted physical turn radius"
            );
            // At speed, velocity is the physical initial tangent even when
            // the nose points elsewhere. The planner must use the SELECTED
            // escape tangent after avoidance, not keep this old velocity ray.
            const glm::dvec3 velocityEntry =
                game::navigation::DockingRouteEntryPolicy::initialCourse(
                    request.velocityMps, glm::dvec3(0.0, 0.0, -1.0)
                );
            require(
                glm::dot(velocityEntry, result.currentCourse) > 0.999999,
                "moving route entry followed hull nose instead of velocity"
            );
            require(
                glm::dot(velocityEntry, result.safeCourse) < 0.999,
                "moving escape tangent matched the unsafe original velocity"
            );
            require(
                std::abs(result.speedMps - 100.0) < 1.0e-9,
                "moving safety-course selection changed initial speed"
            );
            requireSelectedEscapeIsRouteStart(
                result.safeCourse, result.speedMps
            );
        }

        {
            PreplanningSafetyCourseRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {100.0, 0.0, 0.0};
            request.hullForward = {1.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 10.0;
            request.additionalClearanceMeters = 20.0;
            request.courseAccelerationMps2 = 25.0;
            request.brakingAccelerationMps2 = 25.0;
            request.maxAngularVelocityRadPerSecond = 1.0;
            request.newtonian = true;
            request.obstacles.push_back(
                sphere("ahead-newton", {1800.0, 0.0, 0.0}, 250.0)
            );

            const auto result =
                PreplanningSafetyCourse::evaluate(request);

            require(
                result.valid,
                "Newtonian blocked-course request invalid"
            );
            require(
                !result.currentCourseSafe,
                "Newtonian blocked ballistic course was accepted"
            );
            require(
                result.safeCourseFound,
                "Newtonian safety planner invented no physical escape"
            );
            require(
                glm::dot(
                    result.safeCourse,
                    glm::dvec3(1.0, 0.0, 0.0)
                ) < 0.999,
                "Newtonian escape course did not change inertial velocity"
            );
        }

        {
            PreplanningSafetyCourseRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {80.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 10.0;
            request.additionalClearanceMeters = 20.0;
            request.courseAccelerationMps2 = 0.0;
            request.brakingAccelerationMps2 = 40.0;
            request.maxAngularVelocityRadPerSecond = 0.0;
            request.obstacles.push_back(
                sphere("ahead", {3000.0, 0.0, 0.0}, 200.0)
            );

            const auto result =
                PreplanningSafetyCourse::evaluate(request);

            require(result.valid, "stop-fallback request invalid");
            require(
                !result.currentCourseSafe,
                "blocked stop-fallback course was accepted"
            );
            require(
                !result.safeCourseFound,
                "zero turn authority invented an escape course"
            );
            require(
                result.stopFallbackClear,
                "clear physical stopping corridor was rejected"
            );
        }

        {
            // The center ray is clear, but a nearby obstacle clips the
            // requested delta-spot. The old single-ray test would have called
            // this course safe; the tube proof must reject it.
            PreplanningSafetyCourseRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {100.0, 0.0, 0.0};
            request.hullForward = {1.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 5.0;
            request.additionalClearanceMeters = 5.0;
            request.courseSafetySpotRadiusMeters = 25.0;
            request.courseAccelerationMps2 = 25.0;
            request.brakingAccelerationMps2 = 25.0;
            request.maxAngularVelocityRadPerSecond = 1.0;

            // Centerline clearance: 5 + 5 + obstacle radius 5 = 15 m.
            // Obstacle is 30 m off the centerline, so the center ray is clear,
            // while the +Y delta ray at 25 m is blocked.
            request.obstacles.push_back(
                sphere("near-edge", {1200.0, 30.0, 0.0}, 5.0)
            );

            auto noSpot = request;
            noSpot.courseSafetySpotRadiusMeters = 0.0;
            const auto centerOnly =
                PreplanningSafetyCourse::evaluate(noSpot);
            require(
                centerOnly.currentCourseSafe,
                "control case unexpectedly rejected clear center ray"
            );

            const auto withSpot =
                PreplanningSafetyCourse::evaluate(request);
            require(
                !withSpot.currentCourseSafe,
                "delta-spot ignored nearby obstacle beside center ray"
            );
            require(
                withSpot.safeCourseFound,
                "delta-spot rejection did not find an alternate course"
            );
        }

        std::cout
            << "PREPLANNING SAFETY COURSE TESTS: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "PREPLANNING SAFETY COURSE TESTS: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
