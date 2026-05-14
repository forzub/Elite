// // ShipDescriptor - паспорт - что это за корабль
// // не меняется во время игры
// // может грузиться из C++ / JSON позже
// // один дескриптор → много экземпляров


#pragma once
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
    int cargoMass = 0;
    int cargoVolume = 0;
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
    ShipParams          physics;
    ShipHudProfile      hud;

    ShipSystemSlots     systems;
    ShipDockSlots       docks;
    ShipStorageCaps     storage;
    ShipSurvivalCaps    survival;
    SignalProfile       signalProfile;

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
    

private:
    std::string m_meshId;
};
