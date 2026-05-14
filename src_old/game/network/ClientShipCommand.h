#pragma once



    struct ClientShipCommand
    {
        enum Type { 
            DamageRadiator, 
            RepairAllPanels, 
            InjectReactorFailure 
        };
        Type type;
        
        // для DamageRadiator
        int index = 0;          
        double amount = 0.0;
    };
