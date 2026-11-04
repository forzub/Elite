#include "ShipDescriptorRegistry.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include <stdexcept>

#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"

const ShipDescriptor& ShipDescriptorRegistry::resolve(ObjectType id, const std::string& catalogShipId)
{
    if (!catalogShipId.empty())
    {
        if (const auto* d = game::ship::descriptors::findCatalogShip(catalogShipId))
            return *d;
        throw std::runtime_error("[ShipDescriptorRegistry] unknown catalog ship " + catalogShipId + ": " +
                                 game::ship::descriptors::CatalogShipRegistry::instance().errorOf(catalogShipId));
    }
    return get(id);
}


// const ShipDescriptor& ShipDescriptorRegistry::get(ObjectType id)
// {
//     std::cout << "[ShipDescriptorRegistry] type: " << static_cast<uint16_t>(id) << "\n";
//     switch (id)
//     {
//         case ObjectType::CobraMk1:
//             return EliteCobraMk1::EliteCobraMk1Descriptor();
//     }

//     throw std::runtime_error("[ShipDescriptorRegistry] Unknown ObjectType");
// }


const ShipDescriptor& ShipDescriptorRegistry::get(ObjectType id)
{
    
    const auto& base = ObjectDescriptorRegistry::get(id);
    
    auto* shipDesc = dynamic_cast<const ShipDescriptor*>(&base);

    if (!shipDesc)
        throw std::runtime_error("Object is not ShipDescriptor");

    return *shipDesc;
}

