#pragma once

#include <optional>
#include <string>

class SpaceState;
struct ShipAttachmentPoint;

std::optional<ShipAttachmentPoint> getEditedAttachmentPoint(
    const SpaceState& state,
    const ShipAttachmentPoint& basePoint
);