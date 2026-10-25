
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/ship/presets/cobra/CobraRcsEquipmentAssembly.h"
#include <stdexcept>
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

#include <cstdlib>
#include <string>
#include <array>
#include <algorithm>

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

        // Fixed ordinary-flight construction: Assisted-I.
        // Two aft + two fore vectoring main engines; no dedicated ordinary RCS
        // belongs to this physical model. The legacy law field mirrors the
        // selected hardware family until the old aggregate stack is retired.
        desc.physicalControlModel =
            game::ship::plant::PhysicalControlModelId::AssistedI;
        desc.localFlightControlLaw =
            game::ship::plant::nativeFlightLaw(desc.physicalControlModel);

        // -------------------------
        // Physics
        // -------------------------

        desc.physics.angularAccel           = 3.0f;     // Угловое ускорение (рад/с²)	Масса 260т, инерция вращения
        desc.physics.angularDamping         = 2.5f;     // Затухание вращения	Стабилизация после маневра

        desc.physics.maxPitchRate           = 2.5f;     // Макс. скорость тангажа (рад/с) 
        desc.physics.maxYawRate             = 2.5f;     // Макс. скорость рысканья (рад/с)
        desc.physics.maxRollRate            = 3.0f;     // Макс. скорость крена (рад/с)
        
        desc.physics.maxCombatSpeed         = 500.0f;   // Controlled local speed envelope (m/s); external impulse may exceed it
        // desc.physics.maxCruiseSpeed         = 13500.0f; // Крейсерская (м/с) = 48000 км/ч
        desc.physics.maxCruiseSpeed         = 29979245.0f; // Крейсерская (м/с) = 0.1с
        desc.physics.throttleAccel          = 5.0f;     // Рывок дросселя

        // Assisted Cobra has two real longitudinal main-thrust banks.
        // Aft engines push along the nose direction; fore engines push in the
        // opposite direction and provide full-power assisted braking.
        desc.physics.forwardMainEngineAvailable = true;
        desc.physics.reverseMainEngineAvailable = true;
        desc.physics.forwardMainEngineAccelerationMps2 = 73.549875f; // 7.5 g
        desc.physics.reverseMainEngineAccelerationMps2 = 73.549875f; // 7.5 g

        // Main-nozzle TVC hardware used by the Newtonian proving variants.
        // 7 deg travel with 8 deg/s slew is intentionally modest; Assisted
        // currently ignores this explicit gimbal channel.
        desc.physics.mainGimbalPitchLimitRad = 0.122173048f; // 7 deg
        desc.physics.mainGimbalYawLimitRad = 0.122173048f;   // 7 deg
        desc.physics.mainGimbalRateRadPerSec = 0.139626340f; // 8 deg/s
        
        desc.physics.autoLevelStrength      = 0.0f;     // Автовыравнивание (отключено)
        
        desc.physics.strafeAccel            = 73.549875f; // Assisted automatic lateral authority (7.5 g envelope)
        desc.physics.strafeDamping          = 6.0f;     // Assisted velocity-direction response
        desc.physics.maxStrafeSpeed         = 80.0f;    // Legacy assisted translation envelope
        desc.physics.manoeuvreThrusterAccel = 2.0f;     // Small physical RCS acceleration
        desc.physics.manoeuvreGasUsePerSecond = 0.20f;  // Full one-axis manual burn
        desc.physics.manoeuvreGasRechargePerSecond = 0.08f;
        desc.physics.manoeuvreGasRestartFraction = 0.20f;

        desc.physics.maxGs                  = 5.0f;     // Макс. перегрузка (для пилота)
        desc.physics.maxLinearGs            = 7.5f;     // +/- local dv/dt envelope: 1.5x previous response
        desc.physics.turnRadius             = 20.0f;    // Радиус поворота (м)

        // Cobra is a broad, flat platform. 260 t is the current baseline mass.
        // Principal inertias use a conservative box approximation for the
        // 26.0 x 5.0 x 22.2 m logical hull and are intentionally explicit so
        // off-centre impacts can produce physically meaningful spin.
        desc.physics.massKg                  = 260000.0;
        // Current Cobra logical origin is its nominal dry CoM. Keep this
        // explicit so future marks/cargo layouts never inherit origin==CoM.
        desc.physics.centerOfMassLocalMeters  = {0.0, 0.0, 0.0};
        desc.physics.pitchInertiaKgM2        = 11219866.6666667;
        desc.physics.yawInertiaKgM2          = 25324866.6666667;
        desc.physics.rollInertiaKgM2         = 15188333.3333333;


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


        // Авторский базис именно ЭТОГО OBJ, не универсальные оси Blender:
        // -X = forward, +Y = up, -Z = right (forward x up).
        // IObjectDescriptor::meshToLogicalBasis4() maps it once into
        // the ship's fixed logical +X right, +Y up, -Z forward basis.
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
        desc.reactor.nominalPowerMW = 70.0;           // Номинальная мощность (МВт)
        desc.reactor.peakPowerMW    = 70.0;           // Пиковая мощность (МВт)
        
        // desc.reactor.heatFactor     = 2.2;              // Коэф. тепловыделения При КПД 45%: 1/0.45 - 1 ≈ 1.22, плюс запас

        desc.reactor.optimalTempK = 3500;               // 3500 — температура ядра, при которой КПД макс
        desc.reactor.minTempK = 2500;                   // 2500 — мин. рабочая температура
        desc.reactor.maxTempK = 4500;                   // 4500 — макс. до разрушения
        

        desc.reactor.temperature        = 3500.0;           // Начальная температура (K)
        desc.reactor.thermalMass        = 120.0;            // Теплоемкость (МДж/К)	Масса реактора 25т, теплоемкость стали ~0.5 МДж/т/К ×25т ×10 = 125
        
        desc.reactor.efficiencyMinTemp      =  0.9960;            // cold boundary
        desc.reactor.efficiencyPeak         =  0.9965;            // peak at 3500K
        desc.reactor.efficiencyMaxTemp      =  0.9955;             // hot boundary

       
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
        desc.cooling.emissivity         = 0.9;     // Материальная излучательная способность; игровой x2.5 — общий закон
        desc.cooling.maxTransferPower   = 180.0;
        desc.cooling.radiatorMaterialId = "elite.material.radiator.c-sic.v1";    // Макс. теплоперенос (МВт)
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

