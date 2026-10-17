#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"

#include <algorithm>
#include <cmath>

namespace game::ship::plant
{
namespace
{

constexpr double Epsilon = 1.0e-12;

EngineSlotRole activeLongitudinalBank(
    const glm::dvec3& accelerationBody
) noexcept
{
    // Canonical body basis: -Z forward, +Z aft.
    return accelerationBody.z <= 0.0
        ? EngineSlotRole::AftMain
        : EngineSlotRole::ForeMain;
}

double positiveFiniteOr(double value, double fallback) noexcept
{
    return std::isfinite(value) && value > Epsilon
        ? value
        : fallback;
}

std::uint64_t mix64(std::uint64_t value) noexcept
{
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    return value;
}

double deterministicUnitSample(
    std::size_t slotIndex,
    std::uint64_t controlTick
) noexcept
{
    // Hold one sample for four fixed-control ticks so the symptom is a
    // perceptible thrust hiccup rather than high-frequency numerical noise.
    const std::uint64_t bucket = controlTick / 4u;
    const std::uint64_t seed =
        bucket ^
        (0x9e3779b97f4a7c15ULL *
            (static_cast<std::uint64_t>(slotIndex) + 1u));

    const std::uint64_t bits = mix64(seed);
    return static_cast<double>(bits >> 11u) *
        (1.0 / 9007199254740992.0);
}

} // namespace

double instantaneousSlotThrust01(
    const EngineSlotState& state,
    std::size_t slotIndex,
    std::uint64_t controlTick
) noexcept
{
    const double base = effectiveSlotThrust01(state);
    if (base <= Epsilon)
        return 0.0;

    const double irregularity =
        std::clamp(state.thrustIrregularity01, 0.0, 1.0);
    if (irregularity <= Epsilon)
        return base;

    const double sample =
        deterministicUnitSample(slotIndex, controlTick);

    // Irregularity only removes instantaneous thrust. It never fabricates
    // authority above the damage-limited maximum. As damage approaches the
    // 95% failure threshold, dips can approach zero.
    const double multiplier =
        (1.0 - irregularity) + irregularity * sample;

    return base * std::clamp(multiplier, 0.0, 1.0);
}

double evaluateMainPropulsionInstantaneousScale(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick
) noexcept
{
    const auto& slots = physicalControlSlots(model);
    if (state.size() != slots.size())
        return 1.0;

    if (glm::length(mainAccelerationBodyMps2) <= Epsilon)
        return 1.0;

    const EngineSlotRole activeRole =
        activeLongitudinalBank(mainAccelerationBodyMps2);

    double steadyTotal = 0.0;
    double instantaneousTotal = 0.0;

    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        steadyTotal += effectiveSlotThrust01(state[i]);
        instantaneousTotal +=
            instantaneousSlotThrust01(
                state[i],
                i,
                controlTick
            );
    }

    if (steadyTotal <= Epsilon)
        return 1.0;

    return std::clamp(
        instantaneousTotal / steadyTotal,
        0.0,
        1.0
    );
}

MainPropulsionReaction evaluateMainPropulsionReaction(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick
) noexcept
{
    MainPropulsionReaction out;

    const auto& slots = physicalControlSlots(model);
    if (state.size() != slots.size())
        return out;

    if (!std::isfinite(mainAccelerationBodyMps2.x) ||
        !std::isfinite(mainAccelerationBodyMps2.y) ||
        !std::isfinite(mainAccelerationBodyMps2.z))
    {
        return out;
    }

    out.valid = true;

    if (glm::length(mainAccelerationBodyMps2) <= Epsilon)
        return out;

    const EngineSlotRole activeRole =
        activeLongitudinalBank(mainAccelerationBodyMps2);

    double totalWeight = 0.0;
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;
        totalWeight += instantaneousSlotThrust01(
            state[i],
            i,
            controlTick
        );
    }

    if (totalWeight <= Epsilon)
        return out;

    const double massKg =
        positiveFiniteOr(physics.massKg, 1.0);

    const glm::dvec3 configuredHalfExtents =
        physics.physicalActuatorHalfExtentsMeters;
    const bool hasPhysicalGeometry =
        configuredHalfExtents.x > Epsilon &&
        configuredHalfExtents.y > Epsilon &&
        configuredHalfExtents.z > Epsilon;

    // Legacy descriptors/tests that have not yet supplied logical dimensions
    // retain a conservative isotropic fallback. Production capability
    // projection supplies true X/Y/Z half-extents from the descriptor.
    const double legacyLeverArmMeters =
        std::max(0.0, static_cast<double>(physics.turnRadius));
    const glm::dvec3 halfExtentsMeters =
        hasPhysicalGeometry
            ? configuredHalfExtents
            : glm::dvec3(legacyLeverArmMeters);

    const glm::dvec3 centerOfMass =
        physics.centerOfMassLocalMeters;

    glm::dvec3 torque(0.0);

    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].role != activeRole)
            continue;

        const double weight = instantaneousSlotThrust01(
            state[i],
            i,
            controlTick
        );
        if (weight <= Epsilon)
            continue;

        const double fraction = weight / totalWeight;

        const glm::dvec3 applicationPointBodyMeters(
            slots[i].normalizedPositionBody.x * halfExtentsMeters.x,
            slots[i].normalizedPositionBody.y * halfExtentsMeters.y,
            slots[i].normalizedPositionBody.z * halfExtentsMeters.z
        );

        const glm::dvec3 armBodyMeters =
            applicationPointBodyMeters - centerOfMass;

        const glm::dvec3 slotForceBodyNewtons =
            mainAccelerationBodyMps2 * (massKg * fraction);

        torque += glm::cross(
            armBodyMeters,
            slotForceBodyNewtons
        );
    }

    out.torqueBodyNm = torque;

    const double pitchInertia =
        positiveFiniteOr(physics.pitchInertiaKgM2, 1.0);
    const double yawInertia =
        positiveFiniteOr(physics.yawInertiaKgM2, 1.0);
    const double rollInertia =
        positiveFiniteOr(physics.rollInertiaKgM2, 1.0);

    out.angularAccelerationBodyRadPerSec2 = {
        torque.x / pitchInertia,
        torque.y / yawInertia,

        // ShipTransform::rollRate is positive about ship forward, while the
        // canonical body Z axis points aft.
        -torque.z / rollInertia
    };

    return out;
}

} // namespace game::ship::plant
