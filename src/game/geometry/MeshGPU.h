#pragma once

#include <glad/gl.h>
#include <vector>
#include <iostream>
#include <map>
#include <cstddef>
#include "MeshData.h"

namespace render
{

class MeshGPU
{
public:

    void upload(const game::ship::geometry::MeshData& mesh)
    {

        // --- vertices ---
        std::vector<game::ship::geometry::MeshVertex> gpuVerts;

        gpuVerts.reserve(mesh.triangles.size() * 3);



        for (const auto& t : mesh.triangles)
        {

            auto v0 = mesh.vertices[t.v0];
            auto v1 = mesh.vertices[t.v1];
            auto v2 = mesh.vertices[t.v2];

            v0.bary = glm::vec3(1,0,0);
            v1.bary = glm::vec3(0,1,0);
            v2.bary = glm::vec3(0,0,1);

            // == цвет корабля =============
            glm::vec3 color = {0.05, 0.05, 0.1};

            v0.debugColor = color;
            v1.debugColor = color;
            v2.debugColor = color;



            gpuVerts.push_back(v0);
            gpuVerts.push_back(v1);
            gpuVerts.push_back(v2);
        }

        vertexCount = gpuVerts.size();



        // --- edges ---
        std::vector<glm::vec3> edgeVerts;

        for (const auto& e : mesh.edges)
        {
            edgeVerts.push_back(e.a);
            edgeVerts.push_back(e.b);
        }



        edgeVertexCount = edgeVerts.size();

        glGenVertexArrays(1, &vaoEdges);
        glBindVertexArray(vaoEdges);

        glGenBuffers(1,&edgeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);

        glBufferData(
            GL_ARRAY_BUFFER,
            edgeVerts.size()*sizeof(glm::vec3),
            edgeVerts.data(),
            GL_STATIC_DRAW
        );

        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(glm::vec3),
            (void*)0
        );

        glBindVertexArray(0);

        // --- VAO ---
        glGenVertexArrays(1,&vao);
        glBindVertexArray(vao);

        // vertex buffer
        glGenBuffers(1,&vbo);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);

        glBufferData(
            GL_ARRAY_BUFFER,
            gpuVerts.size()*sizeof(game::ship::geometry::MeshVertex),
            gpuVerts.data(),
            GL_STATIC_DRAW
        );


        

        // attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(game::ship::geometry::MeshVertex),
            (void*)offsetof(game::ship::geometry::MeshVertex, position)
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(game::ship::geometry::MeshVertex),
            (void*)offsetof(game::ship::geometry::MeshVertex, normal)
        );

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(game::ship::geometry::MeshVertex),
            (void*)offsetof(game::ship::geometry::MeshVertex, bary)
        );

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(
            3,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(game::ship::geometry::MeshVertex),
            (void*)offsetof(game::ship::geometry::MeshVertex, debugColor)
        );

        glBindVertexArray(0);

        std::cout
            << "[MeshGPU] V=" << mesh.vertices.size()
            << " T=" << mesh.triangles.size()
            << " E=" << mesh.edges.size()
            << std::endl;

        std::cout
            << "[MeshGPU] GPU vertices="
            << gpuVerts.size()
            << std::endl;

        std::cout
            << "[MeshGPU] ShipVertex size: "
            << sizeof(game::ship::geometry::MeshVertex)
            << std::endl;
        
        std::cout << "Edge VBO created: " << edgeVBO << std::endl;
        std::cout << "Edge VAO created: " << vaoEdges << std::endl;
    }




    void draw() const
    {
        glBindVertexArray(vao);

        glDrawArrays(
            GL_TRIANGLES,
            0,
            vertexCount
        );

        glBindVertexArray(0);
    }



    void drawEdges() const
    {
        // std::cout << "Drawing edges - edgeVertexCount: " << edgeVertexCount << std::endl;
        // std::cout << "  vaoEdges: " << vaoEdges << ", edgeVBO: " << edgeVBO << std::endl;

        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);

        glBindVertexArray(vaoEdges);

        glDrawArrays(
            GL_LINES,
            0,
            edgeVertexCount
        );

        glBindVertexArray(0);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }



   


    size_t getEdgeVertexCount() const { return edgeVertexCount; }

private:

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint edgeVBO = 0;
    GLuint vaoEdges = 0;

    size_t vertexCount = 0;
    size_t edgeVertexCount = 0;
};

}