#if defined(ELITE_CLIENT_PRESENTATION)
        // Cockpit geometry/textures are client presentation assets.
        // The authoritative/headless descriptor deliberately leaves
        // this field empty, so the server never needs cockpit geometry.
        CockpitData cp;
        cp.enabled = true;
        cp.geometry = createCockpitGeometry(desc);
        cp.baseTexturePath  = "assets/img/cobra-mk1-2560x1440-cockpits.png";
        cp.glassTexturePath = "assets/img/cobra-mk1-2560x1440-glass.png";
        desc.cockpit = cp;
#endif


        // здесь можно задать 3D модель
        // desc.modelPath = "assets/models/cobra.obj";

        // ----- DAMAGE --------
        desc.boundingEllipsoid.center = {0,0,0};
        desc.boundingEllipsoid.radius = {0.5,0.15,0.35};

auto structuralCore = [](
    const char* moduleId,
    const char* parentId,
    const char* subsystemId,
    std::vector<std::string> meshParts,
    std::vector<std::string> supports,
    game::damage::HitZoneType zone,
    float maxHealth,
    float armor,
    float penRes,
    int layerIndex,
    int hitPriority,
    bool critical = false,
    bool repairable = false
) -> ModuleDescriptor
{
    ModuleDescriptor m;
    m.moduleId = moduleId;
    m.parentModuleId = parentId ? parentId : "";
    m.subsystemId = subsystemId ? subsystemId : "";
    m.meshPartIds = std::move(meshParts);
    m.zone = zone;
    m.enabled = true;
    m.destructible = true;
    m.detachable = false;
    m.salvageable = false;
    m.repairable = repairable;
    m.hangable = false;
    m.detachPolicy = ModuleDetachPolicy::Never;
    m.destroyPolicy = ModuleDestroyPolicy::DisableOnly;
    m.attachmentType = ModuleAttachmentType::Structural;
    m.structuralKind = ModuleStructuralKind::StructuralFrame;
    m.autoSupportLinkHealth = 500.0f;
    m.autoSupportImpulseTolerance = 1200.0f;
    m.layerIndex = layerIndex;
    m.hitPriority = hitPriority;
    m.maxHealth = maxHealth;
    m.armor = armor;
    m.penetrationResistance = penRes;
    m.isCritical = critical;
    m.destroysParentIfCatastrophic = false;
    m.destroysWholeObjectIfCatastrophic = false;

    // ВАЖНО: больше не разносить всё дерево автоматически
    m.detachChildrenOnLoss = false;
    m.destroyChildrenOnLoss = false;
    m.disableChildrenOnLoss = true;

    m.supportModuleIds = std::move(supports);
    m.minSupportsForAttached = m.supportModuleIds.empty() ? 0 : 1;
    m.minSupportsForStable = m.minSupportsForAttached;
    return m;
};

auto internalModule = [](
    const char* moduleId,
    const char* parentId,
    const char* subsystemId,
    std::vector<std::string> meshParts,
    game::damage::HitZoneType zone,
    float maxHealth,
    float armor,
    float penRes,
    int layerIndex,
    int hitPriority,
    bool critical = false,
    bool repairable = true
) -> ModuleDescriptor
{
    ModuleDescriptor m;
    m.moduleId = moduleId;
    m.parentModuleId = parentId ? parentId : "";
    m.subsystemId = subsystemId ? subsystemId : "";
    m.meshPartIds = std::move(meshParts);
    m.zone = zone;
    m.enabled = true;
    m.destructible = true;
    m.detachable = false;
    m.salvageable = false;
    m.repairable = repairable;
    m.hangable = false;
    m.detachPolicy = ModuleDetachPolicy::Never;
    m.destroyPolicy = ModuleDestroyPolicy::DisableOnly;
    m.attachmentType = ModuleAttachmentType::Internal;
    m.structuralKind = ModuleStructuralKind::InternalBlock;
    m.autoSupportLinkHealth = 0.0f;
    m.autoSupportImpulseTolerance = 0.0f;
    m.layerIndex = layerIndex;
    m.hitPriority = hitPriority;
    m.maxHealth = maxHealth;
    m.armor = armor;
    m.penetrationResistance = penRes;
    m.isCritical = critical;
    m.destroysParentIfCatastrophic = false;
    m.destroysWholeObjectIfCatastrophic = false;
    m.detachChildrenOnLoss = false;
    m.destroyChildrenOnLoss = false;
    m.disableChildrenOnLoss = true;
    m.supportModuleIds = {};
    m.minSupportsForAttached = 0;
    m.minSupportsForStable = 0;
    return m;
};

