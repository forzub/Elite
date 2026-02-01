#pragma once

enum class SignalType
{
    Planets,              // навигационный, стабильный
    StationClass,         // станции
    SOSModern,             // точный, но ограниченный
    SOSAntic,              // древний, направление только
    Beacon,                // стабильный маяк
    Transponder,
    Unknown                // аномалия, направление
};