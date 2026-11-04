// // ShipDescriptor - паспорт - что это за корабль
// // не меняется во время игры
// // может грузиться из C++ / JSON позже
// // один дескриптор → много экземпляров


#pragma once
#include "src/game/equipment/reactor/EmergencyThermalBridge.h"
#include <glm/glm.hpp>
#include <string>
#include <optional>
#include <vector>

#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/descriptors/ModuleDescriptor.h"

#include "game/ship/core/ShipParams.h"
#include "game/ship/ShipHudProfile.h"

#include "src/world/types/ObjectType.h"

#include "game/ship/cockpit/CockpitContours.h"
#include "src/world/types/SignalType.h"

#include "game/equipment/jammer/JammerDesc.h"
#include "game/equipment/decryptor/DecryptorDesc.h"
#include "game/equipment/signalNode/ReceiverDesc.h"
#include "game/equipment/signalNode/SignalTransmitterDesc.h"
#include "game/equipment/radar/RadarDesc.h"

#include "src/game/equipment/types/RadarMountPoint.h"
#include "src/game/ship/ShipAttachmentPoint.h"
#include "src/world/descriptors/LogicalDimensions.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/ship/ShipLoadout.h"
#include "src/game/catalog/ships/ShipCargoPassports.h"


struct ShipIdentity
{
    std::string shipType;
    std::string shipName;
};

struct EquipmentPresets
{
    std::optional<DecryptorDesc>         decryptor;
    std::optional<JammerDesc>            jammer;
    std::optional<game::ReceiverDesc>    receiver;
    std::optional<SignalTransmitterDesc> transmitter;
    std::optional<game::RadarDesc>       radar;
};

struct CoreConfig
{
    double reactorMaxOutputMW;
    double thermalMass;
    double maxSafeTemperature;
    double baseCoolingRate;
};

struct ShipSystemSlots
{
    int reactorSlots = 0;

    // LEGACY equipment/UI capacity. This is NOT the number of physical
    // propulsion actuators. Ordinary-flight actuator count comes exclusively
    // from physicalControlModel (Assisted/Newtonian I/Y/X).
    int engineSlots = 0;

    int radarSlots = 0;
    int weaponSlots = 0;
    int decryptorSlots = 0;
    int jammerSlots = 0;
    int dockingComputerSlots = 0;
    int receiverSlots = 0;
    int transmitterSlots = 0;
    int utilitySlots = 0;
    int fuelScopSlots = 0;
    int tractorBeamSlots = 0;
};

struct ShipDockSlots
{
    int fighterSlots = 0;
    int shuttleSlots = 0;
    int droneSlots = 0;
};

struct ShipStorageCaps
{
    // Authoritative cargo model (tools/colonization_lab/canon/ship_economics.json):
    // cargoMass is the structural payload limit in TONNES, cargoVolume the hold in m3.
    // Standard cargo containers (class C20: up to 20 t payload, 23.3 m3) ride in
    // containerSlots; a hull may limit mass before its slots are full (OCS-4).
    int cargoMass = 0;
    int cargoVolume = 0;
    int containerSlots = 0;
    int containerPayloadTonnes = 0;
    int missileCapacity = 0;
};

struct ShipSurvivalCaps
{
    bool hasEscapePod = false;
    bool hasNanokitBay = false;
};

struct SignalProfile
{
    std::vector<SignalType> supportedSignals;
};

// Static binding between physical main-engine directions and damageable module
// identities. Runtime health/state stays in ObjectModuleRuntime / snapshots;
// the descriptor only says which modules contribute to each thrust bank.
struct ShipMainPropulsionBinding
{
    std::vector<std::string> aftEngineModuleIds;
    std::vector<std::string> foreEngineModuleIds;
};

// Physical topology owns slot count/placement. A descriptor may only bind the
// already-existing slots to damageable module IDs, in physicalControlSlots()
// order. Empty IDs are allowed later for non-damageable/integrated actuators;
// adding extra entries is never a way to create new propulsion geometry.
struct ShipPhysicalControlBinding
{
    std::vector<std::string> slotModuleIds;
};

// Layout defines physical reactor/cooling positions; each ship descriptor
// binds actual damageable equipment module IDs into those existing slots.
// The order of engineReactorModuleIds matches installationSlots(model).
// An empty binding leaves the slot uninstalled; it is NOT a free reactor.
struct ShipPowerThermalInstallationBinding
{
    std::vector<std::string> engineReactorModuleIds;
    std::string shipReactorModuleId;
    std::string commonCoolingModuleId;

