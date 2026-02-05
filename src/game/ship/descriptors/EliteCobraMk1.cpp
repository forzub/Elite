
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/equipment/decryptor/DecryptorDesc.h"

#include "src/game/equipment/data/decryptors.h"
#include "src/game/equipment/data/transmitters.h"
#include "src/game/equipment/data/receivers.h" 
#include "src/game/equipment/data/jammers.h"


const ShipDescriptor& EliteCobraMk1::EliteCobraMk1Descriptor()
{
    static ShipDescriptor desc;
    static bool initialized = false;

    if (!initialized)
    {
        initialized = true;

        // ─────────────────────────
        // Identity
        // ─────────────────────────
        desc.identity = {
            "cobra mk1",
            "COBRA MK1"
        };

        // -------------------------
        // Physics
        // -------------------------
        desc.physics = {
            90.0f, 60.0f, 120.0f,   // angular limits
            1000.0f, 9.0f,          // angular accel/damp
            500.0f, 10000.0f, 5.0f, // speeds
            0.0f,                   // auto-level
            20.0f, 6.0f, 50.0f      // strafe
        };

        // -------------------------
        // HUD profile (ВОТ ТУТ)
        // -------------------------
        desc.hud.name = "Cobra Mk1";

        desc.hud.edgeBoundary.contour = {
            {0.10f, 0.12f},
            {0.90f, 0.12f},
            {0.96f, 0.50f},
            {0.90f, 0.88f},
            {0.10f, 0.88f},
            {0.04f, 0.50f}
        };

        // ─────────────────────────
        // System slots
        // ─────────────────────────
        desc.systems.reactorSlots           = 1;
        desc.systems.engineSlots            = 1;
        desc.systems.radarSlots             = 1;
        desc.systems.weaponSlots            = 2;
        desc.systems.decryptorSlots         = 1;
        desc.systems.jammerSlots            = 0;
        desc.systems.receiverSlots          = 1;
        desc.systems.transmitterSlots       = 1;
        desc.systems.utilitySlots           = 1;
        desc.systems.dockingComputerSlots   = 1;
        
        // desc.systems = {
        //     .reactorSlots       = 1,
        //     .engineSlots        = 1,
        //     .radarSlots         = 1,
        //     .weaponSlots        = 2,
        //     .decryptorSlots     = 1,
        //     .jammerSlots        = 0,
        //     .dockingComputer    = 1,
        //     .RX                 = 1,
        //     .TX                 = 1,
        //     .utilitySlots       = 1,
        // };

        // ─────────────────────────
        // Docking
        // ─────────────────────────
        desc.docks = {
            .fighterSlots = 0,
            .shuttleSlots = 0,
            .droneSlots   = 0
        };


        // ─────────────────────────
        // Storage
        // ─────────────────────────
        desc.storage = {
            .cargoMass       = 50,
            .cargoVolume     = 30,
            .missileCapacity = 4
        };

        // ─────────────────────────
        // Survival
        // ─────────────────────────
        desc.survival = {
            .hasEscapePod  = true,
            .hasNanokitBay = true
        };

        // ─────────────────────────
        // Signal capabilities
        // ─────────────────────────
        desc.signalProfile.supportedSignals = {
            SignalType::Silence,
            SignalType::Transponder,
            SignalType::SOSModern,
            SignalType::Beacon,
            SignalType::Message,
            SignalType::None

        };


        // ─────────────────────────
        // Equipment default presets 
        // ─────────────────────────
        desc.defaultEquipment = {
            .decryptor        = DECRYPTOR_STANDARD,      
            .jammer           = std::nullopt,
            .receiver         = STANDARD_RECEIVER,      
            .transmitter      = WSDR_TX13    
        };


    }

    return desc;
}




