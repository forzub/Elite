#pragma once

struct EquipmentModule
{
    float health = 1.0f; // 0..1

    bool isOperational() const
    {
        return health > 0.2f;
    }
};
