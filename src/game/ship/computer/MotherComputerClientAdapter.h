#pragma once

// The only client-side bridge from replicated ship facts to MotherComputer.
// No server calls, no ShipCore references, no mutable world-state pointers.
// Ships outside the observed client state never create MotherComputer work.

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <utility>

#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/capability/ReplicatedInstalledAssistedDescriptor.h"
#include "src/game/ship/computer/MotherComputer.h"
#include "src/game/presentation/EngineLiveTelemetryPresentation.h"
#include "src/game/presentation/ShipBitePresentation.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"
#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"
#include "src/game/equipment/EquipmentDamage.h"

namespace game::ship::computer
{

[[nodiscard]] inline MotherComputerInput collectReplicatedShipFacts(
    const ClientShipState& ship,
    std::uint64_t serverTick
)
{
    MotherComputerInput out;
    out.shipInstanceId =
        static_cast<std::uint64_t>(ship.instanceId);
    out.serverTick = serverTick;
    out.physicalControlModel =
        static_cast<int>(ship.physicalControlModel);
    out.installedMotorAuthority = ship.controlAuthorityTelemetry;
    out.engines = ship.transform.motion.engineTelemetry;
    out.biteAndSystems = ship.shipCoreStatus;
    out.velocityLocalMps = ship.transform.motion.localVelocityMps;
    out.deliveredAngularAccelerationBodyRadPerSec2 =
        ship.transform.motion.mainPropulsionAngularAccelerationBodyRadPerSec2;
    out.requestedAngularAccelerationBodyRadPerSec2 =
        ship.transform.motion.requestedMainAngularAccelerationBodyRadPerSec2;
    out.workingThrustAxisBody =
        ship.transform.motion.assistedWorkingThrustAxisBody;
    out.emergencyWorkingThrustAxis =
        ship.transform.motion.assistedWorkingThrustAxisEmergency;

    if (ship.descriptor)
    {
        const auto installed =
            game::ship::capability::replicatedInstalledAssistedDescriptor(
                *ship.descriptor, ship.physicalControlModel);
        const ShipDescriptor& selected =
            installed ? *installed : *ship.descriptor;
        // The systems terminal already reports module failures, but the
        // MotherComputer BITE channel was never hydrated. Use exactly this
        // replicated installed equipment graph for the CHECK lamp and bars.
        out.installedEquipmentBite =
            game::presentation::buildShipBiteState(
                selected,ship.modules,serverTick);
        const bool diagnosticLayout =
            ship.physicalControlModel !=
                selected.physicalControlModel;
        const auto slotState =
            diagnosticLayout
                ? game::ship::plant::healthySlotState(
                    game::ship::plant::physicalControlSlots(
                        ship.physicalControlModel
                    ))
                : game::ship::capability::buildPhysicalControlSlotState(
                    selected, ship.physicalControlModel, ship.modules
                );
        auto capability = game::ship::capability::buildShipCapability(
            selected, ship.physicalControlModel, slotState
        );

        // Keep the accepted healthy/partially damaged flight envelope
        // untouched. An engine-out main-drive candidate needs the existing
        // CoM-projected emergency working axis and installed drive rating.
        // Future general multi-actuator certificate replaces this narrow
        // condition; it must not change normal flight accidentally.
        if (!diagnosticLayout && capability.valid)
        {
            const auto& topology =
                game::ship::plant::physicalControlSlots(
                    ship.physicalControlModel
                );
            std::size_t survivingAft = 0;
            for (std::size_t i=0;
                 i<topology.size() && i<slotState.size(); ++i)
                if (topology[i].role ==
                        game::ship::plant::EngineSlotRole::AftMain &&
                    game::ship::plant::effectiveSlotThrust01(
                        slotState[i])>1.0e-9)
                    ++survivingAft;
            if (game::ship::plant::familyOf(ship.physicalControlModel) ==
                    game::ship::plant::PhysicalControlFamily::Assisted)
                game::ship::capability::applyMountedEngineAxialAuthority(
                    selected,
                    ship.physicalControlModel,
                    slotState,
                    capability
                );
        }
        out.physicalEnvelopeValid = capability.valid;
        if (capability.valid)
            out.physicalEnvelope = capability.effectivePhysics;
    }

    const auto& systems=ship.shipCoreStatus;
    out.biteAlerts =
        static_cast<std::uint32_t>(systems.alerts.size());
    out.biteWarningSystems =
        static_cast<std::uint32_t>(systems.warningSystems.size());
    out.biteCriticalSystems =
        static_cast<std::uint32_t>(systems.criticalSystems.size());
    out.reactorShutdown = systems.reactor.status == 3;
    out.busOverloaded = systems.powerBus.overloaded;
    out.reactorOutputMW = systems.reactor.outputMW;
    out.busRequestedMW = systems.powerBus.totalRequestedMW;
    out.thermalTemperatureK = systems.thermal.temperature;
    out.thermalCriticalK = systems.thermal.thermalCriticalTemp;
    out.radiatedPowerMW = systems.cooling.radiatedPowerMW;

    const auto& motors=ship.transform.motion.engineTelemetry;
    if (motors.valid)
    {
        const std::size_t count = std::min(
            static_cast<std::size_t>(motors.count),
            motors.electricalMW.size()
        );
        for (std::size_t i=0;i<count;++i)
        {
            out.measuredEngineElectricalMW +=
                std::max(0.0,motors.electricalMW[i]);
            out.measuredEngineWasteHeatMW +=
                std::max(0.0,motors.wasteHeatMW[i]);
        }
    }

    // DO NOT assert "verified" here! These are telemetry observations only.
    // No combined 6-DOF reachability certificate, true electrical motor
    // bus allocation or time-bounded coolant certificate is yet provided.
    out.mobilityEvidence = {};
    return out;
}


[[nodiscard]] inline MotherComputerCockpitView collectCockpitView(
    const ClientShipState& ship,
    const MotherComputerSnapshot& motherboard
)
{
    MotherComputerCockpitView out;
    if (!motherboard.valid ||
        motherboard.input.shipInstanceId !=
            static_cast<std::uint64_t>(ship.instanceId) ||
        motherboard.input.serverTick == 0)
        return out;

    // Even though render position/attitude is interpolated, all equipment,
    // fault and energy facts belong to exactly ONE authoritative snapshot.
    out.valid = true;
    out.shipInstanceId = motherboard.input.shipInstanceId;
    out.serverTick = motherboard.input.serverTick;
    out.renderTransform = ship.renderTransform;
    // The tactical working-axis sight is factual equipment capability, NOT
    // an interpolated orientation/velocity cue. Preserve the same-tick
    // computed installed-motor evidence instead of reading a renderTransform
    // flag that can be stale or absent while the HUD interpolates poses.
    const auto& axisSource = motherboard.input.physicalEnvelope;
    if (motherboard.input.physicalEnvelopeValid &&
        axisSource.assistedWorkingThrustAxisEmergency &&
        axisSource.assistedWorkingThrustMotorIndex >= 0 &&
        std::isfinite(glm::length(axisSource.assistedWorkingThrustAxisBody)) &&
        glm::length(axisSource.assistedWorkingThrustAxisBody)>1.0e-9)
    {
        out.emergencyWorkingThrustAxis = true;
        out.workingThrustAxisBody =
            glm::normalize(axisSource.assistedWorkingThrustAxisBody);
    }
    out.bite = motherboard.input.installedEquipmentBite;
    out.systems = motherboard.input.biteAndSystems;

    if (!ship.descriptor)
        return out;

    out.nominalSpeedLimitMps = std::max(
        0.0, static_cast<double>(
            ship.descriptor->physics.maxCombatSpeed)
    );
    out.engineHudStyleId =
        ship.descriptor->hud.engineStatusStyleId.empty()
            ? "human_amber"
            : ship.descriptor->hud.engineStatusStyleId;
    out.physicalModelLabel =
        game::ship::plant::physicalControlModelName(
            ship.physicalControlModel
        );
    for (char& ch : out.physicalModelLabel)
        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');

    const auto& slots =
        game::ship::plant::physicalControlSlots(
            ship.physicalControlModel
        );
    const auto installed =
        game::ship::capability::replicatedInstalledAssistedDescriptor(
            *ship.descriptor, ship.physicalControlModel);
    const ShipDescriptor& selected =
        installed ? *installed : *ship.descriptor;
    const bool diagnostic =
        ship.physicalControlModel != selected.physicalControlModel;
    const auto states = diagnostic
        ? game::ship::plant::healthySlotState(slots)
        : game::ship::capability::buildPhysicalControlSlotState(
              selected, ship.physicalControlModel, ship.modules
          );
    if (states.size() != slots.size())
        return out;

    // One physical drive group for each installed ER-5, using canonical
    // installation bindings (not inferred from visual proximity).
    const auto& installation =
        game::ship::presets::cobra::installationSlots(
            ship.physicalControlModel);
    const auto& reactorIds =
        selected.powerThermalInstallation.engineReactorModuleIds;
    for (std::size_t n=0;n<installation.engineReactors.size();++n)
    {
        MotherComputerDriveHudGroup drive;
        drive.simulatedPower=diagnostic;
        drive.ignitionOn=out.systems.propulsionReactorsOn &&
            out.systems.propulsionStartupSeconds<=0.0;
        drive.ignitionStarting=out.systems.propulsionReactorsOn &&
            out.systems.propulsionStartupSeconds>0.0;
        drive.id = installation.engineReactors[n].slotId;
        for (const auto* id : installation.engineReactors[n].poweredEngineSlots)
            drive.poweredEngineSlots.emplace_back(id);
        if (!diagnostic && n<reactorIds.size())
        {
            const auto& id=reactorIds[n];
            const auto module=std::find_if(
                ship.modules.begin(),ship.modules.end(),
                [&](const auto& m){return m.moduleId==id;});
            const auto def=std::find_if(
                ship.descriptor->modules.begin(),
                ship.descriptor->modules.end(),
                [&](const auto& d){return d.moduleId==id;});
            if(module!=ship.modules.end() &&
               def!=ship.descriptor->modules.end() && def->engineReactor)
            {
                drive.installed=true;
                const auto damage=std::clamp(
                    static_cast<double>(module->laserDamage01+
                        module->impactDamage01+
                        module->thermalDamage01+
                        module->radiationDamage01+
                        module->wearDamage01+
                        module->internalFailureDamage01),0.0,1.0);
                drive.integrity01=static_cast<float>(1.0-damage);
                drive.failed=module->state!=0 || module->health<=0.0f;
                // No fabricated reactor core telemetry: only valid samples
                // from the authoritative installed generator are displayed.
                const auto sample=std::find_if(
                    out.systems.engineReactors.begin(),
                    out.systems.engineReactors.end(),
                    [&](const auto& item){return item.moduleId==id;});
                if(sample!=out.systems.engineReactors.end() &&
                   sample->safeTemperatureK>0.0 &&
                   std::isfinite(sample->coreTemperatureK) &&
                   sample->coreTemperatureK>0.0)
                {
                    drive.hasCoreTemperature=true;
                    drive.coreTemperatureK=static_cast<float>(
                        sample->coreTemperatureK);
                    drive.safeCoreTemperatureK=static_cast<float>(
                        sample->safeTemperatureK);
                    drive.generatedHeatMW=static_cast<float>(sample->generatedHeatMW);
                    drive.padHeatMW=static_cast<float>(sample->padHeatMW);
                    drive.coolantHeatMW=static_cast<float>(sample->coolantHeatMW);
                    drive.storedHeatMW=static_cast<float>(sample->storedHeatMW);
                }
            }
        }
        out.drives.push_back(std::move(drive));
    }
    if(game::ship::plant::familyOf(ship.physicalControlModel)==
           game::ship::plant::PhysicalControlFamily::Newtonian)
    {
        // Eight actual mount nodes, each with three installed nozzle modules.
        // Never use the obsolete 4/6/8 omnidirectional control-slot count.
        const auto& nozzles =
            game::ship::presets::cobra::newtonianRcsNozzles();
        for(std::size_t group=0;group<8;++group)
        {
            MotherComputerRcsHudSlot rcs;
            rcs.id=std::string(nozzles[group*3].nodeId);
            rcs.known=true;
            float minIntegrity=1.0f;
            for(std::size_t n=0;n<3;++n)
            {
                const auto& id=nozzles[group*3+n].nozzleId;
                const auto module=std::find_if(
                    ship.modules.begin(),ship.modules.end(),
                    [&](const auto& m){return m.moduleId==id;});
                if(module==ship.modules.end())
                {
                    rcs.known=false;
                    minIntegrity=0.0f;
                    continue;
                }
                const double damage=std::clamp(
                    static_cast<double>(module->laserDamage01+
                        module->impactDamage01+module->thermalDamage01+
                        module->radiationDamage01+module->wearDamage01+
                        module->internalFailureDamage01),
                    0.0,1.0);
                minIntegrity=std::min(minIntegrity,
                    module->state==0 && module->health>0.0f
                        ? static_cast<float>(1.0-damage):0.0f);
            }
            rcs.integrity01=minIntegrity;
            if(out.systems.newtonianRcsSampleValid)
            {
                rcs.firingKnown=true;
                for(std::size_t n=0;n<3;++n)
                    rcs.firing01=std::max(rcs.firing01,
                        out.systems.newtonianRcsExecuted01[group*3+n]);
            }
            out.rcsSlots.push_back(std::move(rcs));
        }
    }

    out.engineSlots.reserve(slots.size());
    const auto& bindings =
        selected.physicalControl.slotModuleIds;
    for (std::size_t i=0;i<slots.size();++i)
    {
        const auto role = slots[i].role;
        if (role != game::ship::plant::EngineSlotRole::AftMain &&
            role != game::ship::plant::EngineSlotRole::ForeMain)
            continue;
        if (game::ship::plant::familyOf(ship.physicalControlModel) ==
                game::ship::plant::PhysicalControlFamily::Newtonian &&
            role == game::ship::plant::EngineSlotRole::ForeMain)
            continue;

        MotherComputerEngineHudSlot slot;
        slot.id = slots[i].id;
        slot.groupX = slots[i].normalizedPositionBody.x;
        slot.groupY = slots[i].normalizedPositionBody.y;
        slot.fore =
            role == game::ship::plant::EngineSlotRole::ForeMain;
        const auto live =
            game::presentation::readEngineMotorLiveView(
                ship.transform.motion, i,
                game::ship::plant::effectiveSlotThrust01(states[i])
            );
        slot.available01 = static_cast<float>(live.available01);
        slot.delivered01 = static_cast<float>(
            std::min(live.thrust01,live.available01));
        const auto& motorTelemetry=ship.transform.motion.engineTelemetry;
        if(motorTelemetry.valid && i<motorTelemetry.count &&
           std::isfinite(motorTelemetry.nozzleYawRad[i]) &&
           std::isfinite(motorTelemetry.nozzlePitchRad[i]))
        {
            const double yaw=motorTelemetry.nozzleYawRad[i];
            const double pitch=motorTelemetry.nozzlePitchRad[i];
            slot.hasNozzleAngle=true;
            slot.nozzleAngleDeg=static_cast<float>(
                std::hypot(yaw,pitch)*57.29577951308232);
        }
        // CHECK must also follow genuinely unavailable installed thrust,
        // even while the server's BITE module graph arrives separately.
        // This is a loss-of-authority signal, not a fabricated damage source.
        const double cautionAuthority01 =
            1.0-game::bite::Thresholds{}.cautionLoss01;
        slot.fault = !diagnostic &&
            ((i < bindings.size() && !bindings[i].empty() &&
              game::presentation::biteHudAttentionForEquipment(
                  out.bite, bindings[i])) ||
             live.available01 <= cautionAuthority01+1.0e-6);
        if (!diagnostic && i < bindings.size() &&
            !bindings[i].empty())
        {
            const auto module = std::find_if(
                ship.modules.begin(),ship.modules.end(),
                [&](const auto& state) {
                    return state.moduleId == bindings[i];
                });
            const auto definition = std::find_if(
                ship.descriptor->modules.begin(),
                ship.descriptor->modules.end(),
                [&](const auto& def) {
                    return def.moduleId == bindings[i];
                });
            if (module != ship.modules.end() &&
                definition != ship.descriptor->modules.end() &&
                definition->engine &&
                module->hasEngineThermalState &&
                std::isfinite(module->engineTemperatureK))
            {
                slot.hasEngineTemperature = true;
                slot.engineTemperatureK = module->engineTemperatureK;
                slot.engineSafeTemperatureK = static_cast<float>(
                    definition->engine->safeOperatingTemperatureK);
            }
            if(module!=ship.modules.end())
            {
                slot.hasEmitterIntegrity=true;
                slot.emitterIntegrity01=std::clamp(
                    module->emitterIntactArea01,0.0f,1.0f);
            }
        }
        out.engineSlots.push_back(std::move(slot));
    }
    return out;
}

} // namespace game::ship::computer
