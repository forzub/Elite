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
            DebugFailPhysicalPropulsionSlot,
            DebugRestorePhysicalPropulsionSlots
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

    };
