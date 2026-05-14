#pragma once

#include "game/damage/IDamageHandler.h"
#include "src/game/damage/HitComponent.h"
#include "src/game/damage/DamageEvent.h"

namespace game::damage
{
    struct HitComponent;
    struct DamageEvent;
}


namespace game::ship::core { class ShipCore; }

namespace game::ship
{

class Ship;

class ShipDamageHandler : public game::damage::IDamageHandler
{
public:

    game::ship::core::ShipCore* ship = nullptr;
    

    ShipDamageHandler(game::damage::HitComponent* hit)
        : m_hit(hit)
    {}

    void handleDamage(
        const game::damage::DamageResult& result
    ) override;

    void handleImpact(
        damage::HitComponent& hit,
        const glm::vec3& impactPoint,
        const damage::DamageEvent& event
    );

    

    void applyDamage(const damage::DamageEvent& event);
    void handleLaser(const damage::DamageEvent& event);

private:

    game::damage::HitComponent* m_hit;

};

}