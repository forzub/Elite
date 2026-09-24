#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/navigation/NavigationControlIntent.h"
#include "src/world/navigation/control/PilotSkillExecutor.h"

namespace game::navigation
{

// Live runtime seam between accepted Navigation v2 intent and the existing
// ship-control/physics stack.
//
// The bridge owns one deterministic PilotSkillExecutor state for one pilot. It
// does not move the ship, clamp vehicle capability, search the world, or render
// guidance. Its output is a normal ShipControlState direct-demand sample that
// SharedShipPhysics/DynamicMotionSystem constrain and execute downstream.
class NavigationRuntimeControlBridge final
{
public:
    using PilotExecutor = world::navigation::PilotSkillExecutor;
    using PilotSkillProfile = PilotExecutor::PilotSkillProfile;

    using Intent = NavigationSystemControlIntent;

    struct ExecutionSnapshot
    {
        bool valid = false;

        std::uint64_t intentRevision = 0;
        std::uint64_t activeTargetRevision = 0;

        glm::dvec3 idealLinearAccelerationDemandSystemMps2 {0.0};
        glm::dvec3 idealAngularAccelerationDemandSystemRadPerSec2 {0.0};
        glm::dvec3 executedLinearAccelerationDemandSystemMps2 {0.0};
        glm::dvec3 executedAngularAccelerationDemandSystemRadPerSec2 {0.0};

        bool emergency = false;
        double hazardUrgency01 = 0.0;

        bool reactionBlocked = false;
        bool decisionSampled = false;
        bool queuedCommandApplied = false;
        std::size_t pendingCommandCount = 0;
    };

    struct StepResult
    {
        PilotExecutor::Status status =
            PilotExecutor::Status::NotInitialized;

        ShipControlState control {};
        ExecutionSnapshot snapshot {};
    };

    explicit NavigationRuntimeControlBridge(
        const PilotSkillProfile& profile
    ) noexcept;

    [[nodiscard]] bool reset(
        double timeSeconds,
        const Intent& initialIntent
    ) noexcept;

    [[nodiscard]] StepResult step(
        double timeSeconds,
        double deltaSeconds,
        const Intent& intent
    ) noexcept;

    [[nodiscard]] double maximumStepSeconds() const noexcept
    {
        return executor_.profile().execution.maximumStepSeconds;
    }

private:
    [[nodiscard]] static PilotExecutor::Command toPilotCommand(
        const Intent& intent
    ) noexcept;

    static glm::dvec3 fromPilotVec(
        const PilotExecutor::Vec3d& value
    ) noexcept;

    PilotExecutor executor_;
};

} // namespace game::navigation
