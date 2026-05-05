#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/modules/ObjectDetachedFragmentRuntime.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"

namespace world::modules
{

enum class ObjectRepairJobState
{
    MovingToFragment,
    CapturingFragment,
    ReturningToHome,
    ReadyToReattach,
    Done,
    Failed
};

struct ObjectRepairJobRuntimeState
{
    std::string moduleId;

    ObjectRepairJobState state = ObjectRepairJobState::MovingToFragment;

    glm::vec3 dronePosition {0.0f};

    float droneSpeed = 80.0f;
    float fragmentTowSpeed = 25.0f;

    float captureTimer = 0.0f;
    float captureDuration = 0.35f;

    float captureDistance = 1.0f;
    float reattachDistance = 0.35f;
};

class ObjectRepairJobRuntime
{
public:
    void clear();

    bool hasJob(const std::string& moduleId) const;

    bool startJob(
        const std::string& moduleId,
        const glm::mat4& ownerModel,
        const ObjectDetachedFragmentRuntime& detachedRuntime
    );

    std::vector<std::string> update(
        float dt,
        const glm::mat4& ownerModel,
        ObjectDetachedFragmentRuntime& detachedRuntime
    );

    const std::vector<ObjectRepairJobRuntimeState>& jobs() const
    {
        return m_jobs;
    }

    std::vector<game::simulation::ObjectRepairJobSnapshot> buildSnapshots(
        const glm::mat4& ownerModel,
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