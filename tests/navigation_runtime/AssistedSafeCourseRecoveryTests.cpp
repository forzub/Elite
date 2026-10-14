#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/game/navigation/AssistedSafeCourseRecovery.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const char* message
)
{
    if (std::abs(actual - expected) > tolerance)
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

game::navigation::AssistedSafeCourseRecoveryRequest
blockedRequest()
{
    game::navigation::AssistedSafeCourseRecoveryRequest request;
    request.positionMeters = {0.0, 0.0, 0.0};
    request.velocityMps = {100.0, 0.0, 0.0};
    request.preferredDirection = {1.0, 0.0, 0.0};
    request.hullRadiusMeters = 10.0;
    request.additionalClearanceMeters = 40.0;
    request.courseAccelerationMps2 = 25.0;
    request.brakingAccelerationMps2 = 25.0;
    request.maxCourseAngularVelocityRadPerSecond = 1.0;
    request.obstacles.push_back(
        sphere("ahead", {1800.0, 0.0, 0.0}, 250.0)
    );
    return request;
}
}

int main()
{
    try
    {
        using namespace game::navigation;

        {
            AssistedSafeCourseRecoveryRequest request;
            request.positionMeters = {0.0, 0.0, 0.0};
            request.velocityMps = {100.0, 0.0, 0.0};
            request.preferredDirection = {1.0, 0.0, 0.0};
            request.hullRadiusMeters = 10.0;
            request.additionalClearanceMeters = 40.0;
            request.courseAccelerationMps2 = 25.0;
            request.brakingAccelerationMps2 = 25.0;
            request.maxCourseAngularVelocityRadPerSecond = 1.0;
            request.obstacles.push_back(
                sphere("off-course", {0.0, 1200.0, 0.0}, 150.0)
            );

            const auto plan =
                AssistedSafeCourseRecovery::plan(request);

            require(plan.valid(), "clear Assisted request invalid");
            require(
                plan.disposition ==
                    AssistedSafeCourseRecoveryDisposition::
                        CurrentCourseSafe,
                "clear Assisted ballistic course invented a recovery maneuver"
            );
        }

        const auto request = blockedRequest();
        const auto plan =
            AssistedSafeCourseRecovery::plan(request);

        require(plan.valid(), "blocked Assisted request invalid");
        require(
            plan.disposition ==
                AssistedSafeCourseRecoveryDisposition::Maneuver,
            "blocked Assisted course did not produce a committed maneuver"
        );
        require(
            plan.durationSeconds > 0.25,
            "committed Assisted maneuver duration is not meaningful"
        );
        require(
            plan.turnAngleRad > 0.0,
            "committed Assisted maneuver did not change course"
        );
        require(
            plan.courseAngularRateRadPerSecond <=
                request.maxCourseAngularVelocityRadPerSecond +
                    1.0e-9,
            "committed Assisted maneuver exceeded angular-rate authority"
        );
        require(
            plan.speedMps *
                plan.courseAngularRateRadPerSecond <=
                    request.courseAccelerationMps2 + 1.0e-9,
            "committed Assisted maneuver exceeded lateral acceleration authority"
        );

        const glm::dvec3 committedCourse = plan.targetCourse;

        // Regression for the old 0.25-second panic loop. Exact tracking at
        // successive observation times MUST keep executing the same committed
        // maneuver; elapsed time alone never requests a new direction.
        for (const double elapsed : {0.25, 0.50})
        {
            const auto expected =
                AssistedSafeCourseRecovery::sample(
                    plan,
                    elapsed
                );
            require(expected.valid, "committed maneuver sample invalid");

            const auto assessment =
                AssistedSafeCourseRecovery::assess(
                    plan,
                    elapsed,
                    expected.positionMeters,
                    expected.velocityMps,
                    request.obstacles
                );

            require(
                assessment.disposition ==
                    AssistedSafeCourseRecoveryAssessmentDisposition::
                        Continue,
                "elapsed time alone caused Assisted safe-course replanning"
            );
            require(
                glm::dot(plan.targetCourse, committedCourse) >
                    0.999999999,
                "committed Assisted escape direction changed while tracking"
            );
        }

        {
            const double elapsed = 0.50;
            const auto expected =
                AssistedSafeCourseRecovery::sample(
                    plan,
                    elapsed
                );

            const glm::dvec3 divergedPosition =
                expected.positionMeters +
                glm::dvec3(
                    plan.trackingPositionEnvelopeMeters * 2.0,
                    0.0,
                    0.0
                );

            const auto assessment =
                AssistedSafeCourseRecovery::assess(
                    plan,
                    elapsed,
                    divergedPosition,
                    expected.velocityMps,
                    request.obstacles
                );

            require(
                assessment.disposition ==
                    AssistedSafeCourseRecoveryAssessmentDisposition::
                        ReplanRequired,
                "real position divergence did not request a fresh safety proof"
            );
        }

        {
            auto changedObstacles = request.obstacles;
            changedObstacles.push_back(
                sphere(
                    "new-blocker",
                    plan.endPositionMeters +
                        plan.targetCourse * 500.0,
                    200.0
                )
            );

            const double elapsed = 0.50;
            const auto expected =
                AssistedSafeCourseRecovery::sample(
                    plan,
                    elapsed
                );

            const auto assessment =
                AssistedSafeCourseRecovery::assess(
                    plan,
                    elapsed,
                    expected.positionMeters,
                    expected.velocityMps,
                    changedObstacles
                );

            require(
                assessment.disposition ==
                    AssistedSafeCourseRecoveryAssessmentDisposition::
                        ReplanRequired,
                "new blocker on remaining committed path did not request replan"
            );
        }

        {
            const auto expected =
                AssistedSafeCourseRecovery::sample(
                    plan,
                    plan.durationSeconds
                );

            const auto assessment =
                AssistedSafeCourseRecovery::assess(
                    plan,
                    plan.durationSeconds,
                    expected.positionMeters,
                    expected.velocityMps,
                    request.obstacles
                );

            require(
                assessment.disposition ==
                    AssistedSafeCourseRecoveryAssessmentDisposition::
                        Complete,
                "completed committed Assisted maneuver was not acquired"
            );
            requireNear(
                glm::length(
                    expected.velocityMps -
                    plan.targetVelocityMps
                ),
                0.0,
                1.0e-6,
                "completed Assisted maneuver missed target velocity"
            );
        }

        {
            auto noTurn = blockedRequest();
            noTurn.courseAccelerationMps2 = 0.0;
            noTurn.maxCourseAngularVelocityRadPerSecond = 0.0;

            const auto stop =
                AssistedSafeCourseRecovery::plan(noTurn);

            require(
                stop.disposition ==
                    AssistedSafeCourseRecoveryDisposition::
                        StopFallback,
                "zero Assisted turn authority did not fall back to stop"
            );
        }

        std::cout
            << "ASSISTED SAFE COURSE RECOVERY TESTS: PASS\n"
            << " - safe ballistic course remains untouched\n"
            << " - blocked course commits one physical escape arc\n"
            << " - 0.25 s observations do not reselect the maneuver\n"
            << " - real tracking divergence or new blockers request replan\n"
            << " - completed maneuver acquires the predicted safe velocity\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ASSISTED SAFE COURSE RECOVERY TESTS: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
