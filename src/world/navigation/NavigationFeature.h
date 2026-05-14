#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace world::navigation
{

enum class NavigationFeatureKind : uint8_t
{
    Unknown = 0,

    // Разрешённый проход через объект:
    // отверстие станции, кольцо, арка астероида.
    Portal,

    // Протяжённый безопасный коридор:
    // тоннель, ангарный проход, щель между конструкциями.
    Corridor,

    // Точка выхода из дока / шлюза.
    DockExit,

    // Точка входа в док / шлюз.
    DockEntry,

    // Опасная зона, которую лучше обходить:
    // пожар, радиация, активные двигатели, зона обломков.
    HazardZone
};

struct NavigationFeature
{
    uint32_t ownerEntityId = 0;

    std::string id;
    NavigationFeatureKind kind = NavigationFeatureKind::Unknown;

    glm::vec3 center {0.0f};

    // Основное направление.
    // Для Portal — нормаль плоскости прохода.
    // Для DockExit/DockEntry — направление движения.
    // Для Corridor — направление оси коридора.
    glm::vec3 direction {0.0f, 0.0f, -1.0f};

    // Для Portal/HazardZone — радиус.
    // Для Corridor — половина ширины.
    float radius = 1.0f;

    // Для Corridor — длина.
    // Для Portal/DockExit/DockEntry может быть 0.
    float length = 0.0f;

    // Насколько рискованно использовать.
    // 0 = безопасно, 1 = очень рискованно.
    float risk = 0.0f;

    bool enabled = true;
    bool oneWay = false;
};

} // namespace world::navigation