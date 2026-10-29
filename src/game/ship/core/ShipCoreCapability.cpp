// ShipCore: installed capability snapshot, RCS gas share, control-authority telemetry, effective physics.

#include "src/game/ship/core/ShipCoreInternal.h"

using namespace game::ship::geometry;

namespace game::ship::core
{


game::ship::capability::ShipCapabilitySnapshot
ShipCore::capabilities() const
{
    if (!m_state.valid())
        return {};

    const std::uint64_t constructionRevision =
        m_state.construction.revision;
    const std::uint64_t actuatorRevision =
        m_state.actuators.revision;
    const std::uint64_t massRevision =
        m_state.massProperties.revision;

    if (m_cachedCapability.valid &&
        m_cachedConstructionRevision == constructionRevision &&
        m_cachedActuatorRevision == actuatorRevision &&
        m_cachedMassRevision == massRevision)
    {
        return m_cachedCapability;
    }

    m_cachedNewtonianRcsAuthorityValid = false;
    auto out =
        game::ship::capability::buildShipCapability(
            *m_state.construction.definition,
            m_state.construction.physicalControlModel,
            m_state.actuators.propulsionSlots
        );

    if (out.valid)
    {
        m_state.massProperties.applyTo(out.effectivePhysics);
        // After CURRENT mass is known, calculate force/mass from the actual
        // mounted engine TYPES and their module-derived availability.
        // Diagnostic construction overrides preserve the legacy matrix
        // envelope so I/Y/X probe fixtures cannot invent engine mounts.
        if (m_state.actuators.source ==
                game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
            m_state.construction.physicalControlModel ==
                m_state.construction.definition->physicalControlModel)
        {
            game::ship::capability::applyMountedEngineAxialAuthority(
                *m_state.construction.definition,
                m_state.construction.physicalControlModel,
                m_state.actuators.propulsionSlots,
                out
            );

            // The old nominal 2m/s2 and 3rad/s2 RCS demands were not
            // hardware facts. On the healthy 260t Cobra they requested
            // 520kN of translation and tens of MNm of turn torque while
            // the installed 24 nozzles delivered a fraction of either.
            // Project the actual actuator authority ONCE per physical
            // equipment/mass revision; DO NOT fake extra maneuver power.
            if(game::ship::plant::familyOf(
                m_state.construction.physicalControlModel)==
                    game::ship::plant::PhysicalControlFamily::Newtonian)
            {
                using game::equipment::engine::MountedEngine;
                std::array<MountedEngine,24> mounted {};
                const auto& definition=*m_state.construction.definition;
                const auto& catalog=
                    game::ship::presets::cobra::newtonianRcsNozzles();
                const auto& runtime=
                    static_cast<const world::modules::ObjectModuleRuntime&>(
                        m_moduleRuntime);
                for(std::size_t i=0;i<catalog.size();++i)
                {
                    const std::string id(catalog[i].nozzleId);
                    const auto item=std::find_if(
                        definition.modules.begin(),definition.modules.end(),
                        [&](const auto& m){return m.moduleId==id;});
                    const auto* state=runtime.findModule(id);
                    if(item==definition.modules.end() ||
                        !item->engine || !state)
                        continue;
                    mounted[i]={
                        &*item->engine,
                        &state->equipmentDamage,
                        &state->engineOperating,
                        catalog[i].nozzleId,
                        state->state ==
                            world::modules::ObjectModuleRuntimeState::Attached &&
                        state->health>0.0f
                    };
                }
                auto& p=out.effectivePhysics;
                const auto limits=
                    game::ship::plant::sampleNewtonianRcsSignedAuthority(
                        mounted,p.physicalActuatorHalfExtentsMeters,
                        p.centerOfMassLocalMeters);
                m_cachedNewtonianRcsPairFeasibility01=
                    game::ship::plant::newtonianRcsPairFeasibility01(
                        mounted,p.physicalActuatorHalfExtentsMeters,
                        p.centerOfMassLocalMeters,limits);
                const double mass=std::max(1.0,p.massKg);
                const glm::dvec3 angularInertia {
                    std::max(1.0,p.pitchInertiaKgM2),
                    std::max(1.0,p.yawInertiaKgM2),
                    std::max(1.0,p.rollInertiaKgM2)
                };
                const glm::dvec3 positiveA=limits.positiveForceN/mass;
                const glm::dvec3 negativeA=limits.negativeForceN/mass;
                const glm::dvec3 positiveAlpha=
                    limits.positiveMomentNm/angularInertia;
                const glm::dvec3 negativeAlpha=
                    limits.negativeMomentNm/angularInertia;
                out.strafeAccelerationMps2={positiveA.x,negativeA.x};
                out.liftAccelerationMps2={positiveA.y,negativeA.y};
                out.pitchAccelerationRadPerSec2={
                    positiveAlpha.x,negativeAlpha.x};
                out.yawAccelerationRadPerSec2={
                    positiveAlpha.y,negativeAlpha.y};
                // Hull positive roll rotates about body -Z, while the
                // physical wrench uses +Z; swap signed roll capacity.
                out.rollAccelerationRadPerSec2={
                    negativeAlpha.z,positiveAlpha.z};
                m_cachedNewtonianRcsPositive={
                    positiveA.x,positiveA.y,positiveA.z,
                    positiveAlpha.x,positiveAlpha.y,negativeAlpha.z};
                m_cachedNewtonianRcsNegative={
                    negativeA.x,negativeA.y,negativeA.z,
                    negativeAlpha.x,negativeAlpha.y,positiveAlpha.z};
                m_cachedNewtonianRcsAuthorityValid=true;
                // Single scalar legacy controller accepts only a
                // conservative all-directions budget. Signed per-axis
                // capability remains available in the full snapshot.
                // Clients apply the SAME bound from replicated telemetry.
                game::ship::telemetry::applyNewtonianRcsScalarBounds(
                    p,m_cachedNewtonianRcsPositive,
                    m_cachedNewtonianRcsNegative);
            }
        }
    }

    m_cachedCapability = out;
    m_cachedConstructionRevision = constructionRevision;
    m_cachedActuatorRevision = actuatorRevision;
    m_cachedMassRevision = massRevision;

    return m_cachedCapability;
}

const game::equipment::rcs::RcsGasTankDescriptor*
ShipCore::newtonianRcsGasTank(double& capacity01) const
{
    capacity01 = 1.0;
    if (!m_state.construction.valid())
        return nullptr;
    const auto& desc = *m_state.construction.definition;
    const auto def = std::find_if(desc.modules.begin(), desc.modules.end(),
        [](const auto& module) { return module.gasTank.has_value(); });
    if (def == desc.modules.end())
        return nullptr;
    const auto* state = static_cast<const world::modules::ObjectModuleRuntime&>(
        m_moduleRuntime).findModule(def->moduleId);
    capacity01 = state
        ? game::equipment::rcs::rcsGasTankCapacity01(*def->gasTank,
            state->equipmentDamage.accumulated,
            state->state == world::modules::ObjectModuleRuntimeState::Attached &&
                state->health > 0.0f)
        : 0.0;
    return &*def->gasTank;
}

double ShipCore::newtonianRcsGasThrustShare01() const
{
    double capacity01 = 1.0;
    const auto* tank = newtonianRcsGasTank(capacity01);
    if (!tank)
        return 1.0;
    if (!(capacity01 > 1.0e-9))
        return 0.0;
    return game::equipment::rcs::rcsGasThrustShare01(*tank,
        m_transform.motion.manoeuvreGasPressure01,
        m_transform.motion.manoeuvreGasDepleted);
}

/*
    Electrical power an installed ER-5 propulsion generator can deliver now:
    rated x damage authority x warm-up readiness x thermal derating x ageing,
    zero while the propulsion reactors are off or restarting. The ONE
    definition for the executor (power groups of the actual thrust) and the
    published capability. I/Y/X diagnostic layouts mount synthetic motors and
    get the matching synthetic budget; native construction never does.
*/
double ShipCore::engineReactorAvailableMW(std::size_t group) const
{
    if (!m_desc)
        return 0.0;
    const bool powered = m_propulsionReactorsOn && m_propulsionReactorsStartupRemainingSeconds <= 0.0;
    const auto& reactorIds = m_desc->powerThermalInstallation.engineReactorModuleIds;
    const std::string reactorId = group < reactorIds.size() ? reactorIds[group] : std::string {};
    const auto& runtime = static_cast<const world::modules::ObjectModuleRuntime&>(m_moduleRuntime);
    const auto* reactor = reactorId.empty() ? nullptr : runtime.findModule(reactorId);
    const auto definition = std::find_if(m_desc->modules.begin(), m_desc->modules.end(),
                                         [&](const auto& module) { return module.moduleId == reactorId; });
    double availableMW = 0.0;
    if (reactor && definition != m_desc->modules.end() &&
        definition->engineReactor &&
        reactor->state ==
            world::modules::ObjectModuleRuntimeState::Attached &&
        reactor->health > 0.0f)
    {
        const auto function =
            game::equipment::evaluateEquipmentFunctionAtDamage01(
                game::equipment::totalAccumulatedDamage01(
                    reactor->equipmentDamage.accumulated),
                definition->engineReactor->failure);
        const auto& reactorType=*definition->engineReactor;
        const double t=reactor->engineReactorOperating.temperatureK;
        const double ready=std::clamp(
            (t-game::physics::thermal::DeepSpaceTemperatureK)/
            std::max(1.0,reactorType.idleTemperatureK-
                game::physics::thermal::DeepSpaceTemperatureK),
            0.0,1.0);
        const double thermal= t<=reactorType.safeTemperatureK
            ? 1.0 : std::clamp(
                (reactorType.criticalTemperatureK-t)/
                std::max(1.0,reactorType.criticalTemperatureK-
                    reactorType.safeTemperatureK),0.0,1.0);
        const double age=std::clamp(
            reactor->engineReactorOperating.equivalentOperatingHours/
            std::max(1.0,reactorType.endOfLifeEquivalentHours),
            0.0,1.0);
        availableMW =
            (powered ? 1.0:0.0) *
            std::max(0.0,reactorType.ratedElectricalMW) *
            function.authority01 * ready * thermal *
            (1.0-0.20*age);
    }
    const bool diagnosticLayout=
        m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::DiagnosticOverride ||
        m_state.construction.physicalControlModel !=
            m_state.construction.definition->physicalControlModel;
    if(diagnosticLayout && availableMW<=0.0 && powered)
        availableMW=5000.0;
    return availableMW;
}

/*
    Share of the installed main / RCS authority the propulsion generators
    can power now (1 = not power limited). Main: per generator group, the
    group's available MW against the full-thrust demand of the main engines
    it powers (min over groups: a weak group limits the balanced thrust).
    RCS: all generators' available MW against the full-thrust demand of all
    nozzles plus the argon compressor (conservative: every nozzle at once).
    Execution applies the same power limits, so planner / autopilot / HUD and
    physics agree when an ER-5 is damaged, cold, overheated or old.
*/
void ShipCore::newtonianPropulsionPowerShares(double& mainShare01, double& rcsShare01) const
{
    mainShare01 = 1.0;
    rcsShare01 = 1.0;
    if (!m_desc || !m_state.construction.valid() ||
        m_state.actuators.source != game::ship::state::ShipActuatorStateSource::ModuleRuntime)
        return;
    const auto& runtime = static_cast<const world::modules::ObjectModuleRuntime&>(m_moduleRuntime);
    const auto fullDemandMW = [&](const std::string& id) {
        const auto def = std::find_if(m_desc->modules.begin(), m_desc->modules.end(),
                                      [&](const auto& m) { return m.moduleId == id; });
        const auto* state = runtime.findModule(id);
        if (def == m_desc->modules.end() || !def->engine || !state ||
            state->state != world::modules::ObjectModuleRuntimeState::Attached || state->health <= 0.0f)
            return 0.0;
        const auto effective = game::equipment::engine::evaluateEngineUnit(
            *def->engine, state->equipmentDamage, state->engineOperating);
        return std::max(0.0, game::equipment::engine::requestedElectricalPowerMW(*def->engine, effective, 1.0));
    };
    const auto model = m_state.construction.physicalControlModel;
    const auto& groups = game::ship::engineReactorGroups(*m_desc, model);
    const auto& topology = game::ship::propulsionTopology(*m_state.construction.definition, model);
    const auto& engineIds = m_desc->physicalControl.slotModuleIds;
    double totalAvailableMW = 0.0;
    for (std::size_t g = 0; g < groups.size(); ++g)
    {
        const double available = engineReactorAvailableMW(g);
        totalAvailableMW += available;
        double demand = 0.0;
        for (const auto& slotId : groups[g].poweredEngineSlotIds)
            for (std::size_t i = 0; i < topology.size() && i < engineIds.size(); ++i)
                if (std::string_view(topology[i].id) == slotId &&
                    topology[i].role != game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                    demand += fullDemandMW(engineIds[i]);
        if (demand > 0.0)
            mainShare01 = std::min(mainShare01, std::clamp(available / demand, 0.0, 1.0));
    }
    if (m_newtonianRcsPhysical)
    {
        double rcsDemand = 2.0; // argon pressure-recovery compressor
        for (const auto& nozzle : game::ship::presets::cobra::newtonianRcsNozzles())
            rcsDemand += fullDemandMW(std::string(nozzle.nozzleId));
        rcsShare01 = std::clamp(totalAvailableMW / rcsDemand, 0.0, 1.0);
    }
}

game::ship::telemetry::ShipControlAuthorityTelemetry
ShipCore::controlAuthorityTelemetry() const
{
    using namespace game::ship::plant;
    using game::equipment::engine::MountedEngine;

    if (!m_state.construction.valid())
        return {};

    // Newtonian publishes the signed installed RCS authority already
    // sampled by capabilities(), so planner/autopilot use the same
    // limits as the server execution path instead of descriptor nominals.
    if (familyOf(m_state.construction.physicalControlModel) ==
            PhysicalControlFamily::Newtonian)
    {
        if (!m_propulsionReactorsOn ||
            m_propulsionReactorsStartupRemainingSeconds>0.0)
            return {};
        const auto capability = capabilities();
        if (!capability.valid || !m_cachedNewtonianRcsAuthorityValid)
            return {};
        game::ship::telemetry::ShipControlAuthorityTelemetry out;
        out.valid = true;
        out.model = m_state.construction.physicalControlModel;
        out.sourceActuatorRevision = m_state.actuators.revision;
        out.positive = m_cachedNewtonianRcsPositive;
        out.negative = m_cachedNewtonianRcsNegative;
        // The tank pressure is part of what the nozzles can deliver now:
        // planner and autopilot see weak RCS when the gas runs low.
        // So is the electrical power of the ER-5 generators (damage, warm-up,
        // overheating, age): the same limits the executor applies.
        double mainPowerShare = 1.0, rcsPowerShare = 1.0;
        newtonianPropulsionPowerShares(mainPowerShare, rcsPowerShare);
        const double gasShare = newtonianRcsGasThrustShare01() * rcsPowerShare;
        for (auto& value : out.positive)
            value *= gasShare;
        for (auto& value : out.negative)
            value *= gasShare;
        out.rcsPairFeasibility01 = m_cachedNewtonianRcsPairFeasibility01;
        out.workingForwardAccelerationMps2 = mainPowerShare * std::max(0.0,
            static_cast<double>(capability.effectivePhysics.
                forwardMainEngineAccelerationMps2));
        return out;
    }

    if (familyOf(m_state.construction.physicalControlModel) !=
            PhysicalControlFamily::Assisted)
        return {};

    // The signed actuator basis is NOT usable flight authority while
    // propulsion reactors are switched off or completing their real 10 s
    // restart. Do not advertise a mechanically healthy but unpowered ship
    // to MotherComputer as physically flyable.
    if (!m_propulsionReactorsOn ||
        m_propulsionReactorsStartupRemainingSeconds>0.0)
        return {};

    const auto model = m_state.construction.physicalControlModel;
    const std::uint64_t revision = m_state.actuators.revision;
    const ShipParams physics = effectivePhysics();
    const auto& topology = game::ship::propulsionTopology(
        *m_state.construction.definition, model);

    // System/thermal updates may run AFTER the engine control tick, changing
    // actuator revision before GameSimulation publishes its snapshot. Refresh
    // the SAME motor-program cache as the executor, without applying forces.
    // Checking revision alone would make live authority invalid every frame
    // of continuous engine thermal evolution.
    std::vector<MountedEngine> installed(topology.size());
    std::array<game::equipment::engine::EngineUnitDescriptor,16> diagnostic {};
    std::array<game::equipment::EquipmentDamageLayer,16> diagnosticDamage {};
    std::array<game::equipment::engine::EngineOperatingState,16>
        diagnosticOperating {};
    if (topology.size() > diagnostic.size())
        return {};

    if (m_state.actuators.source ==
            game::ship::state::ShipActuatorStateSource::ModuleRuntime &&
        model == m_state.construction.definition->physicalControlModel)
    {
        const auto& desc = *m_state.construction.definition;
        const auto& ids = desc.physicalControl.slotModuleIds;
        if (ids.size() != topology.size())
            return {};

        const auto& runtime =
            static_cast<const world::modules::ObjectModuleRuntime&>(
                m_moduleRuntime
            );
        for (std::size_t i=0;i<ids.size();++i)
        {
            if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
                continue;
            const auto def = std::find_if(
                desc.modules.begin(), desc.modules.end(),
                [&](const auto& m) { return m.moduleId == ids[i]; }
            );
            const auto* state = runtime.findModule(ids[i]);
            if (def == desc.modules.end() || !def->engine || !state)
                return {};
            installed[i] = {
                &*def->engine,
                &state->equipmentDamage,
                &state->engineOperating,
                ids[i],
                state->state ==
                    world::modules::ObjectModuleRuntimeState::Attached &&
                state->health > 0.0f
            };
        }
    }
    else
    {
        // Diagnostic I/Y/X construction uses the same intentionally
        // synthetic healthy passports as updateTranslationControl(). Do
        // not synthesize those motors for a genuine installed model.
        for (std::size_t i=0;i<topology.size();++i)
        {
            if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
                continue;
            diagnostic[i] =
                game::equipment::engine::presets::
                    CobrettyCR1VectorMainDrive();
            diagnosticDamage[i].failure.failureAtDamage01 = 0.95;
            diagnosticOperating[i].temperatureK =
                diagnostic[i].nominalOperatingTemperatureK;
            installed[i] = {
                &diagnostic[i],
                &diagnosticDamage[i],
                &diagnosticOperating[i],
                topology[i].id,
                true
            };
        }
    }

    // matches() is cheap for unchanged engines; the expensive solver runs
    // only when actual motor force/nozzle availability has changed.
    if (!m_assistedManeuvers.matches(model, installed, physics))
        m_assistedManeuvers.build(model, installed, physics);
    if (!m_assistedManeuvers.valid)
        return {};
    return game::ship::telemetry::readAssistedControlAuthority(
        m_assistedManeuvers,revision
    );
}

ShipParams ShipCore::effectivePhysics() const
{
    const auto capability=capabilities();
    if(capability.valid)
        return capability.effectivePhysics;

    // ShipParams{} is NOT a safe "disabled" preset: its legacy default
    // forwardMainEngineAvailable=true combines with the zero-as-load-limit
    // adapter and can manufacture thrust from a failed capability.
    // Invalid means no capability to accelerate or turn.
    ShipParams disabled {};
    disabled.forwardMainEngineAvailable=false;
    disabled.reverseMainEngineAvailable=false;
    disabled.forwardMainEngineAccelerationMps2=0.0f;
    disabled.reverseMainEngineAccelerationMps2=0.0f;
    disabled.manoeuvreThrusterAuthorityExplicit=true;
    disabled.manoeuvreThrusterAccel=0.0f;
    disabled.strafeAccel=0.0f;
    disabled.angularAccel=0.0f;
    disabled.maxGs=0.0f;
    disabled.maxLinearGs=0.0f;
    return disabled;
}

void ShipCore::updatePhysics(float dt, const WorldParams& world)
{
    updateMotionPhysics(dt, world);
    updateSystems(dt);
}

} // namespace game::ship::core
