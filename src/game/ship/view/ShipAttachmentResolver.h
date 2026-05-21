#pragma once

#include <optional>
#include <string>
#include <glm/glm.hpp>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/core/ShipTransform.h"
#include "game/debug/AttachmentEditorData.h"
#include <vector>
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
struct ShipAttachmentResolved
{
    // Позиция attachment в локальном player-frame.
    // Для камеры это именно то, что надо передавать в Camera::setPosition().
    glm::vec3 localPosition {0.0f};

    // Мировая позиция в новой double/cell системе.
    world::coordinates::WorldPosition worldPosition;

    // Ориентация без трансляции.
    glm::mat4 worldOrientation {1.0f};
};

std::optional<ShipAttachmentResolved> resolveShipAttachment(
    const ShipDescriptor* desc,
    const ShipTransform& shipTransform,
    const std::string& attachmentId,
    const ShipAttachmentOverrideMap* overrides = nullptr,
    const std::vector<game::simulation::ObjectDetachedFragmentSnapshot>* detachedFragments = nullptr
);