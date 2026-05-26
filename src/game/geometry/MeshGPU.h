#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
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


            




// ВАЖНО:
// Не перезаписываем нормали faceNormal'ом.
// ObjLoader уже посчитал сглаженные vertex normals.
//
// Если здесь поставить одну normal на весь треугольник,
// корпус будет выглядеть как лоскутное одеяло:
// каждый треугольник ловит свет отдельно.
auto safeNormalize = [](const glm::vec3& n) -> glm::vec3
{
    float len = glm::length(n);

    if (len < 1e-6f)
        return glm::vec3(0.0f, 0.0f, 1.0f);

    return n / len;
};

v0.normal = safeNormalize(v0.normal);
v1.normal = safeNormalize(v1.normal);
v2.normal = safeNormalize(v2.normal);




            // glm::vec3 p0 = v0.position;
            // glm::vec3 p1 = v1.position;
            // glm::vec3 p2 = v2.position;

            // glm::vec3 faceNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));

            // v0.normal = faceNormal;
            // v1.normal = faceNormal;
            // v2.normal = faceNormal;














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

        // std::cout
        //     << "[MeshGPU] V=" << mesh.vertices.size()
        //     << " T=" << mesh.triangles.size()
        //     << " E=" << mesh.edges.size()
        //     << std::endl;

        // std::cout
        //     << "[MeshGPU] GPU vertices="
        //     << gpuVerts.size()
        //     << std::endl;

        // std::cout
        //     << "[MeshGPU] ShipVertex size: "
        //     << sizeof(game::ship::geometry::MeshVertex)
        //     << std::endl;
        
        // std::cout << "Edge VBO created: " << edgeVBO << std::endl;
        // std::cout << "Edge VAO created: " << vaoEdges << std::endl;
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



void drawInstanced(const std::vector<glm::mat4>& models) const
{
    if (models.empty())
        return;

    glBindVertexArray(vao);

    if (instanceVBO == 0)
    {
        glGenBuffers(1, &instanceVBO);
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(models.size() * sizeof(glm::mat4)),
        models.data(),
        GL_DYNAMIC_DRAW
    );

    const GLsizei vec4Size =
        static_cast<GLsizei>(sizeof(glm::vec4));

    const GLsizei mat4Size =
        static_cast<GLsizei>(sizeof(glm::mat4));

    // instanceModel column 0
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(0)
    );
    glVertexAttribDivisor(4, 1);

    // instanceModel column 1
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(
        5,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(vec4Size)
    );
    glVertexAttribDivisor(5, 1);

    // instanceModel column 2
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(
        6,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(vec4Size * 2)
    );
    glVertexAttribDivisor(6, 1);

    // instanceModel column 3
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(
        7,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(vec4Size * 3)
    );
    glVertexAttribDivisor(7, 1);

    glDrawArraysInstanced(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(vertexCount),
        static_cast<GLsizei>(models.size())
    );

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}



void drawEdgesInstanced(const std::vector<glm::mat4>& models) const
{
    if (models.empty())
        return;

    if (vaoEdges == 0 || edgeVertexCount == 0)
        return;

    glBindVertexArray(vaoEdges);

    if (instanceEdgeVBO == 0)
    {
        glGenBuffers(1, &instanceEdgeVBO);
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceEdgeVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(models.size() * sizeof(glm::mat4)),
        models.data(),
        GL_DYNAMIC_DRAW
    );

    const GLsizei vec4Size =
        static_cast<GLsizei>(sizeof(glm::vec4));

    const GLsizei mat4Size =
        static_cast<GLsizei>(sizeof(glm::mat4));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(0)
    );
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(
        5,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(vec4Size)
    );
    glVertexAttribDivisor(5, 1);

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(
        6,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(vec4Size * 2)
    );
    glVertexAttribDivisor(6, 1);

    glEnableVertexAttribArray(7);
    glVertexAttribPointer(
        7,
        4,
        GL_FLOAT,
        GL_FALSE,
        mat4Size,
        reinterpret_cast<void*>(vec4Size * 3)
    );
    glVertexAttribDivisor(7, 1);

    glDrawArraysInstanced(
        GL_LINES,
        0,
        static_cast<GLsizei>(edgeVertexCount),
        static_cast<GLsizei>(models.size())
    );

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}








   void drawEdges() const
{
    glBindVertexArray(vaoEdges);

    glDrawArrays(
        GL_LINES,
        0,
        edgeVertexCount
    );

    glBindVertexArray(0);
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


    mutable GLuint instanceVBO = 0;
    mutable GLuint instanceEdgeVBO = 0;
};

}