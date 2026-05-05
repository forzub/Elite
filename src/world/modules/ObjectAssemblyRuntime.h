#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "src/game/geometry/ObjectAssembly.h"
#include "src/game/simulation/ObjectAssemblyModuleSnapshot.h"

namespace world::modules
{

struct ObjectAssemblyModuleRuntimeState
{
    std::string moduleId;
    std::string parentModuleId;
    int parentIndex = -1;

    bool rotates = false;
    float rotationSpeed = 0.0f;
    float rotationAngleRad = 0.0f;
};

class ObjectAssemblyRuntime
{
public:
    void init(const game::ship::geometry::ObjectAssembly& assembly);
    bool update(double dt);

    bool hasModule(const std::string& moduleId) const;
    int findModuleIndex(const std::string& moduleId) const;
    float getModuleRotationAngleRad(const std::string& moduleId) const;

    const std::vector<ObjectAssemblyModuleRuntimeState>& modules() const { return m_modules; }
    std::vector<ObjectAssemblyModuleRuntimeState>& modules() { return m_modules; }

    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> buildSnapshots() const;

private:
    std::vector<ObjectAssemblyModuleRuntimeState> m_modules;
    std::unordered_map<std::string, int> m_indexById;
};

} // namespace world::modules