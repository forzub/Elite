#pragma once

// ONE switch-off law for badly damaged main engines, used by the server's
// physical executor (which engines may fire) and by every capability
// projection (server and client), so all of them see the same bank.
//
// A main engine damaged beyond 94 % is switched off while another engine
// of the SAME bank (aft or fore mains) is still within that limit: a few
// percent of irregular thrust only forces the bank allocator to balance
// the healthy engine down to it (or the mixer to throttle everything to
// it). If no such engine remains, the bank's engines keep the thrust they
// really deliver.
//
// The damage is evaluated on float-rounded components, exactly as the
// module snapshot replicates them, so the server and a client never take
// different decisions near the limit.

#include <cstddef>
#include <vector>

#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::plant
{

inline constexpr double MainEngineCutoffDamage01 = 0.94;

[[nodiscard]] inline double replicatedTotalDamage01(
    float laser01, float impact01, float thermal01,
    float radiation01, float wear01, float internalFailure01
) noexcept
{
    game::equipment::EquipmentDamageState state;
    state.laser01 = laser01;
    state.impact01 = impact01;
    state.thermal01 = thermal01;
    state.radiation01 = radiation01;
    state.wear01 = wear01;
    state.internalFailure01 = internalFailure01;
    return game::equipment::totalAccumulatedDamage01(state);
}

[[nodiscard]] inline double replicatedTotalDamage01(
    const game::equipment::EquipmentDamageState& state
) noexcept
{
    return replicatedTotalDamage01(
        static_cast<float>(state.laser01),
        static_cast<float>(state.impact01),
        static_cast<float>(state.thermal01),
        static_cast<float>(state.radiation01),
        static_cast<float>(state.wear01),
        static_cast<float>(state.internalFailure01));
}

struct MainEngineCutoffInput
{
    EngineSlotRole role = EngineSlotRole::ManoeuvreCluster;
    bool attached = false;
    double damage01 = 0.0;
};

// true = this slot's main engine is switched off.
[[nodiscard]] inline std::vector<bool> mainEngineCutoff(
    const std::vector<MainEngineCutoffInput>& slots
)
{
    std::vector<bool> off(slots.size(), false);
    for (const auto bank : {EngineSlotRole::AftMain, EngineSlotRole::ForeMain})
    {
        bool usableRemains = false;
        for (const auto& s : slots)
            usableRemains |= s.role == bank && s.attached &&
                s.damage01 <= MainEngineCutoffDamage01;
        if (!usableRemains)
            continue;
        for (std::size_t i = 0; i < slots.size(); ++i)
            off[i] = off[i] || (slots[i].role == bank && slots[i].attached &&
                                slots[i].damage01 > MainEngineCutoffDamage01);
    }
    return off;
}

} // namespace game::ship::plant
