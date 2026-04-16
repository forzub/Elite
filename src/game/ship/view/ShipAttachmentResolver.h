#pragma once

#include <optional>
#include <string>
#include <glm/glm.hpp>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipTransform.h"
#include "game/debug/AttachmentEditorData.h"

struct ShipAttachmentResolved
{
    glm::vec3 worldPosition {0.0f};
    glm::mat4 worldOrientation {1.0f};
};

std::optional<ShipAttachmentResolved> resolveShipAttachment(
    const ShipDescriptor* desc,
    const ShipTransform& shipTransform,
    const std::string& attachmentId,
    const ShipAttachmentOverrideMap* overrides = nullptr
);