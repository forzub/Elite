#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

#include <glm/glm.hpp>

#include "src/game/equipment/engine/EngineUnit.h"

namespace game::equipment::engine
{
// Connection to a REAL installed engine. Slot/layout never owns its thrust,
// thermodynamics, state or identity. References are valid for one control tick.
struct MountedEngine
{
    const EngineUnitDescriptor* passport = nullptr;
    const game::equipment::EquipmentDamageLayer* damage = nullptr;
    const EngineOperatingState* operating = nullptr;
    std::string_view moduleId {};
    bool attached = false;
};

struct EngineCommand
{
    // Pilot/autopilot demand as a fraction of this engine's AVAILABLE thrust.
    // The allocator may decrease this for torque balancing.
    double throttle01 = 0.0;
    glm::dvec3 directionBody {0.0, 0.0, -1.0};
    std::uint64_t controlTick = 0;
};

struct EngineOutput
{
    glm::dvec3 forceBodyNewtons {0.0};
    double availableThrustNewtons = 0.0;
    double deliveredThrustNewtons = 0.0;
    double deliveredNominalThrust01 = 0.0;
    double consumedElectricalMW = 0.0;
    double generatedWasteHeatMW = 0.0;
    double availableGimbalRangeRad = 0.0;
    double availableGimbalRateRadPerSec = 0.0;
};

// Generic engine actuator: NO Cobra constants and NO ship-control topology.
// Each mounted Engine is a short-lived typed view of per-installation data;
// its damage, age and temperature persist in ObjectModuleRuntime.
class Engine
{
public:
    explicit Engine(MountedEngine mounted) noexcept
        : m_mounted(mounted)
    {}

    [[nodiscard]] double ratedThrustNewtons() const noexcept
    {
        return m_mounted.passport
            ? std::max(0.0, m_mounted.passport->nominalThrustNewtons)
            : 0.0;
    }

    [[nodiscard]] double availableThrustNewtons() const noexcept
    {
        if (!valid())
            return 0.0;
        const auto effective = evaluateEngineUnit(
            *m_mounted.passport, *m_mounted.damage
        );
        return ratedThrustNewtons() * safeEmitterThrustFraction01(
            *m_mounted.passport, effective, *m_mounted.operating
        );
    }

    [[nodiscard]] double instantaneousAvailableThrustNewtons(
        std::uint64_t tick
    ) const noexcept
    {
        if (!valid())
            return 0.0;
        const auto effective = evaluateEngineUnit(
            *m_mounted.passport, *m_mounted.damage
        );
        const double base = ratedThrustNewtons() * safeEmitterThrustFraction01(
            *m_mounted.passport, effective, *m_mounted.operating
        );
        if (base <= 0.0)
            return 0.0;

        // Stable per-installed-engine intermittent output. A time sample is
        // held for four control ticks, matching prior gameplay cadence.
        const double irregular = finiteClamp01(
            effective.thrustIrregularity01
        );
        if (irregular <= 0.0)
            return base;
        std::uint64_t seed = 14695981039346656037ULL;
        for (char ch : m_mounted.moduleId)
        {
            seed ^= static_cast<unsigned char>(ch);
            seed *= 1099511628211ULL;
        }
        seed ^= (tick / 4u) * 0x9e3779b97f4a7c15ULL;
        seed ^= seed >> 30u;
        seed *= 0xbf58476d1ce4e5b9ULL;
        seed ^= seed >> 27u;
        seed *= 0x94d049bb133111ebULL;
        seed ^= seed >> 31u;
        const double sample = static_cast<double>(seed >> 11u) *
            (1.0 / 9007199254740992.0);
        return base * ((1.0 - irregular) + irregular * sample);
    }

    [[nodiscard]] EngineOutput execute(
        const EngineCommand& command
    ) const noexcept
    {
        EngineOutput out;
        if (!valid())
            return out;
        const auto effective = evaluateEngineUnit(
            *m_mounted.passport, *m_mounted.damage
        );
        out.availableThrustNewtons = availableThrustNewtons();
        out.availableGimbalRangeRad = effective.gimbalRangeRad;
        out.availableGimbalRateRadPerSec = effective.gimbalRateRadPerSec;
        const double delivered = instantaneousAvailableThrustNewtons(
            command.controlTick
        ) * finiteClamp01(command.throttle01);

        const double directionLength = glm::length(command.directionBody);
        if (!std::isfinite(directionLength) || directionLength <= 1.0e-12)
            return out;
        const glm::dvec3 direction = command.directionBody / directionLength;
        out.deliveredThrustNewtons = delivered;
        out.deliveredNominalThrust01 = ratedThrustNewtons() > 0.0
            ? finiteClamp01(delivered / ratedThrustNewtons())
            : 0.0;
        out.forceBodyNewtons = direction * delivered;
        out.consumedElectricalMW = requestedElectricalPowerMW(
            *m_mounted.passport, effective, out.deliveredNominalThrust01
        );
        out.generatedWasteHeatMW = generatedWasteHeatMW(
            *m_mounted.passport, effective, out.deliveredNominalThrust01
        );
        return out;
    }

private:
    [[nodiscard]] bool valid() const noexcept
    {
        return m_mounted.attached &&
            m_mounted.passport && m_mounted.damage &&
            m_mounted.operating;
    }

    MountedEngine m_mounted;
};
} // namespace game::equipment::engine
