#pragma once

#include <cstdint>

namespace game::shared
{

/*
    Execution placement for deterministic spatial calculations.

    This is deliberately NOT EntityRuntimeContract::authority. Physical game
    state may remain server-authoritative while an expensive deterministic
    route/prediction calculation is executed on the only interested client.

    The participant count must describe a server-resolved consistency domain
    (Hub/local encounter/formation/etc.), never whatever actors happen to be
    visible in a client's render or replication-interest set.
*/
enum class SpatialComputationPlacement : std::uint8_t
{
    ClientLocal = 0,
    ServerShared
};

struct SpatialComputationContext
{
    // Human-controlled participants whose observations/actions must agree on
    // the result in this spatial consistency domain. Interactive client work
    // normally has at least one participant: the requesting player.
    std::uint32_t humanParticipantCount = 1;

    // Security, persistence, ATC/fleet ownership or another rule may require
    // server execution even for one participant.
    bool serverExecutionRequired = false;

    // A client without the required module/data/capability cannot own the
    // calculation even when it is alone.
    bool clientExecutionAvailable = true;
};

constexpr SpatialComputationPlacement chooseSpatialComputationPlacement(
    const SpatialComputationContext& context
) noexcept
{
    if (context.serverExecutionRequired ||
        !context.clientExecutionAvailable ||
        context.humanParticipantCount > 1)
    {
        return SpatialComputationPlacement::ServerShared;
    }

    return SpatialComputationPlacement::ClientLocal;
}

} // namespace game::shared
