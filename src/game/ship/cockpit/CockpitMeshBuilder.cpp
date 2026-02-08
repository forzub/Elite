#include "CockpitMeshBuilder.h"

#include <iostream>

#include "render/core/earcut.hpp"

using Coord = double;
using Point = std::array<Coord, 2>; 

Mesh2D buildMeshFromPolygon01(const CockpitPolygon& poly)
{
    Mesh2D mesh;

    // earcut ожидает массив контуров
    std::vector<std::vector<Point>> polygon;
    polygon.emplace_back(); // внешний контур

    for (const auto& p01 : poly.contour01)
    {
        // SVG [0..1] → NDC [-1..1]
        double x = p01.x * 2.0 - 1.0;
        double y = 1.0 - p01.y * 2.0;

        // Вершины для OpenGL
        mesh.vertices.push_back({
            { (float)x, (float)y },
            poly.color
        });

        // Те же координаты — для earcut
        polygon[0].push_back({ x, y });
    }

    // ТРИАНГУЛЯЦИЯ
    mesh.indices = mapbox::earcut<uint32_t>(polygon);

    return mesh;
}

