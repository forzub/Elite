#pragma once

#include <unordered_map>
#include <memory>
#include "MeshGPU.h"

#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/world/types/ObjectType.h"


namespace render
{

class MeshLibrary
{
public:

    static MeshGPU& get(ObjectType typeId);
    static const game::ship::geometry::MeshData& getMeshData(ObjectType typeId);
    
private:

    static std::unordered_map<uint16_t, MeshGPU>                            meshes;
    static std::unordered_map<uint16_t, game::ship::geometry::MeshData>     meshData;
    std::unordered_map<std::string, MeshGPU>                                m_meshesByName;
};

}