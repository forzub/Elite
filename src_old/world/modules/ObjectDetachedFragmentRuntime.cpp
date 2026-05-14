#include "ObjectDetachedFragmentRuntime.h"

#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/world/modules/ObjectModuleState.h"

namespace world::modules
{

void ObjectDetachedFragmentRuntime::clear()
{
    m_fragments.clear();
    m_indexByModuleId.clear();
}

void ObjectDetachedFragmentRuntime::rebuildIndex()
{
    m_indexByModuleId.clear();

    for (int i = 0; i < static_cast<int>(m_fragments.size()); ++i)
        m_indexByModuleId[m_fragments[i].moduleId] = i;
}

bool ObjectDetachedFragmentRuntime::hasFragment(const std::string& moduleId) const
{
    return m_indexByModuleId.find(moduleId) != m_indexByModuleId.end();
}








ObjectDetachedFragmentRuntimeState*
ObjectDetachedFragmentRuntime::findFragmentMutable(
    const std::string& moduleId
)
{
    auto it = m_indexByModuleId.find(moduleId);
    if (it == m_indexByModuleId.end())
        return nullptr;

    const int index = it->second;

    if (index < 0 || index >= static_cast<int>(m_fragments.size()))
        return nullptr;

    return &m_fragments[index];
}

const ObjectDetachedFragmentRuntimeState*
ObjectDetachedFragmentRuntime::findFragment(
    const std::string& moduleId
) const
{
    auto it = m_indexByModuleId.find(moduleId);
    if (it == m_indexByModuleId.end())
        return nullptr;

    const int index = it->second;

    if (index < 0 || index >= static_cast<int>(m_fragments.size()))
        return nullptr;

    return &m_fragments[index];
}





bool ObjectDetachedFragmentRuntime::removeFragment(const std::string& moduleId)
{
    auto it = m_indexByModuleId.find(moduleId);
    if (it == m_indexByModuleId.end())
        return false;

    const int index = it->second;

    if (index < 0 || index >= static_cast<int>(m_fragments.size()))
        return false;

    m_fragments.erase(m_fragments.begin() + index);
    rebuildIndex();

    std::cout
        << "[ObjectDetachedFragmentRuntime] removed fragment moduleId="
        << moduleId << "\n";

    return true;
}



bool ObjectDetachedFragmentRuntime::setFragmentMotion(
    const std::string& moduleId,
    const glm::vec3& linearVelocity,
    const glm::vec3& angularVelocity
)
{
    auto it = m_indexByModuleId.find(moduleId);
    if (it == m_indexByModuleId.end())
        return false;

    const int index = it->second;

    if (index < 0 || index >= static_cast<int>(m_fragments.size()))
        return false;

    auto& fragment = m_fragments[index];

    fragment.linearVelocity = linearVelocity;
    fragment.angularVelocity = angularVelocity;

    std::cout
        << "[ObjectDetachedFragmentRuntime] setFragmentMotion moduleId="
        << moduleId
        << " velocity=("
        << linearVelocity.x << ", "
        << linearVelocity.y << ", "
        << linearVelocity.z << ")"
        << "\n";

    return true;
}







void ObjectDetachedFragmentRuntime::update(float dt)
{
    if (dt <= 0.0f)
        return;

    for (auto& f : m_fragments)
    {
        f.position += f.linearVelocity * dt;

        const float wx = f.angularVelocity.x * dt;
        const float wy = f.angularVelocity.y * dt;
        const float wz = f.angularVelocity.z * dt;

        glm::mat4 r(1.0f);

        if (std::abs(wx) > 0.000001f)
            r = glm::rotate(r, wx, glm::vec3(1.0f, 0.0f, 0.0f));

        if (std::abs(wy) > 0.000001f)
            r = glm::rotate(r, wy, glm::vec3(0.0f, 1.0f, 0.0f));

        if (std::abs(wz) > 0.000001f)
            r = glm::rotate(r, wz, glm::vec3(0.0f, 0.0f, 1.0f));

        f.orientation = f.orientation * r;
    }
}

void ObjectDetachedFragmentRuntime::syncFromDetachedModules(
    const glm::mat4& ownerModel,
    const game::ship::geometry::ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const ObjectModuleRuntime& moduleRuntime,
    const game::damage::HitComponent& hitComponent
)
{
    for (const auto& moduleState : moduleRuntime.modules())
    {
        if (moduleState.state != ObjectModuleRuntimeState::Detached)
            continue;

        if (hasFragment(moduleState.moduleId))
            continue;

        const auto* assemblyModule =
            findAssemblyModuleById(assembly, moduleState.moduleId);

        if (!assemblyModule)
        {
            std::cout
                << "[ObjectDetachedFragmentRuntime] no assembly module for detached moduleId="
                << moduleState.moduleId << "\n";
            continue;
        }

        const glm::mat4 localModuleModel =
            buildAssemblyModuleHierarchicalLocalModel(
                assembly,
                assemblyRuntime,
                moduleState.moduleId
            );

        const glm::mat4 worldModuleModel = ownerModel * localModuleModel;

        ObjectDetachedFragmentRuntimeState f;
        f.moduleId = moduleState.moduleId;
        f.originalModuleId = moduleState.moduleId;
        f.moduleClass = moduleState.moduleClass;
        f.providedReplacementTags = moduleState.providedReplacementTags;

        if (f.moduleClass.empty())
            f.moduleClass = moduleState.moduleId;

        if (f.providedReplacementTags.empty())
            f.providedReplacementTags.push_back(moduleState.moduleId);


        f.homeLocalModel = localModuleModel;

        f.position = glm::vec3(worldModuleModel * glm::vec4(0, 0, 0, 1));

        f.orientation = worldModuleModel;
        f.orientation[3] = glm::vec4(0, 0, 0, 1);

        glm::vec3 ownerPos = glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));
        glm::vec3 dir = f.position - ownerPos;

