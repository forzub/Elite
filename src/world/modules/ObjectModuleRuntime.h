#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include "src/world/modules/ObjectModuleState.h"
#include "src/world/descriptors/ModuleDescriptor.h"

namespace world::modules
{

class ObjectModuleRuntime
{
public:
    void init(const std::vector<ModuleDescriptor>& descriptors);
    void buildHierarchy();

    int findModuleIndex(const std::string& moduleId) const;
    bool isMeshPartVisible(const std::string& partId) const;

    void markModuleDestroyed(const std::string& moduleId, float health = 0.0f);
    void setModuleHealth(const std::string& moduleId, float health);

    void onModuleDestroyed(int index);

    void detachSubtree(int index);
    void destroySubtree(int index);
    void disableSubtree(int index);

    void setWholeObjectDestroyed();

    const std::vector<ObjectModuleState>& modules() const { return m_modules; }
    std::vector<ObjectModuleState>& modules() { return m_modules; }

private:
    std::vector<ObjectModuleState>         m_modules;
    std::unordered_map<std::string, int>   m_moduleIndexById;
};

} // namespace world::modules