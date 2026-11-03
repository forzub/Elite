#pragma once

#include <string>


    struct ClientShipCommand
    {
        enum Type {
            DamageRadiator,
            RepairAllPanels,
            InjectReactorFailure,
            EjectCockpitCapsule,
            StartBestRepairJob,
            SelectPhysicalControlModel,
            TogglePropulsionReactors,
            ToggleAssistedFreeRotation,
            InjectEquipmentFailure, // developer-only main-engine malfunction
            RequestDockDeparture,   // docked at the hub dock: ask the dispatcher to leave
            RequestOrbitalTransfer, // hub -> hub Orbital Transfer to `target` (orbital hub id)
            CancelOrbitalTransfer   // stop the active transfer: the carrier coasts on gravity
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

        // RequestOrbitalTransfer: destination orbital hub id
        std::string target;

    };