auto detachableEquipment = [](
    const char* moduleId,
    const char* parentId,
    const char* subsystemId,
    std::vector<std::string> meshParts,
    std::vector<std::string> supports,
    game::damage::HitZoneType zone,
    ModuleAttachmentType attachmentType,
    float maxHealth,
    float armor,
    float penRes,
    int layerIndex,
    int hitPriority,
    bool critical = false,
    bool salvageable = true,
    bool repairable = true
) -> ModuleDescriptor
{
    ModuleDescriptor m;
    m.moduleId = moduleId;
    m.parentModuleId = parentId ? parentId : "";
    m.subsystemId = subsystemId ? subsystemId : "";
    m.meshPartIds = std::move(meshParts);
    m.zone = zone;
    m.enabled = true;
    m.destructible = true;
    m.detachable = true;
    m.salvageable = salvageable;
    m.repairable = repairable;
    m.hangable = false;
    m.detachPolicy = ModuleDetachPolicy::OnDestroyed;
    m.destroyPolicy = ModuleDestroyPolicy::Detach;
    m.attachmentType = attachmentType;
    m.structuralKind = ModuleStructuralKind::Equipment;
    m.autoSupportLinkHealth = 80.0f;
    m.autoSupportImpulseTolerance = 450.0f;
    m.layerIndex = layerIndex;
    m.hitPriority = hitPriority;
    m.maxHealth = maxHealth;
    m.armor = armor;
    m.penetrationResistance = penRes;
    m.isCritical = critical;
    m.destroysParentIfCatastrophic = false;
    m.destroysWholeObjectIfCatastrophic = false;
    m.detachChildrenOnLoss = false;
    m.destroyChildrenOnLoss = false;
    m.disableChildrenOnLoss = false;
    m.supportModuleIds = std::move(supports);
    m.minSupportsForAttached = m.supportModuleIds.empty() ? 0 : 1;
    m.minSupportsForStable = m.minSupportsForAttached;
    return m;
};

auto detachablePanel = [](
    const char* moduleId,
    const char* parentId,
    const char* subsystemId,
    std::vector<std::string> meshParts,
    std::vector<std::string> supports,
    float maxHealth,
    float armor,
    float penRes,
    int hitPriority
) -> ModuleDescriptor
{
    ModuleDescriptor m;
    m.moduleId = moduleId;
    m.parentModuleId = parentId ? parentId : "";
    m.subsystemId = subsystemId ? subsystemId : "";
    m.meshPartIds = std::move(meshParts);
    m.zone = game::damage::HitZoneType::Hull;
    m.enabled = true;
    m.destructible = true;
    m.detachable = true;
    m.salvageable = true;
    m.repairable = true;
    m.hangable = true;
    m.detachPolicy = ModuleDetachPolicy::OnDestroyed;
    m.destroyPolicy = ModuleDestroyPolicy::Detach;
    m.attachmentType = ModuleAttachmentType::SurfaceMounted;
    m.structuralKind = ModuleStructuralKind::OuterPanel;
    m.autoSupportLinkHealth = 45.0f;
    m.autoSupportImpulseTolerance = 220.0f;
    m.layerIndex = 0;
    m.hitPriority = hitPriority;
    m.maxHealth = maxHealth;
    m.armor = armor;
    m.penetrationResistance = penRes;
    m.isCritical = false;
    m.destroysParentIfCatastrophic = false;
    m.destroysWholeObjectIfCatastrophic = false;
    m.detachChildrenOnLoss = false;
    m.destroyChildrenOnLoss = false;
    m.disableChildrenOnLoss = false;
    m.supportModuleIds = std::move(supports);
    m.minSupportsForAttached = m.supportModuleIds.size() >= 2 ? 2 : (m.supportModuleIds.empty() ? 0 : 1);
    m.minSupportsForStable = m.supportModuleIds.empty() ? 0 : 1;
    return m;
};


