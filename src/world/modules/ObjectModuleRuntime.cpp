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
        state.parentIndex = -1;
        state.subsystemId = descMod.subsystemId;
        state.moduleClass = descMod.moduleClass;
        state.providedReplacementTags = descMod.providedReplacementTags;
        state.acceptedReplacementTags = descMod.acceptedReplacementTags;

        state.meshPartIds = descMod.meshPartIds;

        state.destroyPolicy = descMod.destroyPolicy;
        state.detachPolicy = descMod.detachPolicy;
        state.attachmentType = descMod.attachmentType;
        state.structuralKind = descMod.structuralKind;

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

        state.hangable = descMod.hangable;
        state.supportModuleIds = descMod.supportModuleIds;
        state.minSupportsForAttached = descMod.minSupportsForAttached;
        state.minSupportsForStable = descMod.minSupportsForStable;
        state.aliveSupportCount = 0;

        // Явные support-links
        if (!descMod.supportLinks.empty())
        {
            for (const auto& linkDesc : descMod.supportLinks)
            {
                ModuleSupportLinkState link;
                link.linkId = linkDesc.linkId;
                link.supportModuleId = linkDesc.supportModuleId;
                link.maxHealth = linkDesc.maxHealth;
                link.health = linkDesc.maxHealth;
                link.impulseTolerance = linkDesc.impulseTolerance;
                link.loadBearing = linkDesc.loadBearing;
                link.destroyed = false;
                state.supportLinks.push_back(std::move(link));
            }
        }
        else
        {
            // Автогенерация link’ов из supportModuleIds
            for (const auto& supportId : descMod.supportModuleIds)
            {
                ModuleSupportLinkState link;
                link.linkId = descMod.moduleId + "::support::" + supportId;
                link.supportModuleId = supportId;
                link.maxHealth = descMod.autoSupportLinkHealth;
                link.health = descMod.autoSupportLinkHealth;
                link.impulseTolerance = descMod.autoSupportImpulseTolerance;
                link.loadBearing = true;
                link.destroyed = false;
                state.supportLinks.push_back(std::move(link));
            }
        }

        m_moduleIndexById[state.moduleId] = static_cast<int>(m_modules.size());
        m_modules.push_back(std::move(state));
    }

    buildHierarchy();
    reevaluateStructuralStates();

    std::cout << "[ObjectModuleRuntime] initialized modules="
              << m_modules.size() << "\n";
}

void ObjectModuleRuntime::buildHierarchy()
{
    for (auto& m : m_modules)
    {
        m.children.clear();
        m.parentIndex = -1;
    }

    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        const auto& parentId = m_modules[i].parentModuleId;
        if (parentId.empty())
            continue;

        if (parentId == m_modules[i].moduleId)
        {
            std::cout << "[ObjectModuleRuntime] WARNING self-parent moduleId="
                      << m_modules[i].moduleId << "\n";
            continue;
        }

        auto it = m_moduleIndexById.find(parentId);
        if (it == m_moduleIndexById.end())
        {
            std::cout << "[ObjectModuleRuntime] WARNING parent not found for "
                      << m_modules[i].moduleId
                      << " parent=" << parentId << "\n";
            continue;
        }

        m_modules[i].parentIndex = it->second;
        m_modules[it->second].children.push_back(i);
    }

    validateHierarchy();
}

bool ObjectModuleRuntime::validateHierarchy() const
{
    bool ok = true;

    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        int current = i;
        int guard = 0;

        while (current >= 0)
        {
            current = m_modules[current].parentIndex;
            ++guard;

            if (guard > static_cast<int>(m_modules.size()) + 1)
            {
                std::cout << "[ObjectModuleRuntime] ERROR hierarchy cycle detected at module="
                          << m_modules[i].moduleId << "\n";
                ok = false;
                break;
            }
        }
    }

    return ok;
}

int ObjectModuleRuntime::findModuleIndex(const std::string& moduleId) const
{
    auto it = m_moduleIndexById.find(moduleId);
    if (it == m_moduleIndexById.end())
        return -1;

    return it->second;
}

const ObjectModuleState* ObjectModuleRuntime::findModule(const std::string& moduleId) const
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return nullptr;

    return &m_modules[index];
}

ObjectModuleState* ObjectModuleRuntime::findModule(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return nullptr;

    return &m_modules[index];
}

