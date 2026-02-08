
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
                {
                    {0.952427f, 0.129796f},
                    {0.952427f, 0.129796f},
                    {0.957859f, 0.195369f},
                    {0.962292f, 0.261179f},
                    {0.965635f, 0.327191f},
                    {0.967802f, 0.393357f},
                    {0.968724f, 0.459616f},
                    {0.968318f, 0.525891f},
                    {0.966531f, 0.592093f},
                    {0.963307f, 0.658122f},
                    {0.958615f, 0.723872f},
                    {0.952432f, 0.789231f},
                    {0.921760f, 0.794069f},
                    {0.891073f, 0.798611f},
                    {0.860370f, 0.802820f},
                    {0.829651f, 0.806660f},
                    {0.798922f, 0.810103f},
                    {0.768177f, 0.813130f},
                    {0.737417f, 0.815733f},
                    {0.706646f, 0.817915f},
                    {0.675870f, 0.819689f},
                    {0.645089f, 0.821074f},
                    {0.634771f, 0.798181f},
                    {0.622839f, 0.777914f},
                    {0.609510f, 0.760618f},
                    {0.595083f, 0.746384f},
                    {0.579849f, 0.735094f},
                    {0.564063f, 0.726498f},
                    {0.547927f, 0.720295f},
                    {0.531578f, 0.716186f},
                    {0.515120f, 0.713893f},
                    {0.498614f, 0.713176f},
                    {0.482233f, 0.713936f},
                    {0.465904f, 0.716347f},
                    {0.449697f, 0.720620f},
                    {0.433709f, 0.726987f},
                    {0.418074f, 0.735688f},
                    {0.402968f, 0.746948f},
                    {0.388606f, 0.760952f},
                    {0.375247f, 0.777796f},
                    {0.363169f, 0.797458f},
                    {0.352656f, 0.819778f},
                    {0.321701f, 0.818154f},
                    {0.290753f, 0.816185f},
                    {0.259813f, 0.813869f},
                    {0.228881f, 0.811207f},
                    {0.197958f, 0.808214f},
                    {0.167046f, 0.804911f},
                    {0.136144f, 0.801328f},
                    {0.105251f, 0.797496f},
                    {0.074366f, 0.793456f},
                    {0.043489f, 0.789241f},
                    {0.039415f, 0.723503f},
                    {0.036106f, 0.657631f},
                    {0.033612f, 0.591644f},
                    {0.031984f, 0.525573f},
                    {0.031275f, 0.459451f},
                    {0.031541f, 0.393318f},
                    {0.032838f, 0.327224f},
                    {0.035224f, 0.261227f},
                    {0.038755f, 0.195392f},
                    {0.043489f, 0.129796f},
                    {0.134119f, 0.113426f},
                    {0.224922f, 0.100455f},
                    {0.315862f, 0.090978f},
                    {0.406898f, 0.085089f},
                    {0.497986f, 0.082878f},
                    {0.589078f, 0.084430f},
                    {0.680125f, 0.089827f},
                    {0.771068f, 0.099144f},
                    {0.861854f, 0.112446f},
                    {0.952427f, 0.129796f},
                }
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




