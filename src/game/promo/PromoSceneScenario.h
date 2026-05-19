#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "src/game/visual/VisualShip.h"
#include "src/game/promo/PromoSceneTuning.h"

class ClientWorldState;

namespace game::promo
{

class PromoSceneScenario
{
public:
    static constexpr bool Enabled = true;

    void setup(ClientWorldState& world);
    void update(ClientWorldState& world, float dt);
    void reset(ClientWorldState& world);

    bool active() const { return m_active; }
    
    glm::vec3 cameraTarget(const ClientWorldState& world) const;
    bool cameraTargetValid(const ClientWorldState& world) const;

private:
    struct WingShipRef
    {
        game::visual::VisualShipId id = 0;
        int groupIndex = 0;
        int slotIndex = 0;
    };

private:
    bool m_active = false;

    float m_time = 0.0f;

    std::vector<WingShipRef> m_wing;





    glm::vec3 m_playerPos {0.0f, 50.0f, 0.0f};

    // Звено летит из +Z в -Z над игроком.
    glm::vec3 m_flightDir {0.0f, 0.0f, -1.0f};

    float m_wingSpeed = 120.0f;

    float m_passHeight = tuning::PassHeight;
    float m_spawnDistance = tuning::SpawnDistance;
    float m_splitAfterPlayerDistance = tuning::SplitAfterPlayerDistance;
    float m_sideSplitDistance = tuning::SideSplitDistance;
    float m_centerClimb = tuning::CenterClimb;





private:
    void spawnWing(ClientWorldState& world);

    glm::vec3 formationOffset(int groupIndex, int slotIndex) const;

    glm::vec3 computeShipPosition(
        int groupIndex,
        int slotIndex,
        float t
    ) const;

    glm::mat4 makeOrientation(
        const glm::vec3& forward,
        const glm::vec3& upHint
    ) const;

    static float smootherStep(float x);
    glm::vec3 m_lastCameraTarget {0.0f, 100.0f, 0.0f};
};

} // namespace game::promo