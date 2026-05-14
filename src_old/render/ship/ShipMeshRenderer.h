#pragma once

#include "src/game/geometry/MeshData.h"
#include "src/game/geometry/MeshGPU.h"

namespace render
{

class ShipMeshRenderer
{
public:

    static void draw(
        const render::MeshGPU& mesh
    );

};

}

