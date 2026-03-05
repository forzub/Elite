#pragma once


enum class DamageType
{
    Impact,
    EMP,
    PowerFailure,
    Overheat,
    Radiation,
    Jamming
};

struct ShipDamageEvent
{
    DamageType type;
    DamageSeverity severity;
    float duration; // 0 = постоянное
};