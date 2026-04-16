#pragma once

#include <string_view>

enum class HtmlUiPanelId
{
    None = 0,

    MainMenu,
    ConfirmExit,
    ShipCore,
    FrustumDebug,
    Trade,
    Shipyard,
    GalaxyMap,
    WorldInfo,
    PauseMenu,
    DebugControl,
    AttachmentEditor
};

inline std::string_view toString(HtmlUiPanelId id)
{
    switch (id)
    {
        case HtmlUiPanelId::MainMenu:     return "main_menu";
        case HtmlUiPanelId::ConfirmExit:  return "confirm_exit";
        case HtmlUiPanelId::ShipCore:     return "ship_core";
        case HtmlUiPanelId::FrustumDebug: return "frustum_debug";
        case HtmlUiPanelId::Trade:        return "trade";
        case HtmlUiPanelId::Shipyard:     return "shipyard";
        case HtmlUiPanelId::GalaxyMap:    return "galaxy_map";
        case HtmlUiPanelId::WorldInfo:    return "world_info";
        case HtmlUiPanelId::PauseMenu:    return "pause_menu";
        case HtmlUiPanelId::AttachmentEditor: return "attachment_editor";
        case HtmlUiPanelId::DebugControl: return "debug_control";
        default:                          return "none";
    }
}