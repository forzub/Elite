#pragma once

enum class SignalDisplayClass
{
    Global, // планеты, крупные станции — всегда Text
    Local,  // корабли, маяки — Wave → Text
    Other   // древние / чужие / аномалии — всегда Wave
};