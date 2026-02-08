#pragma once

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

// Один закрашенный многоугольник кабины
struct CockpitPolygon
{
    std::vector<glm::vec2>      contour01; // координаты [0..1] в SVG-пространстве
    glm::vec4                   color;                  // базовый цвет заливки
};

// Полная геометрия кабины корабля
struct CockpitGeometry
{
    std::vector<CockpitPolygon> polygons;
    std::vector<CockpitStroke>  strokes;
};

// Одна линия кабины (обводка, ребро, линия интерфейса)
struct CockpitStroke
{
    std::vector<glm::vec2> path01;     // [0..1], НЕ замкнута
    glm::vec4              color;      // цвет линии
    float                  thickness;  // толщина в NDC (например 0.003f)
    bool                   extend;     // продолжать за экран
};
