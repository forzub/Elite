#pragma once

#include <glm/glm.hpp>

#include "src/game/navigation/KinematicFrame.h"

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
        return frame_.valid && frame_.systemId >= 0;
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

private:
    KinematicFrame frame_ {};
};

} // namespace game::navigation
