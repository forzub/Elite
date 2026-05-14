#include "ui/html/HtmlUiCommandQueue.h"

void HtmlUiCommandQueue::push(const HtmlUiMessage& msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(msg);
}

std::vector<HtmlUiMessage> HtmlUiCommandQueue::popAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<HtmlUiMessage> out;
    out.reserve(m_queue.size());

    while (!m_queue.empty())
    {
        out.push_back(m_queue.front());
        m_queue.pop_front();
    }

    return out;
}