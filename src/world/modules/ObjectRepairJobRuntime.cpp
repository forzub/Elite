#include "ObjectRepairJobRuntime.h"

#include <algorithm>
#include <iostream>

#include <glm/gtx/norm.hpp>

namespace world::modules
{

void ObjectRepairJobRuntime::clear()
{
    m_jobs.clear();
}

bool ObjectRepairJobRuntime::hasJob(const std::string& moduleId) const
{
    for (const auto& job : m_jobs)
    {
        if (job.moduleId == moduleId &&
            job.state != ObjectRepairJobState::Done &&
            job.state != ObjectRepairJobState::Failed)
        {
            return true;
        }
    }

    return false;
}

glm::vec3 ObjectRepairJobRuntime::moveTowards(
    const glm::vec3& current,
    const glm::vec3& target,
    float maxDistance
)
{
    const glm::vec3 delta = target - current;
    const float dist2 = glm::length2(delta);

    if (dist2 <= 0.000001f)
        return target;

    const float dist = std::sqrt(dist2);

    if (dist <= maxDistance)
        return target;

    return current + delta / dist * maxDistance;
}

bool ObjectRepairJobRuntime::startJob(
    const std::string& moduleId,
    const glm::mat4& ownerModel,
    const ObjectDetachedFragmentRuntime& detachedRuntime
)
{
    if (hasJob(moduleId))
    {
        std::cout
            << "[ObjectRepairJobRuntime] startJob ignored, already active moduleId="
            << moduleId << "\n";
        return false;
    }

    if (!detachedRuntime.hasFragment(moduleId))
    {
        std::cout
            << "[ObjectRepairJobRuntime] startJob failed, no detached fragment moduleId="
            << moduleId << "\n";
        return false;
    }

    ObjectRepairJobRuntimeState job;
    job.moduleId = moduleId;

    // Пока дрон стартует из центра корабля.
    // Потом можно заменить на docking bay / drone bay attachment.
    job.dronePosition = glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

    m_jobs.push_back(std::move(job));

    std::cout
        << "[ObjectRepairJobRuntime] repair job started moduleId="
        << moduleId << "\n";

    return true;
}

std::vector<std::string> ObjectRepairJobRuntime::update(
    float dt,
    const glm::mat4& ownerModel,
    ObjectDetachedFragmentRuntime& detachedRuntime
)
{
    std::vector<std::string> completed;

    if (dt <= 0.0f)
        return completed;

    for (auto& job : m_jobs)
    {
        if (job.state == ObjectRepairJobState::Done ||
            job.state == ObjectRepairJobState::Failed)
        {
            continue;
        }

        auto* fragment =
            detachedRuntime.findFragmentMutable(job.moduleId);

        if (!fragment)
        {
            job.state = ObjectRepairJobState::Failed;

            std::cout
                << "[ObjectRepairJobRuntime] job failed, fragment missing moduleId="
                << job.moduleId << "\n";

            continue;
        }

        const glm::mat4 targetWorldModel =
            ownerModel * fragment->homeLocalModel;

        const glm::vec3 homePosition =
            glm::vec3(targetWorldModel * glm::vec4(0, 0, 0, 1));

        glm::mat4 homeOrientation = targetWorldModel;
        homeOrientation[3] = glm::vec4(0, 0, 0, 1);

        switch (job.state)
        {
            case ObjectRepairJobState::MovingToFragment:
                {
                    // Repair job включает "tractor lock":
                    // фрагмент перестаёт улетать, иначе дрон может никогда его не догнать.
                    // fragment->linearVelocity = glm::vec3(0.0f);
                    // fragment->angularVelocity = glm::vec3(0.0f);

                    job.dronePosition = moveTowards(
                        job.dronePosition,
                        fragment->position,
                        job.droneSpeed * dt
                    );

                const float dist2 =
                    glm::length2(job.dronePosition - fragment->position);

                if (dist2 <= job.captureDistance * job.captureDistance)
                {
                    // fragment->linearVelocity = glm::vec3(0.0f);
                    // fragment->angularVelocity = glm::vec3(0.0f);

                    job.captureTimer = 0.0f;
                    job.state = ObjectRepairJobState::CapturingFragment;

                    std::cout
                        << "[ObjectRepairJobRuntime] fragment captured moduleId="
                        << job.moduleId << "\n";
                }

                break;
            }

            case ObjectRepairJobState::CapturingFragment:
            {
                fragment->linearVelocity = glm::vec3(0.0f);
                fragment->angularVelocity = glm::vec3(0.0f);

                job.captureTimer += dt;

                if (job.captureTimer >= job.captureDuration)
                {
                    job.state = ObjectRepairJobState::ReturningToHome;

                    std::cout
                        << "[ObjectRepairJobRuntime] returning fragment home moduleId="
                        << job.moduleId << "\n";
                }

                break;
            }

            case ObjectRepairJobState::ReturningToHome:
            {
                fragment->linearVelocity = glm::vec3(0.0f);
                fragment->angularVelocity = glm::vec3(0.0f);

                job.dronePosition = moveTowards(
                    job.dronePosition,
                    homePosition,
                    job.droneSpeed * dt
                );

                fragment->position = moveTowards(
                    fragment->position,
                    homePosition,
                    job.fragmentTowSpeed * dt
                );

                const float dist2 =
                    glm::length2(fragment->position - homePosition);

                if (dist2 <= job.reattachDistance * job.reattachDistance)
                {
                    fragment->position = homePosition;
                    fragment->orientation = homeOrientation;

                    job.state = ObjectRepairJobState::ReadyToReattach;

                    completed.push_back(job.moduleId);

                    std::cout
                        << "[ObjectRepairJobRuntime] ready to reattach moduleId="
                        << job.moduleId << "\n";
                }

                break;
            }

            case ObjectRepairJobState::ReadyToReattach:
            case ObjectRepairJobState::Done:
            case ObjectRepairJobState::Failed:
                break;
        }
    }

    m_jobs.erase(
        std::remove_if(
            m_jobs.begin(),
            m_jobs.end(),
            [](const ObjectRepairJobRuntimeState& job)
            {
                return job.state == ObjectRepairJobState::ReadyToReattach ||
                       job.state == ObjectRepairJobState::Done ||
                       job.state == ObjectRepairJobState::Failed;
            }
        ),
        m_jobs.end()
    );

    return completed;
}





int ObjectRepairJobRuntime::activeJobCount() const
{
    int count = 0;

    for (const auto& job : m_jobs)
    {
        if (job.state != ObjectRepairJobState::Done &&
            job.state != ObjectRepairJobState::Failed)
        {
            ++count;
        }
    }

    return count;
}



std::vector<game::simulation::ObjectRepairJobSnapshot>
ObjectRepairJobRuntime::buildSnapshots(
    const glm::mat4& ownerModel,
    const ObjectDetachedFragmentRuntime& detachedRuntime
) const
{
    std::vector<game::simulation::ObjectRepairJobSnapshot> out;

    for (const auto& job : m_jobs)
    {
        const auto* fragment =
            detachedRuntime.findFragment(job.moduleId);

        if (!fragment)
            continue;

        const glm::mat4 homeWorldModel =
            ownerModel * fragment->homeLocalModel;

        game::simulation::ObjectRepairJobSnapshot s;
        s.moduleId = job.moduleId;
        s.dronePosition = job.dronePosition;
        s.fragmentPosition = fragment->position;
        s.homePosition = glm::vec3(homeWorldModel * glm::vec4(0, 0, 0, 1));
        s.state = static_cast<uint8_t>(job.state);

        out.push_back(std::move(s));
    }

    return out;
}





} // namespace world::modules