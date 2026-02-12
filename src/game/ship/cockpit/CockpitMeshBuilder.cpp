#include "CockpitMeshBuilder.h"

#include <glm/common.hpp>
#include <iostream>
#include <limits>
#include "render/core/earcut.hpp"

using Coord = double;
using Point = std::array<Coord, 2>; 



Mesh2D buildMeshFromPolygon01(const CockpitPolygon& poly)
{
    Mesh2D mesh;

    // --- Вычисляем границы полигона ---
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();

    for (const auto& p : poly.contour01)
    {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

        float width  = maxX - minX;
        float height = maxY - minY;

        // защита от деления на 0
        if (width  < 0.00001f) width  = 1.0f;
        if (height < 0.00001f) height = 1.0f;


    // ---------- 1. bounding box в SVG [0..1] ----------
    glm::vec2 min01( 1e9f);
    glm::vec2 max01(-1e9f);

    for (const auto& p : poly.contour01)
    {
        min01 = glm::min(min01, p);
        max01 = glm::max(max01, p);
    }

    glm::vec2 size01 = max01 - min01;
    if (size01.x < 1e-6f) size01.x = 1.0f;
    if (size01.y < 1e-6f) size01.y = 1.0f;

    // ---------- 2. earcut ----------
    std::vector<std::vector<Point>> polygon;
    polygon.emplace_back();

    // ---------- 3. вершины ----------
    for (const auto& p01 : poly.contour01)
    {
        // NDC
        float x_ndc = p01.x * 2.0f - 1.0f;
        float y_ndc = 1.0f - p01.y * 2.0f;

        // ЛОКАЛЬНЫЕ координаты ОБЪЕКТА
        glm::vec2 localNormalized;

        localNormalized.x = (p01.x - minX) / width;
        localNormalized.y = (p01.y - minY) / height;

        mesh.vertices.push_back({
            {x_ndc, y_ndc},
            localNormalized,
            poly.colorA
        });

        polygon[0].push_back(Point{ (double)p01.x, (double)p01.y });
        // polygon[0].push_back({ x_ndc, y_ndc });
    }

    mesh.indices = mapbox::earcut<uint32_t>(polygon);
    return mesh;
}





// Mesh2D buildMeshFromPolygon01(const CockpitPolygon& poly)
// {
//     Mesh2D mesh;

//     // ---------- 1. bounding box в SVG [0..1] ----------
//     glm::vec2 min01( 1e9f);
//     glm::vec2 max01(-1e9f);

//     for (const auto& p : poly.contour01)
//     {
//         min01 = glm::min(min01, p);
//         max01 = glm::max(max01, p);
//     }

//     glm::vec2 size01 = max01 - min01;
//     if (size01.x < 1e-6f) size01.x = 1.0f;
//     if (size01.y < 1e-6f) size01.y = 1.0f;

//     // ---------- 2. earcut ----------
//     std::vector<std::vector<Point>> polygon;
//     polygon.emplace_back();

//     // ---------- 3. вершины ----------
//     for (const auto& p01 : poly.contour01)
//     {
//         // NDC
//         float x = p01.x * 2.0f - 1.0f;
//         float y = 1.0f - p01.y * 2.0f;

//         // ЛОКАЛЬНЫЕ координаты ОБЪЕКТА
//         glm::vec2 local01 = (p01 - min01) / size01;

//         mesh.vertices.push_back({
//             { x, y },        // position (NDC)
//             local01,         // local01 [0..1] ВНУТРИ ПОЛИГОНА
//             poly.colorA
//         });

//         polygon[0].push_back({ x, y });
//     }

//     mesh.indices = mapbox::earcut<uint32_t>(polygon);
//     return mesh;
// }



