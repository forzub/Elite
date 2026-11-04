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
            CancelOrbitalTransfer,  // stop the active transfer: the carrier coasts on gravity
            ToggleFreeFlight,       // Ctrl+J: free flight (global frame, no speed envelope) on / off
            SetFreeFlightThrust,    // free flight: index 0 off (INSERT), 1 full (HOME), 2 brake (END)
            RefitPreview,           // F7: index = RefitOperationKind, target = module id, detail = product id,
                                    //     amount = subject ship entity id (0 = this ship)
            RefitApply,             // F7 DEV apply of the same operation (no payment yet)
            FreeFlightAutopilot,    // J autopilot: index 1 = fly to `target` body and land (empty = the
                                    //     dominant body: land here), index 0 = cancel
            RouteLeg                // route autopilot leg: target "hub:<id>" | "orbit:<body>:<alt km>"
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

        // RequestOrbitalTransfer: destination orbital hub id
        std::string target;

        // RefitPreview / RefitApply: replacement product id
        std::string detail;

    };
