#pragma once

#include <optional>
#include <string>
#include <vector>

#include "render/types/Viewport.h"
#include "src/game/presentation/RepairRefitPresentation.h"
#include "src/game/presentation/ShipyardPresentation.h"
#include "src/game/presentation/TradePresentation.h"

// Native F5 Government services, F6 Shipyard, F7 Repair & Refit and F8 Trade surfaces (same single OpenGL
// presentation surface as every F1-F12 target). Immediate-mode: each render
// records its clickable regions; the next input pass resolves a click
// against the regions of the frame the player actually saw.
namespace ui::presentation
{
enum class ServicePanelActionType
{
    ShipyardSelectModel,
    ShipyardTogglePassport,
    ShipyardBuy,             // F6 (step 14): text = offer id of the selected model (new ships only)
    RefitBoard,              // F7 (base 2): text = ship entity id - the pilot moves to this owned ship
    RefitPreviousShip,
    RefitNextShip,
    RefitSelectGroup,
    RefitSelectModule,
    RefitPreviewRepair,
    RefitPreviewReplace,     // text = product id ("" = same product)
    RefitApply,
    TradeTab,                // text = "0" contracts / "1" market / "2" hubs / "4" finance / "5" cargo (TradeTabOrder)
    GovernmentTab,           // F5: text = "0" the state services app / "1" Галера+
    GovernmentSection,       // F5 state app: text = "0" taxes / "1" licences / "2" navigation data
    GovernmentBuyLicence,    // F5 state app -> licences: text = licence type (step 11)
    GovernmentHire,          // F5 employment service: text = vacancy id
    GovernmentQuit,          // F5 employment service: quit the job
    GovernmentTakeSos,       // F5 МЧС (step 12): text = SOS call id
    TradeSelectHub,          // text = hub id
    TradeShowOnMap,          // text = hub id
    // F5 state app -> navigation data (moved from F8, TRADE_DESIGN §11a)
    NavDataSelect,           // text = bank record id
    NavDataLoad,             // text = bank record id -> the onboard computer
    NavDataCarrier,          // text = bank record id -> a new data carrier
    NavDataInsert,           // text = carrier number (1-based) -> the onboard computer
    // F8 CONTRACTS (TRADE_DESIGN §8.2, trade step 5)
    TradeContractSelect,     // text = posting / contract id
    TradeContractTake,       // text = posting id: asks "take?" (the confirm state)
    TradeContractAbandon,    // text = contract id: asks "abandon?"
    TradeContractConfirm,    // sends the asked take / abandon
    TradeContractCancel,     // drops the question
    // F8 MARKET deal window (TRADE_DESIGN §8.1, trade step 3)
    TradeSelectGood,         // text = EconomyModel::goods() index
    TradeDealSide,           // text = "buy" | "sell"
    TradeDealUnits,          // text = "t" | "c20"
    TradeDealStep,           // text = signed steps ("-1", "+10"; tonnes or boxes)
    TradeDealSlider,         // text = the clicked fraction of the track 0..1 (filled by handleInput)
    TradeDealMax,
    TradeDealConfirm,
    TradeDealCancel,
    TradeDealRetry,
    CargoBlackSell,          // F8 ГРУЗ: text = good id - all of it aboard to the hub's black market (TRADE_DESIGN §2.8, step 9)
    CargoInsure,             // F8 ГРУЗ: insure the own lots aboard (step 13)
    BotHire,                 // F8 ФЛОТ (step 15): text = candidate id
    BotLoop,                 // F8 ФЛОТ: text = bot id - the trade loop from its ship's hub
    BotFire                  // F8 ФЛОТ: text = bot id
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
    void renderTrade(const Viewport& viewport, const game::presentation::TradePresentation& page);
    void renderGovernment(const Viewport& viewport, const game::presentation::GovernmentPresentation& page);

    // Mouse in window pixels; returns the action of a fresh left press.
    void renderNavData(const game::presentation::NavDataPage& nav, float listX, float listY, float listW, float listH, float w,
                       float h, int px, int small, int big);
    std::optional<ServicePanelAction> handleInput(const Viewport& viewport, double mouseX, double mouseY, bool leftDown,
                                                  double scrollY);
    // DEV: a click at the centre (slider: at `fraction` of the track) of the last frame's region with that action -
    // the same action a real click there returns; nullopt = the frame had no such region
    std::optional<ServicePanelAction> devClick(ServicePanelActionType type, const std::string& text, double fraction = 0.5) const;

private:
    void renderTradeDeal(const game::presentation::TradePresentation& page, float cx, float y, float cw, float h, int px, int small,
                         int big);
    void renderTradeContracts(const game::presentation::TradePresentation& page, float listX, float listY, float listW, float listH,
                              float w, float h, int px, int small, int big);
    struct Hit
    {
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        int scrollZone = 0;              // 1 = shipyard list, 2 = refit module list, 3 = right card, 4 = trade list, 5 = deal panel
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
    float m_tradeListScroll = 0.0f;
    std::string m_tradeContractShown;    // the CONTRACTS selection the list was last scrolled to
};
} // namespace ui::presentation
