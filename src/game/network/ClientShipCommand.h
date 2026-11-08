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
            RequestDockDeparture,   // docked at the hub dock: ask the dispatcher to leave (index 0);
                                    //     index 1: the pilot takes the controls during the station's
                                    //     departure (released early, outside the portal only)
            ToggleStabilization,    // Ctrl+K: stabilization (hub / orbit / ground frame, 500 m/s envelope)
                                    //     on / off = normal flight (no speed envelope) off / on
            SetFreeFlightThrust,    // free flight: index 0 off (INSERT), 1 full (HOME), 2 brake (END)
            RefitPreview,           // F7: index = RefitOperationKind, target = module id, detail = product id,
                                    //     amount = subject ship entity id (0 = this ship)
            RefitApply,             // F7 DEV apply of the same operation (no payment yet)
            RouteService,           // a route stop's timed service stay at hub `target` (detail: the action,
                                    //     "refuel" / "repair"); the CLIENT flies routes (NAVIGATION_V2 §1)
            FireWeapon,             // index 0 laser off, 1 laser on, 2 missile, 3 / 4 next laser / missile
                                    //     type (DEV), 5 DEV combat target `amount` m ahead
            ToggleJEngine,          // Ctrl+J: the J engine on / off (only far from objects, above the J floor)
            RequestDockArrival,     // automatic docking engaged (index 1) / dropped (index 0): the dock
                                    //     dispatcher clears a direct entry or assigns a hold point
            RequestJump             // the interstellar jump: index = the target system id (START: the
                                    //     charge-up), index -1 = cancel; detail = the exit body id (optional)
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

        // RouteService: the hub id
        std::string target;

        // RefitPreview / RefitApply: replacement product id
        std::string detail;

    };