bool ObjectModuleRuntime::isModuleEffectivelyVisible(const std::string& moduleId) const
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    int guard = 0;
    while (index >= 0)
    {
        const auto& mod = m_modules[index];

        if (!mod.isVisibleInHull())
            return false;

        index = mod.parentIndex;
        ++guard;

        if (guard > static_cast<int>(m_modules.size()) + 1)
        {
            std::cout << "[ObjectModuleRuntime] ERROR visibility hierarchy cycle at module="
                      << moduleId << "\n";
            return false;
        }
    }

    return true;
}

bool ObjectModuleRuntime::moduleParticipatesInHits(const std::string& moduleId) const
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    int guard = 0;
    while (index >= 0)
    {
        const auto& mod = m_modules[index];

        if (!mod.participatesInHits())
            return false;

        index = mod.parentIndex;
        ++guard;

        if (guard > static_cast<int>(m_modules.size()) + 1)
        {
            std::cout << "[ObjectModuleRuntime] ERROR hit hierarchy cycle at module="
                      << moduleId << "\n";
            return false;
        }
    }

    return true;
}

bool ObjectModuleRuntime::isMeshPartVisible(const std::string& partId) const
{
    for (const auto& mod : m_modules)
    {
        for (const auto& meshId : mod.meshPartIds)
        {
            if (meshId == partId)
                return isModuleEffectivelyVisible(mod.moduleId);
        }
    }

    return true;
}

int ObjectModuleRuntime::countAliveSupports(
    int index,
    const ObjectStructuralLinkRuntime* structuralLinks
) const
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return 0;

    const auto& module = m_modules[index];

    if (structuralLinks)
    {
        int alive = 0;
        bool foundAnyCentralLinks = false;

        for (const auto& link : structuralLinks->links())
        {
            const bool touchesA = (link.moduleAId == module.moduleId);
            const bool touchesB = (link.moduleBId == module.moduleId);

            if (!touchesA && !touchesB)
                continue;

            foundAnyCentralLinks = true;

            if (!link.loadBearing)
                continue;

            if (!link.isAlive())
                continue;

            const std::string& otherId = touchesA ? link.moduleBId : link.moduleAId;

            int supportIndex = findModuleIndex(otherId);
            if (supportIndex < 0)
                continue;

            const auto& support = m_modules[supportIndex];
            if (support.isAttachedLike())
                ++alive;
        }

        if (foundAnyCentralLinks)
            return alive;
    }



    
    // legacy fallback
    if (!module.supportLinks.empty())
    {
        int alive = 0;

        for (const auto& link : module.supportLinks)
        {
            if (!link.loadBearing)
                continue;

            if (!link.isAlive())
                continue;

            int supportIndex = findModuleIndex(link.supportModuleId);
            if (supportIndex < 0)
                continue;

            const auto& support = m_modules[supportIndex];
            if (support.isAttachedLike())
                ++alive;
        }

        return alive;
    }

    if (module.supportModuleIds.empty())
    {
        if (module.parentIndex >= 0)
        {
            const auto& parent = m_modules[module.parentIndex];
            return parent.isAttachedLike() ? 1 : 0;
        }
        return 0;
    }

    int alive = 0;

    for (const auto& supportId : module.supportModuleIds)
    {
        int supportIndex = findModuleIndex(supportId);
        if (supportIndex < 0)
            continue;

        const auto& support = m_modules[supportIndex];
        if (support.isAttachedLike())
            ++alive;
    }

    return alive;
}

void ObjectModuleRuntime::setModuleHealth(const std::string& moduleId, float health)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    m_modules[index].health = health;
}

bool ObjectModuleRuntime::setSupportLinkHealth(
    const std::string& moduleId,
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];

    for (auto& link : module.supportLinks)
    {
        if (link.linkId != linkId)
            continue;

        link.health = health;
        link.destroyed = destroyed || health <= 0.0f;

        if (link.health < 0.0f)
            link.health = 0.0f;

        return true;
    }

    return false;
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

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
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

    if (module.disableChildrenOnLoss)
    {
        for (int child : module.children)
            disableSubtree(child);
    }

    if (module.detachChildrenOnLoss)
    {
        for (int child : module.children)
            detachSubtree(child);
    }

    reevaluateStructuralStates();
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

void ObjectModuleRuntime::restoreSubtree(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];
    module.state = ObjectModuleRuntimeState::Attached;
    module.health = module.maxHealth;

    for (auto& link : module.supportLinks)
    {
        link.health = link.maxHealth;
        link.destroyed = false;
    }

    for (int child : module.children)
        restoreSubtree(child);
}

