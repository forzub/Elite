#pragma once

#include "src/game/damage/HitComponent.h"
#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/types/ObjectType.h"

namespace world::modules
{

class ObjectHitBuilder
{
public:
    static void build(
        game::damage::HitComponent& hitComponent,
        ObjectType typeId,
        const IObjectDescriptor& descriptor
    );
};

} // namespace world::modules