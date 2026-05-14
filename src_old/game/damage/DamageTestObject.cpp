#include "DamageTestObject.h"
#include <iostream>

using namespace game::damage;

namespace game::debug
{

    static const char* zoneToString(game::damage::HitZoneType zone)
    {
        using namespace game::damage;

        switch (zone)
        {
            case HitZoneType::Hull: return "Hull";
            case HitZoneType::Cockpit: return "Cockpit";
            case HitZoneType::Reactor: return "Reactor";
            case HitZoneType::Engine: return "Engine";
            case HitZoneType::Radiator: return "Radiator";
            case HitZoneType::Cargo: return "Cargo";
            default: return "Generic";
        }
    }



DamageTestObject::DamageTestObject()
{
    using namespace game::damage;

    HitVolume hull;
    hull.zone = HitZoneType::Hull;
    hull.center = {0.0f, 0.0f, 0.0f};
    hull.halfSize = {5.0f, 5.0f, 5.0f};

    
    HitVolume reactor;
    reactor.zone = HitZoneType::Reactor;
    reactor.center = {0.0f, 0.0f, -2.0f};
    reactor.halfSize = {1.5f, 1.5f, 1.5f};

    
    HitVolume engine;
    engine.zone = HitZoneType::Engine;
    engine.center = {0.0f, 0.0f, -4.0f};
    engine.halfSize = {1.0f, 1.0f, 1.0f};

    hitComponent.volumes.push_back(reactor);
    hitComponent.volumes.push_back(engine);
    hitComponent.volumes.push_back(hull);
}







void DamageTestObject::applyDamage(const DamageEvent& event)
{
    DamageEvent localEvent = event;

    localEvent.position = transform.worldToLocal(event.position);

    auto result = hitComponent.resolve(localEvent);

    if (handler)
        handler->handleDamage(result);
    }


}