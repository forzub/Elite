#pragma once

#include <string>
#include <glm/vec3.hpp>

enum class SignalType
{
    Planets,              // навигационный, стабильный
    StationClass,         // станции
    SOSModern,             // точный, но ограниченный
    SOSAntic,              // древний, направление только
    Beacon,                // стабильный маяк
    CivilianTransponder,
    MilitaryTransponder,
    PirateTransponder,
    Unknown                // аномалия, направление
};

struct WorldSignal
{
    SignalType type;
    glm::vec3  position;

    float power;      // базовая мощность сигнала
    float maxRange;   // максимальная дальность
   
    bool  enabled;
    std::string label;
};
