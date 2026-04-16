#pragma once

#include <string>
#include <nlohmann/json.hpp>

class SpaceState;

nlohmann::json buildAttachmentEditorPayload(const SpaceState& state);