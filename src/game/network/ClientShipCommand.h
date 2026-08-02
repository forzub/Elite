#pragma once



    struct ClientShipCommand
    {
        enum Type {
            DamageRadiator,
            RepairAllPanels,
            InjectReactorFailure,
            EjectCockpitCapsule,
            StartBestRepairJob
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;
    };