desc.modules =
{
    // =====================================================
    // FRONT AVIONICS / COCKPIT
    // =====================================================

    internalModule(
        "ship_cockpit",
        "ship_frame_CF",
        "avionics_front",
        { "ship_cockpit" },
        game::damage::HitZoneType::Cockpit,
        90.0f,
        10.0f,
        16.0f,
        1,
        120,
        true,
        true
    ),

    detachableEquipment(
        "ship_radar",
        "ship_frame_CF",
        "sensor_radar",
        { "ship_radar" },
        { "ship_frame_CF" },
        game::damage::HitZoneType::Generic,
        ModuleAttachmentType::SurfaceMounted,
        25.0f,
        4.0f,
        6.0f,
        0,
        80,
        false,
        true,
        true
    ),

    internalModule(
        "ship_control_unit_L",
        "ship_frame_FL",
        "flight_control",
        { "ship_control_unit_L" },
        game::damage::HitZoneType::Generic,
        40.0f,
        8.0f,
        10.0f,
        1,
        95,
        true,
        true
    ),

    internalModule(
        "ship_control_unit_R",
        "ship_frame_FR",
        "flight_control",
        { "ship_control_unit_R" },
        game::damage::HitZoneType::Generic,
        40.0f,
        8.0f,
        10.0f,
        1,
        95,
        true,
        true
    ),

    // =====================================================
    // WEAPONS
    // =====================================================

    detachableEquipment(
        "ship_laser_L",
        "ship_frame_FL",
        "weapon_mounts",
        { "ship_laser_L" },
        { "ship_frame_FL" },
        game::damage::HitZoneType::Generic,
        ModuleAttachmentType::Hardpoint,
        35.0f,
        6.0f,
        8.0f,
        0,
        90,
        false,
        true,
        true
    ),

    detachableEquipment(
        "ship_laser_R",
        "ship_frame_FR",
        "weapon_mounts",
        { "ship_laser_R" },
        { "ship_frame_FR" },
        game::damage::HitZoneType::Generic,
        ModuleAttachmentType::Hardpoint,
        35.0f,
        6.0f,
        8.0f,
        0,
        90,
        false,
        true,
        true
    ),

    // =====================================================
    // OUTER SKIN PANELS
    // =====================================================

    detachablePanel(
        "ship_panel_bottom_LB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_bottom_LB" },
        { "ship_frame_CB", "ship_frame_FL" },
        28.0f,
        8.0f,
        10.0f,
        20
    ),

    detachablePanel(
        "ship_panel_bottom_LF",
        "ship_frame_FL",
        "outer_skin",
        { "ship_panel_bottom_LF" },
        { "ship_frame_FL", "ship_frame_CF" },
        28.0f,
        8.0f,
        10.0f,
        20
    ),

    detachablePanel(
        "ship_panel_bottom_RB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_bottom_RB" },
        { "ship_frame_CB", "ship_frame_FR" },
        28.0f,
        8.0f,
        10.0f,
        20
    ),

    detachablePanel(
        "ship_panel_bottom_RF",
        "ship_frame_FR",
        "outer_skin",
        { "ship_panel_bottom_RF" },
        { "ship_frame_FR", "ship_frame_CF" },
        28.0f,
        8.0f,
        10.0f,
        20
    ),

    detachablePanel(
        "ship_panel_bottom_C",
        "ship_frame_CF",
        "outer_skin",
        { "ship_panel_bottom_C" },
        { "ship_frame_CF", "ship_frame_reactor" },
        32.0f,
        9.0f,
        11.0f,
        20
    ),

    detachablePanel(
        "ship_panel_side_LB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_side_LB" },
        { "ship_frame_CB", "ship_frame_FL" },
        30.0f,
        8.0f,
        10.0f,
        22
    ),

    detachablePanel(
        "ship_panel_side_LF",
        "ship_frame_FL",
        "outer_skin",
        { "ship_panel_side_LF" },
        { "ship_frame_FL", "ship_frame_CF" },
        30.0f,
        8.0f,
        10.0f,
        22
    ),

    detachablePanel(
        "ship_panel_side_RB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_side_RB" },
        { "ship_frame_CB", "ship_frame_FR" },
        30.0f,
        8.0f,
        10.0f,
        22
    ),

    detachablePanel(
        "ship_panel_side_RF",
        "ship_frame_FR",
        "outer_skin",
        { "ship_panel_side_RF" },
        { "ship_frame_FR", "ship_frame_CF" },
        30.0f,
        8.0f,
        10.0f,
        22
    ),

    detachablePanel(
        "ship_panel_back",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_back" },
        { "ship_frame_CB", "ship_frame_reactor" },
        36.0f,
        10.0f,
        12.0f,
        30
    ),

    detachablePanel(
        "ship_panel_top_CB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_top_CB" },
        { "ship_frame_CB", "ship_frame_reactor" },
        30.0f,
        8.0f,
        10.0f,
        24
    ),

    detachablePanel(
        "ship_panel_top_CF",
        "ship_frame_CF",
        "outer_skin",
        { "ship_panel_top_CF" },
        { "ship_frame_CF", "ship_frame_reactor" },
        30.0f,
        8.0f,
        10.0f,
        24
    ),

    detachablePanel(
        "ship_panel_top_LB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_top_LB" },
        { "ship_frame_CB", "ship_frame_FL" },
        30.0f,
        8.0f,
        10.0f,
        24
    ),

    detachablePanel(
        "ship_panel_top_LF",
        "ship_frame_FL",
        "outer_skin",
        { "ship_panel_top_LF" },
        { "ship_frame_FL", "ship_frame_CF" },
        30.0f,
        8.0f,
        10.0f,
        24
    ),

    detachablePanel(
        "ship_panel_top_RB",
        "ship_frame_CB",
        "outer_skin",
        { "ship_panel_top_RB" },
        { "ship_frame_CB", "ship_frame_FR" },
        30.0f,
        8.0f,
        10.0f,
        24
    ),

    detachablePanel(
        "ship_panel_top_RF",
        "ship_frame_FR",
        "outer_skin",
        { "ship_panel_top_RF" },
        { "ship_frame_FR", "ship_frame_CF" },
        30.0f,
        8.0f,
        10.0f,
        24
    ),

    // =====================================================
    // MAIN STRUCTURE
    // =====================================================

    structuralCore(
        "ship_frame_CB",
        "",
        "main_frame",
        { "ship_frame_CB" },
        {},
        game::damage::HitZoneType::Hull,
        160.0f,
        28.0f,
        36.0f,
        2,
        70,
        true,
        false
    ),

    structuralCore(
        "ship_frame_CF",
        "ship_frame_CB",
        "main_frame",
        { "ship_frame_CF" },
        { "ship_frame_CB" },
        game::damage::HitZoneType::Hull,
        170.0f,
        30.0f,
        38.0f,
        2,
        75,
        true,
        false
    ),

    structuralCore(
        "ship_frame_FL",
        "ship_frame_CF",
        "main_frame",
        { "ship_frame_FL" },
        { "ship_frame_CF", "ship_frame_reactor" },
        game::damage::HitZoneType::Hull,
        145.0f,
        26.0f,
        34.0f,
        2,
        85,
        false,
        false
    ),

    structuralCore(
        "ship_frame_FR",
        "ship_frame_CF",
        "main_frame",
        { "ship_frame_FR" },
        { "ship_frame_CF", "ship_frame_reactor" },
        game::damage::HitZoneType::Hull,
        145.0f,
        26.0f,
        34.0f,
        2,
        85,
        false,
        false
    ),

    // =====================================================
    // FORE PROPULSION BLOCK
    // =====================================================
    //
    // Physical nose-mounted longitudinal engines. Their module identities
    // participate in damage/repair/propulsion truth even before dedicated
    // visible nozzle meshes are authored.
    internalModule(
        "ship_fore_engine_L",
        "ship_frame_CF",
        "fore_propulsion_block",
        {},
        game::damage::HitZoneType::Engine,
        80.0f,
        14.0f,
        18.0f,
        2,
        98,
        true,
        true
    ),

    internalModule(
        "ship_fore_engine_R",
        "ship_frame_CF",
        "fore_propulsion_block",
        {},
        game::damage::HitZoneType::Engine,
        80.0f,
        14.0f,
        18.0f,
        2,
        98,
        true,
        true
    ),

    // =====================================================
    // REAR PROPULSION BLOCK
    // =====================================================

    structuralCore(
        "ship_frame_reactor",
        "ship_frame_CB",
        "propulsion_block",
        { "ship_frame_reactor" },
        { "ship_frame_CB", "ship_frame_CF" },
        game::damage::HitZoneType::Reactor,
        180.0f,
        34.0f,
        44.0f,
        3,
        110,
        true,
        false
    ),

    detachableEquipment(
        "ship_engine_L",
        "ship_frame_reactor",
        "propulsion_block",
        { "ship_engine_L" },
        { "ship_frame_reactor", "ship_frame_CB" },
        game::damage::HitZoneType::Engine,
        ModuleAttachmentType::Hardpoint,
        90.0f,
        16.0f,
        20.0f,
        3,
        100,
        true,
        true,
        true
    ),

    detachableEquipment(
        "ship_engine_R",
        "ship_frame_reactor",
        "propulsion_block",
        { "ship_engine_R" },
        { "ship_frame_reactor", "ship_frame_CB" },
        game::damage::HitZoneType::Engine,
        ModuleAttachmentType::Hardpoint,
        90.0f,
        16.0f,
        20.0f,
        3,
        100,
        true,
        true,
        true
    ),

    detachableEquipment(
        "ship_tank_L",
        "ship_frame_reactor",
        "fuel_system",
        { "ship_tank_L" },
        { "ship_frame_reactor", "ship_frame_FL" },
        game::damage::HitZoneType::Cargo,
        ModuleAttachmentType::Internal,
        65.0f,
        10.0f,
        14.0f,
        3,
        96,
        true,
        false,
        false
    ),

    detachableEquipment(
        "ship_tank_R",
        "ship_frame_reactor",
        "fuel_system",
        { "ship_tank_R" },
        { "ship_frame_reactor", "ship_frame_FR" },
        game::damage::HitZoneType::Cargo,
        ModuleAttachmentType::Internal,
        65.0f,
        10.0f,
        14.0f,
        3,
        96,
        true,
        false,
        false
    )
};

