#include <glad/gl.h>
#include "ShipMeshRenderer.h"
#include "src/game/geometry/MeshGPU.h"


using namespace render;
using namespace game::ship::geometry;

void ShipMeshRenderer::draw(const render::MeshGPU& mesh)
{
        mesh.draw();
}
