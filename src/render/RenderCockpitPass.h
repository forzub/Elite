#pragma once

#include "Mesh.h"
#include <vector>
#include "src/game/ship/cockpit/CockpitContours.h"


struct GLMesh
    {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        uint32_t indexCount = 0;
    };


class RenderCockpitPass
{
public:
    void init();
    void render();
    void setGeometry(const CockpitGeometry& geometry);


private:

    

    // Шейдеры
    unsigned int fillShader = 0;
    unsigned int strokeShader = 0;

    // uniform locations (fill)
    int uFillType  = -1;
    int uColorA    = -1;
    int uColorB    = -1;
    int uGradFrom  = -1;
    int uGradTo    = -1;

    struct CockpitGLCommand
{
    CockpitDrawType type;
    GLMesh mesh;

    // ===== FILL =====
    CockpitFillType fillType = CockpitFillType::Solid;
    glm::vec4 colorA = {1,1,1,1};
    glm::vec4 colorB = {1,1,1,1};
    glm::vec2 gradFrom = {0,0};
    glm::vec2 gradTo   = {0,1};

    // ===== STROKE =====
    glm::vec4 strokeColor = {1,1,1,1};
};




    unsigned int shader = 0;
    // std::vector<GLMesh> glMeshes;
    std::vector<CockpitGLCommand> commands;

    
    GLMesh uploadMesh(const Mesh2D& mesh);
    
};
