#pragma once

// The J / jump field integrity of a ship from its installed modules (author 2026-11-10, ACCEPTED emitter damage
// rule, JDrive.h jFieldFactor): emitters = modules "*jump_field_emitter*", the core = "ship_jump_core". A module
// counts as intact while attached (not destroyed / detached / disabled) with health left.

#include "src/game/navigation/rules/JDrive.h"
#include "src/world/modules/ObjectModuleRuntime.h"

namespace game::simulation
{

inline game::navigation::jdrive::JFieldIntegrity jFieldIntegrity(const world::modules::ObjectModuleRuntime& runtime)
{
    game::navigation::jdrive::JFieldIntegrity j;
    for (const auto& m : runtime.modules())
    {
        const bool intact = m.state == world::modules::ObjectModuleRuntimeState::Attached && m.health > 0.0f;
        if (m.moduleId.find("jump_field_emitter") != std::string::npos)
        {
            ++j.emittersInstalled;
            j.emittersIntact += intact ? 1 : 0;
        }
        else if (m.moduleId == "ship_jump_core")
            j.coreIntact = intact;
    }
    return j;
}

} // namespace game::simulation
