#pragma once

#include <unordered_map>
#include "ObjectAssembly.h"

namespace game::ship::geometry
{

class ObjectAssemblyRegistry
{
public:
    static void init();
    static bool has(ObjectType typeId);
    static const ObjectAssemblyDesc& get(ObjectType typeId);

private:
    static std::unordered_map<uint16_t, ObjectAssemblyDesc> s_registry;
};

} // namespace game::ship::geometry