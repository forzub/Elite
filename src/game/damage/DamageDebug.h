#pragma once

#include <vector>
#include "src/game/damage/DamageEvent.h"

namespace game::debug
{

// сервер записывает сюда перед отправкой
extern std::vector<game::damage::DamageEvent> g_lastSentDamage;

// клиент сравнивает с этим
void validateDamage(
    const std::vector<game::damage::DamageEvent>& received
);

}