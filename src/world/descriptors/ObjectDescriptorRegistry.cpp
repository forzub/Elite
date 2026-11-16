#include "src/core/SafeCallOnce.h"
#include "ObjectDescriptorRegistry.h"
#include "src/world/types/ObjectType.h"

#include "game/ship/descriptors/EliteCobraMk1.h"
#include "game/station/descriptors/Station01.h"

#include "src/game/drone/descriptors/RepairDroneDescriptor.h"
#include "src/game/station/descriptors/GuidanceTestDockDescriptor.h"
#include "src/game/station/descriptors/HubHexDockDescriptor.h"
#include "src/game/ship/descriptors/OrbitalCargoShuttleDescriptor.h"
#include "src/game/station/descriptors/HubKitDescriptor.h"

#include <mutex>

std::unordered_map<ObjectType, std::unique_ptr<IObjectDescriptor>>
    ObjectDescriptorRegistry::registry;

namespace
{
std::once_flag g_objectDescriptorRegistryInitFlag;
}


    const IObjectDescriptor& ObjectDescriptorRegistry::get(ObjectType type)
{
    return *registry.at(type);
}



void ObjectDescriptorRegistry::ensureInitialized()
{
    static std::exception_ptr failure;
    core::safeCallOnce(g_objectDescriptorRegistryInitFlag, failure, []() {
        ObjectDescriptorRegistry::init();
    });
}


void ObjectDescriptorRegistry::init()
{
    // ===== COBRA =====
    {
        auto desc = std::make_unique<ShipDescriptor>(EliteCobraMk1::EliteCobraMk1Descriptor());
        registry[ObjectType::CobraMk1] = std::move(desc);
    }

    // ===== STATION =====
    {
        auto desc = std::make_unique<StationDescriptor>();
        Station1::apply(*desc);  // ← вот это ключевая строка
        registry[ObjectType::Station] = std::move(desc);
    }
    // ===== REPAIR DRONE DEBUG =====
    {
        auto desc = std::make_unique<game::drone::RepairDroneDescriptor>();
        registry[ObjectType::RepairDroneDebug] = std::move(desc);
    }

    // ===== GUIDANCE / DOCKING TEST MODULES =====
    {
        auto desc = std::make_unique<game::station::GuidanceDockCubeDescriptor>();
        registry[ObjectType::GuidanceDockCube] = std::move(desc);
    }
    {
        auto desc = std::make_unique<game::station::GuidanceDockCylinderDescriptor>();
        registry[ObjectType::GuidanceDockCylinder] = std::move(desc);
    }

    // ===== EARTH ORBITAL HUB HEX DOCK =====
    registry[ObjectType::HubHexDock] = std::make_unique<game::station::HubHexDockDescriptor>();
    registry[ObjectType::HubDockServiceBlock] = std::make_unique<game::station::HubDockServiceBlockDescriptor>();

    // ===== OCS-4 ORBITAL CARGO SHUTTLE (dock traffic) =====
    registry[ObjectType::OrbitalCargoShuttle] =
        std::make_unique<ShipDescriptor>(game::ship::descriptors::orbitalCargoShuttleDescriptor());

    // ===== HUB DISTRICT PROTOTYPE KIT (static decoration, packages drawn in their own metres) =====
    registry[ObjectType::HubKitTruss] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_truss", glm::vec3(9.0f, 9.0f, 100.0f));
    registry[ObjectType::HubKitHabA] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_hab_a", glm::vec3(41.0f, 92.0f, 31.0f));
    registry[ObjectType::HubKitHabB] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_hab_b", glm::vec3(25.0f, 150.0f, 25.0f));
    registry[ObjectType::HubKitHabC] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_hab_c", glm::vec3(61.0f, 46.0f, 37.0f));
    registry[ObjectType::HubKitTower] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_tower", glm::vec3(22.0f, 187.0f, 22.0f));
    registry[ObjectType::HubKitMushroom] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_mushroom", glm::vec3(96.0f, 112.0f, 96.0f));
    registry[ObjectType::HubKitFactory] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_factory", glm::vec3(70.0f, 63.0f, 52.0f));
    registry[ObjectType::HubKitTanks] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_tanks", glm::vec3(50.0f, 23.0f, 34.0f));
    registry[ObjectType::HubDistrictProto] = std::make_unique<game::station::HubKitModuleDescriptor>("hub_proto_district", glm::vec3(2900.0f, 1010.0f, 1150.0f));
    registry[ObjectType::HubKitRock] = std::make_unique<game::station::HubKitModuleDescriptor>("hubk_rock", glm::vec3(760.0f, 760.0f, 760.0f));
}