#pragma once

#include <unordered_map>
#include "ObjectAssembly.h"

namespace game::ship::geometry
{

class AssemblyMeshLibrary
{
public:
    static bool has(ObjectType typeId);
    static const ObjectAssembly& get(ObjectType typeId);

private:
    static ObjectAssembly loadAssembly(ObjectType typeId);

    static void computeRawBoundsFromObj(
        const std::string& path,
        glm::vec3& outMin,
        glm::vec3& outMax,
        glm::vec3& outCenter
    );

    static void computeModuleBounds(AssemblyModule& module);
    static void computeAssemblyBounds(ObjectAssembly& assembly);
    static void normalizeAssemblyToDescriptorSize(ObjectAssembly& assembly);
    static void computeAssemblyBoundingSphere(ObjectAssembly& assembly);

private:
    static std::unordered_map<uint16_t, ObjectAssembly> s_cache;
};

} // namespace game::ship::geometry