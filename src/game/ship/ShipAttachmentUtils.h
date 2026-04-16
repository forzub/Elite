#pragma once

#include "src/game/ship/ShipDescriptor.h"

inline const ShipAttachmentPoint* findShipAttachmentPoint(
    const ShipDescriptor* desc,
    const std::string& id
)
{
    if (!desc)
        return nullptr;

    for (const auto& p : desc->attachments)
    {
        if (p.id == id)
            return &p;
    }

    return nullptr;
}