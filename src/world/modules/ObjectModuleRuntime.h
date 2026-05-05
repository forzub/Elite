#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include "src/world/modules/ObjectModuleState.h"
#include "src/world/descriptors/ModuleDescriptor.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"

namespace world::modules
{

class ObjectModuleRuntime
{
public:
    void init(const std::vector<ModuleDescriptor>& descriptors);
    void buildHierarchy();

    int findModuleIndex(const std::string& moduleId) const;
    const ObjectModuleState* findModule(const std::string& moduleId) const;
    ObjectModuleState* findModule(const std::string& moduleId);

    bool isMeshPartVisible(const std::string& partId) const;
    bool isModuleEffectivelyVisible(const std::string& moduleId) const;
    bool moduleParticipatesInHits(const std::string& moduleId) const;

    void markModuleDestroyed(const std::string& moduleId, float health = 0.0f);
    void setModuleHealth(const std::string& moduleId, float health);

    void onModuleDestroyed(int index);

    void detachSubtree(int index);
    void destroySubtree(int index);
    void disableSubtree(int index);

    void restoreModuleBranch(const std::string& moduleId);
    void restoreSubtree(int index);
    void restoreAncestors(int index);

    void setWholeObjectDestroyed();

    const std::vector<ObjectModuleState>& modules() const { return m_modules; }
    std::vector<ObjectModuleState>& modules() { return m_modules; }

    void reevaluateStructuralStates(
        const ObjectStructuralLinkRuntime* structuralLinks = nullptr
    );

    bool debugForceDetach(const std::string& moduleId);
    bool reattachModule(const std::string& moduleId);
    bool debugForceHang(const std::string& moduleId);
    bool debugForceDestroy(const std::string& moduleId);

    // Новый API для support-links
    bool setSupportLinkHealth(
        const std::string& moduleId,
        const std::string& linkId,
        float health,
        bool destroyed
    );

private:
    bool validateHierarchy() const;
    int countAliveSupports(
        int index,
        const ObjectStructuralLinkRuntime* structuralLinks = nullptr
    ) const;

    void applyStructuralStateFromSupports(
        int index,
        const ObjectStructuralLinkRuntime* structuralLinks = nullptr
    );

private:
    std::vector<ObjectModuleState>         m_modules;
    std::unordered_map<std::string, int>   m_moduleIndexById;
};

} // namespace world::modules