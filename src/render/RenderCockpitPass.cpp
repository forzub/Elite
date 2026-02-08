#include "RenderCockpitPass.h"
#include <glad/gl.h>
#include <iostream>

#include "render/ShaderUtils.h"



//    ##                ##       ##
//                               ##
//   ###     #####     ###      #####
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ## ##
//   ####    ##  ##    ####       ###

void RenderCockpitPass::init()
{
    // glGenVertexArrays(1, &vao);
    // glGenBuffers(1, &vbo);
    // glGenBuffers(1, &ebo);

    shader = compileShaderFromFiles(
        "assets/shaders/cockpit/cockpit.vert",
        "assets/shaders/cockpit/cockpit.frag"
    );
   
   
    if(!shader){std::cerr << "Cockpits shaders is not found\n";}  
}


//                      ##                                         ###
//                      ##                                          ##
//   #####    ####     #####            ##  ##    ####     #####    ##
//  ##       ##  ##     ##              #######  ##  ##   ##        #####
//   #####   ######     ##              ## # ##  ######    #####    ##  ##
//       ##  ##         ## ##           ##   ##  ##            ##   ##  ##
//  ######    #####      ###            ##   ##   #####   ######   ###  ##

void RenderCockpitPass::setMesh(const Mesh2D& mesh)
{
    // очистка
    for (auto& m : glMeshes)
    {
        glDeleteVertexArrays(1, &m.vao);
        glDeleteBuffers(1, &m.vbo);
        glDeleteBuffers(1, &m.ebo);
    }
    glMeshes.clear();

    addMesh(mesh);
    
}

//              ###      ###                                       ###
//               ##       ##                                        ##
//   ####        ##       ##            ##  ##    ####     #####    ##
//      ##    #####    #####            #######  ##  ##   ##        #####
//   #####   ##  ##   ##  ##            ## # ##  ######    #####    ##  ##
//  ##  ##   ##  ##   ##  ##            ##   ##  ##            ##   ##  ##
//   #####    ######   ######           ##   ##   #####   ######   ###  ##


void RenderCockpitPass::addMesh(const Mesh2D& mesh)
{
    // Добавьте отладку:
    std::cout << "Adding mesh with " << mesh.vertices.size() 
            << " vertices and " << mesh.indices.size() 
            << " indices" << std::endl;

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        std::cout << "  Vertex " << i << ": (" 
                << mesh.vertices[i].position.x << ", "
                << mesh.vertices[i].position.y << ")" << std::endl;
    }


    GLMesh gm;
    gm.indexCount = static_cast<uint32_t>(mesh.indices.size());

    glGenVertexArrays(1, &gm.vao);
    glGenBuffers(1, &gm.vbo);
    glGenBuffers(1, &gm.ebo);

    glBindVertexArray(gm.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        mesh.vertices.size() * sizeof(Vertex2D),
        mesh.vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        mesh.indices.size() * sizeof(uint32_t),
        mesh.indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vertex2D),
        (void*)offsetof(Vertex2D, position)
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE,
        sizeof(Vertex2D),
        (void*)offsetof(Vertex2D, color)
    );

    glBindVertexArray(0);

    glMeshes.push_back(gm);
}



//                                ###
//                                 ##
//  ######    ####    #####        ##    ####    ######
//   ##  ##  ##  ##   ##  ##    #####   ##  ##    ##  ##
//   ##      ######   ##  ##   ##  ##   ######    ##
//   ##      ##       ##  ##   ##  ##   ##        ##
//  ####      #####   ##  ##    ######   #####   ####

void RenderCockpitPass::render()
{
    if (shader == 0 || glMeshes.empty())
        return;



    glDisable(GL_DEPTH_TEST);
    glUseProgram(shader);

    for (const auto& m : glMeshes)
    {
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}



//    ##                         ##
//    ##                         ##
//   #####    ####     #####    #####
//    ##     ##  ##   ##         ##
//    ##     ######    #####     ##
//    ## ##  ##            ##    ## ##
//     ###    #####   ######      ###


Mesh2D RenderCockpitPass::makeTestQuad()
{
    Mesh2D mesh;

    // Квадрат в центре экрана
    mesh.vertices = {
        { {-0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} }, // левый низ
        { { 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} }, // правый низ
        { { 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} }, // правый верх
        { {-0.5f,  0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} }  // левый верх
    };

    // Два треугольника
    mesh.indices = {
        0, 1, 2,
        2, 3, 0
    };

    return mesh;
}



