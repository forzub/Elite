#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/capability/ShipCapabilitySnapshot.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/world/modules/ObjectModuleState.h"

namespace game::ship::capability
{
namespace detail
{

inline const ModuleDescriptor* findModuleDescriptor(
    const ShipDescriptor& descriptor,
    const std::string& moduleId
) noexcept
{
    const auto it = std::find_if(
        descriptor.modules.begin(),
        descriptor.modules.end(),
        [&](const auto& module)
        {
            return module.moduleId == moduleId;
        }
    );
    return it == descriptor.modules.end() ? nullptr : &*it;
}

inline double healthFraction(float health, float maxHealth) noexcept
{
    if (!std::isfinite(health) ||
        !std::isfinite(maxHealth) ||
        maxHealth <= 0.0f)
    {
        return 0.0;
    }

    return std::clamp(
        static_cast<double>(health) /
            static_cast<double>(maxHealth),
        0.0,
        1.0
    );
}

inline plant::EngineSlotState healthySlot() noexcept
{
    return plant::EngineSlotState {
        true,
        1.0,
        1.0
    };
}

inline plant::EngineSlotState slotFromModule(
    const ModuleDescriptor* definition,
    const world::modules::ObjectModuleState* module
) noexcept
{
    using State = world::modules::ObjectModuleRuntimeState;

    if (!module ||
        module->state != State::Attached ||
        module->health <= 0.0f)
    {
        return plant::EngineSlotState {
            false,
            0.0,
            0.0
        };
    }

    const double health01 =
        healthFraction(module->health, module->maxHealth);

    if (definition && definition->engine)
    {
        const auto effective =
            game::equipment::engine::evaluateEngineUnit(
                *definition->engine,
                module->equipmentDamage
            );

        return plant::EngineSlotState {
            true,
            effective.efficiencyAuthority01,
            effective.gimbalAuthority01,
            effective.thrustIrregularity01
        };
    }

    // Structural/non-migrated modules keep their legacy health projection.
    return plant::EngineSlotState {
        health01 > 0.0,
        health01,
        health01
    };
}

inline plant::EngineSlotState slotFromSnapshot(
    const ShipDescriptor& descriptor,
    const game::simulation::ObjectModuleSnapshot* module
) noexcept
{
    using State = world::modules::ObjectModuleRuntimeState;

    if (!module ||
        module->state != static_cast<std::uint8_t>(State::Attached) ||
        module->health <= 0.0f)
    {
        return plant::EngineSlotState {
            false,
            0.0,
            0.0
        };
    }

    const ModuleDescriptor* definition =
        findModuleDescriptor(descriptor, module->moduleId);

    const float maxHealth =
        definition ? definition->maxHealth : 100.0f;

    const double health01 =
        healthFraction(module->health, maxHealth);

    if (definition && definition->engine)
    {
        game::equipment::EquipmentDamageLayer damage;
        damage.sensitivity =
            definition->equipmentDamageSensitivity;
        damage.failure =
            definition->equipmentFailure;
        damage.accumulated.laser01 = module->laserDamage01;
        damage.accumulated.impact01 = module->impactDamage01;
        damage.accumulated.thermal01 = module->thermalDamage01;
        damage.accumulated.radiation01 = module->radiationDamage01;
        damage.accumulated.wear01 = module->wearDamage01;

        const auto effective =
            game::equipment::engine::evaluateEngineUnit(
                *definition->engine,
                damage
            );

        return plant::EngineSlotState {
            true,
            effective.efficiencyAuthority01,
            effective.gimbalAuthority01,
            effective.thrustIrregularity01
        };
    }

    return plant::EngineSlotState {
        health01 > 0.0,
        health01,
        health01
    };
}

inline std::vector<plant::EngineSlotState> slotStates(
    const ShipDescriptor& descriptor,
    const world::modules::ObjectModuleRuntime& runtime
)
{
    const auto& topology =
        plant::physicalControlSlots(descriptor.physicalControlModel);
    const auto& binding =
        descriptor.physicalControl.slotModuleIds;

    if (binding.empty())
        return plant::healthySlotState(topology);

    if (binding.size() != topology.size())
        return {};

    std::vector<plant::EngineSlotState> out;
    out.reserve(topology.size());

    for (const auto& moduleId : binding)
    {
        if (moduleId.empty())
        {
            out.push_back(healthySlot());
            continue;
        }

        out.push_back(
            slotFromModule(
                findModuleDescriptor(descriptor, moduleId),
                runtime.findModule(moduleId)
            )
        );
    }

    return out;
}

inline std::vector<plant::EngineSlotState> slotStates(
    const ShipDescriptor& descriptor,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
)
{
    const auto& topology =
        plant::physicalControlSlots(descriptor.physicalControlModel);
    const auto& binding =
        descriptor.physicalControl.slotModuleIds;

    // Sparse/no graph at startup must not fabricate engine failure.
    if (binding.empty() || modules.empty())
        return plant::healthySlotState(topology);

    if (binding.size() != topology.size())
        return {};

    std::vector<plant::EngineSlotState> out;
    out.reserve(topology.size());

    for (const auto& moduleId : binding)
    {
        if (moduleId.empty())
        {
            out.push_back(healthySlot());
            continue;
        }

        const auto it = std::find_if(
            modules.begin(),
            modules.end(),
            [&](const auto& module)
            {
                return module.moduleId == moduleId;
            }
        );

        out.push_back(
            slotFromSnapshot(
                descriptor,
                it == modules.end() ? nullptr : &*it
            )
        );
    }

    return out;
}

inline ControlHealthState controlHealth(
    plant::RuntimeFlightRegime regime
) noexcept
{
    using Regime = plant::RuntimeFlightRegime;

    switch (regime)
    {
        case Regime::FullAssisted:
        case Regime::FullNewtonian:
            return ControlHealthState::Normal;

        case Regime::DegradedAssisted:
        case Regime::DegradedNewtonian:
        case Regime::ThrustAxisFallback:
            return ControlHealthState::Degraded;

        case Regime::SurvivalControl:
            return ControlHealthState::Survival;

        case Regime::Uncontrollable:
        default:
            return ControlHealthState::Uncontrollable;
    }
}

inline ShipCapabilitySnapshot makeCapability(
    const plant::PhysicalControlSnapshot& physical
) noexcept
{
    ShipCapabilitySnapshot out;
    if (!physical.valid)
        return out;

    out.valid = true;
    out.controlState = controlHealth(physical.regime);
    out.effectivePhysics = physical.effectivePhysics;

    const auto& p = out.effectivePhysics;
    out.longitudinalAccelerationMps2 = {
        game::ship::forwardMainAccelerationLimitMps2(p),
        game::ship::reverseMainAccelerationLimitMps2(p)
    };

    const bool assisted =
        plant::familyOf(physical.model) ==
        plant::PhysicalControlFamily::Assisted;

    const double lateral = assisted
        ? game::ship::assistedLateralStabilizationAccelerationLimitMps2(p)
        : game::ship::manoeuvreAccelerationLimitMps2(p);

    out.strafeAccelerationMps2 = {lateral, lateral};
    out.liftAccelerationMps2 = {lateral, lateral};

    const double angular =
        game::ship::angularAccelerationLimitRadPerSec2(p);
    out.pitchAccelerationRadPerSec2 = {angular, angular};
    out.yawAccelerationRadPerSec2 = {angular, angular};
    out.rollAccelerationRadPerSec2 = {angular, angular};

    const double pitchRate =
        game::ship::pitchRateLimitRadPerSec(p);
    const double yawRate =
        game::ship::yawRateLimitRadPerSec(p);
    const double rollRate =
        game::ship::rollRateLimitRadPerSec(p);

    out.pitchRateRadPerSec = {pitchRate, pitchRate};
    out.yawRateRadPerSec = {yawRate, yawRate};
    out.rollRateRadPerSec = {rollRate, rollRate};

    return out;
}

inline void applyDescriptorActuatorGeometry(
    const ShipDescriptor& descriptor,
    ShipCapabilitySnapshot& capability
) noexcept
{
    if (!capability.valid)
        return;

    const auto& dims = descriptor.logicalDimensionsValue;
    if (dims.enabled &&
        dims.width > 0.0f &&
        dims.height > 0.0f &&
        dims.length > 0.0f)
    {
        capability.effectivePhysics.physicalActuatorHalfExtentsMeters = {
            static_cast<double>(dims.width) * 0.5,
            static_cast<double>(dims.height) * 0.5,
            static_cast<double>(dims.length) * 0.5
        };
    }
}

} // namespace detail

// State-driven projection for a construction selected by ShipState. The
// descriptor supplies immutable module bindings/ratings only; the per-instance
// construction model is supplied explicitly.
inline std::vector<plant::EngineSlotState> buildPhysicalControlSlotState(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model,
    const world::modules::ObjectModuleRuntime& runtime
)
{
    const auto& topology = plant::physicalControlSlots(model);
    const auto& binding = descriptor.physicalControl.slotModuleIds;

    if (binding.empty())
        return plant::healthySlotState(topology);

    if (binding.size() != topology.size())
        return {};

    std::vector<plant::EngineSlotState> out;
    out.reserve(topology.size());
    for (const auto& moduleId : binding)
    {
        if (moduleId.empty())
            out.push_back(detail::healthySlot());
        else
            out.push_back(
                detail::slotFromModule(
                    detail::findModuleDescriptor(descriptor, moduleId),
                    runtime.findModule(moduleId)
                )
            );
    }
    return out;
}

inline std::vector<plant::EngineSlotState> buildPhysicalControlSlotState(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
)
{
    const auto& topology = plant::physicalControlSlots(model);
    const auto& binding = descriptor.physicalControl.slotModuleIds;

    if (binding.empty() || modules.empty())
        return plant::healthySlotState(topology);

    if (binding.size() != topology.size())
        return {};

    std::vector<plant::EngineSlotState> out;
    out.reserve(topology.size());
    for (const auto& moduleId : binding)
    {
        if (moduleId.empty())
        {
            out.push_back(detail::healthySlot());
            continue;
        }

        const auto it = std::find_if(
            modules.begin(),
            modules.end(),
            [&](const auto& module)
            {
                return module.moduleId == moduleId;
            }
        );
        out.push_back(
            detail::slotFromSnapshot(
                descriptor,
                it == modules.end() ? nullptr : &*it
            )
        );
    }
    return out;
}

inline ShipCapabilitySnapshot buildShipCapability(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model,
    const std::vector<plant::EngineSlotState>& state
)
{
    if (state.size() != plant::physicalControlSlots(model).size())
        return {};

    auto out = detail::makeCapability(
        plant::evaluatePhysicalControlModel(
            model,
            descriptor.physics,
            state
        )
    );
    detail::applyDescriptorActuatorGeometry(descriptor, out);
    return out;
}

// Public per-slot capability projection for diagnostics/presentation and the
// future actuator allocator. The physical topology remains the source of slot
// count/order; consumers must not reconstruct engine health from ad-hoc banks.
inline std::vector<plant::EngineSlotState> buildPhysicalControlSlotState(
    const ShipDescriptor& descriptor,
    const world::modules::ObjectModuleRuntime& runtime
)
{
    return detail::slotStates(descriptor, runtime);
}

inline std::vector<plant::EngineSlotState> buildPhysicalControlSlotState(
    const ShipDescriptor& descriptor,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
)
{
    return detail::slotStates(descriptor, modules);
}

inline ShipCapabilitySnapshot buildShipCapability(
    const ShipDescriptor& descriptor,
    const world::modules::ObjectModuleRuntime& runtime
)
{
    const auto state = detail::slotStates(descriptor, runtime);
    if (state.empty() &&
        !plant::physicalControlSlots(
            descriptor.physicalControlModel
        ).empty())
    {
        return {};
    }

    auto out = detail::makeCapability(
        plant::evaluatePhysicalControlModel(
            descriptor.physicalControlModel,
            descriptor.physics,
            state
        )
    );
    detail::applyDescriptorActuatorGeometry(descriptor, out);
    return out;
}

inline ShipCapabilitySnapshot buildShipCapability(
    const ShipDescriptor& descriptor,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
)
{
    const auto state = detail::slotStates(descriptor, modules);
    if (state.empty() &&
        !plant::physicalControlSlots(
            descriptor.physicalControlModel
        ).empty())
    {
        return {};
    }

    auto out = detail::makeCapability(
        plant::evaluatePhysicalControlModel(
            descriptor.physicalControlModel,
            descriptor.physics,
            state
        )
    );
    detail::applyDescriptorActuatorGeometry(descriptor, out);
    return out;
}

inline ShipCapabilitySnapshot buildHealthyShipCapability(
    const ShipDescriptor& descriptor
)
{
    const auto& topology =
        plant::physicalControlSlots(
            descriptor.physicalControlModel
        );

    auto out = detail::makeCapability(
        plant::evaluatePhysicalControlModel(
            descriptor.physicalControlModel,
            descriptor.physics,
            plant::healthySlotState(topology)
        )
    );
    detail::applyDescriptorActuatorGeometry(descriptor, out);
    return out;
}

} // namespace game::ship::capability
