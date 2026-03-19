
#include "ObjLoader.h"

#include <iostream>
#include <unordered_map>
#include <cmath>
#include <vector>
#include <algorithm>

#define TINYOBJLOADER_IMPLEMENTATION
#include "render/tiny_obj_loader.h"

using namespace game::ship::geometry;

bool ObjLoader::load(
    const std::string& path,
    MeshData& mesh
)
{

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool triangulate = false;

    bool ok = tinyobj::LoadObj(
        &attrib,
        &shapes,
        &materials,
        &warn,
        &err,
        path.c_str(),
        nullptr,
        triangulate
    );

    if(!warn.empty()) std::cout << "[ObjectLoader] OBJ WARNING: " << warn << std::endl;
    if(!err.empty()) std::cout << "[ObjectLoader] OBJ ERROR: " << err << std::endl;

    if(!ok)
    {
        std::cout << "[ObjectLoader] OBJ LOAD FAILED" << std::endl;
        return false;
    }

    mesh.vertices.clear();
    mesh.triangles.clear();
    mesh.edges.clear();

    // -------------------------
    // Загрузка вершин
    // -------------------------
    for(size_t v = 0; v < attrib.vertices.size() / 3; v++)
    {
        MeshVertex vert;
        vert.position = glm::vec3(
            attrib.vertices[3*v+0],
            attrib.vertices[3*v+1],
            attrib.vertices[3*v+2]
        );
        vert.normal = glm::vec3(0,0,0);
        vert.bary   = glm::vec3(0,0,0);
        mesh.vertices.push_back(vert);
    }


    // ============================================================
    // НОВОЕ: Центрирование модели
    // ============================================================
    // Вычисляем bounding box
    glm::vec3 minBound = glm::vec3(1e9f);
    glm::vec3 maxBound = glm::vec3(-1e9f);
    
    for (const auto& vert : mesh.vertices) {
        minBound = glm::min(minBound, vert.position);
        maxBound = glm::max(maxBound, vert.position);
    }
    
    // Вычисляем центр
    glm::vec3 center = (minBound + maxBound) * 0.5f;
    
    // std::cout << "[ObjLoader] Model \"" << path << "\" original bounds:"
    //           << " min(" << minBound.x << ", " << minBound.y << ", " << minBound.z << ")"
    //           << " max(" << maxBound.x << ", " << maxBound.y << ", " << maxBound.z << ")"
    //           << " center(" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
    
    // Сдвигаем все вершины так, чтобы центр оказался в (0,0,0)
    for (auto& vert : mesh.vertices) {
        vert.position -= center;
    }



    // -------------------------
    // Сварка вершин (удаление дубликатов)
    // -------------------------
    const float weldEps = 1e-4f;

    struct VecHash
    {
        size_t operator()(const glm::ivec3& v) const
        {
            return std::hash<int>()(v.x * 73856093) ^ 
                   std::hash<int>()(v.y * 19349663) ^ 
                   std::hash<int>()(v.z * 83492791);
        }
    };

    std::unordered_map<glm::ivec3, int, VecHash> weldMap;
    std::vector<MeshVertex> weldedVertices;
    std::vector<int> remap(mesh.vertices.size());

    for(size_t i = 0; i < mesh.vertices.size(); i++)
    {
        glm::vec3 p = mesh.vertices[i].position;
        glm::ivec3 key(
            int(std::round(p.x / weldEps)),
            int(std::round(p.y / weldEps)),
            int(std::round(p.z / weldEps))
        );

        auto it = weldMap.find(key);
        if(it == weldMap.end())
        {
            int newIndex = weldedVertices.size();
            weldMap[key] = newIndex;
            weldedVertices.push_back(mesh.vertices[i]);
            remap[i] = newIndex;
        }
        else
        {
            remap[i] = it->second;
        }
    }

    mesh.vertices = weldedVertices;

    // -------------------------
    // Загрузка треугольников (триангуляция полигонов)
    // -------------------------
    int globalFaceId = 0;

    for (const auto& shape : shapes)
    {
        size_t indexOffset = 0;

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            int fv = shape.mesh.num_face_vertices[f];
            
            if (fv < 3)
            {
                indexOffset += fv;
                continue;
            }

            tinyobj::index_t i0 = shape.mesh.indices[indexOffset + 0];

            for (int k = 1; k < fv - 1; k++)
            {
                tinyobj::index_t i1 = shape.mesh.indices[indexOffset + k];
                tinyobj::index_t i2 = shape.mesh.indices[indexOffset + k + 1];

                int v0 = remap[i0.vertex_index];
                int v1 = remap[i1.vertex_index];
                int v2 = remap[i2.vertex_index];

                if(v0 == v1 || v1 == v2 || v2 == v0)
                    continue;

                mesh.triangles.push_back({
                    v0, v1, v2,
                    globalFaceId
                });
            }

            indexOffset += fv;
            globalFaceId++;
        }
    }

    // -------------------------
    // Вычисление нормалей граней (ненормализованных)
    // -------------------------
    std::vector<glm::vec3> faceNormals;
    faceNormals.reserve(mesh.triangles.size());

    for(const auto& t : mesh.triangles)
    {
        glm::vec3 a = mesh.vertices[t.v0].position;
        glm::vec3 b = mesh.vertices[t.v1].position;
        glm::vec3 c = mesh.vertices[t.v2].position;

        glm::vec3 n = glm::cross(b - a, c - a);
        faceNormals.push_back(n); // Сохраняем ненормализованную для проверки объема
    }


    
    // обнулить
    for(auto& v : mesh.vertices)
    {
        v.normal = glm::vec3(0,0,0);
    }

    // накопить
    for(size_t i = 0; i < mesh.triangles.size(); i++)
    {
        const auto& t = mesh.triangles[i];
        glm::vec3 n = glm::normalize(faceNormals[i]);

        mesh.vertices[t.v0].normal += n;
        mesh.vertices[t.v1].normal += n;
        mesh.vertices[t.v2].normal += n;
    }

    // нормализовать
    for(auto& v : mesh.vertices)
    {
        v.normal = glm::normalize(v.normal);
    }





    // -------------------------
    // Построение карты рёбер
    // -------------------------
    struct EdgeKey
    {
        int a;
        int b;

        bool operator==(const EdgeKey& o) const
        {
            return a == o.a && b == o.b;
        }
    };

    struct EdgeHash
    {
        size_t operator()(const EdgeKey& k) const
        {
            return std::hash<int>()(k.a * 73856093) ^ 
                   std::hash<int>()(k.b * 19349663);
        }
    };

    struct EdgeInfo
    {
        int triA = -1;
        int triB = -1;
        int polyA = -1;
        int polyB = -1;
    };

    std::unordered_map<EdgeKey, EdgeInfo, EdgeHash> edgeMap;

    for(size_t t = 0; t < mesh.triangles.size(); t++)
    {
        const auto& tri = mesh.triangles[t];
        int ids[3] = { tri.v0, tri.v1, tri.v2 };

        for(int e = 0; e < 3; e++)
        {
            int a = ids[e];
            int b = ids[(e + 1) % 3];
            if(a > b) std::swap(a, b);

            EdgeKey key{a, b};
            auto& info = edgeMap[key];

            if(info.triA == -1)
            {
                info.triA = t;
                info.polyA = tri.faceId;
            }
            else
            {
                info.triB = t;
                info.polyB = tri.faceId;
            }
        }
    }

    // -------------------------
    // ФУНКЦИЯ ПРОВЕРКИ КОМПЛАНАРНОСТИ
    // -------------------------
    auto areTrianglesCoplanar = [&](int triA, int triB) -> bool {
        const auto& tA = mesh.triangles[triA];
        const auto& tB = mesh.triangles[triB];
        
        // Берем три точки из первого треугольника
        glm::vec3 a1 = mesh.vertices[tA.v0].position;
        glm::vec3 a2 = mesh.vertices[tA.v1].position;
        glm::vec3 a3 = mesh.vertices[tA.v2].position;
        
        // Вычисляем нормаль первого треугольника (ненормализованную)
        glm::vec3 nA = faceNormals[triA];
        float lenA = glm::length(nA);
        if(lenA < 1e-6f) return false;
        
        // Проверяем все три вершины второго треугольника
        int bIndices[3] = { tB.v0, tB.v1, tB.v2 };
        
        for(int i = 0; i < 3; i++)
        {
            glm::vec3 b = mesh.vertices[bIndices[i]].position;
            
            // Объем параллелепипеда = |(b - a1) · nA|
            // Если точка b лежит в плоскости, то смешанное произведение = 0
            float volume = std::abs(glm::dot(b - a1, nA));
            
            // Допуск: объем должен быть мал относительно размера треугольника
            if(volume > lenA * 0.001f) 
            {
                return false; // Точка не в плоскости
            }
        }
        
        return true; // Все три точки в плоскости
    };

    // -------------------------
    // ОПРЕДЕЛЕНИЕ ВИДИМЫХ РЁБЕР (ИСПРАВЛЕННАЯ ВЕРСИЯ)
    // -------------------------
    mesh.edges.clear();

    for(const auto& kv : edgeMap)
    {
        const EdgeKey& key = kv.first;
        const EdgeInfo& info = kv.second;

        // Случай 1: Граничное ребро (только один треугольник) - всегда рисуем
        if(info.triB == -1)
        {
            MeshEdge e;
            e.a = mesh.vertices[key.a].position;
            e.b = mesh.vertices[key.b].position;
            mesh.edges.push_back(e);
            continue;
        }

        // Случай 2: Диагональ внутри одного полигона - никогда не рисуем
        if(info.polyA == info.polyB)
        {
            continue;
        }

        // Случай 3: Ребро между двумя разными полигонами
        // Проверяем, лежат ли треугольники в одной геометрической плоскости
        if(areTrianglesCoplanar(info.triA, info.triB))
        {
            // В одной плоскости - не рисуем (это внутреннее ребро)
            continue;
        }

        




        // Если треугольники в разных плоскостях - рисуем ребро (это граница)
        MeshEdge e;
        e.a = mesh.vertices[key.a].position;
        e.b = mesh.vertices[key.b].position;
        mesh.edges.push_back(e);
    }

    mesh.computeBounds();
    mesh.computeBoundingSphere();

    return true;
}