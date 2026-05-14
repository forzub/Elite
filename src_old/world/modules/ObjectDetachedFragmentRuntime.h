#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

#include "src/game/geometry/ObjectAssembly.h"
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/game/damage/HitComponent.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

namespace world::modules
{

struct ObjectDetachedFragmentRuntimeState
{
    std::string moduleId;
    // moduleId — текущий target slot id, если деталь уже заявлена.
    // originalModuleId — откуда эта деталь реально прилетела.
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

class ObjectDetachedFragmentRuntime
{
public:
    void clear();

    void update(float dt);

    void syncFromDetachedModules(
        const glm::mat4& ownerModel,
        const game::ship::geometry::ObjectAssembly& assembly,
        const ObjectAssemblyRuntime& assemblyRuntime,
        const ObjectModuleRuntime& moduleRuntime,
        const game::damage::HitComponent& hitComponent
    );

    bool setFragmentMotion(
        const std::string& moduleId,
        const glm::vec3& linearVelocity,
        const glm::vec3& angularVelocity
    );

    bool hasFragment(const std::string& moduleId) const;

    ObjectDetachedFragmentRuntimeState* findFragmentMutable(
        const std::string& moduleId
    );
    const ObjectDetachedFragmentRuntimeState* findFragment(
        const std::string& moduleId
    ) const;

    bool removeFragment(const std::string& moduleId);

    const std::vector<ObjectDetachedFragmentRuntimeState>& fragments() const
    {
        return m_fragments;
    }



    // Главная функция:
    // любая деталь, своя или чужая, заявляется как replacement для targetModuleId.
bool claimFragmentAsReplacement(
    ObjectDetachedFragmentRuntime& sourceRuntime,
    const std::string& sourceModuleId,
    const std::string& targetModuleId,
    const glm::mat4& targetHomeLocalModel,
    const glm::vec3& targetHomeCenterLocal
);

    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> buildSnapshots() const;
    

private:
    void rebuildIndex();

private:
    std::vector<ObjectDetachedFragmentRuntimeState> m_fragments;
    std::unordered_map<std::string, int> m_indexByModuleId;
};

} // namespace world::modules