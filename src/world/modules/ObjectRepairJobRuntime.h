#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/modules/ObjectDetachedFragmentRuntime.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"
#include "src/world/navigation/SmallCraftNavigation.h"
#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/modules/ObjectRepairProgram.h"

#include "src/world/coordinates/WorldPosition.h"
namespace world::modules
{

enum class ObjectRepairJobState
{
    LeavingDock,
    MovingToFragment,
    CapturingFragment,
    ReturningToStaging,
    AligningToMount,
    FinalMounting,
    PostRepairPause,
    PostRepairRetreat,
    ReadyToReattach,
    ReturningToDock,
    Docking,
    Done,
    Failed
};

struct ObjectRepairJobRuntimeState
{
    std::string moduleId;
    
    ObjectRepairJobState state = ObjectRepairJobState::MovingToFragment;
    
    // Все позиции repair runtime теперь owner-local.
    // Это НЕ глобальный world-float.
    // Глобальные координаты собираются только в snapshots.

    glm::vec3 dronePosition {0.0f}; // legacy mirror / owner-local repair runtime
    glm::vec3 droneVelocity {0.0f};
    glm::vec3 inheritedOwnerVelocity {0.0f};
    
    world::navigation::SmallCraftNavigationState droneNav;
    ObjectRepairProgram program;

    glm::vec3 dockWorldPosition {0.0f};
    glm::vec3 safeExitWorldPosition {0.0f};
    glm::vec3 stagingWorldPosition {0.0f};
    glm::vec3 postRepairExitWorldPosition {0.0f};

    glm::vec3 launchWorldPosition {0.0f};
    glm::vec3 recoveryWorldPosition {0.0f};
    std::vector<glm::vec3> repairWorkWorldPoints;

    bool pathBuilt = false;

    float droneSpeed = 10.0f;
    float droneTowSpeed = 10.0f;
    float finalMountSpeed = 1.6f;
    float captureAttachSpeed = 3.0f;

    float droneAcceleration = 260.0f;
    
    float droneTowAcceleration = 120.0f;

    float fragmentTowSpeed = 25.0f;
    float fragmentAlignRate = 2.25f;

    float captureTimer = 0.0f;
    float captureDuration = 0.35f;

    float captureDistance = 1.0f;
    float reattachDistance = 0.35f;

    float safeExitDistance = 8.0f;
    float stagingDistance = 3.0f;

    float alignTimer = 0.0f;
    float alignDuration = 0.9f;

    float postRepairPauseTimer = 0.0f;
    float postRepairPauseDuration = 0.75f;
    float postRepairRetreatDistance = 6.0f;

    glm::vec3 currentPathTarget {0.0f};
    float targetRepathDistance = 15.0f;

    float dockingSpeed = 1.5f;
    float dockingArrivalRadius = 0.15f;

    glm::vec3 alignStartPosition {0.0f};
    glm::mat4 alignStartOrientation {1.0f};

    float pathRebuildTimer = 0.0f;
    float pathRebuildInterval = 1.0f;

    float captureApproachDistance = 0.5f;
    float captureFinalDistance = 0.25f;
    glm::vec3 towOffsetWorld {0.0f};




    

};

class ObjectRepairJobRuntime
{
public:
    void clear();

    bool hasJob(const std::string& moduleId) const;

    bool startJob(
        const std::string& moduleId,
        const glm::mat4& ownerModel,
        const glm::vec3& ownerLinearVelocity,
        const glm::vec3& dockWorldPosition,
        const glm::vec3& launchWorldPosition,
        const glm::vec3& recoveryWorldPosition,
        const std::vector<glm::vec3>& repairWorkWorldPoints,
        const ObjectDetachedFragmentRuntime& detachedRuntime
    );

    std::vector<std::string> update(
        float dt,
        const glm::mat4& ownerModel,
        ObjectDetachedFragmentRuntime& detachedRuntime,
        const std::vector<world::navigation::NavigationObstacle>& obstacles
    );

    const std::vector<ObjectRepairJobRuntimeState>& jobs() const
    {
        return m_jobs;
    }

    std::vector<game::simulation::ObjectRepairJobSnapshot> buildSnapshots(
        const glm::mat4& ownerLocalModel,
        const world::coordinates::WorldPosition& ownerWorldPosition,
        const ObjectDetachedFragmentRuntime& detachedRuntime
    ) const;

    int activeJobCount() const;

private:
    static glm::vec3 moveTowards(
        const glm::vec3& current,
        const glm::vec3& target,
        float maxDistance
    );

private:
    std::vector<ObjectRepairJobRuntimeState> m_jobs;
};

} // namespace world::modules