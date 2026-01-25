#pragma once

#include <string>
#include <glm/glm.hpp>

struct WorldLabel
{
    glm::vec3 worldPos;
    std::string label;
    float distance;

    bool hasDistance = false;
    float stability = 0.0f;     // понадобится для анимации кругов
   
    // --- визуал ---
    float visibility = 1.0f;


};