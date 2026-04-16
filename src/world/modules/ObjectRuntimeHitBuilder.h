#pragma once

#include "src/game/damage/HitComponent.h"
#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/types/ObjectType.h"

namespace world::modules
{

class ObjectRuntimeHitBuilder
{
public:
    static void rebuild(
        game::damage::HitComponent& hitComponent,
        ObjectType typeId,
        const IObjectDescriptor& descriptor,
        const ObjectAssemblyRuntime& assemblyRuntime
    );
};

} // namespace world::modules