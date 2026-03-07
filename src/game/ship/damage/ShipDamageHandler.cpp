#include "ShipDamageHandler.h"
#include "game/damage/HitVolume.h"
#include "game/ship/Ship.h"

#include <iostream>

using namespace game::damage;

namespace game::ship
{

void ShipDamageHandler::handleDamage(const DamageResult& result)
{
    if (!result.volume || !result.event)
        return;

    // ВНИМАНИЕ:
    // result.volume приходит как const, поэтому нужен const_cast
    HitVolume* volume = const_cast<HitVolume*>(result.volume);

    if (!volume)
        return;

    double energy = result.event->energy;

    // --- Лог попадания ---
    switch (volume->zone)
    {
        case HitZoneType::Reactor:
            std::cout << "[ShipDamage] reactor hit energy=" << energy << "\n";
            break;

        case HitZoneType::Engine:
            std::cout << "[ShipDamage] engine hit energy=" << energy << "\n";
            break;

        case HitZoneType::Radiator:
            std::cout << "[ShipDamage] radiator hit energy=" << energy << "\n";
            break;

        case HitZoneType::Hull:
            std::cout << "[ShipDamage] hull hit energy=" << energy << "\n";
            break;

        default:
            std::cout << "[ShipDamage] generic hit energy=" << energy << "\n";
            break;
    }

    if (volume->health <= 0.0f)
    {
        volume->destroyed = true;

        std::cout << "[ShipDamage] volume destroyed: "
                << volume->m_label << "\n";
    }

    // --- если volume нельзя разрушить ---
    if (!volume->destructible)
        return;

    // --- если уже уничтожен ---
    if (volume->destroyed)
        return;

    // --- наносим урон ---
    volume->health -= static_cast<float>(energy * 0.1f);

    std::cout << "[ShipDamage] volume " << volume->m_label
              << " health=" << volume->health << "\n";

    // --- уничтожение ---
    if (volume->health <= 0.0f)
    {
        volume->destroyed = true;

        std::cout << "[ShipDamage] volume destroyed: "
                  << volume->m_label << "\n";
    }


    
}

}