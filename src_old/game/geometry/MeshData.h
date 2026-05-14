#pragma once

#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <cmath>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace game::ship::geometry
{

    struct MeshFace
    {
        std::vector<int> vertices;
    };

    
    struct MeshEdge
    {
        glm::vec3 a;
        glm::vec3 b;
    };

    struct MeshVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 bary;
        glm::vec3 debugColor;
    };

    struct MeshTriangle
    {
        int v0;
        int v1;
        int v2;

        int faceId;
    };




    
struct MeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<MeshFace> faces;
    std::vector<MeshTriangle> triangles;
    std::vector<MeshEdge> edges;

    std::vector<glm::vec3> triangleNormals;

    glm::vec3 minBounds {0.0f};
    glm::vec3 maxBounds {0.0f};

    glm::vec3 boundCenter {0.0f};
    float boundRadius = 0.0f;

    // -------------------------
    // bounds
    // -------------------------

    
    void computeBounds()
    {
        if (vertices.empty())
            return;

        minBounds = vertices[0].position;
        maxBounds = vertices[0].position;

        for (const auto& v : vertices)
        {
            minBounds = glm::min(minBounds, v.position);
            maxBounds = glm::max(maxBounds, v.position);
        }
    }



    // -------------------------
    // triangle normal
    // -------------------------

    glm::vec3 triangleNormal(int id) const
    {
        const auto& t = triangles[id];

        const glm::vec3& a = vertices[t.v0].position;
        const glm::vec3& b = vertices[t.v1].position;
        const glm::vec3& c = vertices[t.v2].position;

        glm::vec3 n = glm::cross(b - a, c - a);
        
        std::cout << "LEN " << glm::length(n) << std::endl; 

        float len = glm::length(n);

        if(len < 1e-6f)
            return glm::vec3(0,0,1); // защита от NaN

        return n / len;
    }




    // -------------------------
    // edge builder
    // -------------------------

    void buildEdges()
    {
        struct EdgeKey
        {
            int a;
            int b;

            bool operator<(const EdgeKey& o) const
            {
                if (a != o.a) return a < o.a;
                return b < o.b;
            }
        };

        struct EdgeInfo
        {
            int f0 = -1;
            int f1 = -1;
        };

        std::map<EdgeKey, EdgeInfo> edgeMap;

        // --- build adjacency map ---
        for (int f = 0; f < triangles.size(); f++)
        {
            const auto& t = triangles[f];
            int ids[3] = { t.v0, t.v1, t.v2 };

            for (int i = 0; i < 3; i++)
            {
                int a = ids[i];
                int b = ids[(i + 1) % 3];

                if (a > b) std::swap(a, b);

                EdgeKey key{ a, b };
                auto& info = edgeMap[key];

                if (info.f0 == -1)
                    info.f0 = f;
                else
                    info.f1 = f;
            }
        }

        edges.clear();

        // --- evaluate edges ---
        const float threshold = glm::cos(glm::radians(1.0f)); // как в three.js

        for (auto& [key, info] : edgeMap)
        {
            if (info.f0 == -1 || info.f1 == -1)
                continue;

            const auto& t0 = triangles[info.f0];
            const auto& t1 = triangles[info.f1];

            glm::vec3 n0 =
                vertices[t0.v0].normal +
                vertices[t0.v1].normal +
                vertices[t0.v2].normal;

            glm::vec3 n1 =
                vertices[t1.v0].normal +
                vertices[t1.v1].normal +
                vertices[t1.v2].normal;

            n0 = glm::normalize(n0);
            n1 = glm::normalize(n1);

            float dot = glm::dot(n0, n1);

            std::cout << "DOT " << dot << std::endl;

            if (dot <= threshold)
            {
                edges.push_back({
                    vertices[key.a].position,
                    vertices[key.b].position
                });
            }
        }
    }




    void computeBoundingSphere()
    {
        if (vertices.empty())
            return;

        // центр — середина AABB
        boundCenter = (minBounds + maxBounds) * 0.5f;

        float r2 = 0.0f;

        for (const auto& v : vertices)
        {
            float d2 = glm::length2(v.position - boundCenter);
            r2 = std::max(r2, d2);
        }

        boundRadius = std::sqrt(r2);
    }



    void normalizeToUnit(){
        glm::vec3 size = maxBounds - minBounds;

        float maxDim = std::max(size.x, std::max(size.y, size.z));

        if(maxDim <= 0.00001f)
            return;

        float scale = 1.0f / maxDim;

        glm::vec3 center = (minBounds + maxBounds) * 0.5f;

        for(auto& v : vertices)
        {
            v.position -= center;
            v.position *= scale;
        }

        for(auto& e : edges)
        {
            e.a -= center;
            e.b -= center;

            e.a *= scale;
            e.b *= scale;
        }

        computeBounds();
        computeBoundingSphere();
    }


};

}

