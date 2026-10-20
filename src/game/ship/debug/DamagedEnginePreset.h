#pragma once

#include <cstddef>
#include <string_view>

#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::debug
{

// Explicit opt-in diagnostic only. The server applies this percentage to
// ONE real installed aft-main engine via ObjectModuleRuntime, not to a
// simulated HUD value, planner coefficient, or autopilot control surface.
enum class DamagedEnginePreset
{
    None,
    Aft50,
    Aft80,
    AftFailed
};

[[nodiscard]] constexpr DamagedEnginePreset parseDamagedEnginePreset(
    std::string_view text
) noexcept
{
    return text == "aft-50" ? DamagedEnginePreset::Aft50 :
           text == "aft-80" ? DamagedEnginePreset::Aft80 :
           text == "aft-failed" ? DamagedEnginePreset::AftFailed :
           DamagedEnginePreset::None;
}

[[nodiscard]] constexpr double engineDamageForPreset(
    DamagedEnginePreset preset
) noexcept
{
    switch (preset)
    {
        case DamagedEnginePreset::Aft50: return 0.50;
        case DamagedEnginePreset::Aft80: return 0.80;
        case DamagedEnginePreset::AftFailed: return 1.00;
        case DamagedEnginePreset::None: break;
    }
    return 0.0;
}

[[nodiscard]] constexpr const char* damagedEnginePresetName(
    DamagedEnginePreset preset
) noexcept
{
    switch (preset)
    {
        case DamagedEnginePreset::Aft50: return "aft-50";
        case DamagedEnginePreset::Aft80: return "aft-80";
        case DamagedEnginePreset::AftFailed: return "aft-failed";
        case DamagedEnginePreset::None: break;
    }
    return "none";
}

// Select the first real aft bank, independent of I/Y/X count and of
// module-descriptor order. Caller resolves this topology index to the
// descriptor's slotModuleIds and then to the server-owned module instance.
[[nodiscard]] inline std::size_t firstAftMainSlot(
    game::ship::plant::PhysicalControlModelId model
) noexcept
{
    using namespace game::ship::plant;
    const auto& slots = physicalControlSlots(model);
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (slots[i].role == EngineSlotRole::AftMain)
            return i;
    return slots.size();
}

} // namespace game::ship::debug
