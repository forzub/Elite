#pragma once

#include <string>
#include "ui/html/HtmlUiServer.h"
#include "ui/html/HtmlUiCommandQueue.h"

class HtmlUiBridge
{
public:
    HtmlUiBridge();
    ~HtmlUiBridge();

    void start(int port, const std::string& rootDir);
    void stop();

    void broadcast(const HtmlUiMessage& msg);
    std::vector<HtmlUiMessage> popCommands();

private:
    HtmlUiServer m_server;
    HtmlUiCommandQueue m_queue;
};