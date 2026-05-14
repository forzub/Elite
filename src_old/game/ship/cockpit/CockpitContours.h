#pragma once

#include <vector>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>


enum class CockpitFillType
{
    Solid,
    LinearGradient,
    RadialGradient
};

enum class CockpitDrawType
{
    Fill,
    Stroke
};



// --------------------------------------------
// Закрашенный многоугольник кабины
// --------------------------------------------
struct CockpitPolygon
{
    std::vector<glm::vec2> contour01; // [0..1]
    CockpitFillType fillType = CockpitFillType::Solid;

    // Если fillType == Solid → используется colorA

    glm::vec4 colorA;   // основной цвет
    glm::vec4 colorB;   // второй цвет (если градиент)
    
    // Для линейного градиента (в SVG-пространстве)
    glm::vec2 gradFrom01 = {0.0f, 0.0f};
    glm::vec2 gradTo01   = {0.0f, 1.0f};
};


// --------------------------------------------
// Линия / обводка кабины
// --------------------------------------------
struct CockpitStroke
{

    std::vector<glm::vec2> path01;
    float thickness = 0.002f;   
    glm::vec4 color = {1,1,1,1};
};



struct CockpitDrawCommand
{
    CockpitDrawType type;
    std::string id;

    // Только одно из двух используется
    CockpitPolygon polygon;
    CockpitStroke  stroke;

    glm::vec2 pivot01;
    bool hasPivot = false;
};


// --------------------------------------------
// Полная геометрия кабины
// --------------------------------------------
struct CockpitGeometry
{
    std::vector<CockpitDrawCommand> drawList;
};

