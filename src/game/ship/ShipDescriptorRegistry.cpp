#include "ShipDescriptorRegistry.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include <stdexcept>

#include "src/world/descriptors/ObjectDescriptorRegistry.h"


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
    std::cout << "[ShipDescriptorRegistry] type: " << static_cast<uint16_t>(id) << "\n";
    
    const auto& base = ObjectDescriptorRegistry::get(id);
    
    auto* shipDesc = dynamic_cast<const ShipDescriptor*>(&base);

    if (!shipDesc)
        throw std::runtime_error("Object is not ShipDescriptor");

    return *shipDesc;
}

