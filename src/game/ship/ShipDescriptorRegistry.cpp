#include "ShipDescriptorRegistry.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include <stdexcept>

const ShipDescriptor& ShipDescriptorRegistry::get(ShipTypeId id)
{
    switch (id)
    {
        case ShipTypeId::CobraMk1:
            return EliteCobraMk1::EliteCobraMk1Descriptor();
    }

    throw std::runtime_error("Unknown ShipTypeId");
}
