#pragma once

#include <string>
#include <glm/glm.hpp>
#include <vector>
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

namespace game::simulation
{

struct ObjectDetachedFragmentSnapshot
{
    std::string moduleId;
    std::string originalModuleId;
    std::string moduleClass;
    std::vector<std::string> providedReplacementTags;

    glm::vec3 position {0.0f};
    glm::mat4 orientation {1.0f};

    glm::vec3 linearVelocity {0.0f};
    glm::vec3 angularVelocity {0.0f};

    bool salvageable = false;
    bool repairable = false;
    bool canReattach = true;

    std::vector<game::simulation::DebugHitVolumeSnapshot> debugHitVolumes;
    glm::mat4 homeLocalModel {1.0f};
    glm::vec3 homeCenterLocal {0.0f};
};

} // namespace game::simulation