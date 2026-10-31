#pragma once


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
            RequestDockDeparture    // docked at the hub dock: ask the dispatcher to leave
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

    };