        if (glm::length(dir) < 0.001f)
            dir = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            dir = glm::normalize(dir);

        // Пока без настоящей физики удара.
        // Это стартовый импульс, чтобы detached-фрагмент реально отделялся.
        f.linearVelocity = dir * 3.0f;

        // Легкое вращение, чтобы было видно, что кусок живет отдельно.
        f.angularVelocity = glm::vec3(0.15f, 0.35f, 0.10f);

        f.salvageable = moduleState.salvageable;
        f.repairable = moduleState.repairable;
        f.canReattach = moduleState.repairable || moduleState.salvageable;

            glm::vec3 accumulatedCenter(0.0f);
            int accumulatedCount = 0;

        for (const auto& v : hitComponent.volumes)
        {
            if (v.moduleId != moduleState.moduleId)
                continue;

            // Пока берем только основные volumes самого модуля.
            // Support-link volumes остаются у корпуса/швов, пока не введем отдельные связи fragment<->fragment.
            if (v.supportLinkVolume)
                continue;

            accumulatedCenter += v.center;
            ++accumulatedCount;

            game::simulation::DebugHitVolumeSnapshot hv;
            hv.moduleId = v.moduleId;
            hv.subsystemId = v.subsystemId;
            hv.layerIndex = v.layerIndex;
            hv.priority = v.priority;
            hv.center = v.center;
            hv.halfSize = v.halfSize;
            hv.orientation = v.orientation;
            hv.destructible = v.destructible;
            hv.destroyed = v.destroyed;
            hv.health = v.health;
            hv.maxHealth = v.maxHealth;
            hv.supportLinkVolume = false;

            f.debugHitVolumes.push_back(std::move(hv));
        }



        if (accumulatedCount > 0)
            f.homeCenterLocal = accumulatedCenter / static_cast<float>(accumulatedCount);
        else
            f.homeCenterLocal = glm::vec3(f.homeLocalModel * glm::vec4(0, 0, 0, 1));



