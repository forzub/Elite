#pragma once
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct HudStaticItem
{
    std::string text;
    glm::vec2   posNorm;   // 0..1
    glm::vec3   color;
};

enum class HudMessageType
{
    Info,
    Warning,
    Critical
};

struct HudMessage
{
    std::string text;
    float ttl = 0.0f;
    float age = 0.0f;
    HudMessageType type = HudMessageType::Info;
};

struct HudLineRect
{
    glm::vec2 posNorm;   // левый верх (0..1)
    glm::vec2 sizeNorm;  // ширина / высота (0..1)
    glm::vec3 color;
};