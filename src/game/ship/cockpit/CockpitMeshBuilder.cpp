#include "CockpitMeshBuilder.h"

#include <glm/common.hpp>
#include <iostream>

#include "render/core/earcut.hpp"

using Coord = double;
using Point = std::array<Coord, 2>; 

// Mesh2D buildMeshFromPolygon01(const CockpitPolygon& poly)
// {
//     Mesh2D mesh;

//     // earcut ожидает массив контуров
//     std::vector<std::vector<Point>> polygon;
//     polygon.emplace_back(); // внешний контур

//     for (const auto& p01 : poly.contour01)
//     {
//         // SVG [0..1] → NDC [-1..1]
//         double x = p01.x * 2.0 - 1.0;
//         double y = 1.0 - p01.y * 2.0;

//         // Вершины для OpenGL
//         // mesh.vertices.push_back({
//         //     { (float)x, (float)y },
//         //     poly.colorA
//         // });

//         mesh.vertices.push_back({
//             { (float)x, (float)y },   // position (NDC)
//             p01,                      // local01 ← ВАЖНО
//             poly.colorA               // color
//         });

//         // Те же координаты — для earcut
//         polygon[0].push_back({ x, y });
//     }

//     // ТРИАНГУЛЯЦИЯ
//     mesh.indices = mapbox::earcut<uint32_t>(polygon);

//     return mesh;
// }

Mesh2D buildMeshFromPolygon01(const CockpitPolygon& poly)
{
    Mesh2D mesh;

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
        float x = p01.x * 2.0f - 1.0f;
        float y = 1.0f - p01.y * 2.0f;

        // ЛОКАЛЬНЫЕ координаты ОБЪЕКТА
        glm::vec2 local01 = (p01 - min01) / size01;

        mesh.vertices.push_back({
            { x, y },        // position (NDC)
            local01,         // local01 [0..1] ВНУТРИ ПОЛИГОНА
            poly.colorA
        });

        polygon[0].push_back({ x, y });
    }

    mesh.indices = mapbox::earcut<uint32_t>(polygon);
    return mesh;
}