    // Emergency thermal coupling PROPULSION -> SHIP SERVICES: a normally
    // isolated heat exchanger with its own pump, installed as a real module
    // (its damage is its authority). Heat only - never an electrical link.
    // Empty id = not installed.
    std::string emergencyHeatExchangerModuleId;
    game::equipment::reactor::EmergencyThermalBridgeDescriptor emergencyHeatExchanger {};
    // The service coolant accepts emergency heat only below these fractions
    // of its critical temperature (open below, forced closed above).
    double emergencySinkOpenBelowCritical01 = 0.90;
    double emergencySinkCloseAboveCritical01 = 0.97;
};

// Executable part of a catalog ship design passport (GenericShipDescriptorBuilder):
// which installed modules ARE the ship's devices, and the facts of its load model.
// Legacy hull descriptors (Cobra C++ descriptor) leave passportId empty.
struct ShipCatalogRuntime
{
    // Stable runtime identity of a catalog ship (its passport id). Spawn,
    // replication, the client and damage resolve the descriptor by it; no
    // ObjectType is required.
    std::string passportId;

    // Device modules: a device works while one of its modules is attached
    // and alive (radar range / signal / life support / jump follow damage).
    std::vector<std::string> radarModuleIds;
    std::vector<std::string> receiverModuleIds;
    std::vector<std::string> transmitterModuleIds;
    std::vector<std::string> transponderModuleIds;
    std::vector<std::string> lifeSupportModuleIds;
    std::vector<std::string> computerModuleIds;
    std::vector<std::string> jumpCoreModuleIds;
    std::vector<std::string> coolantLoopModuleIds;
    std::vector<std::string> serviceReactorModuleIds;
    std::vector<std::string> dockModuleIds;      // docking ports, bays, hangars

    double radarRangeMeters = 0.0;
    double jumpCoreRatingTonnes = 0.0;
    bool jumpCoreSealed = false;
    double lifeSupportPowerMW = 0.0;
    int lifeSupportPersons = 0;
    int lifeSupportDays = 0;
    int crew = 0;
    int passengers = 0;

    // Load model: the ship's CURRENT mass is dry + these loads (ShipCore).
    double fluidsCapacityTonnes = 0.0;        // tanks: jump charge, pellets, RCS gas, cargo fluids
    double consumablesCapacityTonnes = 0.0;   // life-support stores for the endurance
    double personMassTonnes = 0.09;           // a person with kit
    double containerTareTonnes = catalog::ships::C20TareTonnes; // canonical empty C20 container
    std::vector<std::string> tankModuleIds;
    std::vector<double> tankCapacityTonnes;
    std::vector<std::string> cargoHolderModuleIds;   // C20 racks, bulk holds, cryo tanks, train cars
    std::vector<double> cargoHolderCapacityTonnes;
    std::vector<std::string> cabinModuleIds;         // where people and stores are (life support, passenger modules)

    [[nodiscard]] bool valid() const noexcept { return !passportId.empty(); }
};

// Placeholder geometry of a hull without a mesh: one primitive box per
// installed position (canonical body frame, metres), tied to its module.
struct ShipPrimitiveBlock
{
    std::string moduleId;
    std::string category;      // equipment category or "hull"
    glm::vec3 centerMeters {0.0f};
    glm::vec3 halfSizeMeters {0.0f};
};

struct CockpitData
{
    bool enabled = false;

    CockpitGeometry geometry;
    std::string baseTexturePath;
    std::string glassTexturePath;
};

struct ReactorDescriptor
{
    double nominalPowerMW;
    double peakPowerMW;

    double optimalTempK;
    double minTempK;
    double maxTempK;

    double temperature;
    double thermalMass;

    double efficiencyMinTemp;
    double efficiencyPeak;
    double efficiencyMaxTemp;
};

struct CoolingDescriptor
{
    double radiatorArea;
    int panelCount;
    double emissivity;
    double maxTransferPower;
    double nominalPower;
    double peakPower;
    // Stable universe material ID, independent from any render texture or
    // future trade commodity asset.
    std::string radiatorMaterialId;
};

struct ThermalDescriptor
{
    double thermalMassMJperK = 850.0;
    double initialTempK = 293.0;
};

struct ShipMeshPart
{
    std::string name;

    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;

    int sectionIndex = 0;
};

struct ShipBoundingEllipsoid
{
    glm::vec3 center {0.0f};
    glm::vec3 radius {1.0f};
};

struct ShipMeshSection
{
    std::string name;
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    std::vector<int> triangleIndices;
};

