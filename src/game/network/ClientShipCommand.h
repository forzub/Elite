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
            ToggleStabilization,    // Ctrl+K: stabilization (hub / orbit / ground frame, 500 m/s envelope)
                                    //     on / off = normal flight (no speed envelope) off / on
            SetFreeFlightThrust,    // free flight: index 0 off (INSERT), 1 full (HOME), 2 brake (END)
            RefitPreview,           // F7: index = RefitOperationKind, target = module id, detail = product id,
                                    //     amount = subject ship entity id (0 = this ship)
            RefitApply,             // F7 DEV apply of the same operation (no payment yet)
            FreeFlightAutopilot,    // J autopilot: index 1 = fly to `target` body and land (empty = the
                                    //     dominant body: land here), index 0 = cancel
            RouteLeg,               // route autopilot leg: target "hub:<id>" | "orbit:<body>:<alt km>"
            FireWeapon,             // index 0 laser off, 1 laser on, 2 missile, 3 / 4 next laser / missile
                                    //     type (DEV), 5 DEV combat target `amount` m ahead
            ToggleJEngine           // Ctrl+J: the J engine on / off (only far from objects, above the J floor)
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
