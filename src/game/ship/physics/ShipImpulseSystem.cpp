#include "src/game/ship/physics/ShipImpulseSystem.h"

#include <algorithm>

namespace game::ship::physics
{
namespace
{
constexpr double MinMassKg = 1.0e-9;
constexpr double MinInertiaKgM2 = 1.0e-9;
}

ShipImpulseResult ShipImpulseSystem::applyImpulseAtWorldPoint(
    ShipTransform& ship,
    const ShipParams& params,
    const glm::dvec3& impulseWorldNewtonSeconds,
    const glm::dvec3& contactPointWorldMeters
)
{
    ShipImpulseResult result;

    const double massKg = std::max(params.massKg, MinMassKg);
    result.deltaVelocityWorldMps = impulseWorldNewtonSeconds / massKg;

    if (ship.motion.travelFrame.valid)
    {
        result.deltaVelocityLocalMps =
            ship.motion.travelFrame.worldToLocalVector(
                result.deltaVelocityWorldMps
            );
    }
    else
    {
        // In an inertial/system-local fallback the local axes are world axes.
        result.deltaVelocityLocalMps = result.deltaVelocityWorldMps;
    }

    ship.motion.localVelocityMps += result.deltaVelocityLocalMps;

    if (ship.motion.travelFrame.valid)
    {
        ship.motion.worldVelocityMps =
            ship.motion.travelFrame.localToWorldVelocity(
                ship.motion.localPositionMeters,
                ship.motion.localVelocityMps
            );
    }
    else
    {
        ship.motion.worldVelocityMps += result.deltaVelocityWorldMps;
    }

    const glm::dvec3 centerWorldMeters = ship.fullWorldMeters();
    const glm::dvec3 leverArmWorldMeters =
        contactPointWorldMeters - centerWorldMeters;

    // Angular impulse ΔL = r × J. Project into the body's principal axes,
    // then Δω = I^-1 ΔL. An impact through COM has r=0 and produces no spin;
    // the same J at Cobra's wing edge can produce a large yaw/roll response.
    const glm::dvec3 angularImpulseWorld =
        glm::cross(leverArmWorldMeters, impulseWorldNewtonSeconds);

    const glm::dvec3 rightWorld = glm::dvec3(ship.right());
    const glm::dvec3 upWorld = glm::dvec3(ship.up());
    const glm::dvec3 forwardWorld = glm::dvec3(ship.forward());

    const glm::dvec3 angularImpulseBody(
        glm::dot(angularImpulseWorld, rightWorld),
        glm::dot(angularImpulseWorld, upWorld),
        glm::dot(angularImpulseWorld, forwardWorld)
    );

    result.deltaAngularVelocityBodyRadPerSec = glm::dvec3(
        angularImpulseBody.x /
            std::max(params.pitchInertiaKgM2, MinInertiaKgM2),
        angularImpulseBody.y /
            std::max(params.yawInertiaKgM2, MinInertiaKgM2),
        angularImpulseBody.z /
            std::max(params.rollInertiaKgM2, MinInertiaKgM2)
    );

    ship.pitchRate +=
        static_cast<float>(result.deltaAngularVelocityBodyRadPerSec.x);
    ship.yawRate +=
        static_cast<float>(result.deltaAngularVelocityBodyRadPerSec.y);
    ship.rollRate +=
        static_cast<float>(result.deltaAngularVelocityBodyRadPerSec.z);

    return result;
}

} // namespace game::ship::physics
