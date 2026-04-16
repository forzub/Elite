
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
#include "src/world/descriptors/LogicalDimensions.h"

// #include "game/damage/HitVolume.h"


const ShipDescriptor& EliteCobraMk1::EliteCobraMk1Descriptor()
{
    static ShipDescriptor desc;
    static bool initialized = false;

    if (!initialized)
    {
        initialized = true;

        desc.typeId = ObjectType::CobraMk1;

        // ─────────────────────────
        // Identity
        // ─────────────────────────
        desc.identity = {
            "cobretty-fnr1",
            "COBRETTY-FNR1"
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
        // Real Mesh Size, m
        // ─────────────────────────
        desc.meshSizeMeters = glm::vec3(
                24.81f,
                5.5f,
                15.1f
            );

            

        // Смысловые размеры Cobra в логическом игровом базисе:
        // length = nose -> tail
        // width  = left -> right
        // height = bottom -> top
        desc.logicalDimensionsValue.length = 22.2f;
        desc.logicalDimensionsValue.width  = 26.0f;
        desc.logicalDimensionsValue.height = 5.00f;
        desc.logicalDimensionsValue.scaleReference = ScaleReference::Length;
        desc.logicalDimensionsValue.enabled = true;


        // Авторский базис меша Cobra (Blender / OBJ):
        // -X = forward
        // +Z = up
        // +Y = right
        desc.meshForwardAxisValue = glm::vec3(-1.0f, 0.0f, 0.0f);
        desc.meshUpAxisValue      = glm::vec3( 0.0f, 1.0f, 0.0f);

        // LEGACY: оставляем временно только для старого single-mesh fallback path
        desc.visualBasisRotationDegValue = glm::vec3(0.0f, -90.0f, 0.0f);    


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
        desc.reactor.thermalMass        = 120.0;            // Теплоемкость (МДж/К)	Масса реактора 25т, теплоемкость стали ~0.5 МДж/т/К ×25т ×10 = 125
        
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
        desc.boundingEllipsoid.center = {0,0,0};
        desc.boundingEllipsoid.radius = {0.5,0.15,0.35};


                desc.modules =
        {
            // =====================================================
            // FRONT AVIONICS / COCKPIT
            // =====================================================

            ModuleDescriptor{
                .moduleId = "ship_cockpit",
                .parentModuleId = "ship_frame_CF",
                .subsystemId = "avionics_front",
                .meshPartIds = { "ship_cockpit" },
                .zone = game::damage::HitZoneType::Cockpit,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Internal,
                .layerIndex = 1,
                .hitPriority = 120,
                .maxHealth = 90.0f,
                .armor = 10.0f,
                .penetrationResistance = 16.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "ship_radar",
                .parentModuleId = "ship_frame_CF",
                .subsystemId = "sensor_radar",
                .meshPartIds = { "ship_radar" },
                .zone = game::damage::HitZoneType::Generic,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 80,
                .maxHealth = 25.0f,
                .armor = 4.0f,
                .penetrationResistance = 6.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_control_unit_L",
                .parentModuleId = "ship_frame_FL",
                .subsystemId = "flight_control",
                .meshPartIds = { "ship_control_unit_L" },
                .zone = game::damage::HitZoneType::Generic,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Internal,
                .layerIndex = 1,
                .hitPriority = 95,
                .maxHealth = 40.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "ship_control_unit_R",
                .parentModuleId = "ship_frame_FR",
                .subsystemId = "flight_control",
                .meshPartIds = { "ship_control_unit_R" },
                .zone = game::damage::HitZoneType::Generic,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Internal,
                .layerIndex = 1,
                .hitPriority = 95,
                .maxHealth = 40.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            // =====================================================
            // WEAPONS
            // =====================================================

            ModuleDescriptor{
                .moduleId = "ship_laser_L",
                .parentModuleId = "ship_frame_FL",
                .subsystemId = "weapon_mounts",
                .meshPartIds = { "ship_laser_L" },
                .zone = game::damage::HitZoneType::Generic,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Hardpoint,
                .layerIndex = 0,
                .hitPriority = 90,
                .maxHealth = 35.0f,
                .armor = 6.0f,
                .penetrationResistance = 8.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_laser_R",
                .parentModuleId = "ship_frame_FR",
                .subsystemId = "weapon_mounts",
                .meshPartIds = { "ship_laser_R" },
                .zone = game::damage::HitZoneType::Generic,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Hardpoint,
                .layerIndex = 0,
                .hitPriority = 90,
                .maxHealth = 35.0f,
                .armor = 6.0f,
                .penetrationResistance = 8.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            // =====================================================
            // OUTER SKIN PANELS
            // =====================================================

            ModuleDescriptor{
                .moduleId = "ship_panel_bottom_LB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_bottom_LB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 20,
                .maxHealth = 28.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_bottom_LF",
                .parentModuleId = "ship_frame_FL",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_bottom_LF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 20,
                .maxHealth = 28.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_bottom_RB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_bottom_RB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 20,
                .maxHealth = 28.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_bottom_RF",
                .parentModuleId = "ship_frame_FR",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_bottom_RF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 20,
                .maxHealth = 28.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_bottom_C",
                .parentModuleId = "ship_frame_CF",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_bottom_C" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 20,
                .maxHealth = 32.0f,
                .armor = 9.0f,
                .penetrationResistance = 11.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_side_LB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_side_LB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 22,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_side_LF",
                .parentModuleId = "ship_frame_FL",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_side_LF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 22,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_side_RB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_side_RB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 22,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_side_RF",
                .parentModuleId = "ship_frame_FR",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_side_RF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 22,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_back",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_back" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 30,
                .maxHealth = 36.0f,
                .armor = 10.0f,
                .penetrationResistance = 12.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_top_CB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_top_CB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 24,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_top_CF",
                .parentModuleId = "ship_frame_CF",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_top_CF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 24,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_top_LB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_top_LB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 24,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_top_LF",
                .parentModuleId = "ship_frame_FL",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_top_LF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 24,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_top_RB",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_top_RB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 24,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_panel_top_RF",
                .parentModuleId = "ship_frame_FR",
                .subsystemId = "outer_skin",
                .meshPartIds = { "ship_panel_top_RF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 24,
                .maxHealth = 30.0f,
                .armor = 8.0f,
                .penetrationResistance = 10.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            // =====================================================
            // MAIN STRUCTURE
            // =====================================================

            ModuleDescriptor{
                .moduleId = "ship_frame_CB",
                .parentModuleId = "",
                .subsystemId = "main_frame",
                .meshPartIds = { "ship_frame_CB" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 2,
                .hitPriority = 70,
                .maxHealth = 160.0f,
                .armor = 28.0f,
                .penetrationResistance = 36.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "ship_frame_CF",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "main_frame",
                .meshPartIds = { "ship_frame_CF" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 2,
                .hitPriority = 75,
                .maxHealth = 170.0f,
                .armor = 30.0f,
                .penetrationResistance = 38.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "ship_frame_FL",
                .parentModuleId = "ship_frame_CF",
                .subsystemId = "main_frame",
                .meshPartIds = { "ship_frame_FL" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 2,
                .hitPriority = 85,
                .maxHealth = 145.0f,
                .armor = 26.0f,
                .penetrationResistance = 34.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "ship_frame_FR",
                .parentModuleId = "ship_frame_CF",
                .subsystemId = "main_frame",
                .meshPartIds = { "ship_frame_FR" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 2,
                .hitPriority = 85,
                .maxHealth = 145.0f,
                .armor = 26.0f,
                .penetrationResistance = 34.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            // =====================================================
            // REAR PROPULSION BLOCK
            // =====================================================

            ModuleDescriptor{
                .moduleId = "ship_frame_reactor",
                .parentModuleId = "ship_frame_CB",
                .subsystemId = "propulsion_block",
                .meshPartIds = { "ship_frame_reactor" },
                .zone = game::damage::HitZoneType::Reactor,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 3,
                .hitPriority = 110,
                .maxHealth = 180.0f,
                .armor = 34.0f,
                .penetrationResistance = 44.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "ship_engine_L",
                .parentModuleId = "ship_frame_reactor",
                .subsystemId = "propulsion_block",
                .meshPartIds = { "ship_engine_L" },
                .zone = game::damage::HitZoneType::Engine,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Hardpoint,
                .layerIndex = 3,
                .hitPriority = 100,
                .maxHealth = 90.0f,
                .armor = 16.0f,
                .penetrationResistance = 20.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_engine_R",
                .parentModuleId = "ship_frame_reactor",
                .subsystemId = "propulsion_block",
                .meshPartIds = { "ship_engine_R" },
                .zone = game::damage::HitZoneType::Engine,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Hardpoint,
                .layerIndex = 3,
                .hitPriority = 100,
                .maxHealth = 90.0f,
                .armor = 16.0f,
                .penetrationResistance = 20.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_tank_L",
                .parentModuleId = "ship_frame_reactor",
                .subsystemId = "fuel_system",
                .meshPartIds = { "ship_tank_L" },
                .zone = game::damage::HitZoneType::Cargo,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Internal,
                .layerIndex = 3,
                .hitPriority = 96,
                .maxHealth = 65.0f,
                .armor = 10.0f,
                .penetrationResistance = 14.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "ship_tank_R",
                .parentModuleId = "ship_frame_reactor",
                .subsystemId = "fuel_system",
                .meshPartIds = { "ship_tank_R" },
                .zone = game::damage::HitZoneType::Cargo,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Internal,
                .layerIndex = 3,
                .hitPriority = 96,
                .maxHealth = 65.0f,
                .armor = 10.0f,
                .penetrationResistance = 14.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            }
        };


        desc.attachments =
        {
            ShipAttachmentPoint{
                .id = "camera_cockpit_main",
                .parentModuleId = "ship_cockpit",
                .kind = ShipAttachmentKind::CameraCockpit,
                .localPosition = glm::vec3(0.0f, 1.5f, -11.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "camera_rear_main",
                .parentModuleId = "ship_frame_CB",
                .kind = ShipAttachmentKind::CameraRear,
                .localPosition = glm::vec3(0.0f, 5.0f, -7.0f),
                .localRotationDeg = glm::vec3(3.0f, 180.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "camera_drone_default",
                .parentModuleId = "ship_frame_CB",
                .kind = ShipAttachmentKind::CameraDrone,
                .localPosition = glm::vec3(0.0f, 4.0f, -12.0f),
                .localRotationDeg = glm::vec3(10.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "drone_dock_main",
                .parentModuleId = "ship_frame_CB",
                .kind = ShipAttachmentKind::DroneDock,
                .localPosition = glm::vec3(0.0f, -1.2f, -5.5f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "weapon_muzzle_L",
                .parentModuleId = "ship_laser_L",
                .kind = ShipAttachmentKind::WeaponMuzzle,
                .localPosition = glm::vec3(0.0f, 0.0f, 1.2f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "weapon_muzzle_R",
                .parentModuleId = "ship_laser_R",
                .kind = ShipAttachmentKind::WeaponMuzzle,
                .localPosition = glm::vec3(0.0f, 0.0f, 1.2f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "missile_rack_L",
                .parentModuleId = "ship_frame_FL",
                .kind = ShipAttachmentKind::MissileRack,
                .localPosition = glm::vec3(-1.2f, -0.35f, -1.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "missile_rack_R",
                .parentModuleId = "ship_frame_FR",
                .kind = ShipAttachmentKind::MissileRack,
                .localPosition = glm::vec3(1.2f, -0.35f, -1.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "container_mount_center",
                .parentModuleId = "ship_frame_CB",
                .kind = ShipAttachmentKind::ContainerMount,
                .localPosition = glm::vec3(0.0f, -1.5f, 0.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            }
        };

        


    }

    return desc;
}