void ObjectModuleRuntime::restoreAncestors(int index)
{
    int current = index;
    int guard = 0;

    while (current >= 0)
    {
        auto& module = m_modules[current];
        module.state = ObjectModuleRuntimeState::Attached;
        module.health = module.maxHealth;

        for (auto& link : module.supportLinks)
        {
            link.health = link.maxHealth;
            link.destroyed = false;
        }

        current = module.parentIndex;
        ++guard;

        if (guard > static_cast<int>(m_modules.size()) + 1)
        {
            std::cout << "[ObjectModuleRuntime] ERROR restoreAncestors hierarchy cycle\n";
            return;
        }
    }
}

void ObjectModuleRuntime::restoreModuleBranch(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return;

    restoreAncestors(index);
    restoreSubtree(index);

    std::cout << "[ObjectModuleRuntime] restoreModuleBranch moduleId="
              << moduleId << "\n";
}

void ObjectModuleRuntime::setWholeObjectDestroyed()
{
    for (auto& module : m_modules)
    {
        module.state = ObjectModuleRuntimeState::Destroyed;
        module.health = 0.0f;

        for (auto& link : module.supportLinks)
        {
            link.health = 0.0f;
            link.destroyed = true;
        }
    }
}

void ObjectModuleRuntime::applyStructuralStateFromSupports(int index,
    const ObjectStructuralLinkRuntime* structuralLinks)
{
    if (index < 0 || index >= static_cast<int>(m_modules.size()))
        return;

    auto& module = m_modules[index];

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return;



// ------------------------------------------------------------
// 1. OUTER PANEL RULE
// ------------------------------------------------------------
// Панель обшивки должна отрываться от смерти авто-швов,
// а не от смерти всех связей вообще.
// Старые supportModuleIds к frame не должны удерживать панель,
// если все реальные panel-to-panel seam уничтожены.
if (structuralLinks &&
    module.detachable &&
    module.structuralKind == ModuleStructuralKind::OuterPanel)
{
    int totalAutoPanelSeams = 0;
    int aliveAutoPanelSeams = 0;

    for (const auto& link : structuralLinks->links())
    {
        if (!link.autoGenerated)
            continue;

        if (link.kind != StructuralLinkKind::PanelSeam)
            continue;

        const bool touches =
            link.moduleAId == module.moduleId ||
            link.moduleBId == module.moduleId;

        if (!touches)
            continue;

        if (!link.loadBearing)
            continue;

        ++totalAutoPanelSeams;

        if (link.isAlive())
            ++aliveAutoPanelSeams;
    }

    if (totalAutoPanelSeams > 0)
    {
        module.aliveSupportCount = aliveAutoPanelSeams;

        if (aliveAutoPanelSeams <= 0)
        {
            module.state = ObjectModuleRuntimeState::Detached;

            std::cout
                << "[ObjectModuleRuntime] OUTER PANEL DETACHED BY DEAD AUTO SEAMS: "
                << module.moduleId
                << " totalAutoPanelSeams=" << totalAutoPanelSeams
                << "\n";

            return;
        }
    }
}

// ------------------------------------------------------------
// 2. GENERIC DETACHABLE RULE
// ------------------------------------------------------------
// Для оборудования, кокпита-капсулы, лазеров и прочего:
// если все реальные несущие связи мертвы — отрываем.
if (structuralLinks && module.detachable)
{
    int totalLoadBearingLinks = 0;
    int aliveLoadBearingLinks = 0;

    for (const auto& link : structuralLinks->links())
    {
        const bool touches =
            link.moduleAId == module.moduleId ||
            link.moduleBId == module.moduleId;

        if (!touches)
            continue;

        if (!link.loadBearing)
            continue;

        ++totalLoadBearingLinks;

        if (link.isAlive())
            ++aliveLoadBearingLinks;
    }

    if (totalLoadBearingLinks > 0)
    {
        module.aliveSupportCount = aliveLoadBearingLinks;

        if (aliveLoadBearingLinks <= 0)
        {
            module.state = ObjectModuleRuntimeState::Detached;

            std::cout
                << "[ObjectModuleRuntime] DETACHED BY DEAD STRUCTURAL LINKS: "
                << module.moduleId
                << " totalLoadBearingLinks=" << totalLoadBearingLinks
                << "\n";

            return;
        }
    }
}






const int aliveSupports = countAliveSupports(index, structuralLinks);
module.aliveSupportCount = aliveSupports;

bool hasAnyStructuralLinks = false;

if (structuralLinks)
{
    for (const auto& link : structuralLinks->links())
    {
        if (link.moduleAId == module.moduleId ||
            link.moduleBId == module.moduleId)
        {
            hasAnyStructuralLinks = true;
            break;
        }
    }
}

const bool hasLegacySupports =
    !module.supportLinks.empty() ||
    !module.supportModuleIds.empty();

const bool hasParentSupport =
    module.parentIndex >= 0 &&
    module.structuralKind != ModuleStructuralKind::OuterPanel;

const bool hasAnySupportSource =
    hasAnyStructuralLinks ||
    hasLegacySupports ||
    hasParentSupport;

if (!hasAnySupportSource)
    return;

// Если у модуля есть реальные structural-links, то 0 живых связей
// не может означать Attached, даже если minSupportsForAttached случайно равен 0.
int requiredSupports = module.minSupportsForAttached;

if (hasAnyStructuralLinks && requiredSupports <= 0)
    requiredSupports = 1;

if (aliveSupports >= requiredSupports)
{
    if (module.state == ObjectModuleRuntimeState::Hanging)
        module.state = ObjectModuleRuntimeState::Attached;
    return;
}

    if (aliveSupports > 0)
    {
        if (module.hangable && aliveSupports < module.minSupportsForAttached)
        {
            module.state = ObjectModuleRuntimeState::Hanging;
            return;
        }

        return;
    }

    if (module.detachable)
    {
        module.state = ObjectModuleRuntimeState::Detached;

        if (module.detachChildrenOnLoss)
        {
            for (int child : module.children)
                detachSubtree(child);
        }
        return;
    }

    switch (module.destroyPolicy)
    {
        case ModuleDestroyPolicy::DisableOnly:
            module.state = ObjectModuleRuntimeState::Disabled;
            break;

        case ModuleDestroyPolicy::Detach:
            module.state = ObjectModuleRuntimeState::Detached;
            break;

        case ModuleDestroyPolicy::Vaporize:
        case ModuleDestroyPolicy::Catastrophic:
            module.state = ObjectModuleRuntimeState::Destroyed;
            break;
    }
}

