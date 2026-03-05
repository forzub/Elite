#include "DamageDebug.h"
#include <iostream>

namespace game::debug
{

std::vector<game::damage::DamageEvent> g_lastSentDamage;

static bool equals(
    const game::damage::DamageEvent& a,
    const game::damage::DamageEvent& b)
{
    return
        a.type == b.type &&
        a.severity == b.severity &&
        a.duration == b.duration;
}

void validateDamage(
    const std::vector<game::damage::DamageEvent>& received)
{
    if (received.size() != g_lastSentDamage.size())
    {
        std::cout << "[DAMAGE ERROR] size mismatch\n";
        return;
    }

    for (size_t i = 0; i < received.size(); ++i)
    {
        if (!equals(received[i], g_lastSentDamage[i]))
        {
            std::cout << "[DAMAGE ERROR] event mismatch at index "
                      << i << "\n";
        }
    }

    std::cout << "[DAMAGE OK] validation complete\n";
}

}