#pragma once

#include <cstddef>
#include <cstdint>

#include "src/game/ship/core/ShipControlState.h"
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

    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct Intent
    {
        // High-level maneuver/mission intent revision. Pilot reaction delay
        // restarts only when this identity changes.
        std::uint64_t revision = 0;

        // Concrete accepted execution target/segment revision. Zero preserves
        // legacy behavior by falling back to revision.
        std::uint64_t targetRevision = 0;

        Vec3d idealLinearAccelerationDemandMapMps2 {};
        Vec3d idealAngularAccelerationDemandMapRadPerSec2 {};

        bool emergency = false;
        double hazardUrgency01 = 0.0;
    };

    struct ExecutionSnapshot
    {
        bool valid = false;

        std::uint64_t intentRevision = 0;
        std::uint64_t activeTargetRevision = 0;

        Vec3d idealLinearAccelerationDemandMapMps2 {};
        Vec3d idealAngularAccelerationDemandMapRadPerSec2 {};
        Vec3d executedLinearAccelerationDemandMapMps2 {};
        Vec3d executedAngularAccelerationDemandMapRadPerSec2 {};

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

private:
    [[nodiscard]] static PilotExecutor::Command toPilotCommand(
        const Intent& intent
    ) noexcept;

    static Vec3d fromPilotVec(
        const PilotExecutor::Vec3d& value
    ) noexcept;

    PilotExecutor executor_;
};

} // namespace game::navigation
