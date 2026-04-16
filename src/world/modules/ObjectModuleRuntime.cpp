#include "ObjectModuleRuntime.h"

#include <iostream>

namespace world::modules
{

void ObjectModuleRuntime::init(const std::vector<ModuleDescriptor>& descriptors)
{
    m_modules.clear();
    m_moduleIndexById.clear();

    m_modules.reserve(descriptors.size());

    for (const auto& descMod : descriptors)
    {
        ObjectModuleState state;
        state.moduleId = descMod.moduleId;
        state.parentModuleId = descMod.parentModuleId;
        state.subsystemId = descMod.subsystemId;

        state.meshPartIds = descMod.meshPartIds;

        state.destroyPolicy = descMod.destroyPolicy;
        state.detachPolicy = descMod.detachPolicy;
        state.attachmentType = descMod.attachmentType;

        state.detachChildrenOnLoss = descMod.detachChildrenOnLoss;
        state.destroyChildrenOnLoss = descMod.destroyChildrenOnLoss;
        state.disableChildrenOnLoss = descMod.disableChildrenOnLoss;

        state.destructible = descMod.destructible;
        state.detachable = descMod.detachable;
        state.repairable = descMod.repairable;
        state.salvageable = descMod.salvageable;

        state.isCritical = descMod.isCritical;
        state.destroysParentIfCatastrophic = descMod.destroysParentIfCatastrophic;
        state.destroysWholeObjectIfCatastrophic = descMod.destroysWholeObjectIfCatastrophic;

        state.maxHealth = descMod.maxHealth;
        state.health = descMod.maxHealth;
        state.state = ObjectModuleRuntimeState::Attached;

        m_moduleIndexById[state.moduleId] = static_cast<int>(m_modules.size());
        m_modules.push_back(std::move(state));
    }

    buildHierarchy();

    std::cout << "[ObjectModuleRuntime] initialized modules="
              << m_modules.size() << "\n";
}

void ObjectModuleRuntime::buildHierarchy()
{
    for (auto& m : m_modules)
        m.children.clear();

    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        const auto& parentId = m_modules[i].parentModuleId;
        if (parentId.empty())
            continue;

        auto it = m_moduleIndexById.find(parentId);
        if (it == m_moduleIndexById.end())
        {
            std::cout << "[ObjectModuleRuntime] WARNING parent not found for "
                      << m_modules[i].moduleId
                      << " parent=" << parentId << "\n";
            continue;
        }

        m_modules[it->second].children.push_back(i);
    }
}

int ObjectModuleRuntime::findModuleIndex(const std::string& moduleId) const
{
    auto it = m_moduleIndexById.find(moduleId);
    if (it == m_moduleIndexById.end())
        return -1;

    return it->second;
}

bool ObjectModuleRuntime::isMeshPartVisible(const std::string& partId) const
{
    for (const auto& mod : m_modules)
    {
        for (const auto& meshId : mod.meshPartIds)
        {
            if (meshId == partId)
                return mod.isVisibleInHull();
        }
    }

    return true;
}

void ObjectModuleRuntime::setModuleHealth(const std::string& moduleId, float health)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    m_modules[index].health = health;
}

void ObjectModuleRuntime::markModuleDestroyed(const std::string& moduleId, float health)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    m_modules[index].health = health;
    onModuleDestroyed(index);
}

void ObjectModuleRuntime::onModuleDestroyed(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state != ObjectModuleRuntimeState::Attached)
        return;

    std::cout << "[ObjectModuleRuntime] module destroyed: "
              << module.moduleId << "\n";

    switch (module.destroyPolicy)
    {
        case ModuleDestroyPolicy::DisableOnly:
            module.state = ObjectModuleRuntimeState::Disabled;
            break;

        case ModuleDestroyPolicy::Detach:
            module.state = ObjectModuleRuntimeState::Detached;
            break;

        case ModuleDestroyPolicy::Vaporize:
            module.state = ObjectModuleRuntimeState::Destroyed;
            break;

        case ModuleDestroyPolicy::Catastrophic:
            module.state = ObjectModuleRuntimeState::Destroyed;

            if (module.destroysWholeObjectIfCatastrophic)
            {
                setWholeObjectDestroyed();
                return;
            }
            break;
    }

    if (module.destroyChildrenOnLoss)
    {
        for (int child : module.children)
            destroySubtree(child);
    }

    if (module.detachChildrenOnLoss)
    {
        for (int child : module.children)
            detachSubtree(child);
    }

    if (module.disableChildrenOnLoss)
    {
        for (int child : module.children)
            disableSubtree(child);
    }
}

void ObjectModuleRuntime::detachSubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Detached ||
        module.state == ObjectModuleRuntimeState::Destroyed)
        return;

    module.state = ObjectModuleRuntimeState::Detached;

    for (int child : module.children)
        detachSubtree(child);
}

void ObjectModuleRuntime::destroySubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed)
        return;

    module.state = ObjectModuleRuntimeState::Destroyed;

    for (int child : module.children)
        destroySubtree(child);
}

void ObjectModuleRuntime::disableSubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return;

    module.state = ObjectModuleRuntimeState::Disabled;

    for (int child : module.children)
        disableSubtree(child);
}

void ObjectModuleRuntime::setWholeObjectDestroyed()
{
    for (auto& module : m_modules)
    {
        module.state = ObjectModuleRuntimeState::Destroyed;
        module.health = 0.0f;
    }
}

} // namespace world::modules