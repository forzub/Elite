#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

struct HudBoundary
{
    // Контур HUD в нормализованных координатах [0..1]
    // ВАЖНО: порядок по часовой стрелке
    std::vector<glm::vec2> contour;
};

struct ShipHudProfile
{
    std::string name;          // "Elite Mk1 Cockpit"

    HudBoundary edgeBoundary;     // куда можно ставить edge-метки

    float edgeInset = 0.0f;       // внутренняя безопасная зона (норма)
};
