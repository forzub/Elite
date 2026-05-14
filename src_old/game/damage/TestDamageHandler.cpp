#include "TestDamageHandler.h"
#include "game/damage/HitVolume.h"

using namespace game::damage;

namespace game::damage
{

static const char* zoneToString(HitZoneType zone)
{
    switch (zone)
    {
        case HitZoneType::Hull: return "Hull";
        case HitZoneType::Reactor: return "Reactor";
        case HitZoneType::Engine: return "Engine";
        case HitZoneType::Radiator: return "Radiator";
        case HitZoneType::Cargo: return "Cargo";
        default: return "Generic";
    }
}

void TestDamageHandler::handleDamage(const DamageResult& result)
{
    if (!result.volume)
    {
        std::cout << "[DamageHandler] damage missed\n";
        return;
    }

    auto zone = result.volume->zone;

    std::cout << "[DamageHandler] damage received\n";
    std::cout << "zone: " << zoneToString(zone) << "\n";
    std::cout << "energy: " << result.event->energy << "\n";

    switch (zone)
    {
        case HitZoneType::Reactor:
            std::cout << "-> reactor damaged\n";
            break;

        case HitZoneType::Engine:
            std::cout << "-> engine damaged\n";
            break;

        case HitZoneType::Hull:
            std::cout << "-> hull damaged\n";
            break;

        default:
            std::cout << "-> generic damage\n";
            break;
    }

    std::cout << std::endl;
}

}