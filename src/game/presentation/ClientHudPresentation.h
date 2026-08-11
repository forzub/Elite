#pragma once

#include <string>

#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>

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

struct FlightInstrumentTextProfile
{
    // Renderer receives already formatted text. Future cockpit language packs
    // can change labels, unit scale and glyph-compatible font without touching
    // flight instrument geometry.
    double displayUnitsPerMps = 1.0;
    int speedDecimals = 1;
    std::string speedUnitLabel = "m/s";
    std::string newtonianModeLabel = "NEWTONIAN";
    std::string assistedModeLabel = "ASSISTED";
    std::string alignForwardLabel = "ALIGN +V";
    std::string alignBackwardLabel = "ALIGN -V";
    std::string brakingLabel = "BRAKE";
    std::string fontPath = "assets/fonts/Roboto-Light.ttf";
};

struct FlightVectorIndicatorPresentation
{
    bool visible = false;

    double speedMps = 0.0;
    double speedLimitMps = 0.0;
    float speedFraction01 = 0.0f;

    game::navigation::LocalFlightControlLaw controlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    game::navigation::VelocityAlignmentMode alignmentMode =
        game::navigation::VelocityAlignmentMode::None;

    // Model-space axes: X=ship right, Y=ship forward, Z=ship up. Columns
    // transform them into indicator space where +Y is actual local velocity.
    glm::mat3 shipModelToIndicatorBasis {1.0f};

    std::string speedText;
    std::string modeText;
    std::string actionText;
    std::string fontPath;
};

FlightVectorIndicatorPresentation buildFlightVectorIndicatorPresentation(
    const ClientShipState& ship,
    const FlightInstrumentTextProfile& textProfile = {}
);

// Returns false when a required HUD UIText binding disappeared or changed type.
bool applyPlayerHudTelemetry(
    UIComponent& root,
    const PlayerHudTelemetry& telemetry
);
}
