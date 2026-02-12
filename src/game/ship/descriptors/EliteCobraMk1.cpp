
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/equipment/decryptor/DecryptorDesc.h"
#include "src/game/ship/ShipDescriptor.h"

#include "src/game/equipment/data/decryptors.h"
#include "src/game/equipment/data/transmitters.h"
#include "src/game/equipment/data/receivers.h" 
#include "src/game/equipment/data/jammers.h"


const ShipDescriptor& EliteCobraMk1::EliteCobraMk1Descriptor(ShipRole role)
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
                {0.049211f, 0.101576f},
                {0.075277f, 0.089215f},
                {0.398547f, 0.060972f},
                {0.602887f, 0.060972f},
                {0.923668f, 0.087896f},
                {0.951477f, 0.101576f},
                {0.963645f, 0.174410f},
                {0.966375f, 0.364208f},
                {0.964141f, 0.553125f},
                {0.893129f, 0.654208f},
                {0.826094f, 0.730125f},
                {0.674145f, 0.750868f},
                {0.637398f, 0.715556f},
                {0.605863f, 0.701875f},
                {0.559684f, 0.689958f},
                {0.500000f, 0.684222f},
                {0.456398f, 0.688632f},
                {0.400531f, 0.699667f},
                {0.367016f, 0.712028f},
                {0.326793f, 0.750868f},
                {0.174098f, 0.730569f},
                {0.101598f, 0.649347f},
                {0.036797f, 0.551799f},
                {0.034559f, 0.333750f},
                {0.037293f, 0.174847f},
                {0.049211f, 0.101576f},
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


        // -----------------------------
        // загружаем текстуру кабины или модель корабля
        // -----------------------------
        if (role == ShipRole::Player)
            {
                CockpitData cp;

                cp.enabled = true;
                cp.geometry = createCockpitGeometry(desc);
                cp.baseTexturePath  = "assets/img/cobra-mk1-2560x1440-cockpits.png";
                cp.glassTexturePath = "assets/img/cobra-mk1-2560x1440-glass.png";

                desc.cockpit = cp;
            }
            else
            {
                desc.cockpit = std::nullopt;

                // здесь можно задать 3D модель
                // desc.modelPath = "assets/models/cobra.obj";
            }

    }

    return desc;
}




