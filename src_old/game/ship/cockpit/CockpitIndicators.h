#pragma once
#include <string>
#include <glm/vec2.hpp>

enum class IndicatorVisualType {
    Bar,
    SegmentArray,
    Needle,
    SevenSegment
};

struct CockpitIndicatorLayout {
    std::string id;                 // "speed", "fuel", "shield", "decryptor"
    IndicatorVisualType visualType;

    glm::vec2 pos01;                // позиция в кабине [0..1]
    glm::vec2 size01;               // размер [0..1]

    int segments = 0;               // для SegmentArray
};
