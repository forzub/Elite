#pragma once

#include <unordered_map>
#include "ui/html/HtmlUiBridge.h"
#include "ui/html/HtmlUiState.h"

class HtmlUiManager
{
public:
    void start(int port, const std::string& rootDir);
    void stop();

    void setViewport(int width, int height);
    void setActivePanel(HtmlUiPanelId panel);

    void broadcastState(HtmlUiPanelId panel, const json& payload);
    void rebroadcastPanelState(HtmlUiPanelId panel);

    std::vector<HtmlUiMessage> popCommands();

    const HtmlUiState& state() const { return m_state; }

private:
    HtmlUiBridge m_bridge;
    HtmlUiState m_state;
    std::unordered_map<int, json> m_lastPanelStates;
};