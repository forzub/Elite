#pragma once

#include <string>
#include <glm/glm.hpp>
#include <vector>
#include "src/game/simulation/DebugHitVolumeSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::simulation
{

// Mass a detached module took with it (ShipCore load model): its own dry
// mass, the cargo it carried and the empty C20 boxes of that cargo. Kept on
// the fragment so salvage / repair / UI and a future rigid-body
// materialization never lose mass that left the ship.
struct DetachedFragmentMass
{
    double moduleDryKg = 0.0;
    double cargoKg = 0.0;
    double containerTareKg = 0.0;
    double totalKg = 0.0;
    bool known = false;
};

struct ObjectDetachedFragmentSnapshot
{
    std::string moduleId;
    std::string originalModuleId;
    std::string moduleClass;
    std::vector<std::string> providedReplacementTags;

    world::coordinates::WorldPosition worldPosition;
    glm::vec3 position {0.0f}; // legacy mirror 
    
    glm::mat4 orientation {1.0f};

    glm::vec3 linearVelocity {0.0f};
    glm::vec3 angularVelocity {0.0f};

    bool salvageable = false;
    bool repairable = false;
    bool canReattach = true;

    std::vector<game::simulation::DebugHitVolumeSnapshot> debugHitVolumes;
    glm::mat4 homeLocalModel {1.0f};
    glm::vec3 homeCenterLocal {0.0f};
    DetachedFragmentMass mass;



    void setWorldPosition(
        const world::coordinates::WorldPosition& p
    )
    {
        worldPosition = p;
        position = world::coordinates::legacyFloatMeters(worldPosition);
    }
};

} // namespace game::simulation