#pragma once

#include <cstdint>


    struct ClientShipCommand
    {
        enum Type {
            DamageRadiator,
            RepairAllPanels,
            InjectReactorFailure,
            EjectCockpitCapsule,
            StartBestRepairJob,
            BeginDockingGuidancePreparation,
            CancelDockingGuidancePreparation,
            CompleteDockingGuidancePreparation
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

        // Stable client-side docking request identity for begin/cancel/complete.
        std::uint64_t requestSerial = 0;
    };
