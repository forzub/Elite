#include "ObjectDescriptorRegistry.h"
#include "src/world/types/ObjectType.h"

#include "game/ship/descriptors/EliteCobraMk1.h"
#include "game/station/descriptors/Station01.h"



std::unordered_map<ObjectType, std::unique_ptr<IObjectDescriptor>>
    ObjectDescriptorRegistry::registry;


    const IObjectDescriptor& ObjectDescriptorRegistry::get(ObjectType type)
{
    return *registry.at(type);
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
}