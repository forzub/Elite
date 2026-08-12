#include "ui/html/HtmlUiBridge.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

HtmlUiBridge::HtmlUiBridge()
{
    m_server.setOnMessage([this](const std::string& text)
    {
        try
        {
            json j = json::parse(text);
            HtmlUiMessage msg = HtmlUiMessage::fromJson(j);
            m_queue.push(msg);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[HtmlUiBridge] invalid json: " << e.what() << "\n";
        }
    });
}

HtmlUiBridge::~HtmlUiBridge()
{
    stop();
}

void HtmlUiBridge::start(int port, const std::string& rootDir)
{
    m_server.start(port, rootDir);
}

void HtmlUiBridge::stop()
{
    m_server.stop();
}

void HtmlUiBridge::setVirtualFile(
    const std::string& resource,
    const std::string& content,
    const std::string& contentType
)
{
    m_server.setVirtualFile(resource, content, contentType);
}

void HtmlUiBridge::broadcast(const HtmlUiMessage& msg)
{
    m_server.broadcastText(msg.toJson().dump());
}

std::vector<HtmlUiMessage> HtmlUiBridge::popCommands()
{
    return m_queue.popAll();
}