// Installed Cobra motors all reuse a single named engine TYPE, not
// ship-specific copies of thrust, thermodynamics or service law.
{
    const auto engine =
        game::equipment::engine::presets::CobrettyCR1VectorMainDrive();
    for (auto& module : desc.modules)
    {
        if (module.zone == game::damage::HitZoneType::Engine)
        {
            module.equipmentFailure.failureAtDamage01 = 0.95;
            module.maintenance.wearPerOperatingHour = 0.0;
            module.maintenance.maintenanceWearThreshold01 = 0.20;
            module.engine = engine;
        }
    }
}

// Preinstall the complete Assisted hardware inventory in the real module
// runtime. Ctrl+F9 selects ONE persistent set; it never manufactures motors.
// Extra configurations have logical damageable units but no duplicate mesh
// ownership: the exterior geometry remains the Cobra's common hull.
if (game::ship::plant::familyOf(desc.physicalControlModel) ==
        game::ship::plant::PhysicalControlFamily::Assisted)
{
    const auto findPrototype=[&](const char* id) -> ModuleDescriptor {
        const auto it=std::find_if(desc.modules.begin(),desc.modules.end(),
            [&](const ModuleDescriptor& m){return m.moduleId==id;});
        if(it==desc.modules.end() || !it->engine)
            throw std::logic_error("Missing Cobra CR-1 hardware prototype");
        return *it;
    };
    const ModuleDescriptor aftPrototype=findPrototype("ship_engine_L");
    const ModuleDescriptor forePrototype=findPrototype("ship_fore_engine_L");
    for(const auto model: {
            game::ship::plant::PhysicalControlModelId::AssistedY,
            game::ship::plant::PhysicalControlModelId::AssistedX})
    {
        const auto ids=
            game::ship::presets::cobra::installedAssistedEngineModuleIds(model);
        const auto& slots=game::ship::presets::cobra::propulsionSlots(model);
        for(std::size_t i=0;i<slots.size();++i)
        {
            ModuleDescriptor module=
                slots[i].role==game::ship::plant::EngineSlotRole::AftMain
                    ? aftPrototype : forePrototype;
            module.moduleId=ids[i];
            module.meshPartIds.clear();
            module.supportLinks.clear();
            desc.modules.push_back(std::move(module));
        }
    }
}

