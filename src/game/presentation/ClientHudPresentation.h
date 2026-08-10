#pragma once

#include <string>

#include <glm/glm.hpp>

#include "src/game/client/ClientWorldState.h"

class UIComponent;

namespace game::presentation
{
struct PlayerHudTelemetry
{
    glm::dvec3 globalMeters {0.0};
    double speedMps = 0.0;

    std::string cellLabel;
    std::string xLabel;
    std::string yLabel;
    std::string zLabel;
    std::string speedLabel;
};

PlayerHudTelemetry buildPlayerHudTelemetry(
    const ClientShipState& ship
);

// Returns false when a required HUD UIText binding disappeared or changed type.
bool applyPlayerHudTelemetry(
    UIComponent& root,
    const PlayerHudTelemetry& telemetry
);
}
