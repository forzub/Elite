#pragma once

#include "src/game/damage/HitComponent.h"
#include "src/world/types/ObjectType.h"
#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"

namespace world::modules
{

class ObjectRuntimeHitBuilder
{
public:
    static void rebuild(
        game::damage::HitComponent& hitComponent,
        ObjectType typeId,
        const IObjectDescriptor& descriptor,
        const ObjectModuleRuntime& moduleRuntime,
        ObjectStructuralLinkRuntime& structuralLinkRuntime,
        const ObjectAssemblyRuntime& assemblyRuntime
    );
};

} // namespace world::modules