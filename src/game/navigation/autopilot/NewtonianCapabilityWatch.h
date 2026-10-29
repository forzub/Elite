#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationControlCapability.h"

namespace game::navigation::autopilot
{

// Capability change during a route (damaged stage, plan item 22). Compares
// the capability the active program was accepted with and the live one the
// ship publishes now. Small thermal/telemetry jitter is ignored (relative
// threshold); a real change must persist (hysteresis) before it counts.
// It decides nothing about the route: the route autopilot reprofiles,
// recompiles or asks for a replan from the verdict.
enum class CapabilityChange : int
{
    None = 0,
    // Main forward/reverse thrust or nozzle travel lost: the speed profile
    // (acceleration, braking ramps) is no longer executable as compiled.
    MainDegraded = 1,
    // A translational RCS or angular direction lost: curve speeds and turn
    // times of the route itself may be wrong.
    LateralDegraded = 2,
    // No translation or no attitude authority at all.
    Unavailable = 3,
};

struct NewtonianCapabilityWatch
{
    // A loss counts from this share of the accepted value.
    static constexpr double MeaningfulLossShare = 0.10;
    // ...and only after it persisted this long.
    static constexpr double PersistSeconds = 0.5;

    NavigationControlCapability accepted {};
    // The capability the route was planned with (its nominal speeds are the
    // ceiling a recovery may return to).
    NavigationControlCapability planned {};
    bool valid = false;
    double pendingSinceSeconds = -1.0;
    double recoveryPendingSinceSeconds = -1.0;

    void accept(const NavigationControlCapability& capability) noexcept
    {
        accepted = capability;
        valid = capability.valid;
        pendingSinceSeconds = -1.0;
        recoveryPendingSinceSeconds = -1.0;
    }

    void acceptPlanned(const NavigationControlCapability& capability) noexcept
    {
        accept(capability);
        planned = capability;
    }

    [[nodiscard]] static bool gained(double before, double now) noexcept
    {
        return now > 1.0e-9 && now >= std::max(before, 1.0e-9) * (1.0 + MeaningfulLossShare);
    }

    // A capability regained after a loss (repair, reactor restart, cooled
    // nozzles): meaningful on main / RCS / attitude, no loss anywhere,
    // persisted. Mirror of evaluate(); the caller reprofiles up to the plan.
    [[nodiscard]] bool recoveryDue(
        const NavigationControlCapability& live, double nowSeconds) noexcept
    {
        if (!valid || !planned.valid ||
            classify(accepted, live) != CapabilityChange::None ||
            !(gained(accepted.mainForwardMps2, live.mainForwardMps2) ||
              gained(accepted.rcsScalarMps2, live.rcsScalarMps2) ||
              gained(accepted.angularScalarRadPerSec2, live.angularScalarRadPerSec2)))
        {
            recoveryPendingSinceSeconds = -1.0;
            return false;
        }
        if (recoveryPendingSinceSeconds < 0.0)
            recoveryPendingSinceSeconds = nowSeconds;
        return nowSeconds - recoveryPendingSinceSeconds >= PersistSeconds;
    }

    [[nodiscard]] static bool lost(double before, double now) noexcept
    {
        return before > 1.0e-9 &&
            !(now >= before * (1.0 - MeaningfulLossShare));
    }

    [[nodiscard]] static CapabilityChange classify(
        const NavigationControlCapability& before,
        const NavigationControlCapability& now) noexcept
    {
        if (!now.valid || !now.canManoeuvre() ||
            !(now.controlledSpeedLimitMps > 0.0))
            return CapabilityChange::Unavailable;
        bool lateral = false;
        for (int a = 0; a < 3; ++a)
            lateral = lateral ||
                lost(before.rcsPositiveMps2[a], now.rcsPositiveMps2[a]) ||
                lost(before.rcsNegativeMps2[a], now.rcsNegativeMps2[a]) ||
                lost(before.angularPositiveRadPerSec2[a],
                     now.angularPositiveRadPerSec2[a]) ||
                lost(before.angularNegativeRadPerSec2[a],
                     now.angularNegativeRadPerSec2[a]);
        lateral = lateral ||
            lost(before.maxAngularSpeedRadPerSec, now.maxAngularSpeedRadPerSec);
        if (lateral)
            return CapabilityChange::LateralDegraded;
        if (lost(before.mainForwardMps2, now.mainForwardMps2) ||
            lost(before.mainReverseMps2, now.mainReverseMps2) ||
            lost(before.gimbalPitchRad, now.gimbalPitchRad) ||
            lost(before.gimbalYawRad, now.gimbalYawRad))
            return CapabilityChange::MainDegraded;
        return CapabilityChange::None;
    }

    // The verdict once a change persisted; None while it is pending or gone.
    [[nodiscard]] CapabilityChange evaluate(
        const NavigationControlCapability& live, double nowSeconds) noexcept
    {
        if (!valid)
            return CapabilityChange::None;
        const auto change = classify(accepted, live);
        if (change == CapabilityChange::None)
        {
            pendingSinceSeconds = -1.0;
            return CapabilityChange::None;
        }
        if (pendingSinceSeconds < 0.0)
            pendingSinceSeconds = nowSeconds;
        return nowSeconds - pendingSinceSeconds >= PersistSeconds
            ? change
            : CapabilityChange::None;
    }
};

} // namespace game::navigation::autopilot
