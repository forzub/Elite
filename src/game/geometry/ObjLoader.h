#pragma once

#include <string>
#include "MeshData.h"

namespace game::ship::geometry
{

class ObjLoader
{
public:

    static bool load(
        const std::string& path,
        MeshData& mesh
    );

    
};

}