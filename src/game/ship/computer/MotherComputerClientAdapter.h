#pragma once

// The only client-side bridge from replicated ship facts to MotherComputer.
// No server calls, no ShipCore references, no mutable world-state pointers.
// Ships outside the observed client state never create MotherComputer work.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/computer/MotherComputer.h"

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
        const bool diagnosticLayout =
            ship.physicalControlModel !=
                ship.descriptor->physicalControlModel;
        const auto slotState =
            diagnosticLayout
                ? game::ship::plant::healthySlotState(
                    game::ship::plant::physicalControlSlots(
                        ship.physicalControlModel
                    ))
                : game::ship::capability::buildPhysicalControlSlotState(
                    *ship.descriptor,
                    ship.physicalControlModel,
                    ship.modules
                );
        auto capability = game::ship::capability::buildShipCapability(
            *ship.descriptor,
            ship.physicalControlModel,
            slotState
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
            if (survivingAft == 1)
                game::ship::capability::applyMountedEngineAxialAuthority(
                    *ship.descriptor,
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

} // namespace game::ship::computer
