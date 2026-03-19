#pragma once

#include <stdexcept>

#include "src/game/ship/ShipDescriptor.h"
// #include "src/game/ship/ShipTypeId.h"
#include "src/world/types/ObjectType.h"

class ShipDescriptorRegistry
{
public:
    static const ShipDescriptor& get(ObjectType id);

    static ObjectType mapShipToObjectType(ObjectType id)
    {
        switch (id)
        {
            case ObjectType::CobraMk1:
                return ObjectType::CobraMk1;

            // добавишь другие корабли сюда

            default:
                throw std::runtime_error("Unknown ObjectType");
        }
    }
};
