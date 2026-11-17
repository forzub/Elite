#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/render/HUD/HudPrimitiveBatch.h"
#include "src/render/types/Viewport.h"

class ClientWorldState;
struct ClientShipState;

namespace render::cockpit
{

/*
    The hub's corridors in the flight view (author 2026-11-17: «надо на худ отображать зеленые, синие и красные
    корридоры. таким - еле заметным оттенком»; «я имел в виду видимость корридоров в самой игре на HUD»): the GREEN
    lanes as faint rings round the lane every ~200 m joined by four thin lines, the RED zones as faint box edges -
    only near the ship (fading out to FadeMeters), drawn on the glass like the guidance corridor. Source: the same
    HubLaneNetwork the docking planner uses (assets/data/navigation/hub_lanes/<hub>.json, frame = the hub's <hub>.city
    module, its render pose).
*/
class HubCorridorHudRenderer
{
public:
    static constexpr double FadeMeters = 4500.0;
    static constexpr double RingStepMeters = 250.0;

    void init();
    void render(
        const ClientWorldState& world,
        const ClientShipState& player,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const Viewport& viewport
    );

private:
    bool project(const glm::dvec3& relative, const glm::mat4& view, const glm::mat4& proj, const Viewport& vp,
                 glm::vec2& outPx) const;

    render::hud::HudPrimitiveBatch m_batch;
    std::unordered_map<std::string, std::unique_ptr<game::navigation::traffic::HubLaneNetwork>> m_networks;
};

} // namespace render::cockpit
