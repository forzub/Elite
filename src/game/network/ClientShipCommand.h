#pragma once

#include <cstdint>
#include <string>


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
            CompleteDockingGuidancePreparation,
            BeginAutomaticDocking,
            CancelAutomaticDocking
        };
        Type type;

        // для DamageRadiator
        int index = 0;
        double amount = 0.0;

        // Stable client-side docking request identity for begin/cancel/complete.
        std::uint64_t requestSerial = 0;

        // Automatic docking target identity. Manual guidance preparation keeps
        // these empty because the client owns only advisory-route presentation.
        int dockingTargetSystemId = -1;
        std::string dockingTargetModuleId;
        std::string dockingTargetAnchorId;
    };
