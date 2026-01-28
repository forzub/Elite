#pragma once
#include <glm/glm.hpp>

enum class InterferenceType
{
    Passive,   // бури, астероиды, солнечный ветер
    Active     // РЭБ, глушилки
};

struct InterferenceSource
{
    InterferenceType type;

    glm::vec3 position;

    float power;      // мощность помех
    float radius;     // эффективный радиус действия
    bool  enabled;
};
