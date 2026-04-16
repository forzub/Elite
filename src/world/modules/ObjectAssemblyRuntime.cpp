#include "ObjectAssemblyRuntime.h"

#include <glm/gtc/constants.hpp>

namespace world::modules
{

void ObjectAssemblyRuntime::init(const game::ship::geometry::ObjectAssembly& assembly)
{
    m_modules.clear();
    m_indexById.clear();

    m_modules.reserve(assembly.modules.size());

    for (const auto& module : assembly.modules)
    {
        ObjectAssemblyModuleRuntimeState s;
        s.moduleId = module.id;
        s.rotates = module.rotates;
        s.rotationSpeed = module.rotationSpeed;
        s.rotationAngleRad = 0.0f;

        m_indexById[s.moduleId] = static_cast<int>(m_modules.size());
        m_modules.push_back(std::move(s));
    }
}

void ObjectAssemblyRuntime::update(double dt)
{
    constexpr float TWO_PI = glm::two_pi<float>();

    for (auto& module : m_modules)
    {
        if (!module.rotates)
            continue;

        module.rotationAngleRad += module.rotationSpeed * static_cast<float>(dt);

        while (module.rotationAngleRad >= TWO_PI)
            module.rotationAngleRad -= TWO_PI;

        while (module.rotationAngleRad < 0.0f)
            module.rotationAngleRad += TWO_PI;
    }
}

float ObjectAssemblyRuntime::getModuleRotationAngleRad(const std::string& moduleId) const
{
    auto it = m_indexById.find(moduleId);
    if (it == m_indexById.end())
        return 0.0f;

    return m_modules[it->second].rotationAngleRad;
}

std::vector<game::simulation::ObjectAssemblyModuleSnapshot>
ObjectAssemblyRuntime::buildSnapshots() const
{
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> out;
    out.reserve(m_modules.size());

    for (const auto& m : m_modules)
    {
        game::simulation::ObjectAssemblyModuleSnapshot s;
        s.moduleId = m.moduleId;
        s.rotationAngleRad = m.rotationAngleRad;
        out.push_back(std::move(s));
    }

    return out;
}

} // namespace world::modules