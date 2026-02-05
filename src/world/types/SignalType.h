#pragma once

enum class SignalType
{
    Planets,                // 0 - навигационный, стабильный
    StationClass,           // 1 - станции
    SOSModern,              // 2 - точный, но ограниченный
    SOSAntic,               // 3 - древний, направление только
    Beacon,                 // 4 - стабильный маяк
    Transponder,            // 5 - 
    Unknown,                // 6 - аномалия, направление
    Message,
    Silence,
    None
};