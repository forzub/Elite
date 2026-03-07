
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/equipment/decryptor/DecryptorDesc.h"
#include "src/game/ship/ShipDescriptor.h"

#include "src/game/equipment/data/decryptors.h"
#include "src/game/equipment/data/transmitters.h"
#include "src/game/equipment/data/receivers.h" 
#include "src/game/equipment/data/jammers.h"
#include "src/game/equipment/data/radars.h"

#include "src/game/equipment/types/RadarType.h"
#include "src/game/equipment/types/RadarVisualProfile.h"

#include "game/damage/HitVolume.h"


const ShipDescriptor& EliteCobraMk1::EliteCobraMk1Descriptor()
{
    static ShipDescriptor desc;
    static bool initialized = false;

    if (!initialized)
    {
        initialized = true;

        desc.typeId = ShipTypeId::CobraMk1;

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

        desc.physics.angularAccel           = 3.0f;     // Угловое ускорение (рад/с²)	Масса 260т, инерция вращения
        desc.physics.angularDamping         = 2.5f;     // Затухание вращения	Стабилизация после маневра

        desc.physics.maxPitchRate           = 2.5f;     // Макс. скорость тангажа (рад/с) 
        desc.physics.maxYawRate             = 2.5f;     // Макс. скорость рысканья (рад/с)
        desc.physics.maxRollRate            = 3.0f;     // Макс. скорость крена (рад/с)
        
        desc.physics.maxCombatSpeed         = 350.0f;   // Боевая скорость (м/с) = 1260 км/ч
        desc.physics.maxCruiseSpeed         = 13500.0f; // Крейсерская (м/с) = 48000 км/ч
        desc.physics.throttleAccel          = 5.0f;     // Рывок дросселя
        
        desc.physics.autoLevelStrength      = 0.0f;     // Автовыравнивание (отключено)
        
        desc.physics.strafeAccel            = 20.0f;    // Ускорение стрейфа
        desc.physics.strafeDamping          = 6.0f;     // Затухание стрейфа
        desc.physics.maxStrafeSpeed         = 80.0f;    // Макс. скорость стрейфа

        desc.physics.maxGs                  = 5.0f;     // Макс. перегрузка (для пилота)
        desc.physics.turnRadius             = 20.0f;    // Радиус поворота (м)


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
            .cargoMass       = 50,                          // Грузоподъемность (тонн)
            .cargoVolume     = 140,                         // Объем груза (м³)
            .missileCapacity = 4                            // Ракет
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

        desc.radarCrossSection = 1.5;

        // ─────────────────────────
        // CoreSystems params 
        // ─────────────────────────
        desc.reactor.nominalPowerMW = 2500.0;           // Номинальная мощность (МВт)
        desc.reactor.peakPowerMW    = 3200.0;           // Пиковая мощность (МВт)
        
        // desc.reactor.heatFactor     = 2.2;              // Коэф. тепловыделения При КПД 45%: 1/0.45 - 1 ≈ 1.22, плюс запас

        desc.reactor.optimalTempK = 3500;               // 3500 — температура ядра, при которой КПД макс
        desc.reactor.minTempK = 2500;                   // 2500 — мин. рабочая температура
        desc.reactor.maxTempK = 4500;                   // 4500 — макс. до разрушения
        

        desc.reactor.temperature        = 300.0;           // Начальная температура (K)
        desc.reactor.thermalMass  = 120.0;            // Теплоемкость (МДж/К)	Масса реактора 25т, теплоемкость стали ~0.5 МДж/т/К ×25т ×10 = 125
        
        desc.reactor.efficiencyMinTemp      =  0.35;            // 0.35 — мин КПД (при мин/макс температуре)
        desc.reactor.efficiencyPeak         =  0.45;            // 0.45 — макс КПД
        desc.reactor.efficiencyMaxTemp      =  0.4;             // 0.40 — мин КПД (при мин/макс температуре)

       
        // ─────────────────────────
        // ThermalSystems params 
        // ─────────────────────────
        desc.thermal.thermalMassMJperK      = 100.0;    // Теплоемкость корпуса (МДж/К)	Масса корпуса 65т, теплоемкость композита ~0.8 МДж/т/К ×65т ×7 = 364 
        desc.thermal.initialTempK           = 1000.0;    // Начальная температура (K)	20°C

        // ─────────────────────────
        // CoolingSystems params                       Полная площадь корпуса: ~635 м²
        // ─────────────────────────
        desc.cooling.radiatorArea       = 380.0;    // Площадь радиаторов (м²)	Из ТТХ - 380 м²
        desc.cooling.panelCount         = 40;       // количество панелей (160)
        desc.cooling.emissivity         = 1.2;     // Коэф. черноты	Карбид кремния
        desc.cooling.maxTransferPower   = 180,0;    // Макс. теплоперенос (МВт)
        desc.cooling.nominalPower       = 2.0;      // Мощность насосов номинал (МВт)
        desc.cooling.peakPower          = 4.5;      // Мощность насосов пик (МВт)


        RadarMountPoint mount;
        mount.normalizedPosition    = {0.5f, 0.81f};
        mount.normalizedSize        = {0.3515f, 0.2083f};
        mount.maxSupportedMountSize = 1.0;
        mount.allowedProfiles = {
            game::RadarType::PPI
            // ,game::RadarVisualProfile::DigitalFlat список допустимых типов радара
        };

        desc.radarMount = mount;

        // ─────────────────────────
        // Equipment default presets 
        // ─────────────────────────
        desc.defaultEquipment.decryptor     = std::nullopt;
        desc.defaultEquipment.jammer        = std::nullopt;
        desc.defaultEquipment.receiver      = game::STANDARD_RECEIVER;
        desc.defaultEquipment.transmitter   = WSDR_TX13;
        // desc.defaultEquipment.radar         = game::PPI_CRT_RADAR;
        desc.defaultEquipment.radar         = game::PPI_LCD_RADAR;


        // -----------------------------
        // загружаем текстуру кабины или модель корабля
        // -----------------------------

        CockpitData cp;
        cp.enabled = true;
        cp.geometry = createCockpitGeometry(desc);
        cp.baseTexturePath  = "assets/img/cobra-mk1-2560x1440-cockpits.png";
        cp.glassTexturePath = "assets/img/cobra-mk1-2560x1440-glass.png";
        desc.cockpit = cp;


        // здесь можно задать 3D модель
        // desc.modelPath = "assets/models/cobra.obj";

        // ----- DAMAGE --------
        desc.hitVolumes =
        {
            { game::damage::HitZoneType::Cockpit, "cockpit", 100, {0,0.5f,6.0f}, {4,2,4}, false, -1 },

            { game::damage::HitZoneType::Cargo, "cargo", 60, {0,0,1.0f}, {8,3,7}, false, -1 },

            { game::damage::HitZoneType::Reactor, "reactor", 110, {0,0,-2.0f}, {5,3,4}, true, 1 },

            { game::damage::HitZoneType::Engine, "engine", 120, {0,0,-6.5f}, {7,3,4}, true, 3 },

            { game::damage::HitZoneType::Hull, "hull", 0, {0,0,0}, {14,5.8f,16.5f}, false, -1 }
        };


    }

    return desc;
}