class ShipDescriptor : public IObjectDescriptor
{
public:
    const std::string& meshId() const override { return m_meshId; }
    bool isLargeObject() const override { return false; }
    glm::vec3 getMeshSizeMeters() const override { return meshSizeMeters; }
    const LogicalDimensions& logicalDimensions() const override
    {
        return logicalDimensionsValue;
    }

    const std::vector<ModuleDescriptor>& moduleDescriptors() const override
    {
        return modules;
    }

    const glm::vec3& visualBasisRotationDeg() const override
    {
        return visualBasisRotationDegValue;
    }

    const glm::vec3& meshForwardAxis() const override
    {
        return meshForwardAxisValue;
    }

    const glm::vec3& meshUpAxis() const override
    {
        return meshUpAxisValue;
    }

    

    ObjectType          typeId;
    ShipIdentity        identity;

    // Physical ordinary-flight architecture. This selects exactly one of the
    // six fixed Assisted/Newtonian I/Y/X models: its slot STRUCTURE (count,
    // roles, directions, generator wiring) is fixed; the mount COORDINATES are
    // the hull's own (design passport), registered by its loadout
    // (ShipLoadoutTopology.h registerLoadoutTopology, plant/ActuatorTopology.h).
    game::ship::plant::PhysicalControlModelId physicalControlModel =
        game::ship::plant::PhysicalControlModelId::AssistedI;

    // Compatibility mirror for the current motion stack. During migration this
    // must match nativeFlightLaw(physicalControlModel). The physical model is
    // the long-term source of truth; runtime damage changes capability/regime,
    // not the construction family.
    game::navigation::LocalFlightControlLaw localFlightControlLaw =
        game::navigation::defaultLocalFlightControlLaw();

    ShipParams          physics;
    ShipHudProfile      hud;

    ShipSystemSlots     systems;
    ShipDockSlots       docks;
    ShipStorageCaps     storage;
    ShipSurvivalCaps    survival;
    SignalProfile       signalProfile;
    ShipMainPropulsionBinding mainPropulsion;
    ShipPhysicalControlBinding physicalControl;
    ShipPowerThermalInstallationBinding powerThermalInstallation;

    // Installable hardware configurations of this hull, one per legacy
    // PhysicalControlModelId. Empty only for legacy/test descriptors that
    // still rely on the fixed physicalControlSlots() tables.
    std::vector<ShipLoadoutDefinition> loadouts;

    [[nodiscard]] const ShipLoadoutDefinition* findLoadout(
        game::ship::plant::PhysicalControlModelId model
    ) const noexcept
    {
        for (const auto& loadout : loadouts)
            if (loadout.legacyModelId == model)
                return &loadout;
        return nullptr;
    }

    ReactorDescriptor               reactor;
    CoolingDescriptor               cooling;
    ThermalDescriptor               thermal;
    EquipmentPresets                defaultEquipment;
    std::optional<CockpitData>      cockpit;

    double                          radarCrossSection = 1.0;
    double                          radarSignatureModifier = 1.0;
    RadarMountPoint                 radarMount;

    // glm::vec3                       meshSizeMeters {1.0f,1.0f,1.0f};
    // glm::vec3                       visualBasisRotationDegValue {0.0f, 0.0f, 0.0f};
    // std::vector<ShipMeshPart>       meshParts;

    glm::vec3                       meshSizeMeters {1.0f,1.0f,1.0f};
    LogicalDimensions               logicalDimensionsValue {};

    // LEGACY: старый визуальный костыль для fallback single-mesh path
    glm::vec3                       visualBasisRotationDegValue {0.0f, 0.0f, 0.0f};

    // НОВОЕ: авторский базис меша
    glm::vec3                       meshForwardAxisValue {0.0f, 0.0f, -1.0f};
    glm::vec3                       meshUpAxisValue      {0.0f, 1.0f,  0.0f};

    std::vector<ShipMeshPart>       meshParts;



    ShipBoundingEllipsoid           boundingEllipsoid;
    std::vector<ShipMeshSection>    meshSections;
    std::vector<ShipAttachmentPoint> attachments;

    // НОВОЕ: логические модули корабля
    std::vector<ModuleDescriptor>   modules;

    // Catalog ship passport facts the runtime executes (empty for legacy hulls)
    // and the placeholder geometry of a hull without a mesh.
    ShipCatalogRuntime              catalog;
    std::vector<ShipPrimitiveBlock> primitiveBlocks;
    

private:
    std::string m_meshId;
};
