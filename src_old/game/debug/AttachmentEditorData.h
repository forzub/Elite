#pragma once

#include <string>
#include <unordered_map>
#include <glm/vec3.hpp>

struct AttachmentEditorOverride
{
    glm::vec3 localPosition{0.0f, 0.0f, 0.0f};
    glm::vec3 localRotationDeg{0.0f, 0.0f, 0.0f};
    bool enabled = true;
};

using ShipAttachmentOverrideMap = std::unordered_map<std::string, AttachmentEditorOverride>;