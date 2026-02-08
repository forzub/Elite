#pragma once

#include "Mesh.h"
#include <vector>

class RenderCockpitPass
{
public:
    void init();
    void render();

    void setMesh(const Mesh2D& mesh);
    void addMesh(const Mesh2D& mesh);

private:
    struct GLMesh
    {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        uint32_t indexCount = 0;
    };

    unsigned int shader = 0;
    std::vector<GLMesh> glMeshes;

    Mesh2D makeTestQuad();
};
