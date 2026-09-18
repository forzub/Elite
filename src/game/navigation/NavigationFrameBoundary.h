#pragma once

#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/NavigationControlIntent.h"

namespace game::navigation
{

// The ONLY coordinate conversion seam admitted by Navigation v2.
//
// Repository runtime state outside this class may use system-local WorldPosition,
// KinematicFrame, body axes, authored visual axes or render-relative coordinates.
// NavigationMap/NavigationSpace/planner/follower are deliberately NavLocal-only.
//
// The wrappers below are intentionally not implicitly convertible. Crossing the
// boundary therefore requires naming the source semantic explicitly.
class NavigationFrameBoundary final
{
public:
    struct SystemPosition
    {
        glm::dvec3 meters {0.0};
    };

    struct SystemVelocity
    {
        glm::dvec3 metersPerSecond {0.0};
    };

    struct SystemAcceleration
    {
        glm::dvec3 metersPerSecond2 {0.0};
    };

    struct SystemVector
    {
        glm::dvec3 value {0.0};
    };

    struct SystemAngularVelocity
    {
        glm::dvec3 radiansPerSecond {0.0};
    };

    struct NavPosition
    {
        glm::dvec3 meters {0.0};
    };

    struct NavVelocity
    {
        glm::dvec3 metersPerSecond {0.0};
    };

    struct NavAcceleration
    {
        glm::dvec3 metersPerSecond2 {0.0};
    };

    struct NavVector
    {
        glm::dvec3 value {0.0};
    };

    struct NavAngularVelocity
    {
        glm::dvec3 radiansPerSecond {0.0};
    };

    struct SystemKinematics
    {
        SystemPosition position {};
        SystemVelocity velocity {};
        SystemAcceleration acceleration {};
    };

    struct NavKinematics
    {
        NavPosition position {};
        NavVelocity velocity {};
        NavAcceleration acceleration {};
    };

    explicit NavigationFrameBoundary(const KinematicFrame& frame) noexcept
        : frame_(frame)
    {
    }

    [[nodiscard]] bool valid() const noexcept
    {
        if (!frame_.valid || frame_.systemId < 0 ||
            !finite(frame_.originMeters) ||
            !finite(frame_.linearVelocityMps) ||
            !finite(frame_.linearAccelerationMps2) ||
            !finite(frame_.angularVelocityWorldRadPerSecond) ||
            !finite(frame_.angularAccelerationWorldRadPerSecond2))
        {
            return false;
        }

        const glm::dvec3 x = frame_.localToWorldBasis[0];
        const glm::dvec3 y = frame_.localToWorldBasis[1];
        const glm::dvec3 z = frame_.localToWorldBasis[2];

        constexpr double UnitTolerance = 1.0e-6;
        constexpr double OrthogonalTolerance = 1.0e-6;
        return
            finite(x) && finite(y) && finite(z) &&
            std::abs(glm::length(x) - 1.0) <= UnitTolerance &&
            std::abs(glm::length(y) - 1.0) <= UnitTolerance &&
            std::abs(glm::length(z) - 1.0) <= UnitTolerance &&
            std::abs(glm::dot(x, y)) <= OrthogonalTolerance &&
            std::abs(glm::dot(x, z)) <= OrthogonalTolerance &&
            std::abs(glm::dot(y, z)) <= OrthogonalTolerance &&
            glm::dot(glm::cross(x, y), z) > 1.0 - UnitTolerance;
    }

    [[nodiscard]] int systemId() const noexcept
    {
        return frame_.systemId;
    }

    [[nodiscard]] const KinematicFrame& frame() const noexcept
    {
        return frame_;
    }

    [[nodiscard]] NavPosition toNavigation(
        const SystemPosition& position
    ) const noexcept
    {
        return {frame_.worldToLocalPosition(position.meters)};
    }

    [[nodiscard]] NavVelocity toNavigation(
        const SystemPosition& position,
        const SystemVelocity& velocity
    ) const noexcept
    {
        return {
            frame_.worldToLocalVelocity(
                position.meters,
                velocity.metersPerSecond
            )
        };
    }

    [[nodiscard]] NavKinematics toNavigation(
        const SystemKinematics& state
    ) const noexcept
    {
        const WorldKinematicState world {
            state.position.meters,
            state.velocity.metersPerSecond,
            state.acceleration.metersPerSecond2
        };
        const LocalKinematicState local =
            worldToLocalKinematics(frame_, world);
        return {
            NavPosition {local.positionMeters},
            NavVelocity {local.velocityMps},
            NavAcceleration {local.accelerationMps2}
        };
    }

    [[nodiscard]] NavVector toNavigationVector(
        const SystemVector& vector
    ) const noexcept
    {
        return {frame_.worldToLocalVector(vector.value)};
    }

    // Angular velocity is state, not a free vector. Navigation needs motion
    // relative to its own rotating frame, so subtract frame omega before
    // expressing the remainder in NavLocal axes.
    [[nodiscard]] NavAngularVelocity toNavigation(
        const SystemAngularVelocity& angularVelocity
    ) const noexcept
    {
        return {
            frame_.worldToLocalVector(
                angularVelocity.radiansPerSecond -
                frame_.angularVelocityWorldRadPerSecond
            )
        };
    }

    [[nodiscard]] SystemPosition toSystem(
        const NavPosition& position
    ) const noexcept
    {
        return {frame_.localToWorldPosition(position.meters)};
    }

    [[nodiscard]] SystemVelocity toSystem(
        const NavPosition& position,
        const NavVelocity& velocity
    ) const noexcept
    {
        return {
            frame_.localToWorldVelocity(
                position.meters,
                velocity.metersPerSecond
            )
        };
    }

    [[nodiscard]] SystemVector toSystemVector(
        const NavVector& vector
    ) const noexcept
    {
        return {frame_.localToWorldVector(vector.value)};
    }

    [[nodiscard]] NavigationSystemControlIntent toSystemControlIntent(
        const NavigationLocalControlIntent& local
    ) const noexcept
    {
        NavigationSystemControlIntent out;
        out.revision = local.revision;
        out.targetRevision = local.targetRevision;
        out.idealLinearAccelerationSystemMps2 =
            frame_.localToWorldVector(
                local.idealLinearAccelerationLocalMps2
            );
        out.idealAngularAccelerationSystemRadPerSec2 =
            frame_.localToWorldVector(
                local.idealAngularAccelerationLocalRadPerSec2
            );
        out.emergency = local.emergency;
        out.hazardUrgency01 = local.hazardUrgency01;
        return out;
    }

    [[nodiscard]] NavigationLocalControlIntent toNavigationControlIntent(
        const NavigationSystemControlIntent& system
    ) const noexcept
    {
        NavigationLocalControlIntent out;
        out.revision = system.revision;
        out.targetRevision = system.targetRevision;
        out.idealLinearAccelerationLocalMps2 =
            frame_.worldToLocalVector(
                system.idealLinearAccelerationSystemMps2
            );
        out.idealAngularAccelerationLocalRadPerSec2 =
            frame_.worldToLocalVector(
                system.idealAngularAccelerationSystemRadPerSec2
            );
        out.emergency = system.emergency;
        out.hazardUrgency01 = system.hazardUrgency01;
        return out;
    }

private:
    static bool finite(const glm::dvec3& value) noexcept
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    KinematicFrame frame_ {};
};

} // namespace game::navigation
