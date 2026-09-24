#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/world/modules/ObjectModuleState.h"

namespace game::ship
{

namespace detail
{

inline bool propulsionModuleOperational(
    const world::modules::ObjectModuleState* module
) noexcept
{
    return module &&
        module->state ==
            world::modules::ObjectModuleRuntimeState::Attached &&
        module->health > 0.0f;
}

inline bool propulsionBankOperational(
    const std::vector<std::string>& moduleIds,
    const world::modules::ObjectModuleRuntime& runtime
) noexcept
{
    if (moduleIds.empty())
        return true;

    for (const auto& id : moduleIds)
    {
        if (!propulsionModuleOperational(runtime.findModule(id)))
            return false;
    }

    return true;
}

inline bool propulsionModuleOperational(
    const game::simulation::ObjectModuleSnapshot& module
) noexcept
{
    return module.state == static_cast<std::uint8_t>(
               world::modules::ObjectModuleRuntimeState::Attached
           ) &&
        module.health > 0.0f;
}

inline bool propulsionBankOperational(
    const std::vector<std::string>& moduleIds,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
) noexcept
{
    if (moduleIds.empty() || modules.empty())
        return true;

    for (const auto& id : moduleIds)
    {
        const auto it = std::find_if(
            modules.begin(),
            modules.end(),
            [&](const auto& module)
            {
                return module.moduleId == id;
            }
        );

        if (it == modules.end() || !propulsionModuleOperational(*it))
            return false;
    }

    return true;
}

inline ShipParams applyRuntimeMainPropulsionState(
    const ShipDescriptor& descriptor,
    bool aftOperational,
    bool foreOperational
) noexcept
{
    ShipParams effective = descriptor.physics;

    const double staticForwardAuthority =
        game::ship::forwardMainAccelerationLimitMps2(descriptor.physics);
    const double staticReverseAuthority =
        game::ship::reverseMainAccelerationLimitMps2(descriptor.physics);

    effective.forwardMainEngineAvailable =
        descriptor.physics.forwardMainEngineAvailable &&
        aftOperational &&
        staticForwardAuthority > 0.0;
    effective.reverseMainEngineAvailable =
        descriptor.physics.reverseMainEngineAvailable &&
        foreOperational &&
        staticReverseAuthority > 0.0;

    // Main propulsion is deliberately binary. Damage never synthesizes a
    // partially derated main engine: an operational bank retains the complete
    // descriptor rating; a failed bank contributes zero authority.
    effective.forwardMainEngineAccelerationMps2 =
        effective.forwardMainEngineAvailable
            ? static_cast<float>(staticForwardAuthority)
            : 0.0f;
    effective.reverseMainEngineAccelerationMps2 =
        effective.reverseMainEngineAvailable
            ? static_cast<float>(staticReverseAuthority)
            : 0.0f;

    return effective;
}

} // namespace detail

// Server-authoritative effective propulsion. Damageable engine modules are the
// runtime truth; no second health flag exists inside navigation or physics.
inline ShipParams effectiveShipPhysics(
    const ShipDescriptor& descriptor,
    const world::modules::ObjectModuleRuntime& runtime
) noexcept
{
    return detail::applyRuntimeMainPropulsionState(
        descriptor,
        detail::propulsionBankOperational(
            descriptor.mainPropulsion.aftEngineModuleIds,
            runtime
        ),
        detail::propulsionBankOperational(
            descriptor.mainPropulsion.foreEngineModuleIds,
            runtime
        )
    );
}

// Client/planning equivalent derived from replicated authoritative module state.
// An entirely absent graph retains descriptor-default health so startup/sparse
// graph publication cannot fabricate a failure before the first module payload.
inline ShipParams effectiveShipPhysics(
    const ShipDescriptor& descriptor,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
) noexcept
{
    return detail::applyRuntimeMainPropulsionState(
        descriptor,
        detail::propulsionBankOperational(
            descriptor.mainPropulsion.aftEngineModuleIds,
            modules
        ),
        detail::propulsionBankOperational(
            descriptor.mainPropulsion.foreEngineModuleIds,
            modules
        )
    );
}

} // namespace game::ship
