#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/capability/ShipCapabilitySnapshot.h"
#include "src/game/ship/capability/WorkingThrustAxisAnalysis.h"
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
                module->equipmentDamage,
                module->engineOperating
            );

        return plant::EngineSlotState {
            true,
            game::equipment::engine::safeEmitterThrustFraction01(
                *definition->engine,
                effective,
                module->engineOperating
            ),
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
        damage.accumulated.internalFailure01 = module->internalFailureDamage01;

        // Snapshot projection must use exactly the same thermal governor as
        // the authoritative module path. Without this, damaged annulus state
        // changes acceleration/braking only on the server and the remote ship
        // predictor/autopilot sees a fictitious healthy engine.
        game::equipment::engine::EngineOperatingState operating;
        if (module->hasEngineThermalState)
        {
            operating.temperatureK = module->engineTemperatureK;
            operating.coolantTemperatureK =
                module->engineCoolantTemperatureK;
            operating.impactShock01 =
                module->engineImpactShock01;
            operating.impactShockSecondsRemaining =
                module->engineImpactShockSecondsRemaining;
            operating.emitter.surface.intactArea01 =
                module->emitterIntactArea01;
            operating.emitter.surface.microcrack01 =
                module->emitterMicrocrack01;
            operating.emitter.damage.laser01 =
                module->emitterLaserDamage01;
            operating.emitter.damage.impact01 =
                module->emitterImpactDamage01;
            operating.emitter.damage.thermal01 =
                module->emitterThermalDamage01;
            operating.emitter.damage.radiation01 =
                module->emitterRadiationDamage01;
            operating.emitter.damage.wear01 =
                module->emitterWearDamage01;
            operating.emitter.equivalentOperatingHours =
                module->emitterEquivalentOperatingHours;
        }
        const auto effective=
            game::equipment::engine::evaluateEngineUnit(
                *definition->engine,damage,operating
            );
        return plant::EngineSlotState {
            true,
            game::equipment::engine::safeEmitterThrustFraction01(
                *definition->engine, effective, operating
            ),
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

// Project engine TYPE ratings and per-installed-engine availability into
// the planner/control envelope. This is only a capability SUMMARY: execution
// itself calls each real Engine and sums its output force/torque in newtons.
// The slot array supplies location and ID binding, never engine constants.
inline void applyMountedEngineAxialAuthority(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model,
    const std::vector<plant::EngineSlotState>& projected,
    ShipCapabilitySnapshot& capability
) noexcept
{
    if (!capability.valid)
        return;
    const auto& topology = plant::physicalControlSlots(model);
    const auto& bindings = descriptor.physicalControl.slotModuleIds;
    if (bindings.size() != topology.size() ||
        projected.size() != topology.size())
        return;

    double aftThrustNewtons = 0.0;
    double foreThrustNewtons = 0.0;
    double weakestAftNewtons = std::numeric_limits<double>::infinity();
    double weakestForeNewtons = std::numeric_limits<double>::infinity();
    std::size_t survivingAft = 0;
    std::size_t survivingFore = 0;
     bool foundAft = false;
    bool foundFore = false;
    for (std::size_t i = 0; i < topology.size(); ++i)
    {
        const auto role = topology[i].role;
        if (role == plant::EngineSlotRole::ManoeuvreCluster)
            continue;
        const auto* installed =
            detail::findModuleDescriptor(descriptor, bindings[i]);
        if (!installed || !installed->engine)
            return; // Legacy/diagnostic model, preserve prior envelope.
        const double ratedN = std::max(
            0.0, installed->engine->nominalThrustNewtons
        );
        const double availableN =
            ratedN * plant::effectiveSlotThrust01(projected[i]);
        if (role == plant::EngineSlotRole::AftMain)
        {
            aftThrustNewtons += availableN;
            foundAft = true;
            if (availableN > 1.0e-12)
            {
                ++survivingAft;
                 weakestAftNewtons = std::min(
                    weakestAftNewtons, availableN
                );
            }
        }
        else if (role == plant::EngineSlotRole::ForeMain)
        {
            foreThrustNewtons += availableN;
            foundFore = true;
            if (availableN > 1.0e-12)
            {
                ++survivingFore;
                weakestForeNewtons = std::min(
                    weakestForeNewtons, availableN
                );
            }
        }
    }

    // The physical main allocator balances every surviving motor down to
    // the weakest rated available FORCE in its bank. Planner envelopes must
    // use that same sum of balanced forces, otherwise autopilot over-promises
    // acceleration in asymmetric damage or mixed motor configurations.
    if (survivingAft > 1)
        aftThrustNewtons =
            weakestAftNewtons * static_cast<double>(survivingAft);
    if (survivingFore > 1)
        foreThrustNewtons =
            weakestForeNewtons * static_cast<double>(survivingFore);

    // All layouts share one geometry-only physical feasibility provider.
    // A healthy or throttle-balanceable bank retains the factory nose axis;
    // after asymmetrical damage a candidate single-motor CoM ray is proposed.
    // MotherComputer may select a strategy using these physically attainable
    // candidates; the server remains authoritative over their execution.
    auto& working=capability.effectivePhysics;
    working.assistedWorkingThrustAxisEmergency=false;
    working.assistedWorkingThrustAxisBody={0.0,0.0,-1.0};
    working.assistedWorkingThrustAxisAngleRad=0.0;
    working.assistedWorkingThrustMotorIndex=-1;
    if(plant::familyOf(model)==plant::PhysicalControlFamily::Assisted)
    {
        std::vector<ForwardMotorGeometry> physicalMotors;
        physicalMotors.reserve(topology.size());
        for(std::size_t i=0;i<topology.size();++i)
        {
            if(topology[i].role!=plant::EngineSlotRole::AftMain)
                continue;
            const auto* installed =
                detail::findModuleDescriptor(descriptor,bindings[i]);
            if(!installed || !installed->engine)
                continue;
            const double ratedN=std::max(0.0,
                installed->engine->nominalThrustNewtons);
            const glm::dvec3 arm =
                topology[i].normalizedPositionBody *
                    working.physicalActuatorHalfExtentsMeters -
                working.centerOfMassLocalMeters;
            physicalMotors.push_back({
                i, arm,
                ratedN*plant::effectiveSlotThrust01(projected[i]),
                std::max(0.0,
                    installed->engine->nominalGimbalRangeRad *
                    plant::effectiveSlotVectoring01(projected[i]))
            });
        }
        const auto candidate=analyzeWorkingThrustGeometry(physicalMotors);
        if(candidate.offAxis &&
            candidate.preferredAftSlot<topology.size())
        {
            working.assistedWorkingThrustAxisEmergency=true;
            working.assistedWorkingThrustAxisBody =
                candidate.forceDirectionBody;
            working.assistedWorkingThrustAxisAngleRad =
                candidate.axisOffsetRad;
            working.assistedWorkingThrustMotorIndex =
                static_cast<int>(candidate.preferredAftSlot);
            // A safe outboard emergency burn uses exactly the selected
            // engine; never advertise total bank thrust when the other
            // engines cannot be trimmed to a zero-moment axial program.
            aftThrustNewtons =
                candidate.conservativeMotorForceNewtons;
        }
        else if(survivingAft>0 &&
                !candidate.centeredForwardAvailable)
        {
            // No verified zero-torque forward working axis: retain the
            // ability to command attitude pulses, but DO NOT promise a
            // forward straight burn for this damaged installation.
            aftThrustNewtons=0.0;
        }
    }

    const double massKg =
        std::max(1.0, capability.effectivePhysics.massKg);
    if (foundAft)
        capability.effectivePhysics.forwardMainEngineAccelerationMps2 =
            static_cast<float>(aftThrustNewtons / massKg);
    if (foundAft && aftThrustNewtons<=1.0e-9)
        capability.effectivePhysics.forwardMainEngineAvailable=false;
    if (foundFore)
        capability.effectivePhysics.reverseMainEngineAccelerationMps2 =
            static_cast<float>(foreThrustNewtons / massKg);
    // Newtonian reverse RCS remains independent; never fabricate a fore main.
    capability.longitudinalAccelerationMps2 = {
        game::ship::forwardMainAccelerationLimitMps2(
            capability.effectivePhysics
        ),
        game::ship::reverseMainAccelerationLimitMps2(
            capability.effectivePhysics
        )
    };
}

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
    applyMountedEngineAxialAuthority(
        descriptor, descriptor.physicalControlModel, state, out
    );
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
    applyMountedEngineAxialAuthority(
        descriptor, descriptor.physicalControlModel, state, out
    );
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
