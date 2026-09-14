#pragma once

#include "src/game/geometry/ObjectAssembly.h"
#include "src/model_asset/ModelAsset.h"

namespace game::assets
{

// Transitional compatibility adapter. Legacy OBJ/ObjectAssembly input is
// lifted into the canonical ModelAsset schema so runtime consumers can migrate
// toward one data model instead of teaching new code about both formats.
class LegacyAssemblyModelAdapter
{
public:
    static elite::model_asset::ModelAsset convert(
        ObjectType typeId,
        const game::ship::geometry::ObjectAssembly& assembly);
};

} // namespace game::assets
