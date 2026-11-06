#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <cstddef>
#include "MeshData.h"

namespace render
{

class MeshGPU
{
public:

    // CPU side of an upload (no GL): the expanded triangle vertices (barycentrics, face colour,
    // normalised normals) and the edge lines. Thread-safe - a loader thread may build it.
    struct Prepared
    {
        std::vector<game::ship::geometry::MeshVertex> verts;
        std::vector<glm::vec3> edgeVerts;
        std::vector<float> edgeWeights;
    };

    static Prepared prepare(const game::ship::geometry::MeshData& mesh)
    {
        Prepared out;
        out.verts.reserve(mesh.triangles.size() * 3);
        // Do not replace the normals by the face normal: the loader computed smoothed vertex
        // normals (one normal per triangle reads as a patchwork quilt).
        const auto safeNormalize = [](const glm::vec3& n) -> glm::vec3 {
            const float len = glm::length(n);
            return len < 1e-6f ? glm::vec3(0.0f, 0.0f, 1.0f) : n / len;
        };
        for (const auto& t : mesh.triangles)
        {
            auto v0 = mesh.vertices[t.v0];
            auto v1 = mesh.vertices[t.v1];
            auto v2 = mesh.vertices[t.v2];
            v0.bary = glm::vec3(1, 0, 0);
            v1.bary = glm::vec3(0, 1, 0);
            v2.bary = glm::vec3(0, 0, 1);
            // material base colour of the face (attribute 3): model data, each style interprets it
            v0.debugColor = v1.debugColor = v2.debugColor = t.baseColor;
            v0.normal = safeNormalize(v0.normal);
            v1.normal = safeNormalize(v1.normal);
            v2.normal = safeNormalize(v2.normal);
            out.verts.push_back(v0);
            out.verts.push_back(v1);
            out.verts.push_back(v2);
        }
        out.edgeVerts.reserve(mesh.edges.size() * 2);
        out.edgeWeights.reserve(mesh.edges.size() * 2);
        for (const auto& e : mesh.edges)
        {
            out.edgeVerts.push_back(e.a);
            out.edgeVerts.push_back(e.b);
            out.edgeWeights.push_back(e.weight);
            out.edgeWeights.push_back(e.weight);
        }
        return out;
    }

    void upload(const game::ship::geometry::MeshData& mesh)
    {
        const Prepared prepared = prepare(mesh);
        allocate(prepared, true);
    }

    // GL side: the VAOs and buffers. withVertexData = false leaves the triangle buffer
    // allocated but empty, to be filled by uploadChunk() over several frames.
    void allocate(const Prepared& p, bool withVertexData)
    {
        vertexCount = p.verts.size();
        edgeVertexCount = p.edgeVerts.size();

        glGenVertexArrays(1, &vaoEdges);
        glBindVertexArray(vaoEdges);
        glGenBuffers(1, &edgeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
        glBufferData(GL_ARRAY_BUFFER, p.edgeVerts.size() * sizeof(glm::vec3), p.edgeVerts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        // structural edge weight (attribute 1)
        glGenBuffers(1, &edgeWeightVBO);
        glBindBuffer(GL_ARRAY_BUFFER, edgeWeightVBO);
        glBufferData(GL_ARRAY_BUFFER, p.edgeWeights.size() * sizeof(float), p.edgeWeights.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
        glBindVertexArray(0);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, p.verts.size() * sizeof(game::ship::geometry::MeshVertex),
                     withVertexData ? p.verts.data() : nullptr, GL_STATIC_DRAW);
        using V = game::ship::geometry::MeshVertex;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, bary));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, debugColor));
        glBindVertexArray(0);
    }

    // Fills the triangle buffer from vertex `offset` with at most `maxVerts` vertices; returns
    // the new offset (== p.verts.size() when complete).
    std::size_t uploadChunk(const Prepared& p, std::size_t offset, std::size_t maxVerts)
    {
        const std::size_t n = std::min(maxVerts, p.verts.size() - offset);
        if (n == 0)
            return offset;
        using V = game::ship::geometry::MeshVertex;
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset * sizeof(V)), static_cast<GLsizeiptr>(n * sizeof(V)),
                        p.verts.data() + offset);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return offset + n;
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
    GLuint edgeWeightVBO = 0;

    size_t vertexCount = 0;
    size_t edgeVertexCount = 0;


    mutable GLuint instanceVBO = 0;
    mutable GLuint instanceEdgeVBO = 0;
};

}