#pragma once

struct WorldParams
{
    float linearDrag = 0.0f;      // сопротивление среды
    float maxSafeDecel = 50.0f;   // безопасное торможение (для крашей)
};
