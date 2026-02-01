#pragma once

struct EquipmentModule
{
    float health = 1.0f; // 0..1
    bool  enabled = true;    // включено пилотом / ИИ

    bool isOperational() const
    {
        return health > 0.2f;
    }
};



// equipment.receiver.health -= damage * 0.3f;
// equipment.jammer.health   -= damage * 0.6f;
