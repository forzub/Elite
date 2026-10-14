#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{

enum class AssistedSafeCourseRecoveryDisposition : std::uint8_t
{
    Invalid = 0,
    CurrentCourseSafe,
    Maneuver,
    StopFallback
};

struct AssistedSafeCourseRecoveryRequest
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 preferredDirection {0.0, 0.0, -1.0};

    double hullRadiusMeters = 0.0;
    double additionalClearanceMeters = 0.0;

    double courseAccelerationMps2 = 0.0;
    double brakingAccelerationMps2 = 0.0;
    double maxCourseAngularVelocityRadPerSecond = 0.0;

    std::vector<world::navigation::NavigationObstacle> obstacles;
};

struct AssistedSafeCourseRecoveryPlan
{
    AssistedSafeCourseRecoveryDisposition disposition =
        AssistedSafeCourseRecoveryDisposition::Invalid;

    glm::dvec3 startPositionMeters {0.0};
    glm::dvec3 startVelocityMps {0.0};
    glm::dvec3 initialCourse {0.0, 0.0, -1.0};

    glm::dvec3 targetCourse {0.0, 0.0, -1.0};
    glm::dvec3 targetVelocityMps {0.0};

    glm::dvec3 turnAxis {0.0, 1.0, 0.0};
    glm::dvec3 turnCenterMeters {0.0};

    double speedMps = 0.0;
    double turnAngleRad = 0.0;
    double turnRadiusMeters = 0.0;
    double courseAngularRateRadPerSecond = 0.0;
    double durationSeconds = 0.0;

    double sceneRayMeters = 0.0;
    double stopDistanceMeters = 0.0;

    // The original obstacle proof reserves the full requested clearance.
    // Tracking is considered healthy while the craft remains inside half of
    // that reserve; larger divergence requires a fresh maneuver proof.
    double trackingPositionEnvelopeMeters = 0.0;
    double trackingVelocityEnvelopeMps = 0.0;

    double hullRadiusMeters = 0.0;
    double additionalClearanceMeters = 0.0;

    glm::dvec3 endPositionMeters {0.0};

    bool stopCorridorClear = false;
    std::vector<world::navigation::NavigationObstacle> obstacles;

    std::string message;

    [[nodiscard]] bool valid() const noexcept
    {
        return disposition !=
            AssistedSafeCourseRecoveryDisposition::Invalid;
    }

    [[nodiscard]] bool requiresManeuver() const noexcept
    {
        return disposition ==
            AssistedSafeCourseRecoveryDisposition::Maneuver;
    }
};

struct AssistedSafeCourseRecoverySample
{
    bool valid = false;
    bool turnComplete = false;

    double elapsedSeconds = 0.0;
    double turnProgress01 = 0.0;

    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 linearAccelerationMps2 {0.0};
};

enum class AssistedSafeCourseRecoveryAssessmentDisposition :
    std::uint8_t
{
    Invalid = 0,
    Continue,
    Complete,
    ReplanRequired,
    StopFallback
};

struct AssistedSafeCourseRecoveryAssessment
{
    AssistedSafeCourseRecoveryAssessmentDisposition disposition =
        AssistedSafeCourseRecoveryAssessmentDisposition::Invalid;

    AssistedSafeCourseRecoverySample target;

    double positionErrorMeters = 0.0;
    double velocityErrorMps = 0.0;
    bool remainingTrajectoryClear = false;

    std::string message;
};

// Pure Assisted-only safety-recovery API.
//
// plan():
//   Chooses one physically reachable escape maneuver and proves its complete
//   swept path through the supplied static obstacle set.
//
// sample():
//   Returns the committed maneuver state at elapsed time. The maneuver does
//   not change merely because time has passed.
//
// assess():
//   Checks whether the craft is still tracking that SAME committed maneuver
//   and whether its remaining proven path is still clear. It never chooses a
//   replacement direction. Replanning is requested only after real tracking
//   divergence or loss of the remaining safety proof.
//
// Newtonian is deliberately NOT part of this API. Its rotate/settle/burn
// recovery sequence has different actuator semantics and should get its own
// executor instead of being hidden behind Assisted assumptions.
class AssistedSafeCourseRecovery final
{
public:
    [[nodiscard]] static AssistedSafeCourseRecoveryPlan plan(
        const AssistedSafeCourseRecoveryRequest& request
    );

    [[nodiscard]] static AssistedSafeCourseRecoverySample sample(
        const AssistedSafeCourseRecoveryPlan& plan,
        double elapsedSeconds
    ) noexcept;

    [[nodiscard]] static AssistedSafeCourseRecoveryAssessment assess(
        const AssistedSafeCourseRecoveryPlan& plan,
        double elapsedSeconds,
        const glm::dvec3& actualPositionMeters,
        const glm::dvec3& actualVelocityMps,
        const std::vector<world::navigation::NavigationObstacle>&
            currentObstacles
    );
};

} // namespace game::navigation
