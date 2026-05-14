#include "ui/html/HtmlUiManager.h"

void HtmlUiManager::start(int port, const std::string& rootDir)
{
    m_bridge.start(port, rootDir);
}

void HtmlUiManager::stop()
{
    m_bridge.stop();
}

void HtmlUiManager::setViewport(int width, int height)
{
    m_state.viewport.width = width;
    m_state.viewport.height = height;
}

void HtmlUiManager::setActivePanel(HtmlUiPanelId panel)
{
    m_state.activePanel = panel;

    HtmlUiMessage msg;
    msg.type = HtmlUiMessageType::Event;
    msg.panel = panel;
    msg.command = "panel_activated";
    msg.payload = {
        { "viewportWidth",  m_state.viewport.width },
        { "viewportHeight", m_state.viewport.height }
    };

    m_bridge.broadcast(msg);

    rebroadcastPanelState(panel);
}

void HtmlUiManager::broadcastState(HtmlUiPanelId panel, const json& payload)
{
    m_lastPanelStates[(int)panel] = payload;

    HtmlUiMessage msg;
    msg.type = HtmlUiMessageType::StateUpdate;
    msg.panel = panel;
    msg.payload = payload;
    m_bridge.broadcast(msg);
}

void HtmlUiManager::rebroadcastPanelState(HtmlUiPanelId panel)
{
    auto it = m_lastPanelStates.find((int)panel);
    if (it == m_lastPanelStates.end())
        return;

    HtmlUiMessage msg;
    msg.type = HtmlUiMessageType::StateUpdate;
    msg.panel = panel;
    msg.payload = it->second;
    m_bridge.broadcast(msg);
}

std::vector<HtmlUiMessage> HtmlUiManager::popCommands()
{
    return m_bridge.popCommands();
}