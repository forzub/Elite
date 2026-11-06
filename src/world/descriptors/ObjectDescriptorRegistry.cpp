#include "src/core/SafeCallOnce.h"
#include "ObjectDescriptorRegistry.h"
#include "src/world/types/ObjectType.h"

#include "game/ship/descriptors/EliteCobraMk1.h"
#include "game/station/descriptors/Station01.h"

#include "src/game/drone/descriptors/RepairDroneDescriptor.h"
#include "src/game/station/descriptors/GuidanceTestDockDescriptor.h"
#include "src/game/station/descriptors/HubHexDockDescriptor.h"
#include "src/game/ship/descriptors/OrbitalCargoShuttleDescriptor.h"

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
}