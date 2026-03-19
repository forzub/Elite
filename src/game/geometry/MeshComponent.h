#pragma once

#include "MeshData.h"

namespace game::ship::geometry
{

struct MeshComponent
{
    MeshData mesh;
    bool loaded = false;
};

}