void ObjectModuleRuntime::reevaluateStructuralStates(
    const ObjectStructuralLinkRuntime* structuralLinks
)
{
    for (int i = 0; i < static_cast<int>(m_modules.size()); ++i)
    {
        applyStructuralStateFromSupports(i, structuralLinks);
    }
}

bool ObjectModuleRuntime::reattachModule(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];

    if (!module.repairable && !module.salvageable)
    {
        std::cout
            << "[ObjectModuleRuntime] reattach rejected, module is not repairable/salvageable: "
            << moduleId << "\n";
        return false;
    }

    module.state = ObjectModuleRuntimeState::Attached;
    module.health = module.maxHealth;

    module.aliveSupportCount = module.minSupportsForAttached;

    for (auto& link : module.supportLinks)
    {
        link.health = link.maxHealth;
        link.destroyed = false;
    }

    std::cout
        << "[ObjectModuleRuntime] module reattached: "
        << moduleId << "\n";

    return true;
}

bool ObjectModuleRuntime::debugForceDetach(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];

    // Каркас, reactor-frame и прочие несъемные structural core
    // нельзя отрывать как панель. Иначе отрывается всё дерево.
    if (!module.detachable)
    {
        std::cout
            << "[ObjectModuleRuntime] debugForceDetach rejected non-detachable module="
            << moduleId << "\n";
        return false;
    }

    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return false;

    // ВАЖНО: detach только самого модуля.
    // Поддерево отрываем только через реальные политики damage-логики.
    module.state = ObjectModuleRuntimeState::Detached;

    reevaluateStructuralStates();
    return true;
}

bool ObjectModuleRuntime::debugForceHang(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    auto& module = m_modules[index];
    if (module.state == ObjectModuleRuntimeState::Destroyed ||
        module.state == ObjectModuleRuntimeState::Detached)
        return false;

    if (!module.hangable)
        return false;

    module.state = ObjectModuleRuntimeState::Hanging;
    return true;
}

bool ObjectModuleRuntime::debugForceDestroy(const std::string& moduleId)
{
    int index = findModuleIndex(moduleId);
    if (index < 0)
        return false;

    m_modules[index].health = 0.0f;
    onModuleDestroyed(index);
    return true;
}

} // namespace world::modules