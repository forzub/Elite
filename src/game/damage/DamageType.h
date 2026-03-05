#pragma once

namespace game::damage
{

enum class DamageType
{
    Impact,        // физический удар
    EMP,           // электромагнитный импульс
    PowerFailure,  // нехватка энергии
    Overheat,      // перегрев
    Radiation,     // повышенная радиация
    Jamming        // внешние помехи
};

}