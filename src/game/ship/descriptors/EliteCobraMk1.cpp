
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

        // desc.hud.edgeBoundary.contour = {
        //     {0.10f, 0.12f},
        //     {0.90f, 0.12f},
        //     {0.96f, 0.50f},
        //     {0.90f, 0.88f},
        //     {0.10f, 0.88f},
        //     {0.04f, 0.50f}
        // };

        desc.hud.edgeBoundary.contour = {
                {0.9524f, 0.1298f},
                {0.9021f, 0.1196f},
                {0.8518f, 0.1108f},
                {0.8013f, 0.1031f},
                {0.7509f, 0.0967f},
                {0.7003f, 0.0916f},
                {0.6498f, 0.0876f},
                {0.5992f, 0.0848f},
                {0.5486f, 0.0833f},
                {0.4979f, 0.0829f},
                {0.4473f, 0.0836f},
                {0.3968f, 0.0856f},
                {0.3462f, 0.0886f},
                {0.2956f, 0.0928f},
                {0.2451f, 0.0980f},
                {0.1946f, 0.1044f},
                {0.1442f, 0.1118f},
                {0.0938f, 0.1203f},
                {0.0435f, 0.1298f},

                {0.0378f, 0.2118f},
                {0.0339f, 0.2942f},
                {0.0318f, 0.3768f},
                {0.0313f, 0.4595f},
                {0.0323f, 0.5421f},
                {0.0348f, 0.6247f},
                {0.0385f, 0.7070f},
                {0.0435f, 0.7892f},

                {0.0898f, 0.7954f},
                {0.1360f, 0.8009f},
                {0.1823f, 0.8060f},
                {0.2287f, 0.8103f},
                {0.2750f, 0.8132f},
                {0.3214f, 0.8173f},
                {0.3678f, 0.8207f},

                // нижний вырез
                {0.4056f, 0.7708f},
                {0.4512f, 0.7456f},
                {0.4986f, 0.7382f},
                {0.5463f, 0.7457f},
                {0.5921f, 0.7712f},

                {0.6300f, 0.8211f},
                {0.6761f, 0.8198f},
                {0.7222f, 0.8169f},
                {0.7683f, 0.8132f},
                {0.8144f, 0.8077f},
                {0.8604f, 0.8024f},
                {0.9064f, 0.7962f},
                {0.9524f, 0.7892f},

                {0.9599f, 0.7075f},
                {0.9651f, 0.6251f},
                {0.9680f, 0.5425f},
                {0.9687f, 0.4596f},
                {0.9674f, 0.3768f},
                {0.9641f, 0.2942f},
                {0.9590f, 0.2118f}
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
        desc.systems.fuelScopSlots          = 1;
        desc.systems.tractorBeamSlots       = 1;
        
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