// Native Newtonian I/Y/X use REAL installed aft drive modules as well.
// The same equipment graph contains all alternatives; switching the selected
// construction does not manufacture engines or erase their prior damage.
{
    const auto prototype=std::find_if(desc.modules.begin(),desc.modules.end(),
        [](const ModuleDescriptor& m){return m.moduleId=="ship_engine_L";});
    if(prototype==desc.modules.end() || !prototype->engine)
        throw std::logic_error("Missing Cobra Newtonian main prototype");
    const ModuleDescriptor mainPrototype=*prototype;
    for(const auto model:{
        game::ship::plant::PhysicalControlModelId::NewtonianI,
        game::ship::plant::PhysicalControlModelId::NewtonianY,
        game::ship::plant::PhysicalControlModelId::NewtonianX})
    {
        const auto& slots=game::ship::presets::cobra::propulsionSlots(model);
        const auto ids=
            game::ship::presets::cobra::installedNewtonianEngineModuleIds(model);
        if(ids.size()!=slots.size())
            throw std::logic_error("Invalid Newtonian main bindings");
        for(std::size_t i=0;i<slots.size();++i)
        {
            if(slots[i].role!=game::ship::plant::EngineSlotRole::AftMain)
                continue;
            ModuleDescriptor module=mainPrototype;
            module.moduleId=ids[i];
            module.meshPartIds.clear();
            module.supportLinks.clear();
            desc.modules.push_back(std::move(module));
        }
    }
}

desc.mainPropulsion.aftEngineModuleIds = {
    "ship_engine_L",
    "ship_engine_R"
};
desc.mainPropulsion.foreEngineModuleIds = {
    "ship_fore_engine_L",
    "ship_fore_engine_R"
};

desc.physicalControl.slotModuleIds = {
    // Assisted-I topology order:
    // aft_left, aft_right, fore_left, fore_right.
    "ship_engine_L",
    "ship_engine_R",
    "ship_fore_engine_L",
    "ship_fore_engine_R"
};

// TEST-ONLY selection of an installed physical-model topology.
//
// An earlier diagnostic test cleared the actuator module bindings to get
// synthetic healthy motors. That breaks the actual server->client
// equipment-state contract: the server executes REAL installed CR-1s while
// the client/HUD fabricates healthy motors and suppresses emergency-axis
// evidence. Assisted-I/Y/X always bind their genuine installed module IDs.
// The legacy unbound Newtonian test topology remains separate.
if (const char* rawModel =
        std::getenv("ELITE_SHIP_PHYSICAL_TEST_MODEL"))
{
    using Model =
        game::ship::plant::PhysicalControlModelId;

    const std::string model(rawModel);
    bool recognized = true;

    if (model == "Assisted-I")
        desc.physicalControlModel = Model::AssistedI;
    else if (model == "Assisted-Y")
        desc.physicalControlModel = Model::AssistedY;
    else if (model == "Assisted-X")
        desc.physicalControlModel = Model::AssistedX;
    else if (model == "Newtonian-I")
        desc.physicalControlModel = Model::NewtonianI;
    else if (model == "Newtonian-Y")
        desc.physicalControlModel = Model::NewtonianY;
    else if (model == "Newtonian-X")
        desc.physicalControlModel = Model::NewtonianX;
    else
        recognized = false;

    if (recognized)
    {
        desc.localFlightControlLaw =
            game::ship::plant::nativeFlightLaw(
                desc.physicalControlModel
            );
        if (game::ship::plant::familyOf(desc.physicalControlModel) ==
                game::ship::plant::PhysicalControlFamily::Assisted)
        {
            // Exact IDs of persistent installed CR-1s in the module runtime.
            // No diagnostic healthy fallback on the client.
            desc.physicalControl.slotModuleIds =
                game::ship::presets::cobra::installedAssistedEngineModuleIds(
                    desc.physicalControlModel);
        }
        else
        {
            desc.physicalControl.slotModuleIds =
                game::ship::presets::cobra::installedNewtonianEngineModuleIds(
                    desc.physicalControlModel);
            desc.mainPropulsion.aftEngineModuleIds.clear();
            desc.mainPropulsion.foreEngineModuleIds.clear();
            const auto& slots=game::ship::plant::physicalControlSlots(
                desc.physicalControlModel);
            for(std::size_t i=0;i<slots.size();++i)
                if(slots[i].role==game::ship::plant::EngineSlotRole::AftMain)
                    desc.mainPropulsion.aftEngineModuleIds.push_back(
                        desc.physicalControl.slotModuleIds[i]);
        }
    }
}


