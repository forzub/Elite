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
            InjectEquipmentFailure // developer-only main-engine malfunction
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

    };
