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
            RequestJump,            // the interstellar jump: index = the target system id (START: the
                                    //     charge-up), index -1 = cancel; detail = the exit body id (optional)
            NavData,                // navigation data (JUMP_MECHANICS 10l), docked at a hub: index 0 = the bank's
                                    //     record `detail` into the onboard computer, 1 = onto a new data carrier,
                                    //     2 = carrier number `amount` into the computer
            RequestRefuel,          // docked at a station (or held in a hub's frame): index 1 = fill the jump charge,
                                    //     index 0 = stop (docs/FUEL_STATIONS.md); the server decides
            BoardShip,              // base 2 (2026-11-20): the pilot moves to another owned, Player-role ship nearby;
                                    //     amount = its entity id; the server decides (owner, distance, free)
            BuyShip,                // F6 (step 14): detail = passport id, target = offer id; at a berth; a new ship of
                                    //     the player appears 5 km off (boarded from F7)
            HireJob,                // F5 employment service: detail = vacancy id, at a berth of that hub
            QuitJob,                // F5: back to the pilot's own ship (within 20 km)
            DevFullRestore,         // TEMPORARY (author 2026-11-21): Ctrl+Alt+R - the ship fully restored (structure,
                                    //     reactors, containment, heat, spin, "out of the fight" cleared)
            LandOnPad,              // a hub city pad (author 2026-11-21): index 1 = land at target = the city frame module,
                                    //     detail = the building address ("B4-07"); index 0 = the pilot takes the controls
            ToggleLandingGear,      // G in flight (author 2026-11-21): the landing legs out / in (a broken set stays broken)
            EnterHubFrame           // a docking / route request near a hub in normal flight (2026-11-21): the ship is taken
                                    // into the frame of the hub of the module `target` where it is (no jump, the hub's velocity)
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
