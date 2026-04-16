#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "ui/html/HtmlUiPanelId.h"

using json = nlohmann::json;

enum class HtmlUiMessageType
{
    StateUpdate,
    Command,
    Subscribe,
    Event,
    Ack,
    Error
};

inline const char* toString(HtmlUiMessageType type)
{
    switch (type)
    {
        case HtmlUiMessageType::StateUpdate: return "state_update";
        case HtmlUiMessageType::Command:     return "command";
        case HtmlUiMessageType::Subscribe:   return "subscribe";
        case HtmlUiMessageType::Event:       return "event";
        case HtmlUiMessageType::Ack:         return "ack";
        case HtmlUiMessageType::Error:       return "error";
        default:                             return "unknown";
    }
}

struct HtmlUiMessage
{
    HtmlUiMessageType type = HtmlUiMessageType::Event;
    HtmlUiPanelId panel = HtmlUiPanelId::None;
    std::string command;
    json payload;

    json toJson() const
    {
        json j;
        j["type"] = toString(type);
        j["panel"] = toString(panel);
        if (!command.empty())
            j["command"] = command;
        j["payload"] = payload;
        return j;
    }

    static HtmlUiMessage fromJson(const json& j)
    {
        HtmlUiMessage msg;

        const std::string type = j.value("type", "event");
        if (type == "state_update") msg.type = HtmlUiMessageType::StateUpdate;
        else if (type == "command") msg.type = HtmlUiMessageType::Command;
        else if (type == "subscribe") msg.type = HtmlUiMessageType::Subscribe;
        else if (type == "ack") msg.type = HtmlUiMessageType::Ack;
        else if (type == "error") msg.type = HtmlUiMessageType::Error;
        else msg.type = HtmlUiMessageType::Event;

        const std::string panel = j.value("panel", "none");
        if      (panel == "main_menu")        msg.panel = HtmlUiPanelId::MainMenu;
        else if (panel == "confirm_exit")     msg.panel = HtmlUiPanelId::ConfirmExit;
        else if (panel == "ship_core")        msg.panel = HtmlUiPanelId::ShipCore;
        else if (panel == "frustum_debug")    msg.panel = HtmlUiPanelId::FrustumDebug;
        else if (panel == "trade")            msg.panel = HtmlUiPanelId::Trade;
        else if (panel == "shipyard")         msg.panel = HtmlUiPanelId::Shipyard;
        else if (panel == "galaxy_map")       msg.panel = HtmlUiPanelId::GalaxyMap;
        else if (panel == "world_info")       msg.panel = HtmlUiPanelId::WorldInfo;
        else if (panel == "pause_menu")       msg.panel = HtmlUiPanelId::PauseMenu;
        else if (panel == "attachment_editor") msg.panel = HtmlUiPanelId::AttachmentEditor;
        else if (panel == "debug_control")     msg.panel = HtmlUiPanelId::DebugControl;
        else                                   msg.panel = HtmlUiPanelId::None;

        msg.command = j.value("command", "");
        msg.payload = j.value("payload", json::object());
        return msg;
    }
};