#pragma once

#include <algorithm>
#include <cmath>

namespace game::equipment
{

// Five universal causes accumulated by every equipment instance.
// These are DAMAGE VALUES (runtime state), never sensitivity weights.
enum class EquipmentDamageKind
{
    Laser,
    Impact,
    Thermal,
    Radiation,
    Wear
};

struct EquipmentDamageState
{
    double laser01 = 0.0;
    double impact01 = 0.0;
    double thermal01 = 0.0;
    double radiation01 = 0.0;
    double wear01 = 0.0;
};

// Immutable sensitivity of this equipment type to each damage cause.
// These are WEIGHTS, never accumulated damage values.
struct EquipmentDamageSensitivity
{
    double laser = 0.0;
    double impact = 0.0;
    double thermal = 0.0;
    double radiation = 0.0;
    double wear = 0.0;
};

// Generic failure policy for one equipment function. The threshold is applied
// to weighted functional damage, not to module existence/attachment state.
struct EquipmentFailureProfile
{
    double failureAtDamage01 = 1.0;
};

struct EquipmentFunctionState
{
    double damage01 = 0.0;
    double authority01 = 1.0;
    bool failed = false;
};

// Service policy is descriptor data. Wear remains the fifth accumulated damage
// cause; this only says how fast normal operation accumulates it and when BITE
// should request maintenance.
struct EquipmentMaintenanceProfile
{
    double wearPerOperatingHour = 0.0;
    double maintenanceWearThreshold01 = 0.20;
};

// Common equipment damage layer: one per equipment instance.
// State changes only when damage/wear changes; sensitivity is copied from the
// equipment descriptor and remains constant for the instance.
struct EquipmentDamageLayer
{
    EquipmentDamageState accumulated;
    EquipmentDamageSensitivity sensitivity;
    EquipmentFailureProfile failure;
};

inline double clampDamage01(double value) noexcept
{
    return std::isfinite(value)
        ? std::clamp(value, 0.0, 1.0)
        : 0.0;
}

inline double accumulatedDamage(
    const EquipmentDamageState& state,
    EquipmentDamageKind kind
) noexcept
{
    switch (kind)
    {
        case EquipmentDamageKind::Laser:
            return state.laser01;
        case EquipmentDamageKind::Impact:
            return state.impact01;
        case EquipmentDamageKind::Thermal:
            return state.thermal01;
        case EquipmentDamageKind::Radiation:
            return state.radiation01;
        case EquipmentDamageKind::Wear:
            return state.wear01;
    }
    return 0.0;
}

inline bool addEquipmentDamage(
    EquipmentDamageState& state,
    EquipmentDamageKind kind,
    double amount01
) noexcept
{
    if (!std::isfinite(amount01) || amount01 <= 0.0)
        return false;

    double* target = nullptr;
    switch (kind)
    {
        case EquipmentDamageKind::Laser:
            target = &state.laser01;
            break;
        case EquipmentDamageKind::Impact:
            target = &state.impact01;
            break;
        case EquipmentDamageKind::Thermal:
            target = &state.thermal01;
            break;
        case EquipmentDamageKind::Radiation:
            target = &state.radiation01;
            break;
        case EquipmentDamageKind::Wear:
            target = &state.wear01;
            break;
    }

    if (!target)
        return false;

    const double before = clampDamage01(*target);
    const double after = clampDamage01(before + amount01);
    if (after == before)
        return false;

    *target = after;
    return true;
}

// Normalized functional loss already resolved by each physical cause.
// No second set of weights belongs between these accumulated percentages
// and the engine's output curve. Saturate the SUM, not the individual history.
inline double totalAccumulatedDamage01(
    const EquipmentDamageState& state
) noexcept
{
    return std::clamp(
        clampDamage01(state.laser01) +
        clampDamage01(state.impact01) +
        clampDamage01(state.thermal01) +
        clampDamage01(state.radiation01) +
        clampDamage01(state.wear01),
        0.0,
        1.0
    );
}

// A common function curve, usable with either a generic weighted device
// score or a directly accumulated engine-damage percentage.
inline EquipmentFunctionState evaluateEquipmentFunctionAtDamage01(
    double damage01,
    const EquipmentFailureProfile& failure
) noexcept
{
    EquipmentFunctionState out;
    out.damage01 = clampDamage01(damage01);

    const double threshold =
        std::isfinite(failure.failureAtDamage01)
            ? std::clamp(failure.failureAtDamage01, 0.0, 1.0)
            : 1.0;

    out.failed = out.damage01 + 1.0e-12 >= threshold;
    out.authority01 = out.failed
        ? 0.0
        : std::clamp(1.0 - out.damage01, 0.0, 1.0);
    return out;
}

// Legacy/generic non-engine equipment may still have authored sensitivities.
// Engines consume totalAccumulatedDamage01 directly instead.
inline double weightedFunctionalDamage01(
    const EquipmentDamageLayer& layer
) noexcept
{
    const auto& d = layer.accumulated;
    const auto& w = layer.sensitivity;

    return std::clamp(
        clampDamage01(d.laser01) * std::max(0.0, w.laser) +
        clampDamage01(d.impact01) * std::max(0.0, w.impact) +
        clampDamage01(d.thermal01) * std::max(0.0, w.thermal) +
        clampDamage01(d.radiation01) * std::max(0.0, w.radiation) +
        clampDamage01(d.wear01) * std::max(0.0, w.wear),
        0.0,
        1.0
    );
}

inline EquipmentFunctionState evaluateEquipmentFunction(
    const EquipmentDamageLayer& layer
) noexcept
{
    return evaluateEquipmentFunctionAtDamage01(
        weightedFunctionalDamage01(layer),
        layer.failure
    );
}

inline double functionalAuthority01(
    const EquipmentDamageLayer& layer
) noexcept
{
    return evaluateEquipmentFunction(layer).authority01;
}

} // namespace game::equipment
