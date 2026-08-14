#pragma once

#include <cstdint>
#include <string>
#include "ui/html/HtmlUiServer.h"
#include "ui/html/HtmlUiCommandQueue.h"

class HtmlUiBridge
{
public:
    HtmlUiBridge();
    ~HtmlUiBridge();

    std::uint16_t start(std::uint16_t port, const std::string& rootDir);
    void stop();
    void setVirtualFile(const std::string& resource, const std::string& content, const std::string& contentType);

    void broadcast(const HtmlUiMessage& msg);
    std::vector<HtmlUiMessage> popCommands();

private:
    HtmlUiServer m_server;
    HtmlUiCommandQueue m_queue;
};