// A real ER-5 belongs to each installed hardware group, with independent
// damage, temperature, output and lifetime. Alternate Assisted layouts keep
// their own reactor identities across Ctrl+F9 selections.
{
    using Model=game::ship::plant::PhysicalControlModelId;
    // Every one of the six hardware layouts is physically installed.
    // Only the selected reactor groups are active at any instant.
    const auto er5 =
        game::equipment::reactor::presets::
            CobraCobrettyER5PropulsionReactor();
    desc.powerThermalInstallation.engineReactorModuleIds.clear();
    const auto addReactorSet=[&](Model model)
    {
        const auto& installation =
            game::ship::presets::cobra::installationSlots(model);
        const bool assisted=game::ship::plant::familyOf(model)==
            game::ship::plant::PhysicalControlFamily::Assisted;
        const auto realIds=assisted
            ? game::ship::presets::cobra::installedAssistedReactorModuleIds(model)
            : game::ship::presets::cobra::installedNewtonianReactorModuleIds(model);
        if(realIds.size()!=installation.engineReactors.size())
            throw std::logic_error("Invalid Cobra drive reactor bindings");
        for(std::size_t i=0;i<installation.engineReactors.size();++i)
        {
            const auto& slot=installation.engineReactors[i];
            const std::string reactorId=realIds[i];
            ModuleDescriptor m;
            m.moduleId = reactorId;
        m.parentModuleId = "ship_frame_CB";
        m.subsystemId = "propulsion_reactor";
        m.moduleClass = "cobra_engine_reactor";
        m.zone = game::damage::HitZoneType::Reactor;
        m.attachmentType = ModuleAttachmentType::Internal;
        m.structuralKind = ModuleStructuralKind::Equipment;
        m.supportModuleIds = {"ship_frame_CB"};
        m.maxHealth = 100.0f;
        m.destructible = true;
        m.repairable = true;
        m.isCritical = false;
        m.equipmentFailure = er5.failure;
        m.maintenance = er5.maintenance;
        m.engineReactor = er5;
            if(model==desc.physicalControlModel)
                desc.powerThermalInstallation.engineReactorModuleIds.push_back(
                    m.moduleId);
            desc.modules.push_back(std::move(m));
        }
    };
    for(const auto model:{
        Model::AssistedI,Model::AssistedY,Model::AssistedX,
        Model::NewtonianI,Model::NewtonianY,Model::NewtonianX})
        addReactorSet(model);
    // Ordinary ship services remain separately powered and cooled.
    desc.powerThermalInstallation.shipReactorModuleId =
        "ship_service_reactor";
    desc.powerThermalInstallation.commonCoolingModuleId =
        "ship_common_cooling";
    for (const auto& service :
            {desc.powerThermalInstallation.shipReactorModuleId,
             desc.powerThermalInstallation.commonCoolingModuleId})
    {
        ModuleDescriptor m;
        m.moduleId = service;
        m.parentModuleId = "ship_frame_CB";
        m.subsystemId = service;
        m.moduleClass = service;
        m.zone = game::damage::HitZoneType::Generic;
        m.attachmentType = ModuleAttachmentType::Internal;
        m.structuralKind = ModuleStructuralKind::Equipment;
        m.supportModuleIds = {"ship_frame_CB"};
        m.maxHealth = 100.0f;
        m.destructible = true;
        m.repairable = true;
        desc.modules.push_back(std::move(m));
    }
}

// Newtonian Cobra has the same 24 individually installed RCS nozzle
// equipment units for I/Y/X. Create these before the module runtime is
// initialized so hits, wear and BITE refer to real module identities.
// Native Newtonian flight uses this installed 24-nozzle physical executor;
// no virtual omni-cluster propulsion is allowed in the production model.
{
    // These 24 stable ObjectModuleRuntime nozzle IDs must exist even while
    // an Assisted construction is selected, so Ctrl+F10 can select a native
    // Newtonian build without creating or repairing any equipment.
    if (!game::ship::presets::cobra::addNewtonianRcsEquipmentModules(
            desc.modules))
    {
        throw std::logic_error(
            "Cobra Newtonian RCS module IDs are not unique"
        );
    }
}


