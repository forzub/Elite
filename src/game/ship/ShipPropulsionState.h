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

inline double operationalFraction(
    const std::vector<std::string>& moduleIds,
    const world::modules::ObjectModuleRuntime& runtime
) noexcept
{
    if (moduleIds.empty())
        return 1.0;

    std::size_t operational = 0;
    for (const auto& id : moduleIds)
    {
        if (propulsionModuleOperational(runtime.findModule(id)))
            ++operational;
    }

    return static_cast<double>(operational) /
        static_cast<double>(moduleIds.size());
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

inline double operationalFraction(
    const std::vector<std::string>& moduleIds,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
) noexcept
{
    if (moduleIds.empty() || modules.empty())
        return 1.0;

    std::size_t operational = 0;
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

        if (it != modules.end() && propulsionModuleOperational(*it))
            ++operational;
    }

    return static_cast<double>(operational) /
        static_cast<double>(moduleIds.size());
}

inline ShipParams applyRuntimeMainPropulsionFractions(
    const ShipDescriptor& descriptor,
    double aftFraction,
    double foreFraction
) noexcept
{
    ShipParams effective = descriptor.physics;

    const double staticForwardAuthority =
        game::ship::forwardMainAccelerationLimitMps2(descriptor.physics);
    const double staticReverseAuthority =
        game::ship::reverseMainAccelerationLimitMps2(descriptor.physics);

    aftFraction = std::clamp(aftFraction, 0.0, 1.0);
    foreFraction = std::clamp(foreFraction, 0.0, 1.0);

    effective.forwardMainEngineAvailable =
        descriptor.physics.forwardMainEngineAvailable &&
        aftFraction > 0.0 &&
        staticForwardAuthority > 0.0;
    effective.reverseMainEngineAvailable =
        descriptor.physics.reverseMainEngineAvailable &&
        foreFraction > 0.0 &&
        staticReverseAuthority > 0.0;

    // Store the already damage-scaled rating explicitly. The shared dynamics
    // helper still applies the crew/structure load cap, so damage can reduce
    // authority but can never increase it.
    effective.forwardMainEngineAccelerationMps2 =
        effective.forwardMainEngineAvailable
            ? static_cast<float>(staticForwardAuthority * aftFraction)
            : 0.0f;
    effective.reverseMainEngineAccelerationMps2 =
        effective.reverseMainEngineAvailable
            ? static_cast<float>(staticReverseAuthority * foreFraction)
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
    return detail::applyRuntimeMainPropulsionFractions(
        descriptor,
        detail::operationalFraction(
            descriptor.mainPropulsion.aftEngineModuleIds,
            runtime
        ),
        detail::operationalFraction(
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
    return detail::applyRuntimeMainPropulsionFractions(
        descriptor,
        detail::operationalFraction(
            descriptor.mainPropulsion.aftEngineModuleIds,
            modules
        ),
        detail::operationalFraction(
            descriptor.mainPropulsion.foreEngineModuleIds,
            modules
        )
    );
}

} // namespace game::ship
