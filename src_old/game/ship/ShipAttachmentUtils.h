#pragma once

#include <vector>   
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


inline const ShipAttachmentPoint* findFirstShipAttachmentPointByKind(
    const ShipDescriptor* desc,
    ShipAttachmentKind kind
)
{
    if (!desc)
        return nullptr;

    for (const auto& p : desc->attachments)
    {
        if (p.kind == kind && p.enabled)
            return &p;
    }

    return nullptr;
}

inline std::vector<const ShipAttachmentPoint*> findShipAttachmentPointsByKind(
    const ShipDescriptor* desc,
    ShipAttachmentKind kind
)
{
    std::vector<const ShipAttachmentPoint*> out;

    if (!desc)
        return out;

    for (const auto& p : desc->attachments)
    {
        if (p.kind == kind && p.enabled)
            out.push_back(&p);
    }

    return out;
}