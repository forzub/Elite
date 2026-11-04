#pragma once

#include <stdexcept>
#include <string>

#include "src/game/ship/ShipDescriptor.h"
// #include "src/game/ship/ShipTypeId.h"
#include "src/world/types/ObjectType.h"

class ShipDescriptorRegistry
{
public:
    static const ShipDescriptor& get(ObjectType id);

    // A catalog ship (design passport id) resolves through the catalog
    // registry; an empty id falls back to the legacy ObjectType hull.
    static const ShipDescriptor& resolve(ObjectType id, const std::string& catalogShipId);

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
