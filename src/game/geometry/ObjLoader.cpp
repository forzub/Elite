
#include "ObjLoader.h"
#include "src/model_asset/RuntimeMeshNormalizer.h"

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
    MeshData& mesh,
    bool centerModel
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

    // if(!warn.empty()) std::cout << "[ObjectLoader] OBJ WARNING: " << warn << std::endl;
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
    // glm::vec3 minBound = glm::vec3(1e9f);
    // glm::vec3 maxBound = glm::vec3(-1e9f);
    
    // for (const auto& vert : mesh.vertices) {
    //     minBound = glm::min(minBound, vert.position);
    //     maxBound = glm::max(maxBound, vert.position);
    // }
    
    // // Вычисляем центр
    // glm::vec3 center = (minBound + maxBound) * 0.5f;
    
    // // std::cout << "[ObjLoader] Model \"" << path << "\" original bounds:"
    // //           << " min(" << minBound.x << ", " << minBound.y << ", " << minBound.z << ")"
    // //           << " max(" << maxBound.x << ", " << maxBound.y << ", " << maxBound.z << ")"
    // //           << " center(" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
    
    // // Сдвигаем все вершины так, чтобы центр оказался в (0,0,0)
    // for (auto& vert : mesh.vertices) {
    //     vert.position -= center;
    // }


    // ============================================================
    // Опциональное центрирование модели
    // ============================================================
    if (centerModel)
    {
        glm::vec3 minBound = glm::vec3(1e9f);
        glm::vec3 maxBound = glm::vec3(-1e9f);

        for (const auto& vert : mesh.vertices)
        {
            minBound = glm::min(minBound, vert.position);
            maxBound = glm::max(maxBound, vert.position);
        }

        glm::vec3 center = (minBound + maxBound) * 0.5f;

        for (auto& vert : mesh.vertices)
        {
            vert.position -= center;
        }
    }



    // -------------------------
    // Shared runtime mesh normalization
    // -------------------------
    // The editor uses the same topology normalizer. Keep OBJ parsing here, but
    // do not maintain a second weld/cleanup implementation: one contract now
    // owns positional weld, collapsed triangles and exact duplicate removal.
    std::vector<elite::model_asset::RuntimeMeshTriangleInput> rawTriangles;
    int globalFaceId = 0;
    for (const auto& shape : shapes)
    {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            const int fv = shape.mesh.num_face_vertices[f];
            if (fv < 3)
            {
                indexOffset += static_cast<size_t>(std::max(fv, 0));
                ++globalFaceId;
                continue;
            }

            const tinyobj::index_t i0 = shape.mesh.indices[indexOffset];
            for (int k = 1; k < fv - 1; ++k)
            {
                const tinyobj::index_t i1 = shape.mesh.indices[indexOffset + static_cast<size_t>(k)];
                const tinyobj::index_t i2 = shape.mesh.indices[indexOffset + static_cast<size_t>(k + 1)];
                if (i0.vertex_index < 0 || i1.vertex_index < 0 || i2.vertex_index < 0)
                    continue;
                rawTriangles.push_back({
                    static_cast<std::uint32_t>(i0.vertex_index),
                    static_cast<std::uint32_t>(i1.vertex_index),
                    static_cast<std::uint32_t>(i2.vertex_index),
                    globalFaceId
                });
            }
            indexOffset += static_cast<size_t>(fv);
            ++globalFaceId;
        }
    }

    std::vector<glm::vec3> rawPositions;
    rawPositions.reserve(mesh.vertices.size());
    for (const auto& vertex : mesh.vertices)
        rawPositions.push_back(vertex.position);

    const auto normalized = elite::model_asset::normalizeRuntimeMeshTopology(
        rawPositions, rawTriangles, elite::model_asset::RuntimeMeshWeldEpsilon);
    if (!normalized.success)
    {
        std::cout << "[ObjLoader] RUNTIME NORMALIZATION FAILED: " << normalized.error << std::endl;
        return false;
    }

    mesh.vertices.clear();
    mesh.vertices.reserve(normalized.positions.size());
    for (const auto& position : normalized.positions)
    {
        MeshVertex vertex;
        vertex.position = position;
        vertex.normal = glm::vec3(0.0f);
        vertex.bary = glm::vec3(0.0f);
        mesh.vertices.push_back(vertex);
    }

    mesh.triangles.clear();
    mesh.triangles.reserve(normalized.triangles.size());
    for (const auto& triangle : normalized.triangles)
    {
        mesh.triangles.push_back({
            static_cast<int>(triangle.a),
            static_cast<int>(triangle.b),
            static_cast<int>(triangle.c),
            triangle.sourceTag
        });
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
    // auto areTrianglesCoplanar = [&](int triA, int triB) -> bool {
    //     const auto& tA = mesh.triangles[triA];
    //     const auto& tB = mesh.triangles[triB];
        
    //     // Берем три точки из первого треугольника
    //     glm::vec3 a1 = mesh.vertices[tA.v0].position;
    //     glm::vec3 a2 = mesh.vertices[tA.v1].position;
    //     glm::vec3 a3 = mesh.vertices[tA.v2].position;
        
    //     // Вычисляем нормаль первого треугольника (ненормализованную)
    //     glm::vec3 nA = faceNormals[triA];
    //     float lenA = glm::length(nA);
    //     if(lenA < 1e-6f) return false;
        
    //     // Проверяем все три вершины второго треугольника
    //     int bIndices[3] = { tB.v0, tB.v1, tB.v2 };
        
    //     for(int i = 0; i < 3; i++)
    //     {
    //         glm::vec3 b = mesh.vertices[bIndices[i]].position;
            
    //         // Объем параллелепипеда = |(b - a1) · nA|
    //         // Если точка b лежит в плоскости, то смешанное произведение = 0
    //         float volume = std::abs(glm::dot(b - a1, nA));
            
    //         // Допуск: объем должен быть мал относительно размера треугольника
    //         if(volume > lenA * 0.001f) 
    //         {
    //             return false; // Точка не в плоскости
    //         }
    //     }
        
    //     return true; // Все три точки в плоскости
    // };


    

    // -------------------------
    // ОПРЕДЕЛЕНИЕ ВИДИМЫХ РЁБЕР (ИСПРАВЛЕННАЯ ВЕРСИЯ)
    // -------------------------
    // mesh.edges.clear();

    // for(const auto& kv : edgeMap)
    // {
    //     const EdgeKey& key = kv.first;
    //     const EdgeInfo& info = kv.second;

    //     // Случай 1: Граничное ребро (только один треугольник) - всегда рисуем
    //     if(info.triB == -1)
    //     {
    //         MeshEdge e;
    //         e.a = mesh.vertices[key.a].position;
    //         e.b = mesh.vertices[key.b].position;
    //         mesh.edges.push_back(e);
    //         continue;
    //     }

    //     // Случай 2: Диагональ внутри одного полигона - никогда не рисуем
    //     if(info.polyA == info.polyB)
    //     {
    //         continue;
    //     }

    //     // Случай 3: Ребро между двумя разными полигонами
    //     // Проверяем, лежат ли треугольники в одной геометрической плоскости
    //     if(areTrianglesCoplanar(info.triA, info.triB))
    //     {
    //         // В одной плоскости - не рисуем (это внутреннее ребро)
    //         continue;
    //     }

        




    //     // Если треугольники в разных плоскостях - рисуем ребро (это граница)
    //     MeshEdge e;
    //     e.a = mesh.vertices[key.a].position;
    //     e.b = mesh.vertices[key.b].position;
    //     mesh.edges.push_back(e);
    // }

    // mesh.computeBounds();
    // mesh.computeBoundingSphere();

    // return true;


    // -------------------------
// Фильтрация видимых рёбер
// -------------------------

auto safeFaceNormal = [&](int triIndex) -> glm::vec3
{
    if (triIndex < 0 || triIndex >= static_cast<int>(mesh.triangles.size()))
        return glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec3 n = faceNormals[triIndex];
    float len = glm::length(n);

    if (len < 1e-6f)
        return glm::vec3(0.0f, 0.0f, 1.0f);

    return n / len;
};

auto areTrianglesSamePlane = [&](int triA, int triB) -> bool
{
    if (triA < 0 || triB < 0)
        return false;

    const auto& a = mesh.triangles[triA];
    const auto& b = mesh.triangles[triB];

    glm::vec3 nA = safeFaceNormal(triA);
    glm::vec3 nB = safeFaceNormal(triB);

    // Если нормали почти одинаковые — это кандидат на одну плоскость.
    // Порог маленький: примерно до 3 градусов.
    constexpr float SamePlaneCos = 0.9986f;

    if (glm::dot(nA, nB) < SamePlaneCos)
        return false;

    const glm::vec3 p0 = mesh.vertices[a.v0].position;

    int bIds[3] = { b.v0, b.v1, b.v2 };

    // Проверяем, что вершины второго треугольника лежат в плоскости первого.
    // Это убирает диагонали на одной панели, даже если OBJ уже был нарезан треугольниками.
    constexpr float PlaneEps = 0.0025f;

    for (int i = 0; i < 3; ++i)
    {
        const glm::vec3 p = mesh.vertices[bIds[i]].position;
        float d = std::abs(glm::dot(p - p0, nA));

        if (d > PlaneEps)
            return false;
    }

    return true;
};

auto shouldKeepEdge = [&](const EdgeInfo& info) -> bool
{
    // Один соседний треугольник — это край геометрии.
    // Такое ребро оставляем.
    if (info.triA == -1)
        return false;

    if (info.triB == -1)
        return true;

    // Если оба треугольника получились из одного исходного OBJ-полигона,
    // это точно внутренняя диагональ триангуляции.
    if (info.polyA == info.polyB)
        return false;

    // Если два разных треугольника лежат в одной плоскости,
    // это тоже внутренняя диагональ/стык разрезанной плоскости.
    if (areTrianglesSamePlane(info.triA, info.triB))
        return false;

    glm::vec3 nA = safeFaceNormal(info.triA);
    glm::vec3 nB = safeFaceNormal(info.triB);

    float d = glm::clamp(glm::dot(nA, nB), -1.0f, 1.0f);

    // Рисуем только заметные переломы корпуса.
    // 25 градусов — хороший старт.
    // Если ребер мало: уменьши до 18.
    // Если треугольников всё еще много: увеличь до 35.
    constexpr float HardEdgeAngleDeg = 25.0f;
    const float hardEdgeCos = std::cos(glm::radians(HardEdgeAngleDeg));

    return d < hardEdgeCos;
};

// -------------------------
// Сборка финального списка рёбер
// -------------------------

mesh.edges.clear();

for (const auto& kv : edgeMap)
{
    const EdgeKey& key = kv.first;
    const EdgeInfo& info = kv.second;

    if (!shouldKeepEdge(info))
        continue;

    MeshEdge e;
    e.a = mesh.vertices[key.a].position;
    e.b = mesh.vertices[key.b].position;

    mesh.edges.push_back(e);
}

mesh.computeBounds();
mesh.computeBoundingSphere();

return true;
}