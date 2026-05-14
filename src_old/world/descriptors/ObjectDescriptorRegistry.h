#pragma once

#include <unordered_map>
#include <memory>
#include "src/world/objects/StaticObject.h"
#include "src/world/types/ObjectType.h"
#include "IObjectDescriptor.h"

class ObjectDescriptorRegistry
{
public:
    static const IObjectDescriptor& get(ObjectType type);
    static void init();

private:
    static std::unordered_map<ObjectType, std::unique_ptr<IObjectDescriptor>> registry;
};