#pragma once

#include <algorithm>
#include <cstdint>

#include "src/scene/EntityID.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/ShipRegistry.h"
#include "src/game/ship/ShipVisualIdentity.h"
#include "src/game/ship/core/ShipTransform.h"

namespace game::simulation
{

enum class ShipSimulationTier : std::uint8_t
{
    TrafficShell = 0,
    MaterializedShip,
    Derelict,
    WreckShell
};

enum class ShipActorKind : std::uint8_t
{
    None = 0,
    Player,
    NpcPilot,
    TrafficAutopilot,
    AutonomousDrone
};

struct CoarseShipDamageState
{
    double structuralIntegrity01 = 1.0;
    double mainThrustAuthority01 = 1.0;
    double mainVectoringAuthority01 = 1.0;
    double manoeuvreAuthority01 = 1.0;
    std::uint64_t revision = 0;

    static double clamp01(double value) noexcept
    {
        return std::clamp(value, 0.0, 1.0);
    }

    void set(
        double structure,
        double mainThrust,
        double mainVectoring,
        double manoeuvre
    ) noexcept
    {
        structuralIntegrity01 = clamp01(structure);
        mainThrustAuthority01 = clamp01(mainThrust);
        mainVectoringAuthority01 = clamp01(mainVectoring);
        manoeuvreAuthority01 = clamp01(manoeuvre);
        ++revision;
    }
};

struct ShipSimulationLifecycleState
{
    ShipSimulationTier tier = ShipSimulationTier::MaterializedShip;
    ShipActorKind actorKind = ShipActorKind::NpcPilot;
    bool pilotPresent = true;

    // Zero means no formation ownership. These fields are orthogonal to tier:
    // a materialized ship may remain in a formation until an event removes it.
    std::uint32_t formationId = 0;
    std::uint16_t formationSlot = 0;

    std::uint64_t revision = 0;

    [[nodiscard]] bool acceptsAiControl() const noexcept
    {
        return
            tier == ShipSimulationTier::MaterializedShip &&
            pilotPresent &&
            actorKind != ShipActorKind::None;
    }

    void materializeForInteraction() noexcept
    {
        tier = ShipSimulationTier::MaterializedShip;
        if (actorKind == ShipActorKind::TrafficAutopilot)
            actorKind = ShipActorKind::NpcPilot;
        pilotPresent = true;
        ++revision;
    }

    void onPilotEjected() noexcept
    {
        tier = ShipSimulationTier::Derelict;
        actorKind = ShipActorKind::None;
        pilotPresent = false;
        formationId = 0;
        formationSlot = 0;
        ++revision;
    }

    void dematerializeBackground() noexcept
    {
        tier = pilotPresent
            ? ShipSimulationTier::TrafficShell
            : ShipSimulationTier::WreckShell;
        ++revision;
    }
};

// Lightweight authoritative entity. It deliberately contains no ShipCore,
// module runtime, reactor/thermal systems, hit volumes or autopilot state.
struct ShipShellState
{
    EntityId id {};
    const ShipDescriptor* definition = nullptr;

    ShipVisualIdentity visual;
    ShipRegistry registry;
    ShipTransform transform;

    ShipSimulationLifecycleState lifecycle;
    CoarseShipDamageState damage;

    [[nodiscard]] bool valid() const noexcept
    {
        return id.value != 0 &&
            definition != nullptr &&
            (lifecycle.tier == ShipSimulationTier::TrafficShell ||
             lifecycle.tier == ShipSimulationTier::WreckShell);
    }
};

} // namespace game::simulation
