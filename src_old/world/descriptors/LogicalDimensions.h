#pragma once

enum class ScaleReference
{
    None,
    Length,
    Width,
    Height
};

struct LogicalDimensions
{
    // Смысловые габариты объекта в логическом игровом базисе:
    // +X = right, +Y = up, -Z = forward
    //
    // length = nose -> tail
    // width  = left -> right
    // height = bottom -> top
    float length = 1.0f;
    float width  = 1.0f;
    float height = 1.0f;

    // По какой величине считать uniform scale.
    // Для кораблей обычно Length.
    ScaleReference scaleReference = ScaleReference::Length;

    bool enabled = false;
};