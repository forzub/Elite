#pragma once

#include <optional>
#include <string>
#include <vector>

#include "render/types/Viewport.h"
#include "src/game/presentation/RepairRefitPresentation.h"
#include "src/game/presentation/ShipyardPresentation.h"

// Native F6 Shipyard and F7 Repair & Refit surfaces (same single OpenGL
// presentation surface as every F1-F12 target). Immediate-mode: each render
// records its clickable regions; the next input pass resolves a click
// against the regions of the frame the player actually saw.
namespace ui::presentation
{
enum class ServicePanelActionType
{
    ShipyardSelectModel,
    ShipyardTogglePassport,
    RefitPreviousShip,
    RefitNextShip,
    RefitSelectGroup,
    RefitSelectModule,
    RefitPreviewRepair,
    RefitPreviewReplace,     // text = product id ("" = same product)
    RefitApply
};

struct ServicePanelAction
{
    ServicePanelActionType type = ServicePanelActionType::ShipyardSelectModel;
    std::string text;
};

class ServicePanelsRenderer
{
public:
    void renderShipyard(const Viewport& viewport, const game::presentation::ShipyardPresentation& page);
    void renderRepairRefit(const Viewport& viewport, const game::presentation::RepairRefitPresentation& page);

    // Mouse in window pixels; returns the action of a fresh left press.
    std::optional<ServicePanelAction> handleInput(const Viewport& viewport, double mouseX, double mouseY, bool leftDown,
                                                  double scrollY);

private:
    struct Hit
    {
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        int scrollZone = 0;              // 1 = shipyard list, 2 = refit module list, 3 = right card
        ServicePanelAction action;
    };
    std::vector<Hit> m_hits;             // regions of the last rendered frame
    struct Zone
    {
        int id = 0;
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    };
    std::vector<Zone> m_zones;
    bool m_leftWasDown = false;
    float m_shipyardListScroll = 0.0f;
    float m_refitListScroll = 0.0f;
    float m_cardScroll = 0.0f;
};
} // namespace ui::presentation