for (auto& m : desc.modules)
{
    if (m.moduleClass.empty())
    {
        if (m.moduleId == "ship_cockpit")
            m.moduleClass = "cockpit_capsule";
        else if (m.structuralKind == ModuleStructuralKind::OuterPanel)
            m.moduleClass = "outer_panel";
        else if (m.structuralKind == ModuleStructuralKind::Equipment)
            m.moduleClass = "equipment";
        else
            m.moduleClass = m.moduleId;
    }

    if (m.providedReplacementTags.empty())
    {
        m.providedReplacementTags.push_back("cobra_mk1");
        m.providedReplacementTags.push_back(m.moduleClass);
        m.providedReplacementTags.push_back(m.moduleId);
    }

    if (m.acceptedReplacementTags.empty())
    {
        m.acceptedReplacementTags.push_back("cobra_mk1");
        m.acceptedReplacementTags.push_back(m.moduleClass);
        m.acceptedReplacementTags.push_back(m.moduleId);
    }
}




for (auto& m : desc.modules)
{
    if (m.moduleId == "ship_cockpit")
    {
        m.detachable = true;
        m.salvageable = true;
        m.repairable = true;
        m.hangable = false;

        m.detachPolicy = ModuleDetachPolicy::OnDestroyed;
        m.destroyPolicy = ModuleDestroyPolicy::Detach;
        m.attachmentType = ModuleAttachmentType::Hardpoint;
        m.structuralKind = ModuleStructuralKind::Equipment;

        m.supportModuleIds = { "ship_frame_CF" };
        m.minSupportsForAttached = 1;
        m.minSupportsForStable = 1;

        m.autoSupportLinkHealth = 120.0f;
        m.autoSupportImpulseTolerance = 350.0f;

        m.moduleClass = "cockpit_capsule";

        m.providedReplacementTags.clear();
        m.providedReplacementTags.push_back("cobra_mk1");
        m.providedReplacementTags.push_back("cockpit_capsule");
        m.providedReplacementTags.push_back("cobra_mk1_cockpit_capsule");

        m.acceptedReplacementTags.clear();
        m.acceptedReplacementTags.push_back("cobra_mk1");
        m.acceptedReplacementTags.push_back("cockpit_capsule");
        m.acceptedReplacementTags.push_back("cobra_mk1_cockpit_capsule");

        break;
    }
}


        desc.attachments =
        {
            ShipAttachmentPoint{
                .id = "camera_cockpit_main",
                .parentModuleId = "ship_cockpit",
                .kind = ShipAttachmentKind::CameraCockpit,
                .localPosition = glm::vec3(0.0f, 1.5f, -11.35f),
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
                .localPosition = glm::vec3(-9.0f, 1.0f, 0.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "drone_dock_main",
                .parentModuleId = "ship_frame_CB",
                .kind = ShipAttachmentKind::DroneDock,
                .localPosition = glm::vec3(-9.0f, 1.0f, 0.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },

            ShipAttachmentPoint{
                .id = "drone_launch_main",
                .parentModuleId = "ship_frame_CB",
                .kind = ShipAttachmentKind::DroneLaunch,
                .localPosition = glm::vec3(-9.0f, -4.0f, 0.0f),
                .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
                .enabled = true
            },



ShipAttachmentPoint{
    .id = "repair_drone_dock_main",
    .parentModuleId = "ship_frame_CB",
    .kind = ShipAttachmentKind::DroneDock,
    .localPosition = glm::vec3( 9.0f, 1.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_drone_launch_main",
    .parentModuleId = "ship_frame_CB",
    .kind = ShipAttachmentKind::DroneLaunch,
    .localPosition = glm::vec3(9.0f, -4.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_drone_recovery_main",
    .parentModuleId = "ship_frame_CB",
    .kind = ShipAttachmentKind::DroneRecovery,
    .localPosition = glm::vec3(9.0f, -4.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},





ShipAttachmentPoint{
    .id = "drone_recovery_main",
    .parentModuleId = "ship_frame_CB",
    .kind = ShipAttachmentKind::DroneRecovery,
    .localPosition = glm::vec3(9.0f, 1.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_work_top",
    .parentModuleId = "",
    .kind = ShipAttachmentKind::RepairWorkPoint,
    .localPosition = glm::vec3(0.0f, 8.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_work_bottom",
    .parentModuleId = "",
    .kind = ShipAttachmentKind::RepairWorkPoint,
    .localPosition = glm::vec3(0.0f, -8.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_work_left",
    .parentModuleId = "",
    .kind = ShipAttachmentKind::RepairWorkPoint,
    .localPosition = glm::vec3(-8.0f, 0.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_work_right",
    .parentModuleId = "",
    .kind = ShipAttachmentKind::RepairWorkPoint,
    .localPosition = glm::vec3(8.0f, 0.0f, 0.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_work_front",
    .parentModuleId = "",
    .kind = ShipAttachmentKind::RepairWorkPoint,
    .localPosition = glm::vec3(0.0f, 0.0f, -12.0f),
    .localRotationDeg = glm::vec3(0.0f, 0.0f, 0.0f),
    .enabled = true
},

ShipAttachmentPoint{
    .id = "repair_work_back",
    .parentModuleId = "",
    .kind = ShipAttachmentKind::RepairWorkPoint,
    .localPosition = glm::vec3(0.0f, 0.0f, 8.0f),
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




