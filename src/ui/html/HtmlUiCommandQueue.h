#pragma once

#include <deque>
#include <mutex>
#include <vector>
#include "ui/html/HtmlUiMessage.h"

class HtmlUiCommandQueue
{
public:
    void push(const HtmlUiMessage& msg);
    std::vector<HtmlUiMessage> popAll();

private:
    std::deque<HtmlUiMessage> m_queue;
    std::mutex m_mutex;
};