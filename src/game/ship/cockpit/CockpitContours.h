#pragma once

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

// --------------------------------------------
// Закрашенный многоугольник кабины
// --------------------------------------------
struct CockpitPolygon
{
    std::vector<glm::vec2> contour01; // [0..1]
    glm::vec4              color;
};

// --------------------------------------------
// Линия / обводка кабины
// --------------------------------------------
struct CockpitStroke
{
    std::vector<glm::vec2> path01;    // [0..1], НЕ замкнута
    glm::vec4              color;
    float                  thickness;
    bool                   extend;
};

// --------------------------------------------
// Полная геометрия кабины
// --------------------------------------------
struct CockpitGeometry
{
    std::vector<CockpitPolygon> polygons;
    std::vector<CockpitStroke>  strokes;
};

enum class CockpitRenderType
{
    Polygon,
    Stroke
};

struct CockpitRenderItem
{
    CockpitRenderType type;

    // один из них используется
    CockpitPolygon polygon;
    CockpitStroke  stroke;
};