        m_indexByModuleId[f.moduleId] = static_cast<int>(m_fragments.size());
        m_fragments.push_back(std::move(f));

        std::cout
            << "[ObjectDetachedFragmentRuntime] spawned fragment moduleId="
            << moduleState.moduleId << "\n";
    }
}

std::vector<game::simulation::ObjectDetachedFragmentSnapshot>
ObjectDetachedFragmentRuntime::buildSnapshots() const
{
    std::vector<game::simulation::ObjectDetachedFragmentSnapshot> out;
    out.reserve(m_fragments.size());

    for (const auto& f : m_fragments)
    {
        game::simulation::ObjectDetachedFragmentSnapshot s;
        s.moduleId = f.moduleId;
        s.originalModuleId = f.originalModuleId;
        s.moduleClass = f.moduleClass;
        s.providedReplacementTags = f.providedReplacementTags;
        s.position = f.position;
        s.orientation = f.orientation;
        s.homeLocalModel = f.homeLocalModel;
        s.homeCenterLocal = f.homeCenterLocal;
        s.linearVelocity = f.linearVelocity;
        s.angularVelocity = f.angularVelocity;
        s.salvageable = f.salvageable;
        s.repairable = f.repairable;
        s.canReattach = f.canReattach;

        s.debugHitVolumes = f.debugHitVolumes;
        out.push_back(std::move(s));
    }

    return out;
}



bool ObjectDetachedFragmentRuntime::claimFragmentAsReplacement(
    ObjectDetachedFragmentRuntime& sourceRuntime,
    const std::string& sourceModuleId,
    const std::string& targetModuleId,
    const glm::mat4& targetHomeLocalModel,
    const glm::vec3& targetHomeCenterLocal
)
{
    auto* sourceFragment =
        sourceRuntime.findFragmentMutable(sourceModuleId);

    if (!sourceFragment)
    {
        std::cout
            << "[ObjectDetachedFragmentRuntime] claim failed: source fragment missing sourceModuleId="
            << sourceModuleId << "\n";
        return false;
    }

    // Если это тот же runtime и тот же moduleId:
    // не копируем, не удаляем, просто переназначаем home.
    if (&sourceRuntime == this && sourceModuleId == targetModuleId)
    {
        sourceFragment->homeLocalModel = targetHomeLocalModel;
        sourceFragment->homeCenterLocal = targetHomeCenterLocal;

        // НЕ гасим скорость при claim.
        // Пусть деталь продолжает дрейфовать, пока дрон реально её не догонит.
        sourceFragment->canReattach = true;
        sourceFragment->repairable = true;

        std::cout
            << "[ObjectDetachedFragmentRuntime] own fragment claimed as replacement moduleId="
            << targetModuleId << "\n";

        return true;
    }

    if (hasFragment(targetModuleId))
    {
        std::cout
            << "[ObjectDetachedFragmentRuntime] claim failed: target already has fragment targetModuleId="
            << targetModuleId << "\n";
        return false;
    }

    ObjectDetachedFragmentRuntimeState claimed = *sourceFragment;

    if (claimed.originalModuleId.empty())
        claimed.originalModuleId = sourceModuleId;

    claimed.moduleId = targetModuleId;
    claimed.homeLocalModel = targetHomeLocalModel;
    claimed.homeCenterLocal = targetHomeCenterLocal;

    // НЕ гасим скорость при claim.
    // Фрагмент должен продолжать жить физически до фактического захвата дроном.
    claimed.canReattach = true;

    claimed.repairable = true;
    claimed.salvageable = true;

    for (auto& hv : claimed.debugHitVolumes)
        hv.moduleId = targetModuleId;

    m_fragments.push_back(std::move(claimed));
    rebuildIndex();

    sourceRuntime.removeFragment(sourceModuleId);

    std::cout
        << "[ObjectDetachedFragmentRuntime] fragment claimed as replacement: sourceModuleId="
        << sourceModuleId
        << " targetModuleId="
        << targetModuleId
        << "\n";

    return true;
}







} // namespace world::modules