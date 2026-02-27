#pragma once

namespace game
{

class Integrity
{
public:

    // 1. Физическая целостность (0..1)
    double structural = 1.0;

    // 2. Работоспособность (0..1)
    double functional = 1.0;

    // 3. Перегрев (0..1)
    double thermal = 0.0;

    // ───────────────
    // Damage
    // ───────────────

    void applyStructuralDamage(double value)
    {
        structural -= value;
        if (structural < 0.0)
            structural = 0.0;

        // сильное структурное повреждение снижает функциональность
        functional = structural;
    }

    void degradeFunction(double value)
    {
        functional -= value;
        if (functional < 0.0)
            functional = 0.0;
    }

    void addHeat(double value)
    {
        thermal += value;
        if (thermal > 1.0)
            thermal = 1.0;

        // перегрев снижает функциональность
        if (thermal > 0.7)
            functional -= (thermal - 0.7) * 0.5;

        if (functional < 0.0)
            functional = 0.0;
    }

    void cool(double value)
    {
        thermal -= value;
        if (thermal < 0.0)
            thermal = 0.0;
    }

    // ───────────────
    // State
    // ───────────────

    bool isDestroyed() const
    {
        return structural <= 0.0;
    }

    bool isOperational() const
    {
        return structural > 0.2 && functional > 0.2;
    }

    double health() const
    {
        return structural;
    }
};

}